#ifndef NEXUSWAVE_APP_APP_STATE_HPP__
#define NEXUSWAVE_APP_APP_STATE_HPP__
#include <NeForce/core/string/string.hpp>
namespace newave {
    /**
     * @brief 会话模式
     */
    enum class session_mode : neforce::uint8_t {
        default_, ///< 默认（工作区可写 + 询问）
        plan,     ///< 计划模式（只读）
        full,     ///< 完全访问
    };

    /**
     * @brief 转录行类型
     */
    enum class row_kind : neforce::uint8_t {
        user,      ///< 用户消息
        assistant, ///< 助手正文
        thinking,  ///< 思考块
        tool,      ///< 工具卡
        notice,    ///< 系统提示
        error,     ///< 错误
    };

    /**
     * @brief 工具状态
     */
    enum class tool_state : neforce::uint8_t {
        running, ///< 运行中
        ok,      ///< 成功
        failed,  ///< 失败
    };

    /**
     * @brief 一条转录行
     */
    struct message {
        row_kind kind = row_kind::assistant; ///< 行类型
        neforce::string text;                ///< 正文
        neforce::string title;               ///< 标题（工具名/思考摘要）
        neforce::string detail;              ///< 详情（工具输出）
        tool_state state = tool_state::ok;   ///< 工具状态
        bool expanded = false;               ///< 是否展开
        neforce::int64_t elapsed_ms = 0;     ///< 耗时
        neforce::size_t streamed = 0;        ///< 已揭示字节数（流式）
    };

    /**
     * @brief 会话状态
     *
     * 只持有纯数据与查询函数，渲染与输入在 shell 中完成。
     */
    struct session_state {
        neforce::vector<message> messages;          ///< 转录
        session_mode mode = session_mode::default_; ///< 当前模式
        neforce::string model{"deepseek-v4-flash"}; ///< 模型标识
        neforce::size_t context_used = 0;           ///< 已用上下文 token
        neforce::size_t context_limit = 128000;     ///< 上下文上限
        neforce::size_t prompt_tokens = 0;          ///< 输入 token
        neforce::size_t reply_tokens = 0;           ///< 输出 token
        bool busy = false;                          ///< 是否有回合在运行
        bool math_enabled = true;                   ///< 是否渲染公式

        /**
         * @brief 追加一条消息
         * @param msg 消息
         * @return 消息索引
         */
        neforce::size_t push(message msg);

        /**
         * @brief 追加助手正文片段
         * @param chunk 片段
         *
         * 若最后一行是助手正文则合并，否则新建一行。
         */
        void append_assistant(neforce::string_view chunk);

        /**
         * @brief 当前上下文占用比例
         * @return 0-1，未知时为 0
         */
        NEFORCE_NODISCARD double context_ratio() const noexcept;

        /**
         * @brief 模式名称
         * @return 名称
         */
        NEFORCE_NODISCARD static neforce::string_view mode_name(session_mode mode) noexcept;

        /**
         * @brief 切换到下一个模式
         */
        void cycle_mode() noexcept;

        /**
         * @brief 清空转录
         */
        void clear();
    };
} // namespace newave
#endif
