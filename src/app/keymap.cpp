#include "keymap.hpp"
namespace newave {
    namespace {
        bool is_ctrl(const tui::key_event& key, const char32_t ch) {
            return key.key == tui::key_event::type::printable && key.cp.value() == ch &&
                   (key.mods & tui::key_modifier::ctrl) != tui::key_modifier::none;
        }

        bool is_plain(const tui::key_event& key, const char32_t ch) {
            return key.key == tui::key_event::type::printable && key.cp.value() == ch &&
                   key.mods == tui::key_modifier::none;
        }
    } // namespace


    key_intent resolve_key(const tui::key_event& key, const key_context& ctx) {
        if (key.key == tui::key_event::type::tab && (key.mods & tui::key_modifier::shift) != tui::key_modifier::none) {
            return key_intent::cycle_mode;
        }
        if (key.key == tui::key_event::type::tab) {
            return key_intent::none;
        }
        if (is_ctrl(key, U'c')) {
            if (ctx.busy) {
                return key_intent::interrupt;
            }
            if (!ctx.input_empty) {
                return key_intent::clear_input;
            }
            return key_intent::quit;
        }
        if (is_ctrl(key, U'd')) {
            return ctx.input_empty ? key_intent::force_quit : key_intent::none;
        }
        if (is_ctrl(key, U'l')) {
            return key_intent::redraw;
        }
        if (is_ctrl(key, U'o')) {
            return key_intent::toggle_expand;
        }
        if (is_ctrl(key, U'm')) {
            return key_intent::toggle_math;
        }
        if (key.key == tui::key_event::type::escape) {
            if (ctx.help_open) {
                return key_intent::toggle_help;
            }
            if (ctx.busy) {
                return key_intent::interrupt;
            }
            return ctx.input_empty ? key_intent::none : key_intent::clear_input;
        }
        if (is_plain(key, U'?') && ctx.input_empty) {
            return key_intent::toggle_help;
        }
        if (key.key == tui::key_event::type::page_up) {
            return key_intent::page_up;
        }
        if (key.key == tui::key_event::type::page_down) {
            return key_intent::page_down;
        }
        if (key.key == tui::key_event::type::up && (key.mods & tui::key_modifier::alt) != tui::key_modifier::none) {
            return key_intent::scroll_up;
        }
        if (key.key == tui::key_event::type::down && (key.mods & tui::key_modifier::alt) != tui::key_modifier::none) {
            return key_intent::scroll_down;
        }
        return key_intent::none;
    }

    key_intent resolve_help_key(const tui::key_event& key) {
        if (key.key == tui::key_event::type::escape || is_plain(key, U'?') || is_plain(key, U'q')) {
            return key_intent::toggle_help;
        }
        if (key.key == tui::key_event::type::page_up) {
            return key_intent::page_up;
        }
        if (key.key == tui::key_event::type::page_down) {
            return key_intent::page_down;
        }
        return key_intent::none;
    }
} // namespace newave
