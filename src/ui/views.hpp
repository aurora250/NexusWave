#ifndef NEXUSWAVE_UI_VIEWS_HPP__
#define NEXUSWAVE_UI_VIEWS_HPP__
#include "app/app_state.hpp"
#include <NeForce/tui/dom/element.hpp>
#include <NeForce/tui/dom/theme.hpp>
namespace newave::views {
    namespace tui = neforce::tui;

    /**
     * @brief 启动页：字标 + 副标题 + 提示
     * @param palette 主题
     * @param now_ms 当前时间
     * @param columns 可用宽度
     */
    tui::element splash(const tui::theme& palette, neforce::int64_t now_ms, int columns);

    /**
     * @brief 一行状态栏
     * @param state 会话状态
     * @param palette 主题
     * @param columns 可用宽度
     * @param now_ms 当前时间
     */
    tui::element status_bar(const session_state& state, const tui::theme& palette, int columns,
                            neforce::int64_t now_ms);

    /**
     * @brief 渲染一条转录行
     * @param msg 消息
     * @param palette 主题
     * @param width 内容宽度
     * @param math 是否渲染公式与富文本
     */
    tui::element transcript_row(const message& msg, const tui::theme& palette, int width, bool math);

    /**
     * @brief 审批面板
     * @param tool 工具名
     * @param arguments 参数
     * @param palette 主题
     * @param columns 可用宽度
     */
    tui::element approval_panel(const neforce::string& tool, const neforce::string& arguments,
                                const tui::theme& palette, int columns);

    /**
     * @brief 帮助页
     * @param palette 主题
     * @param columns 可用宽度
     */
    tui::element help_view(const tui::theme& palette, int columns);

    /**
     * @brief 运行中的活动行
     * @param palette 主题
     * @param now_ms 当前时间
     */
    tui::element activity_line(const tui::theme& palette, neforce::int64_t now_ms);
} // namespace newave::views
#endif
