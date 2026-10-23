#include "agent/tools.hpp"
#include <NeForce/core/file/filesystem.hpp>
#include <NeForce/core/system/environment.hpp>
#include <gtest/gtest.h>

using namespace neforce;
using namespace newave::agent;

namespace {
    class ToolsTest : public ::testing::Test {
    protected:
        void SetUp() override {
            root_ = path{environment::temp_directory()} / "nexuswave_tools_test";
            filesystem::remove_all(root_);
            filesystem::create_directories(root_);
            registry = tool_registry::make_builtin();
        }

        void TearDown() override { filesystem::remove_all(root_); }

        tool_result call(const char* name, const string& args) {
            return registry.invoke(name, args.view(), root_.str());
        }

        path root_;
        tool_registry registry;
    };
} // namespace

TEST_F(ToolsTest, WriteThenReadRoundTrip) {
    const string args = R"({"path":"a.txt","content":"one\ntwo\nthree\n"})";
    const tool_result wrote = call("write_file", args);
    EXPECT_TRUE(wrote.ok);

    const tool_result read = call("read_file", R"({"path":"a.txt","offset":2,"limit":1})");
    EXPECT_TRUE(read.ok);
    EXPECT_EQ(read.output, string{"2\ttwo\n"});
}

TEST_F(ToolsTest, ReadMissingFileFails) {
    const tool_result read = call("read_file", R"({"path":"missing.txt"})");
    EXPECT_FALSE(read.ok);
}

TEST_F(ToolsTest, EditReplacesAndGuardsAmbiguity) {
    ASSERT_TRUE(call("write_file", R"({"path":"e.txt","content":"a a b\n"})").ok);
    const tool_result ambiguous = call("edit_file", R"({"path":"e.txt","old":"a","new":"z"})");
    EXPECT_FALSE(ambiguous.ok);
    EXPECT_TRUE(ambiguous.output.view().find(string_view{"2 times"}) != string_view::npos);

    const tool_result all = call("edit_file", R"({"path":"e.txt","old":"a","new":"z","all":true})");
    EXPECT_TRUE(all.ok);
    const tool_result read = call("read_file", R"({"path":"e.txt"})");
    EXPECT_EQ(read.output, string{"1\tz z b\n"});
}

TEST_F(ToolsTest, GlobFindsNestedFiles) {
    ASSERT_TRUE(call("write_file", R"({"path":"src/a.cpp","content":"x"})").ok);
    ASSERT_TRUE(call("write_file", R"({"path":"src/b.hpp","content":"x"})").ok);
    ASSERT_TRUE(call("write_file", R"({"path":"main.cpp","content":"x"})").ok);

    const tool_result cpp = call("glob", R"({"pattern":"**/*.cpp"})");
    EXPECT_TRUE(cpp.ok);
    EXPECT_TRUE(cpp.output.view().find(string_view{"src/a.cpp"}) != string_view::npos);
    EXPECT_TRUE(cpp.output.view().find(string_view{"main.cpp"}) != string_view::npos);
    EXPECT_TRUE(cpp.output.view().find(string_view{"b.hpp"}) == string_view::npos);
}

TEST_F(ToolsTest, GrepReportsPathAndLine) {
    ASSERT_TRUE(call("write_file", R"({"path":"g.txt","content":"alpha\nbeta\ngamma\n"})").ok);
    const tool_result hits = call("grep", R"({"pattern":"^b","path":"."})");
    EXPECT_TRUE(hits.ok);
    EXPECT_TRUE(hits.output.view().find(string_view{"g.txt:2: beta"}) != string_view::npos);
}

TEST_F(ToolsTest, GrepRejectsInvalidRegex) {
    const tool_result hits = call("grep", R"({"pattern":"["})");
    EXPECT_FALSE(hits.ok);
}

TEST_F(ToolsTest, BashRunsInsideWorkspace) {
    const tool_result result = call("bash", R"({"command":"pwd"})");
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.output.view().find(string_view{"nexuswave_tools_test"}) != string_view::npos);
}

TEST_F(ToolsTest, TodoWriteFormatsChecklist) {
    const tool_result result = call(
            "todo_write", R"({"todos":[{"content":"a","status":"completed"},{"content":"b","status":"in_progress"}]})");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.output, string{"[x] a\n[~] b\n"});
}

TEST_F(ToolsTest, UnknownToolFails) {
    const tool_result result = call("nope", "{}");
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.output.view().find(string_view{"unknown tool"}) != string_view::npos);
}

TEST(ToolsPathTest, AcceptsInsideAndRejectsEscape) {
    EXPECT_TRUE(path_is_within("/work", "a/b.txt"));
    EXPECT_TRUE(path_is_within("/work", "../work/a"));
    EXPECT_FALSE(path_is_within("/work", "../etc/passwd"));
    EXPECT_FALSE(path_is_within("/work", "a/../../etc"));
}

TEST(ToolsGlobTest, MatchesWildcards) {
    EXPECT_TRUE(glob_match("*.cpp", "main.cpp"));
    EXPECT_FALSE(glob_match("*.cpp", "src/main.cpp"));
    EXPECT_TRUE(glob_match("**/*.cpp", "src/a/b.cpp"));
    EXPECT_TRUE(glob_match("**/*.cpp", "main.cpp"));
    EXPECT_TRUE(glob_match("a?c", "abc"));
    EXPECT_FALSE(glob_match("a?c", "ac"));
}
