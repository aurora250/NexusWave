#ifndef NEXUSWAVE_AGENT_PROVIDER_HPP__
#define NEXUSWAVE_AGENT_PROVIDER_HPP__
#include "tools.hpp"
#include "types.hpp"
#include <NeForce/core/functional/function.hpp>
#include <NeForce/network/http/http_client.hpp>
namespace newave::agent {
    namespace http = neforce::http;

    using delta_handler = neforce::function<void(const delta&)>;

    /**
     * @brief 对话补全提供者
     */
    class chat_provider {
    public:
        virtual ~chat_provider() = default;

        /**
         * @brief 发起一次流式补全（阻塞直到完成）
         * @param messages 历史消息
         * @param tools 可用工具
         * @param cfg 配置
         * @param on_delta 增量回调（在调用线程执行）
         * @return 结果
         */
        virtual completion complete(const neforce::vector<message>& messages, const neforce::vector<tool_spec>& tools,
                                    const config& cfg, const delta_handler& on_delta) = 0;

        /**
         * @brief 取消进行中的请求
         */
        virtual void cancel() = 0;
    };

    /**
     * @brief OpenAI 兼容的流式补全实现
     */
    class openai_provider final : public chat_provider {
    public:
        completion complete(const neforce::vector<message>& messages, const neforce::vector<tool_spec>& tools,
                            const config& cfg, const delta_handler& on_delta) override;
        void cancel() override;

    private:
        http::http_stream_handle handle_;
    };
} // namespace newave::agent

#endif
