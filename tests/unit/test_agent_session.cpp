#include "agent/session.hpp"
#include <NeForce/core/system/environment.hpp>
#include <NeForce/core/file/filesystem.hpp>
#include <gtest/gtest.h>
using namespace neforce;
using namespace newave::agent;

namespace {
    class SessionTest : public ::testing::Test {
    protected:
        void SetUp() override {
            root_ = path{environment::temp_directory()} / "nexuswave_session_test";
            filesystem::remove_all(root_);
        }
        void TearDown() override { filesystem::remove_all(root_); }
        path root_;
    };
} // namespace

TEST_F(SessionTest, AppendAndReloadPreservesMessages) {
    session_store store{root_};
    const string id = store.start_new();
    EXPECT_FALSE(id.empty());

    message user;
    user.who = role::user;
    user.content = "hello \"world\"\nline2";
    ASSERT_TRUE(store.append(user));

    message assistant;
    assistant.who = role::assistant;
    assistant.content = "answer";
    assistant.reasoning = "why";
    assistant.calls.push_back(tool_call{"call_1", "read_file", R"({"path":"a"})"});
    ASSERT_TRUE(store.append(assistant));

    const vector<message> loaded = store.load_current();
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].content, user.content);
    EXPECT_EQ(loaded[1].reasoning, string{"why"});
    ASSERT_EQ(loaded[1].calls.size(), 1u);
    EXPECT_EQ(loaded[1].calls[0].name, string{"read_file"});
    EXPECT_EQ(loaded[1].calls[0].arguments, string{R"({"path":"a"})"});
}

TEST_F(SessionTest, AppendStartsSessionAutomatically) {
    session_store store{root_};
    EXPECT_FALSE(store.active());
    message msg;
    msg.who = role::user;
    msg.content = "x";
    EXPECT_TRUE(store.append(msg));
    EXPECT_TRUE(store.active());
}

TEST_F(SessionTest, LoadingUnknownSessionIsEmpty) {
    session_store store{root_};
    EXPECT_TRUE(store.load("nope").empty());
}
