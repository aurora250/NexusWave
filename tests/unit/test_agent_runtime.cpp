#include "agent/runtime.hpp"
#include <NeForce/core/file/filesystem.hpp>
#include <NeForce/core/file/json/json_value.hpp>
#include <NeForce/core/system/environment.hpp>
#include <gtest/gtest.h>

using namespace neforce;
using namespace newave::agent;

namespace {
    class scripted_provider : public chat_provider {
    public:
        vector<completion> script;

        completion complete(const vector<message>& messages, const vector<tool_spec>&, const config&,
                            const delta_handler& on_delta) override {
            history = messages;
            if (script.empty()) {
                completion empty;
                empty.error = "script exhausted";
                return empty;
            }
            const completion result = script.front();
            script.erase(script.begin());
            if (!result.content.empty()) {
                delta chunk;
                chunk.content = result.content;
                on_delta(chunk);
            }
            return result;
        }

        void cancel() override { cancelled = true; }

        vector<message> history;
        bool cancelled = false;
    };

    bool wait_for(const function<bool()>& predicate, int ms = 3000) {
        const auto deadline = steady_clock::now() + milliseconds(ms);
        while (steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            this_thread::sleep_for(milliseconds(5));
        }
        return predicate();
    }

    completion text_completion(const string& text) {
        completion result;
        result.ok = true;
        result.content = text;
        return result;
    }

    completion tool_completion(const string& name, const string& arguments) {
        completion result;
        result.ok = true;
        result.calls.push_back(tool_call{"call_1", name, arguments});
        return result;
    }
} // namespace

namespace {
    class RuntimeTest : public ::testing::Test {
    protected:
        void SetUp() override {
            root_ = path{environment::temp_directory()} / "nexuswave_runtime_test";
            filesystem::remove_all(root_);
            filesystem::create_directories(root_);
            config cfg;
            cfg.workspace = root_.str();
            guard = policy{permission_mode::workspace_write, root_.str()};
            cfg.max_steps = 4;
            cfg.system_prompt = "you are a test";
            runtime = make_unique<agent_runtime>(provider, tools, guard, store, cfg);
        }

        void TearDown() override {
            runtime.reset();
            filesystem::remove_all(root_);
        }

        path root_;
        scripted_provider provider;
        tool_registry tools = tool_registry::make_builtin();
        policy guard{permission_mode::full, "."};
        session_store store{{path{environment::temp_directory()} / "nexuswave_runtime_sessions"}};
        unique_ptr<agent_runtime> runtime;
    };
} // namespace

TEST_F(RuntimeTest, RunsToolCallThenFinishes) {
    ASSERT_TRUE(tools.invoke("write_file", R"({"path":"x.txt","content":"hello"})", root_.str()).ok);
    provider.script.push_back(tool_completion("read_file", R"({"path":"x.txt"})"));
    provider.script.push_back(text_completion("done"));

    runtime->start("read it");
    EXPECT_TRUE(wait_for([this] { return !runtime->busy(); }));

    vector<event> events;
    runtime->drain(events);
    bool saw_tool = false;
    bool saw_text = false;
    bool saw_done = false;
    for (const auto& item: events) {
        saw_tool |= item.kind == event_kind::tool_result && item.ok;
        saw_text |= item.kind == event_kind::text && item.text == string{"done"};
        saw_done |= item.kind == event_kind::done;
    }
    EXPECT_TRUE(saw_tool);
    EXPECT_TRUE(saw_text);
    EXPECT_TRUE(saw_done);

    const vector<message> history = runtime->history();
    ASSERT_GE(history.size(), 5u);
    EXPECT_EQ(history.front().who, role::system);
    bool saw_tool_message = false;
    for (const auto& msg: history) {
        if (msg.who == role::tool && msg.content == string{"1\thello\n"}) {
            saw_tool_message = true;
        }
    }
    EXPECT_TRUE(saw_tool_message);
}

TEST_F(RuntimeTest, DeniedToolIsReportedToTheModel) {
    provider.script.push_back(tool_completion("bash", R"({"command":"true"})"));
    provider.script.push_back(text_completion("ok"));

    runtime->start("write it");
    string name;
    string arguments;
    ASSERT_TRUE(wait_for([this, &name, &arguments] { return runtime->approval_pending(&name, &arguments); }));
    EXPECT_EQ(name, string{"bash"});
    runtime->resolve_approval(false);
    EXPECT_TRUE(wait_for([this] { return !runtime->busy(); }));

    const vector<message> history = runtime->history();
    bool saw_denied = false;
    for (const auto& msg: history) {
        if (msg.who == role::tool && msg.content == string{"permission denied"}) {
            saw_denied = true;
        }
    }
    EXPECT_TRUE(saw_denied);
}

TEST_F(RuntimeTest, ProviderFailureEndsTurnWithError) {
    completion failure;
    failure.ok = false;
    failure.error = "boom";
    provider.script.push_back(failure);

    runtime->start("hi");
    EXPECT_TRUE(wait_for([this] { return !runtime->busy(); }));

    vector<event> events;
    runtime->drain(events);
    bool saw_error = false;
    for (const auto& item: events) {
        saw_error |= item.kind == event_kind::error && item.text == string{"boom"};
    }
    EXPECT_TRUE(saw_error);
}

TEST_F(RuntimeTest, StepLimitStopsRunawayTools) {
    for (int i = 0; i < 8; ++i) {
        provider.script.push_back(tool_completion("glob", R"({"pattern":"*"})"));
    }
    runtime->start("loop");
    EXPECT_TRUE(wait_for([this] { return !runtime->busy(); }, 5000));

    vector<event> events;
    runtime->drain(events);
    bool saw_limit = false;
    for (const auto& item: events) {
        saw_limit |= item.kind == event_kind::error && item.text == string{"step limit reached"};
    }
    EXPECT_TRUE(saw_limit);
}

TEST_F(RuntimeTest, ResetKeepsSystemPrompt) {
    runtime->reset_history();
    const vector<message> history = runtime->history();
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].who, role::system);
    EXPECT_EQ(history[0].content, string{"you are a test"});
}

TEST_F(RuntimeTest, SecondStartWhileBusyIsIgnored) {
    provider.script.push_back(text_completion("one"));
    runtime->start("first");
    runtime->start("second");
    EXPECT_TRUE(wait_for([this] { return !runtime->busy(); }));
    vector<event> events;
    runtime->drain(events);
    size_t user_messages = 0;
    for (const auto& msg: runtime->history()) {
        if (msg.who == role::user) {
            ++user_messages;
        }
    }
    EXPECT_EQ(user_messages, 1u);
}
