#include "runtime.hpp"
namespace newave::agent {
    using namespace neforce;

    agent_runtime::agent_runtime(chat_provider& provider, const tool_registry& tools, policy& guard,
                                 session_store& store, config cfg) :
    provider_(provider),
    tools_(tools),
    guard_(guard),
    store_(store),
    cfg_(move(cfg)) {
        if (!cfg_.system_prompt.empty()) {
            message system_message;
            system_message.who = role::system;
            system_message.content = cfg_.system_prompt;
            history_.push_back(move(system_message));
        }
    }

    agent_runtime::~agent_runtime() {
        cancel();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void agent_runtime::start(string prompt) {
        {
            lock<mutex> lock{mutex_};
            if (busy_) {
                return;
            }
            busy_ = true;
            cancel_ = false;
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        worker_ = thread{[this, prompt = move(prompt)]() mutable { run(move(prompt)); }};
    }

    void agent_runtime::cancel() {
        {
            lock<mutex> lock{mutex_};
            cancel_ = true;
        }
        provider_.cancel();
        cv_.notify_all();
    }

    bool agent_runtime::busy() const {
        lock<mutex> lock{mutex_};
        return busy_;
    }

    void agent_runtime::drain(vector<event>& out) {
        lock<mutex> lock{mutex_};
        for (auto& item: queue_) {
            out.push_back(move(item));
        }
        queue_.clear();
    }

    bool agent_runtime::approval_pending(string* name, string* arguments) const {
        lock<mutex> lock{mutex_};
        if (!approval_waiting_) {
            return false;
        }
        if (name != nullptr) {
            *name = approval_name_;
        }
        if (arguments != nullptr) {
            *arguments = approval_arguments_;
        }
        return true;
    }

    void agent_runtime::resolve_approval(const bool allow) {
        {
            lock<mutex> lock{mutex_};
            approval_answered_ = true;
            approval_allow_ = allow;
        }
        cv_.notify_all();
    }

    void agent_runtime::set_model(string model) {
        {
            lock<mutex> lock{mutex_};
            cfg_.model = move(model);
        }
    }

    vector<message> agent_runtime::history() const {
        lock<mutex> lock{mutex_};
        return history_;
    }

    void agent_runtime::reset_history() {
        lock<mutex> lock{mutex_};
        history_.clear();
        if (!cfg_.system_prompt.empty()) {
            message system_message;
            system_message.who = role::system;
            system_message.content = cfg_.system_prompt;
            history_.push_back(move(system_message));
        }
    }

    void agent_runtime::push(event e) {
        lock<mutex> lock{mutex_};
        queue_.push_back(move(e));
    }

    bool agent_runtime::cancelled() const {
        lock<mutex> lock{mutex_};
        return cancel_;
    }

    void agent_runtime::finish() {
        lock<mutex> lock{mutex_};
        busy_ = false;
        approval_waiting_ = false;
        cancel_ = false;
    }

    void agent_runtime::run(string prompt) {
        message user_message;
        user_message.who = role::user;
        user_message.content = move(prompt);
        {
            lock<mutex> lock{mutex_};
            history_.push_back(user_message);
        }
        store_.append(user_message);

        for (int step = 0; step < cfg_.max_steps; ++step) {
            if (cancelled()) {
                event notice;
                notice.kind = event_kind::error;
                notice.text = "cancelled";
                push(move(notice));
                finish();
                return;
            }

            vector<message> snapshot;
            {
                lock<mutex> lock{mutex_};
                snapshot = history_;
            }

            const completion result = provider_.complete(snapshot, tools_.specs(), cfg_, [this](const delta& chunk) {
                if (!chunk.content.empty()) {
                    event item;
                    item.kind = event_kind::text;
                    item.text = chunk.content;
                    push(move(item));
                }
                if (!chunk.reasoning.empty()) {
                    event item;
                    item.kind = event_kind::reasoning;
                    item.text = chunk.reasoning;
                    push(move(item));
                }
            });

            if (cancelled()) {
                event notice;
                notice.kind = event_kind::error;
                notice.text = "cancelled";
                push(move(notice));
                finish();
                return;
            }
            if (!result.ok) {
                event failure;
                failure.kind = event_kind::error;
                failure.text = result.error.empty() ? string{"model call failed"} : result.error;
                push(move(failure));
                finish();
                return;
            }

            message assistant;
            assistant.who = role::assistant;
            assistant.content = result.content;
            assistant.reasoning = result.reasoning;
            assistant.calls = result.calls;

            if (result.tokens.prompt_tokens > 0 || result.tokens.completion_tokens > 0) {
                event tokens;
                tokens.kind = event_kind::usage;
                tokens.tokens = result.tokens;
                push(move(tokens));
            }

            if (assistant.calls.empty()) {
                if (!assistant.content.empty()) {
                    lock<mutex> lock{mutex_};
                    history_.push_back(assistant);
                }
                store_.append(assistant);
                event finished;
                finished.kind = event_kind::done;
                push(move(finished));
                finish();
                return;
            }

            {
                lock<mutex> lock{mutex_};
                history_.push_back(assistant);
            }
            store_.append(assistant);

            for (const auto& call: assistant.calls) {
                if (cancelled()) {
                    break;
                }
                event started;
                started.kind = event_kind::tool_start;
                started.name = call.name;
                started.arguments = call.arguments;
                push(move(started));

                bool allowed = true;
                const decision verdict = guard_.check(call.name.view(), call.arguments.view());
                if (verdict == decision::deny) {
                    allowed = false;
                } else if (verdict == decision::ask) {
                    {
                        lock<mutex> lock{mutex_};
                        approval_waiting_ = true;
                        approval_answered_ = false;
                        approval_allow_ = false;
                        approval_name_ = call.name;
                        approval_arguments_ = call.arguments;
                    }
                    event request;
                    request.kind = event_kind::approval;
                    request.name = call.name;
                    request.arguments = call.arguments;
                    push(move(request));

                    unique_lock<mutex> lock{mutex_};
                    cv_.wait(lock, [this] { return approval_answered_ || cancel_; });
                    allowed = approval_allow_ && !cancel_;
                    approval_waiting_ = false;
                }

                tool_result outcome;
                if (!allowed) {
                    outcome.ok = false;
                    outcome.output = "permission denied";
                } else {
                    outcome = tools_.invoke(call.name.view(), call.arguments.view(), cfg_.workspace);
                }

                message tool_message;
                tool_message.who = role::tool;
                tool_message.tool_call_id = call.id;
                tool_message.name = call.name;
                tool_message.content = outcome.output;
                {
                    lock<mutex> lock{mutex_};
                    history_.push_back(tool_message);
                }
                store_.append(tool_message);

                event completed;
                completed.kind = event_kind::tool_result;
                completed.name = call.name;
                completed.ok = outcome.ok;
                completed.text = outcome.output;
                push(move(completed));
            }
        }

        event limit;
        limit.kind = event_kind::error;
        limit.text = "step limit reached";
        push(move(limit));
        finish();
    }
} // namespace newave::agent
