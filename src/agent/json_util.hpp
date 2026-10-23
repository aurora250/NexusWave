#ifndef NEXUSWAVE_AGENT_JSON_UTIL_HPP__
#define NEXUSWAVE_AGENT_JSON_UTIL_HPP__
#include "types.hpp"
#include <NeForce/core/file/json/json_value.hpp>
namespace newave::agent {
    /**
     * @brief 转义 JSON 字符串内容
     * @param text 原始文本
     * @return 不含引号的转义结果
     */
    neforce::string escape_json(neforce::string_view text);

    /**
     * @brief 把消息序列化为 OpenAI 兼容 JSON
     * @param messages 消息
     * @return JSON 数组文本
     */
    neforce::string messages_to_json(const neforce::vector<message>& messages);

    struct tool_spec;

    /**
     * @brief 把工具定义序列化为 JSON 数组
     * @param specs 工具名、说明与参数 schema 三元组
     * @return JSON 数组文本
     */
    neforce::string tools_to_json(const neforce::vector<tool_spec>& specs);

    /**
     * @brief 读取对象成员字符串
     */
    neforce::string member_string(const neforce::json_value& value, neforce::string_view key);

    /**
     * @brief 读取对象成员整数
     */
    neforce::int64_t member_int(const neforce::json_value& value, neforce::string_view key,
                                neforce::int64_t fallback = 0);
} // namespace newave::agent
#endif
