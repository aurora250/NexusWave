#ifndef NEXUSWAVE_AGENT_TYPES_HPP__
#define NEXUSWAVE_AGENT_TYPES_HPP__
#include <NeForce/core/string/string.hpp>
namespace newave::agent {
    /**
     * @brief 消息角色
     */
    enum class role : neforce::uint8_t {
        system,    ///< 系统提示
        user,      ///< 用户
        assistant, ///< 模型
        tool,      ///< 工具结果
    };

    /**
     * @brief 一次工具调用
     */
    struct tool_call {
        neforce::string id;        ///< 调用 ID
        neforce::string name;      ///< 工具名
        neforce::string arguments; ///< JSON 参数字符串
    };

    /**
     * @brief 一条对话消息
     */
    struct message {
        role who = role::user;            ///< 角色
        neforce::string content;          ///< 正文
        neforce::string reasoning;        ///< 思考内容（仅 assistant）
        neforce::vector<tool_call> calls; ///< 工具调用（仅 assistant）
        neforce::string tool_call_id;     ///< 对应的调用 ID（仅 tool）
        neforce::string name;             ///< 工具名（仅 tool）
    };

    /**
     * @brief token 用量
     */
    struct usage {
        neforce::size_t prompt_tokens = 0;     ///< 输入 token
        neforce::size_t completion_tokens = 0; ///< 输出 token
    };

    /**
     * @brief 流式增量
     */
    struct delta {
        neforce::string content;   ///< 正文增量
        neforce::string reasoning; ///< 思考增量
        bool finished = false;     ///< 是否收到结束标记
        usage tokens;              ///< 随增量上报的用量
    };

    /**
     * @brief 一次模型调用的结果
     */
    struct completion {
        bool ok = false;                  ///< 是否成功
        neforce::string error;            ///< 错误信息
        neforce::string finish_reason;    ///< 结束原因
        neforce::string content;          ///< 完整正文
        neforce::string reasoning;        ///< 完整思考
        neforce::vector<tool_call> calls; ///< 工具调用
        usage tokens;                     ///< 用量
    };

    /**
     * @brief 运行配置
     */
    struct config {
        neforce::string base_url{"https://api.deepseek.com"}; ///< 基址
        neforce::string api_key;                              ///< 密钥
        neforce::string model{"deepseek-chat"};               ///< 模型
        neforce::string system_prompt;                        ///< 系统提示
        neforce::size_t max_tokens = 4096;                    ///< 单次输出上限
        int max_steps = 24;                                   ///< 单回合最大步数
        neforce::string workspace{"."};                       ///< 工作区根目录
    };
} // namespace newave::agent
#endif
