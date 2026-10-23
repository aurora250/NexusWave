#ifndef NEXUSWAVE_AGENT_SESSION_HPP__
#define NEXUSWAVE_AGENT_SESSION_HPP__
#include <NeForce/core/file/file.hpp>
#include "types.hpp"
namespace newave::agent {
    /**
     * @brief 会话日志
     *
     * JSONL，一行一条消息
     */
    class session_store {
    public:
        /**
         * @brief 构造
         * @param directory 会话目录（不存在时自动创建）
         */
        explicit session_store(neforce::path directory);

        /**
         * @brief 开一个新会话
         * @return 会话 ID
         */
        neforce::string start_new();

        /**
         * @brief 追加一条消息
         * @param msg 消息
         * @return 成功返回 true
         */
        bool append(const message& msg);

        /**
         * @brief 读取当前会话的全部消息
         * @return 消息列表
         */
        NEFORCE_NODISCARD neforce::vector<message> load_current() const;

        /**
         * @brief 读取指定会话
         * @param id 会话 ID
         */
        NEFORCE_NODISCARD neforce::vector<message> load(neforce::string_view id) const;

        /**
         * @brief 当前会话 ID
         */
        NEFORCE_NODISCARD const neforce::string& id() const noexcept { return id_; }

        /**
         * @brief 当前会话文件路径
         */
        NEFORCE_NODISCARD const neforce::string& file_path() const noexcept { return file_; }

        /**
         * @brief 是否已开始会话
         */
        NEFORCE_NODISCARD bool active() const noexcept { return !id_.empty(); }

    private:
        neforce::path directory_;
        neforce::string id_;
        neforce::string file_;
    };
} // namespace newave::agent
#endif
