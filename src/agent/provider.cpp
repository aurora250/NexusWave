#include "provider.hpp"
#include "json_util.hpp"
#include "sse.hpp"
#include <NeForce/core/async/io_context.hpp>
#include <NeForce/core/file/json/json_parser.hpp>
#include <NeForce/core/string/to_string.hpp>
#include <NeForce/network/util/url.hpp>
namespace newave::agent {
    using namespace neforce;

    namespace {
        string build_body(const vector<message>& messages, const vector<tool_spec>& tools, const config& cfg) {
            string body{"{\"model\":\""};
            body += escape_json(cfg.model.view());
            body += "\",\"messages\":";
            body += messages_to_json(messages);
            body += ",\"stream\":true,\"max_tokens\":";
            body += to_string(static_cast<int64_t>(cfg.max_tokens));
            if (!tools.empty()) {
                body += ",\"tools\":";
                body += tools_to_json(tools);
            }
            body += ",\"stream_options\":{\"include_usage\":true}}";
            return body;
        }

        struct stream_state {
            completion result;
            vector<tool_call> calls;
            vector<string> pending_arguments;
        };

        void apply_event(const string_view payload, stream_state& state, const delta_handler& on_delta) {
            if (payload == "[DONE]") {
                if (state.result.finish_reason.empty()) {
                    state.result.finish_reason = "done";
                }
                return;
            }
            optional<unique_ptr<json_value>> parsed = json_parser{string{payload}}.try_parse();
            if (!parsed.has_value() || parsed->get() == nullptr || parsed->get()->as_object() == nullptr) {
                return;
            }
            const json_value& root = *parsed->get();
            const auto* object = root.as_object();
            if (const json_value* usage = object->get_member(string{"usage"});
                usage != nullptr && usage->as_object() != nullptr) {
                const int64_t prompt = member_int(*usage, "prompt_tokens");
                const int64_t completion = member_int(*usage, "completion_tokens");
                if (prompt > 0) {
                    state.result.tokens.prompt_tokens = static_cast<size_t>(prompt);
                }
                if (completion > 0) {
                    state.result.tokens.completion_tokens = static_cast<size_t>(completion);
                }
            }

            const json_value* choices = object->get_member(string{"choices"});
            if (choices == nullptr || choices->as_array() == nullptr || choices->as_array()->get_elements().empty()) {
                delta usage_only;
                usage_only.tokens = state.result.tokens;
                on_delta(usage_only);
                return;
            }
            const json_value* first = choices->as_array()->get_elements().front().get();
            if (first == nullptr || first->as_object() == nullptr) {
                return;
            }
            const string finish = member_string(*first, "finish_reason");
            if (!finish.empty()) {
                state.result.finish_reason = finish;
            }
            const json_value* delta_value = first->as_object()->get_member(string{"delta"});
            if (delta_value == nullptr || delta_value->as_object() == nullptr) {
                return;
            }
            delta chunk;
            chunk.content = member_string(*delta_value, "content");
            chunk.reasoning = member_string(*delta_value, "reasoning_content");
            chunk.tokens = state.result.tokens;

            if (const json_value* calls = delta_value->as_object()->get_member(string{"tool_calls"});
                calls != nullptr && calls->as_array() != nullptr) {
                for (const auto& element: calls->as_array()->get_elements()) {
                    if (element == nullptr || element->as_object() == nullptr) {
                        continue;
                    }
                    const int64_t index = member_int(*element, "index");
                    const size_t slot = static_cast<size_t>(max<int64_t>(0, index));
                    if (state.calls.size() <= slot) {
                        state.calls.resize(slot + 1);
                        state.pending_arguments.resize(slot + 1);
                    }
                    const string id = member_string(*element, "id");
                    if (!id.empty()) {
                        state.calls[slot].id = id;
                    }
                    const json_value* function = element->as_object()->get_member(string{"function"});
                    if (function != nullptr && function->as_object() != nullptr) {
                        const string name = member_string(*function, "name");
                        if (!name.empty()) {
                            state.calls[slot].name = name;
                        }
                        const string args = member_string(*function, "arguments");
                        state.pending_arguments[slot] += args;
                    }
                }
            }

            if (!chunk.content.empty()) {
                state.result.content += chunk.content;
            }
            if (!chunk.reasoning.empty()) {
                state.result.reasoning += chunk.reasoning;
            }
            if (!chunk.content.empty() || !chunk.reasoning.empty() || chunk.tokens.prompt_tokens > 0) {
                on_delta(chunk);
            }
        }
    } // namespace


    completion openai_provider::complete(const vector<message>& messages, const vector<tool_spec>& tools,
                                         const config& cfg, const delta_handler& on_delta) {
        completion result;
        if (cfg.api_key.empty()) {
            result.error = "missing API key";
            return result;
        }

        url endpoint;
        try {
            endpoint = url::parse((cfg.base_url + "/chat/completions").view());
        } catch (const exception& e) {
            result.error = string{"invalid base url: "} + e.what();
            return result;
        }

        io_context context;
        http::http_client client{context};
        http::http_client_request request;
        request.method = http::http_method::POST();
        request.scheme = endpoint.scheme;
        request.host = endpoint.host;
        request.port = endpoint.port;
        request.path = endpoint.path;
        request.headers["Content-Type"] = "application/json";
        request.headers["Accept"] = "text/event-stream";
        request.headers["Authorization"] = "Bearer " + cfg.api_key;
        request.body = build_body(messages, tools, cfg);

        stream_state state;
        sse_parser parser;
        http::http_stream_observer observer;
        observer.on_data = [&parser, &state, &on_delta](const string_view chunk) {
            parser.feed(chunk,
                        [&state, &on_delta](const string_view payload) { apply_event(payload, state, on_delta); });
            return true;
        };

        try {
            const http::http_client_response response = client.request_stream(request, observer, &handle_);
            const auto code = static_cast<int>(response.status);
            if (code >= 400) {
                result.error =
                        "HTTP " + to_string(static_cast<int64_t>(code)) + ": " +
                        (response.body.size() > 400 ? string{response.body.view().substr(0, 400)} : response.body);
                return result;
            }
        } catch (const exception& e) {
            result.error = e.what();
            return result;
        }

        result.ok = true;
        result.content = move(state.result.content);
        result.reasoning = move(state.result.reasoning);
        result.tokens = state.result.tokens;
        result.finish_reason = state.result.finish_reason;
        for (size_t i = 0; i < state.calls.size(); ++i) {
            tool_call call = move(state.calls[i]);
            call.arguments = move(state.pending_arguments[i]);
            if (!call.name.empty()) {
                result.calls.push_back(move(call));
            }
        }
        return result;
    }

    void openai_provider::cancel() { handle_.abort(); }
} // namespace newave::agent
