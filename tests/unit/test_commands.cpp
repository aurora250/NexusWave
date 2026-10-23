#include "app/commands.hpp"
#include <gtest/gtest.h>
using namespace neforce;
using namespace newave;

TEST(CommandParseTest, RecognizesNameAndArgs) {
    const parsed_command cmd = parse_command("/theme dark");
    EXPECT_TRUE(cmd.is_command);
    EXPECT_EQ(cmd.name, string{"theme"});
    EXPECT_EQ(cmd.args, string{"dark"});
}

TEST(CommandParseTest, TrimsSurroundingWhitespace) {
    const parsed_command cmd = parse_command("   /mode   plan  ");
    EXPECT_TRUE(cmd.is_command);
    EXPECT_EQ(cmd.name, string{"mode"});
    EXPECT_EQ(cmd.args, string{"plan"});
}

TEST(CommandParseTest, PlainTextIsNotACommand) {
    EXPECT_FALSE(parse_command("hello /theme").is_command);
    EXPECT_FALSE(parse_command("/").is_command);
    EXPECT_FALSE(parse_command("/ theme").is_command);
    EXPECT_FALSE(parse_command("").is_command);
}

TEST(CommandParseTest, RejectsNamesWithPunctuation) {
    EXPECT_FALSE(parse_command("/the/me").is_command);
    EXPECT_FALSE(parse_command("/?").is_command);
}

TEST(CommandParseTest, KeepsEmptyArgs) {
    const parsed_command cmd = parse_command("/status");
    EXPECT_TRUE(cmd.is_command);
    EXPECT_EQ(cmd.name, string{"status"});
    EXPECT_TRUE(cmd.args.empty());
}

TEST(CommandTableTest, LooksUpKnownCommand) {
    const command_spec* spec = find_command("help");
    ASSERT_NE(spec, nullptr);
    EXPECT_EQ(spec->name, string_view{"help"});
    EXPECT_EQ(find_command("nope"), nullptr);
    EXPECT_GE(command_table().size(), 10u);
}

TEST(CommandCompletionTest, MatchesPrefixAndDeduplicates) {
    vector<string> out;
    complete_commands("m", out);
    EXPECT_GE(out.size(), 3u);
    complete_commands("m", out);
    size_t theme_count = 0;
    for (const auto& name: out) {
        if (name == string{"mode"}) {
            ++theme_count;
        }
    }
    EXPECT_EQ(theme_count, 1u);
}

TEST(CommandCompletionTest, EmptyPrefixReturnsEverything) {
    vector<string> out;
    complete_commands("", out);
    EXPECT_EQ(out.size(), command_table().size());
}
