#include "app/shell.hpp"
#include <NeForce/tui/headless.hpp>
#include <gtest/gtest.h>
using namespace neforce;
using namespace neforce::tui;

namespace {
    tui::key_event printable(const char32_t ch, const tui::key_modifier mods = tui::key_modifier::none) {
        tui::key_event key;
        key.key = tui::key_event::type::printable;
        key.cp = codepoint{ch};
        key.mods = mods;
        return key;
    }

    tui::key_event special(const tui::key_event::type type) {
        tui::key_event key;
        key.key = type;
        return key;
    }

    bool contains(const string& text, const string_view needle) {
        return text.view().find(needle) != string_view::npos;
    }
} // namespace

TEST(ShellTest, QuestionMarkOpensHelpWhenInputIsEmpty) {
    newave::shell root;
    headless app(&root, 80, 24);
    app.render();
    EXPECT_TRUE(app.pipeline().dispatch_key(printable(U'?')));
    app.render();
    EXPECT_TRUE(contains(app.plaintext(), "Help"));
    EXPECT_TRUE(contains(app.plaintext(), "/theme"));
}

TEST(ShellTest, EscapeClosesHelp) {
    newave::shell root;
    headless app(&root, 80, 24);
    app.render();
    app.pipeline().dispatch_key(printable(U'?'));
    app.render();
    EXPECT_TRUE(contains(app.plaintext(), "Help"));
    app.pipeline().dispatch_key(special(tui::key_event::type::escape));
    app.render();
    EXPECT_FALSE(contains(app.plaintext(), "Help"));
}

TEST(ShellTest, DemoCommandInsertsTranscriptRows) {
    newave::shell root;
    headless app(&root, 80, 30);
    app.render();
    root.submit("demo" == string{} ? string{} : string{"/demo"});
    app.render();
    const string text = app.plaintext();
    EXPECT_TRUE(contains(text, "parse"));
    bool has_tool = false;
    for (const auto& msg: root.state().messages) {
        if (msg.kind == newave::row_kind::tool && msg.title == string{"bash"} &&
            msg.text == string{"cmake --build build"}) {
            has_tool = true;
        }
    }
    EXPECT_TRUE(has_tool);
}

TEST(ShellTest, ThemeCommandSwitchesPalette) {
    newave::shell root;
    headless app(&root, 80, 24);
    app.render();
    root.submit(string{"/theme light"});
    app.render();
    EXPECT_TRUE(contains(app.plaintext(), "theme: light"));
}

TEST(ShellTest, UnknownCommandReportsAnError) {
    newave::shell root;
    headless app(&root, 80, 24);
    app.render();
    root.submit(string{"/nope"});
    app.render();
    EXPECT_TRUE(contains(app.plaintext(), "unknown command"));
}

TEST(ShellTest, ModeCommandUpdatesStatusBar) {
    newave::shell root;
    headless app(&root, 80, 24);
    app.render();
    root.submit(string{"/mode plan"});
    app.render();
    const string text = app.plaintext();
    EXPECT_TRUE(contains(text, "mode: plan"));
    EXPECT_TRUE(contains(text, "plan"));
}

TEST(ShellTest, HelpPageSwallowsTyping) {
    newave::shell root;
    headless app(&root, 80, 24);
    app.render();
    app.pipeline().dispatch_key(printable(U'?'));
    app.render();
    EXPECT_TRUE(contains(app.plaintext(), "Help"));

    app.pipeline().dispatch_key(printable(U'x'));
    app.render();
    EXPECT_TRUE(contains(app.plaintext(), "Help"));
    app.pipeline().dispatch_key(special(tui::key_event::type::escape));
    app.render();
    EXPECT_FALSE(contains(app.plaintext(), "Help"));
}

TEST(ShellTest, StatusCommandReportsSessionAndKey) {
    newave::shell root;
    headless app(&root, 80, 30);
    app.render();
    root.submit(string{"/status"});
    app.render();
    const string text = app.plaintext();
    EXPECT_TRUE(contains(text, "session"));
    EXPECT_TRUE(contains(text, "key"));
}
