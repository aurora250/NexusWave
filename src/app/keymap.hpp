#ifndef NEXUSWAVE_APP_KEYMAP_HPP__
#define NEXUSWAVE_APP_KEYMAP_HPP__
#include <NeForce/tui/events.hpp>
namespace newave {
    namespace tui = neforce::tui;

    /**
     * @brief 全局按键意图
     */
    enum class key_intent : neforce::uint8_t {
        none,          ///< 交给输入区
        quit,          ///< 退出
        force_quit,    ///< 连续两次退出
        toggle_help,   ///< 打开/关闭帮助
        interrupt,     ///< 中断当前回合
        clear_input,   ///< 清空输入
        cycle_mode,    ///< 切换会话模式
        scroll_up,     ///< 上滚转录
        scroll_down,   ///< 下滚转录
        page_up,       ///< 翻页
        page_down,     ///< 翻页
        toggle_expand, ///< 展开/收起详情
        toggle_math,   ///< 开关公式
        redraw,        ///< 强制重绘
    };

    /**
     * @brief 按键上下文
     */
    struct key_context {
        bool input_empty = true;     ///< 输入框是否为空
        bool help_open = false;      ///< 帮助是否打开
        bool busy = false;           ///< 是否有回合在运行
        bool inline_command = false; ///< 输入是否已构成完整命令
    };

    /**
     * @brief 解析全局按键意图
     * @param key 按键
     * @param ctx 上下文
     * @return 意图；`none` 表示交给输入区处理
     */
    NEFORCE_NODISCARD key_intent resolve_key(const tui::key_event& key, const key_context& ctx);

    /**
     * @brief 帮助页打开时的按键意图
     * @param key 按键
     * @return 意图
     */
    NEFORCE_NODISCARD key_intent resolve_help_key(const tui::key_event& key);
} // namespace newave

#endif
