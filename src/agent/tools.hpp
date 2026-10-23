#ifndef NEXUSWAVE_AGENT_TOOLS_HPP__
#define NEXUSWAVE_AGENT_TOOLS_HPP__
#include <NeForce/core/file/json/json_value.hpp>
#include <NeForce/core/functional/function.hpp>
namespace newave::agent {
    /**
     * @brief 工具描述
     */
    struct tool_spec {
        neforce::string name;        ///< 名称
        neforce::string description; ///< 说明
        neforce::string parameters;  ///< JSON Schema 文本
    };

    /**
     * @brief 工具执行结果
     */
    struct tool_result {
        bool ok = false;        ///< 是否成功
        neforce::string output; ///< 输出文本
    };

    using tool_handler =
            neforce::function<tool_result(const neforce::json_value& args, const neforce::string& workspace)>;

    /**
     * @brief 工具注册表
     */
    class tool_registry {
    public:
        /**
         * @brief 注册一个工具
         */
        void add(tool_spec spec, tool_handler handler);

        /**
         * @brief 已注册工具的描述列表
         */
        NEFORCE_NODISCARD const neforce::vector<tool_spec>& specs() const noexcept { return specs_; }

        /**
         * @brief 是否存在指定工具
         */
        NEFORCE_NODISCARD bool has(neforce::string_view name) const;

        /**
         * @brief 调用工具
         * @param name 工具名
         * @param arguments JSON 参数字符串
         * @param workspace 工作区根目录
         */
        NEFORCE_NODISCARD tool_result invoke(neforce::string_view name, neforce::string_view arguments,
                                             const neforce::string& workspace) const;

        /**
         * @brief 构造内置工具集
         */
        static tool_registry make_builtin();

    private:
        neforce::vector<tool_spec> specs_;
        neforce::vector<tool_handler> handlers_;
    };

    /**
     * @brief 把相对路径解析到工作区
     */
    NEFORCE_NODISCARD neforce::string resolve_path(const neforce::string& workspace, neforce::string_view path);

    /**
     * @brief 判断目标路径是否位于工作区内
     */
    NEFORCE_NODISCARD bool path_is_within(const neforce::string& workspace, neforce::string_view path);

    /**
     * @brief 通配符匹配（支持 `**`、`*`、`?`）
     */
    NEFORCE_NODISCARD bool glob_match(neforce::string_view pattern, neforce::string_view text);
} // namespace newave::agent
#endif
