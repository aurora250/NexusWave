#include "session.hpp"
#include "json_util.hpp"
#include <NeForce/core/file/file.hpp>
#include <NeForce/core/file/filesystem.hpp>
#include <NeForce/core/file/json/json_parser.hpp>
#include <NeForce/core/string/to_string.hpp>
#include <NeForce/core/time/clocks.hpp>
namespace newave::agent {
    using namespace neforce;

    namespace {
        string role_name(const role who) {
            switch (who) {
                case role::system:
                    return string{"system"};
                case role::user:
                    return string{"user"};
                case role::assistant:
                    return string{"assistant"};
                case role::tool:
                    return string{"tool"};
            }
            return string{"user"};
        }

        role parse_role(const string_view name) noexcept {
            if (name == "system") {
                return role::system;
            }
            if (name == "assistant") {
                return role::assistant;
            }
            if (name == "tool") {
                return role::tool;
            }
            return role::user;
        }

        string encode(const message& msg) {
            string line{"{\"role\":\""};
            line += role_name(msg.who);
            line += "\",\"content\":\"";
            line += escape_json(msg.content.view());
            line += "\"";
            if (!msg.reasoning.empty()) {
                line += ",\"reasoning\":\"";
                line += escape_json(msg.reasoning.view());
                line += "\"";
            }
            if (!msg.tool_call_id.empty()) {
                line += ",\"tool_call_id\":\"";
                line += escape_json(msg.tool_call_id.view());
                line += "\"";
            }
            if (!msg.name.empty()) {
                line += ",\"name\":\"";
                line += escape_json(msg.name.view());
                line += "\"";
            }
            if (!msg.calls.empty()) {
                line += ",\"calls\":[";
                for (size_t i = 0; i < msg.calls.size(); ++i) {
                    if (i > 0) {
                        line += ",";
                    }
                    line += "{\"id\":\"";
                    line += escape_json(msg.calls[i].id.view());
                    line += "\",\"name\":\"";
                    line += escape_json(msg.calls[i].name.view());
                    line += "\",\"arguments\":\"";
                    line += escape_json(msg.calls[i].arguments.view());
                    line += "\"}";
                }
                line += "]";
            }
            line += "}";
            return line;
        }

        optional<message> decode(const string_view line) {
            optional<unique_ptr<json_value>> parsed = json_parser{string{line}}.try_parse();
            if (!parsed.has_value() || parsed->get() == nullptr || parsed->get()->as_object() == nullptr) {
                return none;
            }
            message msg;
            msg.who = parse_role(member_string(*parsed->get(), "role").view());
            msg.content = member_string(*parsed->get(), "content");
            msg.reasoning = member_string(*parsed->get(), "reasoning");
            msg.tool_call_id = member_string(*parsed->get(), "tool_call_id");
            msg.name = member_string(*parsed->get(), "name");
            const auto* object = parsed->get()->as_object();
            const json_value* calls = object->get_member(string{"calls"});
            if (calls != nullptr && calls->as_array() != nullptr) {
                for (const auto& element: calls->as_array()->get_elements()) {
                    if (element == nullptr || element->as_object() == nullptr) {
                        continue;
                    }
                    tool_call call;
                    call.id = member_string(*element, "id");
                    call.name = member_string(*element, "name");
                    call.arguments = member_string(*element, "arguments");
                    msg.calls.push_back(move(call));
                }
            }
            return msg;
        }
    } // namespace

    session_store::session_store(path directory) :
    directory_(move(directory)) {
        filesystem::create_directories(path{directory_});
    }

    string session_store::start_new() {
        const auto now = system_clock::now().since_epoch();
        const auto stamp = time_cast<seconds>(now).count();
        id_ = to_string(stamp);
        file_ = directory_.str();
        if (!file_.empty() && file_.back() != '/' && file_.back() != '\\') {
            file_ += '/';
        }
        file_ += id_;
        file_ += ".jsonl";
        return id_;
    }

    bool session_store::append(const message& msg) {
        if (file_.empty()) {
            start_new();
        }
        return filesystem::create_and_write(path{file_}, encode(msg) + "\n", true);
    }

    vector<message> session_store::load_current() const {
        if (id_.empty()) {
            return {};
        }
        return load(id_.view());
    }

    vector<message> session_store::load(const string_view id) const {
        string log_path = directory_.str();
        if (!log_path.empty() && log_path.back() != '/' && log_path.back() != '\\') {
            log_path += '/';
        }
        log_path.append(id);
        log_path += ".jsonl";
        const path location{log_path};
        if (!location.exists()) {
            return {};
        }
        file input{location, false, file_access::read};
        if (!input.is_opened()) {
            return {};
        }
        vector<message> result;
        for (auto it = input.begin_lines(); it != input.end_lines(); ++it) {
            const string& line = *it;
            if (line.empty()) {
                continue;
            }
            if (auto decoded = decode(line.view())) {
                result.push_back(move(*decoded));
            }
        }
        return result;
    }
} // namespace newave::agent
