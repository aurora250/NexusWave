#include "commands.hpp"
namespace newave {
    using namespace neforce;

    namespace {
        constexpr command_spec commands[] = {
                {{"help"}, {""}, {"显示快捷键与命令"}},
                {{"clear"}, {""}, {"清空当前转录"}},
                {{"new"}, {""}, {"开始一个新会话"}},
                {{"theme"}, {"<name|list>"}, {"切换或列出主题"}},
                {{"mode"}, {"<default|plan|full>"}, {"设置会话模式"}},
                {{"model"}, {"<id>"}, {"切换模型"}},
                {{"cancel"}, {""}, {"取消当前回合"}},
                {{"status"}, {""}, {"显示模型、模式、上下文与 token"}},
                {{"math"}, {"<on|off>"}, {"开关公式渲染"}},
                {{"ask"}, {"<text>"}, {"生成一段流式演示回复（无网络）"}},
                {{"demo"}, {""}, {"插入一段演示转录（表格/公式/工具卡）"}},
                {{"export"}, {"<path>"}, {"把转录导出为 Markdown"}},
                {{"about"}, {""}, {"版本与构建信息"}},
                {{"quit"}, {""}, {"退出"}},
        };

        string trim_copy(const string_view text) {
            size_t begin = 0;
            size_t end = text.size();
            while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) {
                ++begin;
            }
            while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
                --end;
            }
            return string{text.substr(begin, end - begin)};
        }
    } // namespace


    parsed_command parse_command(const string_view text) {
        parsed_command result;
        const string_view trimmed = [text] {
            size_t begin = 0;
            size_t end = text.size();
            while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) {
                ++begin;
            }
            while (end > begin && text[end - 1] == ' ') {
                --end;
            }
            return text.substr(begin, end - begin);
        }();

        if (trimmed.size() < 2 || trimmed[0] != '/') {
            return result;
        }
        size_t split = 1;
        while (split < trimmed.size() && trimmed[split] != ' ' && trimmed[split] != '\t') {
            ++split;
        }
        const string_view name = trimmed.substr(1, split - 1);
        if (name.empty()) {
            return result;
        }
        for (const char c: name) {
            const bool ok =
                    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
            if (!ok) {
                return result;
            }
        }
        result.is_command = true;
        result.name = string{name};
        result.args = split < trimmed.size() ? trim_copy(trimmed.substr(split, trimmed.size() - split)) : string();
        return result;
    }

    const vector<command_spec>& command_table() {
        static const vector<command_spec> table = [] {
            vector<command_spec> items;
            for (const auto& spec: commands) {
                items.push_back(spec);
            }
            return items;
        }();
        return table;
    }

    const command_spec* find_command(const string_view name) {
        for (const auto& spec: command_table()) {
            if (spec.name == name) {
                return &spec;
            }
        }
        return nullptr;
    }

    void complete_commands(const string_view prefix, vector<string>& out) {
        for (const auto& spec: command_table()) {
            if (prefix.empty() || spec.name.starts_with(prefix)) {
                const string name{spec.name};
                bool seen = false;
                for (const auto& existing: out) {
                    if (existing == name) {
                        seen = true;
                        break;
                    }
                }
                if (!seen) {
                    out.push_back(name);
                }
            }
        }
    }
} // namespace newave
