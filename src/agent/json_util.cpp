#include "json_util.hpp"
#include "tools.hpp"
#include <NeForce/core/string/to_string.hpp>
namespace newave::agent {
    using namespace neforce;

    string escape_json(const string_view text) {
        string out;
        out.reserve(text.size() + 8);
        for (const char c: text) {
            switch (c) {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        out += "\\u00";
                        constexpr string_view hex = "0123456789abcdef";
                        out += hex[(c >> 4) & 0x0F];
                        out += hex[c & 0x0F];
                    } else {
                        out += c;
                    }
                    break;
            }
        }
        return out;
    }

    string messages_to_json(const vector<message>& messages) {
        string out{"["};
        for (size_t i = 0; i < messages.size(); ++i) {
            if (i > 0) {
                out += ",";
            }
            const message& msg = messages[i];
            out += "{\"role\":\"";
            switch (msg.who) {
                case role::system:
                    out += "system";
                    break;
                case role::user:
                    out += "user";
                    break;
                case role::assistant:
                    out += "assistant";
                    break;
                case role::tool:
                    out += "tool";
                    break;
            }
            out += "\",\"content\":\"";
            out += escape_json(msg.content.view());
            out += "\"";
            if (msg.who == role::tool) {
                out += ",\"tool_call_id\":\"";
                out += escape_json(msg.tool_call_id.view());
                out += "\"";
            }
            if (!msg.calls.empty()) {
                out += ",\"tool_calls\":[";
                for (size_t c = 0; c < msg.calls.size(); ++c) {
                    if (c > 0) {
                        out += ",";
                    }
                    out += "{\"id\":\"";
                    out += escape_json(msg.calls[c].id.view());
                    out += "\",\"type\":\"function\",\"function\":{\"name\":\"";
                    out += escape_json(msg.calls[c].name.view());
                    out += "\",\"arguments\":\"";
                    out += escape_json(msg.calls[c].arguments.view());
                    out += "\"}}";
                }
                out += "]";
            }
            out += "}";
        }
        out += "]";
        return out;
    }

    string tools_to_json(const vector<tool_spec>& specs) {
        string out{"["};
        for (size_t i = 0; i < specs.size(); ++i) {
            if (i > 0) {
                out += ",";
            }
            out += "{\"type\":\"function\",\"function\":{\"name\":\"";
            out += escape_json(specs[i].name.view());
            out += "\",\"description\":\"";
            out += escape_json(specs[i].description.view());
            out += "\",\"parameters\":";
            out += specs[i].parameters;
            out += "}}";
        }
        out += "]";
        return out;
    }

    string member_string(const json_value& value, const string_view key) {
        const auto* object = value.as_object();
        if (object == nullptr) {
            return string();
        }
        const json_value* member = object->get_member(string{key});
        if (member == nullptr || member->as_string() == nullptr) {
            return string();
        }
        return member->as_string()->get_value();
    }

    int64_t member_int(const json_value& value, const string_view key, const int64_t fallback) {
        const auto* object = value.as_object();
        if (object == nullptr) {
            return fallback;
        }
        const json_value* member = object->get_member(string{key});
        if (member == nullptr || member->as_number() == nullptr) {
            return fallback;
        }
        return static_cast<int64_t>(member->as_number()->get_value());
    }
} // namespace newave::agent
