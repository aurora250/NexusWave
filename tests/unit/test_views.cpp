#include "ui/views.hpp"
#include <NeForce/tui/component/component.hpp>
#include <NeForce/tui/dom/theme.hpp>
#include <NeForce/tui/headless.hpp>
#include <gtest/gtest.h>
using namespace neforce;
using namespace neforce::tui;
using namespace newave;

namespace {
    struct view_root : component<> {
        element content = element::empty();
        element render() override { return content; }
    };

    string render_element(const element& tree, const int width, const int height) {
        view_root root;
        root.content = tree;
        headless app(&root, width, height);
        app.render();
        return app.plaintext();
    }

    bool contains(const string& text, const string_view needle) {
        return text.view().find(needle) != string_view::npos;
    }
} // namespace

TEST(ViewTest, AssistantRowRendersMarkdown) {
    const theme palette = make_palette(theme_palette_id::dark);
    message msg;
    msg.kind = row_kind::assistant;
    msg.text = "hello **world**";
    const string text = render_element(views::transcript_row(msg, palette, 40, true), 40, 4);
    EXPECT_TRUE(contains(text, "hello"));
    EXPECT_TRUE(contains(text, "world"));
    EXPECT_TRUE(contains(text, "\xe2\x97\x8f"));
}

TEST(ViewTest, ToolRowShowsTitleAndState) {
    const theme palette = make_palette(theme_palette_id::dark);
    message msg;
    msg.kind = row_kind::tool;
    msg.title = "bash";
    msg.text = "npm test";
    msg.state = tool_state::failed;
    const string text = render_element(views::transcript_row(msg, palette, 40, true), 40, 3);
    EXPECT_TRUE(contains(text, "bash"));
    EXPECT_TRUE(contains(text, "npm test"));
    EXPECT_TRUE(contains(text, "\xe2\x9c\x97"));
}

TEST(ViewTest, UserRowUsesThePointerGlyph) {
    const theme palette = make_palette(theme_palette_id::dark);
    message msg;
    msg.kind = row_kind::user;
    msg.text = "hi there";
    const string text = render_element(views::transcript_row(msg, palette, 40, true), 40, 3);
    EXPECT_TRUE(contains(text, "\xe2\x9d\xaf"));
    EXPECT_TRUE(contains(text, "hi there"));
}

TEST(ViewTest, StatusBarShowsModelAndMode) {
    const theme palette = make_palette(theme_palette_id::dark);
    session_state state;
    state.model = "deepseek-v4-flash";
    state.mode = session_mode::plan;
    state.context_used = 12800;
    const string text = render_element(views::status_bar(state, palette, 80, 0), 80, 4);
    EXPECT_TRUE(contains(text, "deepseek-v4-flash"));
    EXPECT_TRUE(contains(text, "plan"));
}

TEST(ViewTest, HelpViewListsCommands) {
    const theme palette = make_palette(theme_palette_id::dark);
    const string text = render_element(views::help_view(palette, 60), 60, 30);
    EXPECT_TRUE(contains(text, "Help"));
    EXPECT_TRUE(contains(text, "/theme"));
    EXPECT_TRUE(contains(text, "/mode"));
}

TEST(ViewTest, SplashRendersTheWordmark) {
    const theme palette = make_palette(theme_palette_id::dark);
    const string text = render_element(views::splash(palette, 0, 60), 60, 4);
    EXPECT_TRUE(contains(text, "NEXUSWAVE"));
}
