#include "agent/sse.hpp"
#include <gtest/gtest.h>
using namespace neforce;
using namespace newave::agent;

TEST(SseParserTest, DispatchesOnBlankLine) {
    sse_parser parser;
    vector<string> events;
    const auto collect = [&events](const string_view data) { events.push_back(string{data}); };
    parser.feed("data: one\n\n", collect);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], string{"one"});
}

TEST(SseParserTest, HandlesPartialLinesAcrossChunks) {
    sse_parser parser;
    vector<string> events;
    const auto collect = [&events](const string_view data) { events.push_back(string{data}); };
    parser.feed("data: hel", collect);
    parser.feed("lo\n", collect);
    EXPECT_TRUE(events.empty());
    parser.feed("\n", collect);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], string{"hello"});
}

TEST(SseParserTest, JoinsMultipleDataLines) {
    sse_parser parser;
    vector<string> events;
    const auto collect = [&events](const string_view data) { events.push_back(string{data}); };
    parser.feed("data: a\ndata: b\n\n", collect);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], string{"a\nb"});
}

TEST(SseParserTest, IgnoresCommentsAndOtherFields) {
    sse_parser parser;
    vector<string> events;
    const auto collect = [&events](const string_view data) { events.push_back(string{data}); };
    parser.feed(": keep-alive\nevent: message\nid: 7\ndata: x\r\n\r\n", collect);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], string{"x"});
}

TEST(SseParserTest, StripsSingleLeadingSpaceOnly) {
    sse_parser parser;
    vector<string> events;
    const auto collect = [&events](const string_view data) { events.push_back(string{data}); };
    parser.feed("data:  two\n\n", collect);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], string{" two"});
}

TEST(SseParserTest, ResetDropsPartialState) {
    sse_parser parser;
    vector<string> events;
    const auto collect = [&events](const string_view data) { events.push_back(string{data}); };
    parser.feed("data: stale\n", collect);
    parser.reset();
    parser.feed("data: fresh\n\n", collect);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], string{"fresh"});
}

TEST(SseParserTest, IgnoresEmptyDataEvents) {
    sse_parser parser;
    int count = 0;
    const auto collect = [&count](const string_view) { ++count; };
    parser.feed("\n\n", collect);
    EXPECT_EQ(count, 0);
}
