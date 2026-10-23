#ifndef NEXUSWAVE_AGENT_SSE_HPP__
#define NEXUSWAVE_AGENT_SSE_HPP__
#include <NeForce/core/functional/function.hpp>
#include <NeForce/core/string/string.hpp>
namespace newave::agent {
    /**
     * @brief Server-Sent Events 增量解析器
     *
     * 按行缓冲，空行派发一个事件的全部 `data:` 行（以换行拼接）。
     * 注释行（`:` 开头）与未知字段被忽略。
     */
    class sse_parser {
    public:
        /**
         * @brief 喂入一段字节
         * @param chunk 片段
         * @param on_event 收到完整事件时的回调（参数为 data 内容）
         */
        void feed(neforce::string_view chunk, const neforce::function<void(neforce::string_view)>& on_event);

        /**
         * @brief 清空缓冲
         */
        void reset();

    private:
        neforce::string buffer_;
        neforce::string data_;
    };
} // namespace newave::agent
#endif
