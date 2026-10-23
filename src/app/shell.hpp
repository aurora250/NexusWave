#ifndef NEXUSWAVE_APP_SHELL_HPP__
#define NEXUSWAVE_APP_SHELL_HPP__
#include "agent/runtime.hpp"
#include "app_state.hpp"
#include "commands.hpp"
#include "keymap.hpp"
#include <NeForce/tui/component/component.hpp>
#include <NeForce/tui/component/composer.hpp>
#include <NeForce/tui/dom/theme.hpp>
namespace newave {
    /**
     * @brief 聊天主界面
     *
     * 负责三块区域：转录（虚拟列表）、输入区（composer 子组件）、状态栏，
     * 以及命令分发、全局按键与 Agent 运行时事件的消费。普通输入交给
     * `agent::agent_runtime` 在工作线程运行；未配置 API key 时只提供 `/demo` 演示。
     */
    class shell final : public tui::component<> {
    public:
        /**
         * @brief 构造
         * @param theme_name 初始主题名（空表示 auto）
         * @param provider 测试用的补全提供者（为空时按环境变量创建）
         */
        explicit shell(neforce::string theme_name = neforce::string(),
                       neforce::unique_ptr<agent::chat_provider> provider = nullptr);

        void setup() override;

        tui::element render() override;

        bool preview_key(const tui::key_event& e) override;

        bool on_key(const tui::key_event& e) override;

        bool on_paste(const tui::paste_event& e) override;

        bool on_mouse(const tui::mouse_event& e) override;

        void on_animation(neforce::int64_t delta) override;

        NEFORCE_NODISCARD bool focusable() const override { return true; }

        /**
         * @brief 当前会话状态
         */
        NEFORCE_NODISCARD const session_state& state() const noexcept { return state_; }

        /**
         * @brief 注入一段文本并提交
         */
        void submit(const neforce::string& text);

    private:
        void init_agent();
        void drain_agent_events();
        void handle_agent_event(const agent::event& event);
        void sync_policy() const;
        void execute(const parsed_command& command);
        void start_demo_reply(neforce::string_view prompt);
        void scroll_by(int rows);
        void push_notice(neforce::string text);
        void push_tool_demo();
        bool input_empty() const;
        void apply_theme(neforce::string_view name);
        void export_markdown(neforce::string_view path_text);

        session_state state_;
        tui::theme palette_;
        neforce::string theme_name_;
        bool help_open_ = false;
        neforce::string pending_reply_;
        neforce::size_t pending_offset_ = 0;
        neforce::int64_t now_ms_ = 0;

        tui::state<int>* scroll_ = nullptr;
        tui::state<int>* viewport_rows_ = nullptr;
        tui::state<tui::layout_rect>* viewport_rect_ = nullptr;
        neforce::vector<int> row_heights_;

        neforce::unique_ptr<tui::components::composer> composer_;
        tui::components::composer* composer_view_ = nullptr;

        neforce::unique_ptr<agent::chat_provider> injected_provider_;
        agent::config agent_cfg_;
        agent::tool_registry tools_;
        neforce::unique_ptr<agent::openai_provider> provider_;
        neforce::unique_ptr<agent::policy> guard_;
        neforce::unique_ptr<agent::session_store> session_;
        neforce::unique_ptr<agent::agent_runtime> runtime_;
        int assistant_row_ = -1;
        int thinking_row_ = -1;
        int tool_row_ = -1;
        bool approval_open_ = false;
        neforce::string approval_name_;
        neforce::string approval_arguments_;
    };
} // namespace newave
#endif
