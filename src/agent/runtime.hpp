#ifndef NEXUSWAVE_AGENT_RUNTIME_HPP__
#define NEXUSWAVE_AGENT_RUNTIME_HPP__
#include <NeForce/core/async/condition_variable.hpp>
#include "policy.hpp"
#include "provider.hpp"
#include "session.hpp"
namespace newave::agent {
    /**
     * @brief 运行时事件类型
     */
    enum class event_kind : neforce::uint8_t {
        text,        ///< 正文增量
        reasoning,   ///< 思考增量
        tool_start,  ///< 工具开始
        tool_result, ///< 工具结束
        usage,       ///< 用量更新
        done,        ///< 回合结束
        error,       ///< 出错
        approval,    ///< 等待审批
    };

    /**
     * @brief 运行时事件
     */
    struct event {
        event_kind kind = event_kind::text; ///< 类型
        neforce::string text;               ///< 文本
        neforce::string name;               ///< 工具名
        neforce::string arguments;          ///< 工具参数
        bool ok = true;                     ///< 工具是否成功
        usage tokens;                       ///< 用量
    };

    /**
     * @brief 一回合 Agent 运行时（工作线程 + 事件队列）
     *
     * 模型调用与工具执行都在工作线程完成；UI 线程通过 `drain()` 取事件，
     * 通过 `resolve_approval()` 回答审批请求。
     */
    class agent_runtime {
    public:
        /**
         * @brief 构造
         * @param provider 补全提供者
         * @param tools 工具集
         * @param guard 权限策略
         * @param store 会话日志
         * @param cfg 配置
         */
        agent_runtime(chat_provider& provider, const tool_registry& tools, policy& guard, session_store& store,
                      config cfg);

        ~agent_runtime();

        agent_runtime(const agent_runtime&) = delete;
        agent_runtime& operator=(const agent_runtime&) = delete;

        /**
         * @brief 开始一回合
         * @param prompt 用户输入
         */
        void start(neforce::string prompt);

        /**
         * @brief 请求取消
         */
        void cancel();

        /**
         * @brief 是否正在运行
         */
        NEFORCE_NODISCARD bool busy() const;

        /**
         * @brief 取走队列中的事件
         */
        void drain(neforce::vector<event>& out);

        /**
         * @brief 是否有待审批的请求
         * @param name 工具名输出
         * @param arguments 参数输出
         */
        NEFORCE_NODISCARD bool approval_pending(neforce::string* name, neforce::string* arguments) const;

        /**
         * @brief 回答审批
         * @param allow 是否允许
         */
        void resolve_approval(bool allow);

        /**
         * @brief 历史消息副本
         */
        NEFORCE_NODISCARD neforce::vector<message> history() const;

        /**
         * @brief 清空历史（保留系统提示）
         */
        void reset_history();

        /**
         * @brief 更新模型名
         * @param model 模型标识
         */
        void set_model(neforce::string model);

        /**
         * @brief 会话存储
         */
        NEFORCE_NODISCARD session_store& store() noexcept { return store_; }

    private:
        void run(neforce::string prompt);
        void push(event e);
        bool cancelled() const;
        void finish();

        chat_provider& provider_;
        const tool_registry& tools_;
        policy& guard_;
        session_store& store_;
        config cfg_;

        mutable neforce::mutex mutex_;
        neforce::condition_variable cv_;
        neforce::vector<event> queue_;
        neforce::vector<message> history_;
        neforce::thread worker_;
        bool busy_ = false;
        bool cancel_ = false;
        bool approval_waiting_ = false;
        bool approval_answered_ = false;
        bool approval_allow_ = false;
        neforce::string approval_name_;
        neforce::string approval_arguments_;
    };
} // namespace newave::agent
#endif
