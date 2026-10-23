#include "agent/provider.hpp"
#include <NeForce/core/async/io_context.hpp>
#include <NeForce/network/http/http_server.hpp>
#include <NeForce/network/tcp/tcp_acceptor.hpp>
#include <NeForce/network/util/ip_address.hpp>
#include <gtest/gtest.h>

using namespace neforce;
using namespace newave::agent;

namespace {
    class ProviderServerTest : public ::testing::Test {
    protected:
        void SetUp() override {
            tcp_acceptor probe;
            probe.open(ip_address::loopback());
            const auto bound = probe.local_endpoint();
            port_ = bound.has_value() ? bound->port() : ports(0u);
            probe.close();

            server = make_unique<http::http_server>(port_, context);
            server->router().post("/chat/completions", [](http::http_request&, http::http_response& res) {
                res.status = http::http_status::S2_OK;
                res.headers["Content-Type"] = "text/event-stream";
                res.body = "data: {\"choices\":[{\"delta\":{\"reasoning_content\":\"think\"}}]}\n\n"
                           "data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}\n\n"
                           "data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}]}\n\n"
                           "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"c1\","
                           "\"function\":{\"name\":\"read_file\",\"arguments\":\"{\\\"path\\\":\\\"a\\\"}\"}}]}}]}\n\n"
                           "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}],"
                           "\"usage\":{\"prompt_tokens\":12,\"completion_tokens\":3}}\n\n"
                           "data: [DONE]\n\n";
            });
            if (!server->start()) {
                GTEST_SKIP() << "local http server failed to bind";
            }
        }

        void TearDown() override {
            if (server != nullptr) {
                server->stop();
            }
        }

        ports port_{0u};
        io_context context;
        unique_ptr<http::http_server> server;
    };
} // namespace

TEST_F(ProviderServerTest, ParsesStreamedTextToolsAndUsage) {
    config cfg;
    cfg.base_url = "http://127.0.0.1:" + to_string(static_cast<int64_t>(port_.value()));
    cfg.api_key = "test-key";
    cfg.model = "test-model";

    openai_provider provider;
    string streamed;
    const completion result =
            provider.complete({}, {}, cfg, [&streamed](const delta& chunk) { streamed += chunk.content; });

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.content, string{"Hello"});
    EXPECT_EQ(streamed, string{"Hello"});
    EXPECT_EQ(result.reasoning, string{"think"});
    EXPECT_EQ(result.finish_reason, string{"tool_calls"});
    ASSERT_EQ(result.calls.size(), 1u);
    EXPECT_EQ(result.calls[0].id, string{"c1"});
    EXPECT_EQ(result.calls[0].name, string{"read_file"});
    EXPECT_EQ(result.calls[0].arguments, string{"{\"path\":\"a\"}"});
    EXPECT_EQ(result.tokens.prompt_tokens, 12u);
    EXPECT_EQ(result.tokens.completion_tokens, 3u);
}

TEST(ProviderFailureTest, MissingKeyFailsWithoutNetwork) {
    config cfg;
    cfg.api_key.clear();
    openai_provider provider;
    const completion result = provider.complete({}, {}, cfg, [](const delta&) {});
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.error.view().find(string_view{"API key"}) != string_view::npos);
}
