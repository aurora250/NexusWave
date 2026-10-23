#include "app/keymap.hpp"
#include <gtest/gtest.h>
using namespace neforce;
using namespace newave;

namespace {
    tui::key_event ch(const char32_t value, const tui::key_modifier mods = tui::key_modifier::none) {
        tui::key_event key;
        key.key = tui::key_event::type::printable;
        key.cp = codepoint{value};
        key.mods = mods;
        return key;
    }

    tui::key_event special(const tui::key_event::type type, const tui::key_modifier mods = tui::key_modifier::none) {
        tui::key_event key;
        key.key = type;
        key.mods = mods;
        return key;
    }
} // namespace

TEST(KeymapTest, CtrlCInterruptsOrClearsOrQuits) {
    key_context busy;
    busy.busy = true;
    EXPECT_EQ(resolve_key(ch(U'c', tui::key_modifier::ctrl), busy), key_intent::interrupt);

    key_context typing;
    typing.input_empty = false;
    EXPECT_EQ(resolve_key(ch(U'c', tui::key_modifier::ctrl), typing), key_intent::clear_input);

    key_context idle;
    EXPECT_EQ(resolve_key(ch(U'c', tui::key_modifier::ctrl), idle), key_intent::quit);
}

TEST(KeymapTest, ShiftTabCyclesMode) {
    key_context ctx;
    EXPECT_EQ(resolve_key(special(tui::key_event::type::tab, tui::key_modifier::shift), ctx), key_intent::cycle_mode);
}

TEST(KeymapTest, EscapeLadder) {
    key_context help;
    help.help_open = true;
    EXPECT_EQ(resolve_key(special(tui::key_event::type::escape), help), key_intent::toggle_help);

    key_context busy;
    busy.busy = true;
    EXPECT_EQ(resolve_key(special(tui::key_event::type::escape), busy), key_intent::interrupt);

    key_context typing;
    typing.input_empty = false;
    EXPECT_EQ(resolve_key(special(tui::key_event::type::escape), typing), key_intent::clear_input);

    key_context idle;
    EXPECT_EQ(resolve_key(special(tui::key_event::type::escape), idle), key_intent::none);
}

TEST(KeymapTest, QuestionMarkOpensHelpOnlyWhenIdle) {
    key_context idle;
    EXPECT_EQ(resolve_key(ch(U'?'), idle), key_intent::toggle_help);

    key_context typing;
    typing.input_empty = false;
    EXPECT_EQ(resolve_key(ch(U'?'), typing), key_intent::none);
}

TEST(KeymapTest, PagingAndWheelIntents) {
    key_context ctx;
    EXPECT_EQ(resolve_key(special(tui::key_event::type::page_up), ctx), key_intent::page_up);
    EXPECT_EQ(resolve_key(special(tui::key_event::type::page_down), ctx), key_intent::page_down);
    EXPECT_EQ(resolve_key(special(tui::key_event::type::up, tui::key_modifier::alt), ctx), key_intent::scroll_up);
    EXPECT_EQ(resolve_key(special(tui::key_event::type::down, tui::key_modifier::alt), ctx), key_intent::scroll_down);
}

TEST(KeymapTest, PrintableKeysGoToTheComposer) {
    key_context ctx;
    EXPECT_EQ(resolve_key(ch(U'a'), ctx), key_intent::none);
    EXPECT_EQ(resolve_key(special(tui::key_event::type::enter), ctx), key_intent::none);
}

TEST(KeymapTest, HelpPageOwnsItsKeys) {
    EXPECT_EQ(resolve_help_key(special(tui::key_event::type::escape)), key_intent::toggle_help);
    EXPECT_EQ(resolve_help_key(ch(U'q')), key_intent::toggle_help);
    EXPECT_EQ(resolve_help_key(special(tui::key_event::type::page_down)), key_intent::page_down);
    EXPECT_EQ(resolve_help_key(ch(U'x')), key_intent::none);
}
