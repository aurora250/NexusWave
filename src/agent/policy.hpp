#ifndef NEXUSWAVE_AGENT_POLICY_HPP__
#define NEXUSWAVE_AGENT_POLICY_HPP__
#include "types.hpp"
namespace newave::agent {
    /**
     * @brief 权限模式
     */
    enum class permission_mode : neforce::uint8_t {
        read_only,       ///< 只读
        workspace_write, ///< 工作区内可写，执行命令需审批
        full,            ///< 完全访问
    };

    /**
     * @brief 权限裁决
     */
    enum class decision : neforce::uint8_t {
        allow, ///< 直接允许
        ask,   ///< 需要用户确认
        deny,  ///< 拒绝
    };

    /**
     * @brief 工具权限策略
     */
    class policy {
    public:
        /**
         * @brief 构造
         * @param mode 模式
         * @param workspace 工作区根目录
         */
        policy(permission_mode mode, neforce::string workspace);

        /**
         * @brief 裁决一次工具调用
         * @param tool 工具名
         * @param arguments JSON 参数字符串
         * @return 裁决
         */
        NEFORCE_NODISCARD decision check(neforce::string_view tool, neforce::string_view arguments) const;

        /**
         * @brief 设置模式
         */
        void set_mode(permission_mode mode) noexcept { mode_ = mode; }

        /**
         * @brief 当前模式
         */
        NEFORCE_NODISCARD permission_mode mode() const noexcept { return mode_; }

        /**
         * @brief 工具类别
         */
        enum class category : neforce::uint8_t {
            read,  ///< 只读
            write, ///< 写文件
            exec,  ///< 执行命令
        };

        /**
         * @brief 工具类别
         */
        NEFORCE_NODISCARD static category classify(neforce::string_view tool) noexcept;

    private:
        permission_mode mode_;
        neforce::string workspace_;
    };
} // namespace newave::agent
#endif
