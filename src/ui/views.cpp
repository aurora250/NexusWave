#include "views.hpp"
#include "app/commands.hpp"
#include "glyphs.hpp"
#include <NeForce/core/string/to_string.hpp>
#include <NeForce/tui/component/motion.hpp>
#include <NeForce/tui/dom/gauge.hpp>
#include <NeForce/tui/dom/markdown.hpp>
#include <NeForce/tui/dom/text_layout.hpp>
namespace newave::views {
    using namespace neforce;
    using namespace neforce::tui;

    namespace {
        style text_style_of(const theme& palette) {
            style st;
            st.fg = palette.get(theme_role::text);
            return st;
        }

        style dim_style_of(const theme& palette) {
            style st;
            st.fg = palette.get(theme_role::inactive);
            st.dim = true;
            return st;
        }

        string human_tokens(const size_t tokens) {
            if (tokens >= 1000000) {
                return to_string(static_cast<int64_t>(tokens / 100000)) + "." +
                       to_string(static_cast<int64_t>((tokens / 10000) % 10)) + "M";
            }
            if (tokens >= 1000) {
                return to_string(static_cast<int64_t>(tokens / 1000)) + "." +
                       to_string(static_cast<int64_t>((tokens / 100) % 10)) + "k";
            }
            return to_string(static_cast<int64_t>(tokens));
        }

        element markdown_block(const string& text, const theme& palette, const int width, const bool math) {
            markdown_option opt;
            opt.width = width;
            opt.parse_math = math;
            opt.style.fg = palette.get(theme_role::text);
            opt.heading_style.fg = palette.get(theme_role::accent);
            opt.heading_style.bold = true;
            opt.quote_style.fg = palette.get(theme_role::inactive);
            opt.list_style.fg = palette.get(theme_role::permission);
            opt.link_style.fg = palette.get(theme_role::accent);
            opt.math_style.fg = palette.get(theme_role::syntax_function);
            opt.code_style.fg = palette.get(theme_role::syntax_variable);
            return markdown(text.view(), opt);
        }
    } // namespace

    element splash(const theme& palette, const int64_t now_ms, const int columns) {
        constexpr string_view wordmark{"NEXUSWAVE"};
        const int width = text_display_width(wordmark);
        const vector<neforce::color> ramp =
                shimmer_sweep(width, now_ms, palette.get(theme_role::accent), palette.get(theme_role::accent_shimmer));

        vector<text_span> spans;
        for (int i = 0; i < width; ++i) {
            style st;
            st.fg = ramp[static_cast<size_t>(i)];
            st.bold = true;
            spans.push_back(text_span{string{wordmark.substr(static_cast<size_t>(i), 1)}, st, string()});
        }

        elements rows;
        rows.push_back(element::rich_text(move(spans), style{}, style::wrap_mode::none));
        rows.push_back(element::text(string("terminal agent shell"), dim_style_of(palette), style::wrap_mode::none));
        rows.push_back(element::text(string(), style{}));
        rows.push_back(element::text(string("type a message, or /help for commands"), dim_style_of(palette),
                                     style::wrap_mode::none));
        (void) columns;
        return element::vbox(move(rows));
    }

    element status_bar(const session_state& state, const theme& palette, const int columns, const int64_t now_ms) {
        elements rows;

        const int bar_width = max(8, columns - 12);
        const double ratio = state.context_ratio();
        element bar = gauge(static_cast<float>(ratio), bar_width);
        bar = bar.with_style(style{});

        string percent = to_string(static_cast<int64_t>(ratio * 100.0)) + "%";
        style bar_style;
        bar_style.fg = ratio >= 0.95 ? palette.get(theme_role::error)
                                     : (ratio >= 0.8 ? palette.get(theme_role::warning)
                                                     : palette.get(theme_role::context_bar_assistant));

        elements bar_row;
        bar_row.push_back(element::text(string(" "), style{}));
        bar_row.push_back(bar);
        bar_row.push_back(element::text(string("  ctx ") + move(percent), bar_style, style::wrap_mode::none));
        rows.push_back(element::hbox(move(bar_row)));

        string fields = string{state.model} + "  ·  " + string{session_state::mode_name(state.mode)} +
                        "  ·  \xe2\x86\x91 " + human_tokens(state.prompt_tokens) + "  \xe2\x86\x93 " +
                        human_tokens(state.reply_tokens);
        rows.push_back(element::text(string(" ") + move(fields), dim_style_of(palette), style::wrap_mode::none));

        string hint = state.busy ? string{"esc to interrupt"}
                                 : string{"? shortcuts  ·  /help commands  ·  ctrl+c twice to quit"};
        if (state.busy) {
            const auto* spinner = find_spinner(string_view{"braille"});
            if (spinner != nullptr) {
                hint = string{spinner_frame(*spinner, now_ms)} + " working  ·  " + hint;
            }
        }
        style hint_style = dim_style_of(palette);
        hint_style.fg = state.busy ? palette.get(theme_role::activity) : palette.get(theme_role::subtle);
        rows.push_back(element::text(string(" ") + hint, hint_style, style::wrap_mode::none));

        return element::vbox(move(rows));
    }

    element activity_line(const theme& palette, const int64_t now_ms) {
        const auto* spinner = find_spinner(string_view{"braille"});
        const string_view frame = spinner != nullptr ? spinner_frame(*spinner, now_ms) : string_view{"*"};
        style st;
        st.fg = palette.get(theme_role::activity);
        elements row;
        row.push_back(element::text(string(" ") + string{frame} + " ", st, style::wrap_mode::none));
        row.push_back(element::text(string("thinking"), dim_style_of(palette), style::wrap_mode::none));
        return element::hbox(move(row));
    }

    element transcript_row(const message& msg, const theme& palette, const int width, const bool math) {
        const int body_width = max(8, width - 2);
        switch (msg.kind) {
            case row_kind::user: {
                elements row;
                style pointer_style;
                pointer_style.fg = palette.get(theme_role::accent);
                pointer_style.bold = true;
                row.push_back(element::text(string{glyphs::pointer} + " ", pointer_style, style::wrap_mode::none));
                style body = text_style_of(palette);
                body.bold = true;
                style width_style;
                width_style.width = style::size_hint{style::size_hint::fill, 1};
                element text_body = element::rich_text(vector<text_span>{text_span{msg.text, body, string()}}, body,
                                                       style::wrap_mode::word);
                text_body = text_body.with_style(width_style);
                row.push_back(move(text_body));
                return element::hbox(move(row));
            }
            case row_kind::assistant: {
                elements row;
                style bullet;
                bullet.fg = palette.get(theme_role::accent);
                row.push_back(element::text(string{glyphs::assistant} + " ", bullet, style::wrap_mode::none));
                row.push_back(markdown_block(msg.text, palette, body_width, math));
                return element::hbox(move(row));
            }
            case row_kind::thinking: {
                style st = dim_style_of(palette);
                st.italic = true;
                string label = string{glyphs::thinking_settled} + " Thinking";
                if (msg.elapsed_ms > 0) {
                    label += " " + to_string(msg.elapsed_ms / 1000) + "s";
                }
                if (!msg.expanded) {
                    return element::text(string("  ") + label, st, style::wrap_mode::none);
                }
                elements rows;
                rows.push_back(element::text(string("  ") + label, st, style::wrap_mode::none));
                rows.push_back(element::text(string("  ") + msg.text, st, style::wrap_mode::word));
                return element::vbox(move(rows));
            }
            case row_kind::tool: {
                style dot;
                dot.fg = msg.state == tool_state::failed
                                 ? palette.get(theme_role::error)
                                 : (msg.state == tool_state::running ? palette.get(theme_role::activity)
                                                                     : palette.get(theme_role::tool_dot_exec));
                const char* glyph = msg.state == tool_state::failed ? "\xe2\x9c\x97" : "\xe2\x97\x8f";
                style name_style;
                name_style.fg = palette.get(theme_role::tool_name_exec);
                name_style.bold = true;

                elements header;
                header.push_back(element::text(string(" ") + glyph + " ", dot, style::wrap_mode::none));
                header.push_back(element::text(msg.title, name_style, style::wrap_mode::none));
                if (!msg.text.empty()) {
                    header.push_back(
                            element::text(string("  ") + msg.text, dim_style_of(palette), style::wrap_mode::none));
                }
                elements rows;
                rows.push_back(element::hbox(move(header)));
                if (msg.expanded && !msg.text.empty()) {
                    rows.push_back(
                            element::text(string("   ") + msg.text, dim_style_of(palette), style::wrap_mode::word));
                }
                return element::vbox(move(rows));
            }
            case row_kind::error: {
                style st;
                st.fg = palette.get(theme_role::error);
                return element::text(string("  ") + msg.text, st, style::wrap_mode::word);
            }
            case row_kind::notice:
            default: {
                return element::text(string("  ") + msg.text, dim_style_of(palette), style::wrap_mode::word);
            }
        }
    }

    element approval_panel(const string& tool, const string& arguments, const theme& palette, const int columns) {
        (void) columns;
        style title;
        title.fg = palette.get(theme_role::warning);
        title.bold = true;
        style body = dim_style_of(palette);
        elements rows;
        rows.push_back(element::text(string("approval required"), title, style::wrap_mode::none));
        rows.push_back(element::text(string("  ") + tool + " " + arguments, body, style::wrap_mode::word));
        rows.push_back(element::text(string("  [y] allow   [n] deny   [a] allow all this session"), move(body),
                                     style::wrap_mode::none));
        return element::vbox(move(rows));
    }

    element help_view(const theme& palette, const int columns) {
        (void) columns;
        elements rows;
        style title;
        title.fg = palette.get(theme_role::accent);
        title.bold = true;
        rows.push_back(element::text(string("Help"), title, style::wrap_mode::none));
        rows.push_back(element::text(string(), style{}));

        const char* keys[] = {"enter        send / run command",
                              "shift+enter  newline",
                              "esc          close help / clear input / interrupt",
                              "ctrl+c       interrupt, then twice to quit",
                              "ctrl+o       expand tool output",
                              "ctrl+m       toggle math rendering",
                              "ctrl+t       cycle session mode",
                              "pgup/pgdn     scroll transcript",
                              "shift+tab    cycle session mode (from composer)",
                              "esc / q      close this page"};
        for (const char* line: keys) {
            rows.push_back(element::text(string("  ") + line, dim_style_of(palette), style::wrap_mode::none));
        }
        rows.push_back(element::text(string(), style{}));

        style command_style;
        command_style.fg = palette.get(theme_role::permission);
        for (const auto& spec: command_table()) {
            string line = "  /";
            line += spec.name;
            for (size_t i = spec.name.size(); i < 10; ++i) {
                line += ' ';
            }
            line += "  ";
            line += spec.summary;
            rows.push_back(element::text(line, command_style, style::wrap_mode::none));
        }
        return element::vbox(move(rows));
    }
} // namespace newave::views
