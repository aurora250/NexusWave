#include "app_state.hpp"
namespace newave {
    size_t session_state::push(message msg) {
        messages.push_back(neforce::move(msg));
        return messages.size() - 1;
    }

    void session_state::append_assistant(const neforce::string_view chunk) {
        if (messages.empty() || messages.back().kind != row_kind::assistant) {
            message msg;
            msg.kind = row_kind::assistant;
            messages.push_back(neforce::move(msg));
        }
        messages.back().text.append(chunk);
    }

    double session_state::context_ratio() const noexcept {
        if (context_limit == 0) {
            return 0.0;
        }
        return static_cast<double>(context_used) / static_cast<double>(context_limit);
    }

    neforce::string_view session_state::mode_name(const session_mode mode) noexcept {
        switch (mode) {
            case session_mode::plan:
                return "plan";
            case session_mode::full:
                return "full";
            case session_mode::default_:
            default:
                return "default";
        }
    }

    void session_state::cycle_mode() noexcept {
        switch (mode) {
            case session_mode::default_:
                mode = session_mode::plan;
                break;
            case session_mode::plan:
                mode = session_mode::full;
                break;
            case session_mode::full:
            default:
                mode = session_mode::default_;
                break;
        }
    }

    void session_state::clear() {
        messages.clear();
        context_used = 0;
        prompt_tokens = 0;
        reply_tokens = 0;
        busy = false;
    }
} // namespace newave
