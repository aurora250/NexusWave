#include "app/app_state.hpp"
#include <gtest/gtest.h>
using namespace neforce;
using namespace newave;

TEST(SessionStateTest, PushAppendsRows) {
    session_state state;
    message msg;
    msg.kind = row_kind::user;
    msg.text = "hello";
    EXPECT_EQ(state.push(msg), 0u);
    EXPECT_EQ(state.messages.size(), 1u);
    EXPECT_EQ(state.messages[0].text, string{"hello"});
}

TEST(SessionStateTest, AssistantChunksMergeIntoOneRow) {
    session_state state;
    state.append_assistant("he");
    state.append_assistant("llo");
    ASSERT_EQ(state.messages.size(), 1u);
    EXPECT_EQ(state.messages[0].text, string{"hello"});
    EXPECT_EQ(state.messages[0].kind, row_kind::assistant);
}

TEST(SessionStateTest, AssistantChunksStartANewRowAfterOtherKinds) {
    session_state state;
    state.append_assistant("first");
    message notice;
    notice.kind = row_kind::notice;
    state.push(notice);
    state.append_assistant("second");
    ASSERT_EQ(state.messages.size(), 3u);
    EXPECT_EQ(state.messages[2].text, string{"second"});
}

TEST(SessionStateTest, ModeCycleWraps) {
    session_state state;
    EXPECT_EQ(state.mode, session_mode::default_);
    state.cycle_mode();
    EXPECT_EQ(state.mode, session_mode::plan);
    state.cycle_mode();
    EXPECT_EQ(state.mode, session_mode::full);
    state.cycle_mode();
    EXPECT_EQ(state.mode, session_mode::default_);
}

TEST(SessionStateTest, ModeNames) {
    EXPECT_EQ(session_state::mode_name(session_mode::default_), string_view{"default"});
    EXPECT_EQ(session_state::mode_name(session_mode::plan), string_view{"plan"});
    EXPECT_EQ(session_state::mode_name(session_mode::full), string_view{"full"});
}

TEST(SessionStateTest, ContextRatioClampsToLimit) {
    session_state state;
    state.context_limit = 1000;
    state.context_used = 250;
    EXPECT_DOUBLE_EQ(state.context_ratio(), 0.25);
    state.context_limit = 0;
    EXPECT_DOUBLE_EQ(state.context_ratio(), 0.0);
}

TEST(SessionStateTest, ClearResetsEverything) {
    session_state state;
    message msg;
    msg.kind = row_kind::assistant;
    state.push(msg);
    state.context_used = 42;
    state.busy = true;
    state.clear();
    EXPECT_TRUE(state.messages.empty());
    EXPECT_EQ(state.context_used, 0u);
    EXPECT_FALSE(state.busy);
}
