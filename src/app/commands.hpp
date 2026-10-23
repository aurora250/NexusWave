#ifndef NEXUSWAVE_APP_COMMANDS_HPP__
#define NEXUSWAVE_APP_COMMANDS_HPP__
#include <NeForce/core/string/string.hpp>
namespace newave {
    /**
     * @brief 斜杠命令描述
     */
    struct command_spec {
        neforce::string_view name;    ///< 命令名（不含 `/`）
        neforce::string_view args;    ///< 参数提示
        neforce::string_view summary; ///< 一行说明
    };

    /**
     * @brief 解析后的输入
     */
    struct parsed_command {
        bool is_command = false; ///< 是否以 `/` 开头
        neforce::string name;    ///< 命令名
        neforce::string args;    ///< 参数（已去除首尾空白）
    };

    /**
     * @brief 解析一行输入
     * @param text 输入文本
     * @return 解析结果
     *
     * 只有整行形如 `/name [args]` 且 name 非空时才视为命令；
     * `/` 单独出现或 `/ name` 都按普通文本处理。
     */
    NEFORCE_NODISCARD parsed_command parse_command(neforce::string_view text);

    /**
     * @brief 内置命令表
     * @return 命令列表
     */
    NEFORCE_NODISCARD const neforce::vector<command_spec>& command_table();

    /**
     * @brief 按名称查找命令
     * @param name 命令名
     * @return 描述，未知返回 nullptr
     */
    NEFORCE_NODISCARD const command_spec* find_command(neforce::string_view name);

    /**
     * @brief 前缀匹配命令名
     * @param prefix 前缀（不含 `/`）
     * @param out 追加匹配项
     */
    void complete_commands(neforce::string_view prefix, neforce::vector<neforce::string>& out);
} // namespace newave
#endif
