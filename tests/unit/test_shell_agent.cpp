#include "agent/provider.hpp"
#include "app/shell.hpp"
#include <NeForce/tui/headless.hpp>
#include <gtest/gtest.h>

using namespace neforce;
using namespace neforce::tui;
using namespace newave;

namespace {
    class scripted_provider : public agent::chat_provider {
    public:
        vector<agent::completion> script;

        agent::completion complete(const vector<agent::message>&, const vector<agent::tool_spec>&, const agent::config&,
                                   const agent::delta_handler& on_delta) override {
            if (script.empty()) {
                agent::completion empty;
                empty.error = "script exhausted";
                return empty;
            }
            const agent::completion result = script.front();
            script.erase(script.begin());
            if (!result.content.empty()) {
                agent::delta chunk;
                chunk.content = result.content;
                on_delta(chunk);
            }
            return result;
        }

        void cancel() override {}
    };

    agent::completion text_completion(const string& text) {
        agent::completion result;
        result.ok = true;
        result.content = text;
        return result;
    }

    agent::completion tool_completion(const string& name, const string& arguments) {
        agent::completion result;
        result.ok = true;
        result.calls.push_back(agent::tool_call{"call_1", name, arguments});
        return result;
    }

    void type_line(reconciler& pipe, const string& text) {
        for (const char c: text) {
            key_event e;
            e.key = key_event::type::printable;
            e.cp = codepoint{static_cast<char32_t>(c)};
            pipe.dispatch_key(e);
        }
        key_event enter;
        enter.key = key_event::type::enter;
        pipe.dispatch_key(enter);
    }

    bool wait_for(headless& app, const function<bool()>& predicate, const int ms = 4000) {
        const auto deadline = steady_clock::now() + milliseconds(ms);
        while (steady_clock::now() < deadline) {
            app.render();
            if (predicate()) {
                return true;
            }
            this_thread::sleep_for(milliseconds(5));
        }
        return predicate();
    }

    bool contains(const string& text, const string_view needle) {
        return text.view().find(needle) != string_view::npos;
    }
} // namespace

TEST(ShellAgentTest, StreamsTextIntoTheTranscript) {
    auto provider = make_unique<scripted_provider>();
    provider->script.push_back(text_completion("Hello from the model"));
    shell root{string(), _NEFORCE move(provider)};
    headless app(&root, 90, 30);
    app.render();

    type_line(app.pipeline(), "hi");
    EXPECT_TRUE(wait_for(app, [&] { return contains(app.plaintext(), "Hello from the model"); }));
    EXPECT_FALSE(root.state().busy);
}

TEST(ShellAgentTest, ApprovalPanelAcceptsAndRunsTheTool) {
    auto provider = make_unique<scripted_provider>();
    provider->script.push_back(tool_completion("bash", R"({"command":"echo approved"})"));
    provider->script.push_back(text_completion("all done"));
    shell root{string(), _NEFORCE move(provider)};
    headless app(&root, 90, 30);
    app.render();

    type_line(app.pipeline(), "run it");
    EXPECT_TRUE(wait_for(app, [&] { return contains(app.plaintext(), "approval required"); }));
    key_event yes;
    yes.key = key_event::type::printable;
    yes.cp = codepoint{U'y'};
    app.pipeline().dispatch_key(yes);
    EXPECT_TRUE(wait_for(app, [&] { return contains(app.plaintext(), "all done"); }));
    EXPECT_TRUE(contains(app.plaintext(), "bash"));
}

TEST(ShellAgentTest, DeniedToolKeepsTheTurnGoing) {
    auto provider = make_unique<scripted_provider>();
    provider->script.push_back(tool_completion("bash", R"({"command":"echo nope"})"));
    provider->script.push_back(text_completion("understood"));
    shell root{string(), _NEFORCE move(provider)};
    headless app(&root, 90, 30);
    app.render();

    type_line(app.pipeline(), "run it");
    EXPECT_TRUE(wait_for(app, [&] { return contains(app.plaintext(), "approval required"); }));
    key_event no;
    no.key = key_event::type::printable;
    no.cp = codepoint{U'n'};
    app.pipeline().dispatch_key(no);
    EXPECT_TRUE(wait_for(app, [&] { return contains(app.plaintext(), "understood"); }));
}

TEST(ShellAgentTest, ErrorEventRendersAndClearsBusy) {
    auto provider = make_unique<scripted_provider>();
    agent::completion failure;
    failure.ok = false;
    failure.error = "model exploded";
    provider->script.push_back(failure);
    shell root{string(), _NEFORCE move(provider)};
    headless app(&root, 90, 30);
    app.render();

    type_line(app.pipeline(), "hi");
    EXPECT_TRUE(wait_for(app, [&] { return contains(app.plaintext(), "model exploded"); }));
    EXPECT_FALSE(root.state().busy);
}
