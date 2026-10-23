#include "shell.hpp"
#include "agent/json_util.hpp"
#include "ui/views.hpp"
#include <NeForce/core/file/filesystem.hpp>
#include <NeForce/core/file/json/json_parser.hpp>
#include <NeForce/core/file/path.hpp>
#include <NeForce/core/string/to_string.hpp>
#include <NeForce/core/system/environment.hpp>
#include <NeForce/tui/application.hpp>
#include <NeForce/tui/component/motion.hpp>
#include <NeForce/tui/dom/theme.hpp>
namespace newave {
    using namespace neforce;
    using namespace neforce::tui;

    using tui::components::completion_item;
    using tui::components::completion_request;
    using tui::components::composer;
    using tui::components::composer_option;

    namespace {
        constexpr string_view nexus_wave_reply =
                "NexusWave is running on **NexusForce**.\n"
                "\n"
                "This reply is streamed frame by frame so the transcript, markdown layout and\n"
                "status bar can be exercised without a network call.\n"
                "\n"
                "| feature | state |\n"
                "| :--- | ---: |\n"
                "| theme | ready |\n"
                "| motion | ready |\n"
                "| markdown | ready |\n"
                "\n"
                "> Type `/demo` for a richer transcript, or `/help` for commands.\n"
                "\n"
                "Inline math $e^{i\\pi}+1=0$ is rendered as Unicode.\n";
    } // namespace


    shell::shell(string theme_name, unique_ptr<agent::chat_provider> provider) :
    theme_name_(move(theme_name)),
    injected_provider_(move(provider)) {
        const auto& catalog = theme_catalog::instance();
        palette_ = catalog.resolve(theme_name_.empty() ? string_view{"auto"} : theme_name_.view());
        init_agent();
    }

    void shell::init_agent() {
        agent_cfg_.api_key = environment::get("DEEPSEEK_API_KEY");
        const string base = environment::get("NEXUSWAVE_BASE_URL");
        if (!base.empty()) {
            agent_cfg_.base_url = base;
        }
        const string model = environment::get("NEXUSWAVE_MODEL");
        if (!model.empty()) {
            agent_cfg_.model = model;
        }
        const string workspace = environment::get("NEXUSWAVE_WORKSPACE");
        agent_cfg_.workspace = workspace.empty() ? string{path::current_path().to_string()} : workspace;
        agent_cfg_.system_prompt = "You are NexusWave, a terminal coding agent. You can read, edit and search files in "
                                   "the workspace, and run shell commands when the user allows it. Prefer small, "
                                   "verifiable steps and explain what you changed.";
        state_.model = agent_cfg_.model;

        if (injected_provider_ != nullptr) {
            agent_cfg_.api_key = "test";
        }
        tools_ = agent::tool_registry::make_builtin();
        provider_ = make_unique<agent::openai_provider>();
        guard_ = make_unique<agent::policy>(agent::permission_mode::workspace_write, agent_cfg_.workspace);
        string home = environment::get("HOME");
        if (home.empty()) {
            home = environment::get("USERPROFILE");
        }
        if (home.empty()) {
            home = ".";
        }
        string sessions = home;
        if (sessions.back() != '/' && sessions.back() != '\\') {
            sessions += '/';
        }
        sessions += ".nexuswave/sessions";
        session_ = make_unique<agent::session_store>(path{sessions});
        agent::chat_provider& chat = injected_provider_ != nullptr ? *injected_provider_ : *provider_;
        runtime_ = make_unique<agent::agent_runtime>(chat, tools_, *guard_, *session_, agent_cfg_);
    }

    void shell::sync_policy() const {
        if (guard_ == nullptr) {
            return;
        }
        auto mode = agent::permission_mode::workspace_write;
        if (state_.mode == session_mode::plan) {
            mode = agent::permission_mode::read_only;
        } else if (state_.mode == session_mode::full) {
            mode = agent::permission_mode::full;
        }
        guard_->set_mode(mode);
    }

    void shell::handle_agent_event(const agent::event& event) {
        switch (event.kind) {
            case agent::event_kind::text: {
                if (assistant_row_ < 0) {
                    message row;
                    row.kind = row_kind::assistant;
                    state_.push(move(row));
                    assistant_row_ = static_cast<int>(state_.messages.size()) - 1;
                }
                state_.messages[static_cast<size_t>(assistant_row_)].text += event.text;
                *scroll_ = 1 << 20;
                break;
            }
            case agent::event_kind::reasoning: {
                if (thinking_row_ < 0) {
                    message row;
                    row.kind = row_kind::thinking;
                    row.title = "Thinking";
                    state_.push(move(row));
                    thinking_row_ = static_cast<int>(state_.messages.size()) - 1;
                }
                state_.messages[static_cast<size_t>(thinking_row_)].text += event.text;
                break;
            }
            case agent::event_kind::tool_start: {
                message row;
                row.kind = row_kind::tool;
                row.title = event.name;
                row.state = tool_state::running;
                optional<unique_ptr<json_value>> parsed = json_parser{event.arguments}.try_parse();
                if (parsed.has_value() && parsed->get() != nullptr) {
                    row.text = agent::member_string(*parsed->get(), "path");
                    if (row.text.empty()) {
                        row.text = agent::member_string(*parsed->get(), "command");
                    }
                    if (row.text.empty()) {
                        row.text = agent::member_string(*parsed->get(), "pattern");
                    }
                }
                if (row.text.size() > 80) {
                    row.text = string{row.text.view().substr(0, 80)};
                }
                state_.push(move(row));
                tool_row_ = static_cast<int>(state_.messages.size()) - 1;
                *scroll_ = 1 << 20;
                break;
            }
            case agent::event_kind::tool_result: {
                if (tool_row_ >= 0) {
                    auto& row = state_.messages[static_cast<size_t>(tool_row_)];
                    row.state = event.ok ? tool_state::ok : tool_state::failed;
                    row.detail = event.text;
                    if (row.detail.size() > 4000) {
                        row.detail = string{row.detail.view().substr(0, 4000)} + "\n... (truncated)";
                    }
                }
                break;
            }
            case agent::event_kind::usage: {
                if (event.tokens.prompt_tokens > 0) {
                    state_.prompt_tokens = event.tokens.prompt_tokens;
                }
                if (event.tokens.completion_tokens > 0) {
                    state_.reply_tokens = event.tokens.completion_tokens;
                }
                state_.context_used = state_.prompt_tokens + state_.reply_tokens;
                break;
            }
            case agent::event_kind::approval: {
                approval_open_ = true;
                approval_name_ = event.name;
                approval_arguments_ = event.arguments;
                break;
            }
            case agent::event_kind::done: {
                state_.busy = false;
                assistant_row_ = -1;
                thinking_row_ = -1;
                tool_row_ = -1;
                *scroll_ = 1 << 20;
                break;
            }
            case agent::event_kind::error: {
                message row;
                row.kind = row_kind::error;
                row.text = event.text;
                state_.push(move(row));
                state_.busy = false;
                assistant_row_ = -1;
                thinking_row_ = -1;
                tool_row_ = -1;
                *scroll_ = 1 << 20;
                break;
            }
        }
    }

    void shell::setup() {
        scroll_ = &create_state<int>(0);
        viewport_rows_ = &create_state<int>(10);
        viewport_rect_ = &create_state<layout_rect>(layout_rect{});

        composer_option opt;
        opt.placeholder = "Ask anything, or type /help";
        opt.hint = "enter send  ·  shift+enter newline";
        opt.running_hint = "esc to interrupt";
        opt.history_limit = 100;
        opt.complete = [](const completion_request& request, vector<completion_item>& items) {
            vector<string> names;
            complete_commands(request.query.view(), names);
            for (const auto& name: names) {
                completion_item item;
                item.label = "/" + name;
                const command_spec* spec = find_command(name.view());
                if (spec != nullptr) {
                    item.detail = string{spec->summary};
                }
                item.insert = "/" + name + " ";
                items.push_back(move(item));
            }
        };
        opt.on_submit = [this](const string& text) { submit(text); };

        auto owned = make_unique<composer>(move(opt));
        composer_view_ = owned.get();
        // Unhandled keys bubble to the shell so the global ladder still works.
        composer_view_->set_pass_through(true);
        add_child(move(owned));
    }

    element shell::render() {
        drain_agent_events();
        const int columns = max(20, viewport_rect_->value().w > 0 ? viewport_rect_->value().w : 80);
        const int body_width = max(10, columns - 2);

        elements layout;

        if (state_.messages.empty()) {
            layout.push_back(views::splash(palette_, now_ms_, body_width));
            layout.push_back(element::text(string(), tui::style{}));
            layout.push_back(element::spacer(1));
        } else {
            row_heights_.resize(state_.messages.size(), 0);
            for (size_t i = 0; i < state_.messages.size(); ++i) {
                if (row_heights_[i] <= 0) {
                    row_heights_[i] = 1;
                }
            }

            element list = element::virtual_list(
                    state_.messages.size(), 3, &row_heights_,
                    [this, body_width](const size_t index) {
                        return views::transcript_row(state_.messages[index], palette_, body_width, state_.math_enabled);
                    },
                    scroll_->value());
            list = list.with_scroll_y_state(scroll_);
            list = list.with_viewport_state(viewport_rows_);
            list = list.with_viewport_rect_state(viewport_rect_);
            list = list.with_style([&] {
                tui::style st;
                st.height = tui::style::size_hint{tui::style::size_hint::fill, 1};
                return st;
            }());
            layout.push_back(move(list));
        }

        if (state_.busy) {
            layout.push_back(views::activity_line(palette_, now_ms_));
        }

        if (approval_open_) {
            layout.push_back(views::approval_panel(approval_name_, approval_arguments_, palette_, body_width));
        }

        if (help_open_) {
            layout.push_back(views::help_view(palette_, body_width));
        } else if (composer_view_ != nullptr) {
            composer_view_->take_focus();
            layout.push_back(composer_view_->render());
        }

        layout.push_back(views::status_bar(state_, palette_, body_width, now_ms_));

        tui::style frame;
        struct tui::style::padding pad;
        pad.left = 1;
        pad.right = 1;
        frame.padding = pad;
        return element::vbox(move(layout), tui::box_props{}).with_style(frame);
    }

    bool shell::input_empty() const { return composer_view_ == nullptr || composer_view_->text().empty(); }

    bool shell::preview_key(const tui::key_event& e) {
        if (approval_open_) {
            const bool printable = e.key == tui::key_event::type::printable;
            const char32_t key = printable ? e.cp.value() : 0;
            if (key == U'y') {
                if (runtime_ != nullptr) {
                    runtime_->resolve_approval(true);
                }
                approval_open_ = false;
            } else if (key == U'a') {
                state_.mode = session_mode::full;
                sync_policy();
                if (runtime_ != nullptr) {
                    runtime_->resolve_approval(true);
                }
                approval_open_ = false;
            } else if (key == U'n' || e.key == tui::key_event::type::escape) {
                if (runtime_ != nullptr) {
                    runtime_->resolve_approval(false);
                }
                approval_open_ = false;
            }
            return true;
        }

        if (help_open_) {
            const key_intent intent = resolve_help_key(e);
            if (intent == key_intent::toggle_help) {
                help_open_ = false;
                return true;
            }
            if (intent == key_intent::page_up || intent == key_intent::page_down) {
                return false;
            }
        }
        if (e.key == tui::key_event::type::tab && (e.mods & tui::key_modifier::shift) != tui::key_modifier::none) {
            state_.cycle_mode();
            return true;
        }
        key_context ctx;
        ctx.input_empty = input_empty();
        ctx.help_open = help_open_;
        ctx.busy = state_.busy;
        if (resolve_key(e, ctx) == key_intent::toggle_help) {
            help_open_ = !help_open_;
            return true;
        }
        // An exact `/command` runs instead of accepting the completion popup:
        // Enter on a fully typed command is an execution, not a fill.
        if (e.key == tui::key_event::type::enter && composer_view_ != nullptr) {
            const parsed_command typed = parse_command(composer_view_->text());
            if (typed.is_command && typed.args.empty() && find_command(typed.name.view()) != nullptr) {
                const auto text = string{composer_view_->text()};
                composer_view_->set_text(string());
                submit(text);
                return true;
            }
        }
        return false;
    }

    bool shell::on_key(const tui::key_event& e) {
        key_context ctx;
        ctx.input_empty = input_empty();
        ctx.help_open = help_open_;
        ctx.busy = state_.busy;

        const key_intent intent = help_open_ ? resolve_help_key(e) : resolve_key(e, ctx);
        switch (intent) {
            case key_intent::toggle_help: {
                help_open_ = !help_open_;
                if (!help_open_ && composer_view_ != nullptr) {
                    composer_view_->take_focus();
                }
                return true;
            }
            case key_intent::interrupt: {
                state_.busy = false;
                push_notice("interrupted");
                return true;
            }
            case key_intent::clear_input: {
                if (composer_view_ != nullptr) {
                    composer_view_->set_text(string());
                }
                return true;
            }
            case key_intent::cycle_mode: {
                state_.cycle_mode();
                return true;
            }
            case key_intent::scroll_up: {
                scroll_by(-3);
                return true;
            }
            case key_intent::scroll_down: {
                scroll_by(3);
                return true;
            }
            case key_intent::page_up: {
                scroll_by(-max(1, viewport_rows_->value() - 1));
                return true;
            }
            case key_intent::page_down: {
                scroll_by(max(1, viewport_rows_->value() - 1));
                return true;
            }
            case key_intent::toggle_expand: {
                for (auto& msg: state_.messages) {
                    if (msg.kind == row_kind::tool || msg.kind == row_kind::thinking) {
                        msg.expanded = !msg.expanded;
                    }
                }
                return true;
            }
            case key_intent::toggle_math: {
                state_.math_enabled = !state_.math_enabled;
                push_notice(state_.math_enabled ? "math rendering on" : "math rendering off");
                return true;
            }
            case key_intent::redraw:
            case key_intent::quit:
            case key_intent::force_quit: {
                return false;
            }
            case key_intent::none:
            default:
                break;
        }

        if (!help_open_ && composer_view_ != nullptr) {
            composer_view_->take_focus();
            return composer_view_->on_key(e);
        }
        return false;
    }

    bool shell::on_paste(const tui::paste_event& e) {
        if (help_open_ || composer_view_ == nullptr) {
            return false;
        }
        return composer_view_->on_paste(e);
    }

    bool shell::on_mouse(const tui::mouse_event& e) {
        if (e.action == tui::mouse_action::wheel) {
            scroll_by(e.button == tui::mouse_button::wheelup ? -3 : 3);
            return true;
        }
        return false;
    }

    void shell::drain_agent_events() {
        if (runtime_ == nullptr) {
            return;
        }
        vector<agent::event> events;
        runtime_->drain(events);
        for (const auto& event: events) {
            handle_agent_event(event);
        }
        state_.busy = runtime_->busy();
    }

    void shell::on_animation(const int64_t delta) {
        now_ms_ += delta;
        drain_agent_events();
        if (!state_.busy || pending_offset_ >= pending_reply_.size()) {
            if (state_.busy && pending_offset_ >= pending_reply_.size()) {
                state_.busy = false;
                state_.reply_tokens = pending_reply_.size() / 4;
                state_.context_used += pending_reply_.size() / 4;
            }
            return;
        }
        const size_t step = max<size_t>(3, pending_reply_.size() / 120);
        const size_t end = min(pending_reply_.size(), pending_offset_ + step);
        state_.append_assistant(pending_reply_.view().substr(pending_offset_, end - pending_offset_));
        pending_offset_ = end;
        state_.reply_tokens = pending_offset_ / 4;
        state_.context_used += step / 4;
    }

    void shell::scroll_by(const int rows) {
        const int target = max(0, scroll_->value() + rows);
        *scroll_ = target;
    }

    void shell::push_notice(string text) {
        message msg;
        msg.kind = row_kind::notice;
        msg.text = move(text);
        state_.push(move(msg));
        *scroll_ = 1 << 20;
    }

    void shell::submit(const string& text) {
        if (text.empty()) {
            return;
        }
        const parsed_command command = parse_command(text.view());
        if (command.is_command) {
            execute(command);
            return;
        }
        if (runtime_ == nullptr || agent_cfg_.api_key.empty()) {
            push_notice("no DEEPSEEK_API_KEY set - use /demo for an offline sample");
            return;
        }
        message msg;
        msg.kind = row_kind::user;
        msg.text = text;
        state_.push(move(msg));
        *scroll_ = 1 << 20;

        assistant_row_ = -1;
        thinking_row_ = -1;
        tool_row_ = -1;
        approval_open_ = false;
        state_.busy = true;
        runtime_->start(text);
    }

    void shell::start_demo_reply(const string_view prompt) {
        message thinking;
        thinking.kind = row_kind::thinking;
        thinking.title = "Thinking";
        thinking.elapsed_ms = 1400;
        thinking.text = "The user asked: " + string(prompt);
        state_.push(move(thinking));

        message tool;
        tool.kind = row_kind::tool;
        tool.title = "fs.read";
        tool.text = "README.md";
        tool.state = tool_state::ok;
        state_.push(move(tool));

        pending_reply_ = string{nexus_wave_reply};
        pending_offset_ = 0;
        state_.busy = true;
        *scroll_ = 1 << 20;
    }

    void shell::push_tool_demo() {
        message user;
        user.kind = row_kind::user;
        user.text = "show me a table, some math and a tool card";
        state_.push(move(user));

        message thinking;
        thinking.kind = row_kind::thinking;
        thinking.text = "Planning the answer with a table and a formula.";
        thinking.elapsed_ms = 900;
        state_.push(move(thinking));

        message answer;
        answer.kind = row_kind::assistant;
        answer.text = "Here is a **table** and a display formula.\n"
                      "\n"
                      "| step | owner | state |\n"
                      "| :--- | :---: | ---: |\n"
                      "| parse | al | done |\n"
                      "| render | me | wip |\n"
                      "\n"
                      "$$\n"
                      "\\frac{-b\\pm\\sqrt{b^2-4ac}}{2a}\n"
                      "$$\n"
                      "\n"
                      "```mermaid\n"
                      "graph TD\n"
                      "A[Prompt] --> B[Model]\n"
                      "B --> C{Tool?}\n"
                      "C -->|yes| D[Run]\n"
                      "C -->|no| E[Answer]\n"
                      "```\n";
        state_.push(move(answer));

        message tool;
        tool.kind = row_kind::tool;
        tool.title = "bash";
        tool.text = "cmake --build build";
        tool.state = tool_state::ok;
        tool.expanded = true;
        state_.push(move(tool));

        *scroll_ = 1 << 20;
    }

    void shell::apply_theme(const string_view name) {
        const auto found = theme_catalog::instance().find(name);
        if (!found.has_value()) {
            push_notice("unknown theme: " + string(name));
            return;
        }
        palette_ = *found;
        theme_name_ = string{name};
        push_notice("theme: " + string{name});
    }

    void shell::export_markdown(const string_view path_text) {
        if (path_text.empty()) {
            push_notice("usage: /export <path>");
            return;
        }
        string content;
        for (const auto& msg: state_.messages) {
            switch (msg.kind) {
                case row_kind::user:
                    content += "**You:** " + msg.text + "\n\n";
                    break;
                case row_kind::assistant:
                    content += msg.text + "\n\n";
                    break;
                case row_kind::tool:
                    content += "- tool `" + msg.title + "`: " + msg.text + "\n";
                    break;
                default:
                    break;
            }
        }
        const path target{string{path_text}};
        if (filesystem::create_and_write(target, content)) {
            push_notice("exported to " + string{path_text});
        } else {
            push_notice("export failed: " + string{path_text});
        }
    }

    void shell::execute(const parsed_command& command) {
        if (command.name == "help") {
            help_open_ = true;
            return;
        }
        if (command.name == "clear" || command.name == "new") {
            state_.clear();
            *scroll_ = 0;
            pending_reply_.clear();
            pending_offset_ = 0;
            push_notice(command.name == "new" ? "new session" : "cleared");
            return;
        }
        if (command.name == "theme") {
            if (command.args == "list") {
                string names;
                for (const auto& name: theme_catalog::instance().names()) {
                    names += name + " ";
                }
                push_notice(names);
            } else if (command.args.empty()) {
                push_notice("theme: " + theme_name_);
            } else {
                apply_theme(command.args.view());
            }
            return;
        }
        if (command.name == "mode") {
            if (command.args == "plan") {
                state_.mode = session_mode::plan;
            } else if (command.args == "full") {
                state_.mode = session_mode::full;
            } else if (command.args == "default") {
                state_.mode = session_mode::default_;
            } else {
                push_notice("usage: /mode <default|plan|full>");
                return;
            }
            push_notice(string{"mode: "} + string{session_state::mode_name(state_.mode)});
            return;
        }
        if (command.name == "model") {
            if (!command.args.empty()) {
                state_.model = command.args;
            }
            push_notice("model: " + state_.model);
            return;
        }
        if (command.name == "status") {
            const string session_id = session_ != nullptr && session_->active() ? session_->id() : string{"(new)"};
            push_notice("session " + session_id + " · key " + (agent_cfg_.api_key.empty() ? "missing" : "set"));
            push_notice("model " + state_.model + " · mode " + string{session_state::mode_name(state_.mode)} +
                        " · ctx " + to_string(static_cast<int64_t>(state_.context_used)) + "/" +
                        to_string(static_cast<int64_t>(state_.context_limit)) + " · ↑" +
                        to_string(static_cast<int64_t>(state_.prompt_tokens)) + " ↓" +
                        to_string(static_cast<int64_t>(state_.reply_tokens)));
            return;
        }
        if (command.name == "math") {
            state_.math_enabled = command.args != "off";
            push_notice(state_.math_enabled ? "math rendering on" : "math rendering off");
            return;
        }
        if (command.name == "ask") {
            if (command.args.empty()) {
                push_notice("usage: /ask <text>");
                return;
            }
            message user;
            user.kind = row_kind::user;
            user.text = command.args;
            state_.push(move(user));
            start_demo_reply(command.args.view());
            return;
        }
        if (command.name == "demo") {
            push_tool_demo();
            return;
        }
        if (command.name == "export") {
            export_markdown(command.args.view());
            return;
        }
        if (command.name == "about") {
            push_notice("NexusWave 0.1.0 · NexusForce TUI · theme " + theme_name_);
            return;
        }
        if (command.name == "quit") {
            tui::application::request_quit(0);
            return;
        }
        push_notice("unknown command: /" + command.name);
    }
} // namespace newave
