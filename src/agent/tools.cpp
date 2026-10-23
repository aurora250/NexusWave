#include "tools.hpp"
#include "json_util.hpp"
#include <NeForce/core/async/io_context.hpp>
#include <NeForce/core/file/file.hpp>
#include <NeForce/core/file/filesystem.hpp>
#include <NeForce/core/file/json/json_parser.hpp>
#include <NeForce/core/string/regex.hpp>
#include <NeForce/core/string/to_string.hpp>
#include <NeForce/network/http/http_client.hpp>
#ifdef NEFORCE_PLATFORM_WINDOWS
#    include <windows.h>
#else
#    include <dirent.h>
#    include <sys/stat.h>
#endif
namespace newave::agent {
    using namespace neforce;

    namespace {
        constexpr size_t max_output = 24000;
        constexpr size_t max_read_lines = 2000;

        string truncate_output(string text) {
            if (text.size() <= max_output) {
                return text;
            }
            text = string{text.view().substr(0, max_output)};
            text += "\n... (truncated)";
            return text;
        }

        vector<string> split_lines(const string_view text) {
            vector<string> lines;
            size_t start = 0;
            for (size_t i = 0; i <= text.size(); ++i) {
                if (i == text.size() || text[i] == '\n') {
                    lines.push_back(string{text.substr(start, i - start)});
                    start = i + 1;
                }
            }
            return lines;
        }

        bool is_binary(const string_view data) {
            const size_t limit = min<size_t>(data.size(), 4096);
            for (size_t i = 0; i < limit; ++i) {
                if (data[i] == '\0') {
                    return true;
                }
            }
            return false;
        }

        string read_file_text(const string& file_text) {
            const path location{file_text};
            if (!location.exists()) {
                return string();
            }
            file input{location, false, file_access::read};
            if (!input.is_opened()) {
                return string();
            }
            return input.read();
        }

        void walk_files(const string& root, vector<string>& out) {
#ifdef NEFORCE_PLATFORM_WINDOWS
            string pattern{root};
            pattern += "\\*";
            WIN32_FIND_DATAA data{};
            const HANDLE handle = ::FindFirstFileA(pattern.c_str(), &data);
            if (handle == INVALID_HANDLE_VALUE) {
                return;
            }
            do {
                const string_view name{data.cFileName};
                if (name == "." || name == "..") {
                    continue;
                }
                string child = root + "\\" + string{name};
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                    walk_files(child, out);
                } else {
                    out.push_back(move(child));
                }
            } while (::FindNextFileA(handle, &data) != 0);
            ::FindClose(handle);
#else
            ::DIR* dir = ::opendir(root.data());
            if (dir == nullptr) {
                return;
            }
            const ::dirent* entry = nullptr;
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            while ((entry = ::readdir(dir)) != nullptr) {
                const string_view name{entry->d_name};
                if (name == "." || name == "..") {
                    continue;
                }
                string child = root + "/" + string{name};
                struct ::stat st{};
                if (::stat(child.data(), &st) != 0) {
                    continue;
                }
                if (S_ISDIR(st.st_mode)) {
                    walk_files(child, out);
                } else if (S_ISREG(st.st_mode)) {
                    out.push_back(move(child));
                }
            }
            ::closedir(dir);
#endif
        }

        string relative_to(const string& base, const string& path) {
            if (path.size() > base.size() && path.view().starts_with(base.view())) {
                size_t cut = base.size();
                while (cut < path.size() && (path[cut] == '/' || path[cut] == '\\')) {
                    ++cut;
                }
                return string{path.view().substr(cut)};
            }
            return path;
        }

        string string_member(const json_value& args, const string_view key, const string& fallback = string()) {
            const auto* object = args.as_object();
            if (object == nullptr) {
                return fallback;
            }
            const json_value* member = object->get_member(string{key});
            if (member == nullptr || member->as_string() == nullptr) {
                return fallback;
            }
            return member->as_string()->get_value();
        }

        int64_t int_member(const json_value& args, const string_view key, const int64_t fallback) {
            const auto* object = args.as_object();
            if (object == nullptr) {
                return fallback;
            }
            const json_value* member = object->get_member(string{key});
            if (member == nullptr || member->as_number() == nullptr) {
                return fallback;
            }
            return static_cast<int64_t>(member->as_number()->get_value());
        }

        bool bool_member(const json_value& args, const string_view key, const bool fallback) {
            const auto* object = args.as_object();
            if (object == nullptr) {
                return fallback;
            }
            const json_value* member = object->get_member(string{key});
            if (member == nullptr || member->as_bool() == nullptr) {
                return fallback;
            }
            return member->as_bool()->get_value();
        }

        constexpr string_view schema_read =
                R"({"type":"object","properties":{"path":{"type":"string"},"offset":{"type":"integer"},"limit":{"type":"integer"}},"required":["path"]})";
        constexpr string_view schema_write =
                R"({"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]})";
        constexpr string_view schema_edit =
                R"({"type":"object","properties":{"path":{"type":"string"},"old":{"type":"string"},"new":{"type":"string"},"all":{"type":"boolean"}},"required":["path","old","new"]})";
        constexpr string_view schema_glob =
                R"({"type":"object","properties":{"pattern":{"type":"string"}},"required":["pattern"]})";
        constexpr string_view schema_grep =
                R"({"type":"object","properties":{"pattern":{"type":"string"},"path":{"type":"string"}},"required":["pattern"]})";
        constexpr string_view schema_bash =
                R"({"type":"object","properties":{"command":{"type":"string"}},"required":["command"]})";
        constexpr string_view schema_todo =
                R"({"type":"object","properties":{"todos":{"type":"array","items":{"type":"object","properties":{"content":{"type":"string"},"status":{"type":"string"}},"required":["content","status"]}}},"required":["todos"]})";
        constexpr string_view schema_fetch =
                R"({"type":"object","properties":{"url":{"type":"string"}},"required":["url"]})";

        tool_result run_read(const json_value& args, const string& workspace) {
            const string file_path = resolve_path(workspace, string_member(args, "path").view());
            const string text = read_file_text(file_path);
            if (text.empty()) {
                return tool_result{false, "file not found or empty: " + file_path};
            }
            const int64_t offset = max<int64_t>(1, int_member(args, "offset", 1));
            const int64_t limit = max<int64_t>(1, min<int64_t>(int_member(args, "limit", 2000), max_read_lines));
            vector<string> lines = split_lines(text.view());
            if (!lines.empty() && lines.back().empty()) {
                lines.pop_back();
            }
            string out;
            for (int64_t i = offset - 1; i < static_cast<int64_t>(lines.size()) && i < offset - 1 + limit; ++i) {
                out += to_string(i + 1);
                out += "\t";
                out += lines[static_cast<size_t>(i)];
                out += "\n";
            }
            return tool_result{true, truncate_output(move(out))};
        }

        tool_result run_write(const json_value& args, const string& workspace) {
            const string file_path = resolve_path(workspace, string_member(args, "path").view());
            const string content = string_member(args, "content");
            const path location{file_path};
            const path parent = location.parent_path();
            if (!parent.empty()) {
                filesystem::create_directories(parent);
            }
            if (!filesystem::create_and_write(location, content)) {
                return tool_result{false, "write failed: " + file_path};
            }
            return tool_result{true,
                               "wrote " + to_string(static_cast<int64_t>(content.size())) + " bytes to " + file_path};
        }

        tool_result run_edit(const json_value& args, const string& workspace) {
            const string file_path = resolve_path(workspace, string_member(args, "path").view());
            const string old_text = string_member(args, "old");
            const string new_text = string_member(args, "new");
            const bool replace_all = bool_member(args, "all", false);
            const string text = read_file_text(file_path);
            if (text.empty()) {
                return tool_result{false, "file not found or empty: " + file_path};
            }
            if (old_text.empty()) {
                return tool_result{false, "old text must not be empty"};
            }
            size_t count = 0;
            size_t pos = 0;
            while ((pos = text.view().find(string_view{old_text.view()}, pos)) != string_view::npos) {
                ++count;
                pos += old_text.size();
            }
            if (count == 0) {
                return tool_result{false, "old text not found in " + file_path};
            }
            if (count > 1 && !replace_all) {
                return tool_result{false, "old text matches " + to_string(static_cast<int64_t>(count)) +
                                                  " times; set all=true to replace every occurrence"};
            }
            string result;
            result.reserve(text.size() + new_text.size());
            size_t cursor = 0;
            pos = 0;
            while ((pos = text.view().find(string_view{old_text.view()}, pos)) != string_view::npos) {
                result.append(text.view().substr(cursor, pos - cursor));
                result += new_text;
                pos += old_text.size();
                cursor = pos;
                if (!replace_all) {
                    break;
                }
            }
            result.append(text.view().substr(cursor));
            if (!filesystem::create_and_write(path{file_path}, result)) {
                return tool_result{false, "write failed: " + file_path};
            }
            return tool_result{true, "replaced " + to_string(static_cast<int64_t>(replace_all ? count : 1)) +
                                             " occurrence(s) in " + file_path};
        }

        tool_result run_glob(const json_value& args, const string& workspace) {
            const string pattern = string_member(args, "pattern", "*");
            vector<string> files;
            walk_files(workspace, files);
            string out;
            size_t matches = 0;
            for (const auto& file: files) {
                const string relative = relative_to(workspace, file);
                if (glob_match(pattern.view(), relative.view())) {
                    out += relative;
                    out += "\n";
                    ++matches;
                    if (matches >= 200) {
                        break;
                    }
                }
            }
            if (matches == 0) {
                return tool_result{true, "no files matched " + pattern};
            }
            return tool_result{true, truncate_output(move(out))};
        }

        tool_result run_grep(const json_value& args, const string& workspace) {
            const string pattern = string_member(args, "pattern");
            if (pattern.empty()) {
                return tool_result{false, "pattern is required"};
            }
            try {
                const regex probe{pattern};
                (void) probe;
            } catch (const exception&) {
                return tool_result{false, "invalid regex: " + pattern};
            }
            string root = string_member(args, "path", ".");
            root = resolve_path(workspace, root.view());
            vector<string> files;
            const path root_path{root};
            if (!root_path.exists()) {
                return tool_result{false, "path not found: " + root};
            }
            walk_files(root, files);
            string out;
            size_t matches = 0;
            for (const auto& file: files) {
                if (file.size() > 512 * 1024) {
                    continue;
                }
                const string text = read_file_text(file);
                if (text.empty() || is_binary(text.view())) {
                    continue;
                }
                const vector<string> lines = split_lines(text.view());
                for (size_t i = 0; i < lines.size(); ++i) {
                    if (!regex{pattern}.search(lines[i]).matched()) {
                        continue;
                    }
                    out += relative_to(workspace, file);
                    out += ":";
                    out += to_string(static_cast<int64_t>(i + 1));
                    out += ": ";
                    if (lines[i].size() > 200) {
                        out.append(lines[i].view().substr(0, 200));
                    } else {
                        out += lines[i];
                    }
                    out += "\n";
                    if (++matches >= 200) {
                        break;
                    }
                }
                if (matches >= 200) {
                    break;
                }
            }
            if (matches == 0) {
                return tool_result{true, "no matches for " + pattern};
            }
            return tool_result{true, truncate_output(move(out))};
        }

        tool_result run_bash(const json_value& args, const string& workspace) {
            const string command = string_member(args, "command");
            if (command.empty()) {
                return tool_result{false, "command is required"};
            }
            string full = "cd \"" + workspace + "\" && " + command;
#ifdef NEFORCE_PLATFORM_WINDOWS
            FILE* pipe = ::_popen(full.data(), "r");
#else
            FILE* pipe = ::popen(full.data(), "r");
#endif
            if (pipe == nullptr) {
                return tool_result{false, "failed to start command"};
            }
            string output;
            char buffer[4096];
            while (::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
                if (output.size() > max_output) {
                    break;
                }
            }
#ifdef NEFORCE_PLATFORM_WINDOWS
            const int status = ::_pclose(pipe);
#else
            const int status = ::pclose(pipe);
#endif
            if (output.empty()) {
                output = "(no output)";
            }
            output += "\nexit status: " + to_string(static_cast<int64_t>(status));
            const bool ok = status == 0;
            return tool_result{ok, truncate_output(move(output))};
        }

        tool_result run_todo(const json_value& args, const string&) {
            const auto* object = args.as_object();
            if (object == nullptr) {
                return tool_result{false, "todos are required"};
            }
            const json_value* todos = object->get_member(string{"todos"});
            if (todos == nullptr || todos->as_array() == nullptr) {
                return tool_result{false, "todos must be an array"};
            }
            string out;
            for (const auto& item: todos->as_array()->get_elements()) {
                if (item == nullptr || item->as_object() == nullptr) {
                    continue;
                }
                const string content = member_string(*item, "content");
                const string status = member_string(*item, "status");
                const string_view mark = status == "completed" ? "[x]" : (status == "in_progress" ? "[~]" : "[ ]");
                out += mark;
                out += " ";
                out += content;
                out += "\n";
            }
            return tool_result{true, out.empty() ? string{"(empty todo list)"} : out};
        }

        tool_result run_fetch(const json_value& args, const string& /*unused*/) {
            const string url = string_member(args, "url");
            if (url.empty()) {
                return tool_result{false, "url is required"};
            }
            try {
                io_context context;
                http::http_client client{context};
                const auto response = client.get(url);
                string body = response.body;
                if (body.size() > 24000) {
                    body = string{body.view().substr(0, 24000)} + "\n... (truncated)";
                }
                return tool_result{true, body};
            } catch (const exception& e) {
                return tool_result{false, string{"fetch failed: "} + e.what()};
            }
        }
    } // namespace

    string resolve_path(const string& workspace, const string_view path) {
        if (path.empty()) {
            return workspace;
        }
        const bool absolute = path[0] == '/' || (path.size() > 1 && path[1] == ':') || path[0] == '\\';
        if (absolute) {
            return string{path};
        }
        string result = workspace;
        if (!result.empty() && result.back() != '/' && result.back() != '\\') {
            result += '/';
        }
        result.append(path);
        return result;
    }

    namespace {
        vector<string> split_path(const string_view text) {
            vector<string> parts;
            size_t start = 0;
            for (size_t i = 0; i <= text.size(); ++i) {
                if (i == text.size() || text[i] == '/' || text[i] == '\\') {
                    if (i > start) {
                        parts.push_back(string{text.substr(start, i - start)});
                    }
                    start = i + 1;
                }
            }
            return parts;
        }

        // Collapse `.` and `..` lexically so prefix checks compare real targets.
        string normalize_path(const string_view input) {
            const bool absolute = !input.empty() && (input[0] == '/' || input[0] == '\\');
            vector<string> parts;
            for (const auto& part: split_path(input)) {
                if (part == ".") {
                    continue;
                }
                if (part == "..") {
                    if (!parts.empty() && parts.back() != "..") {
                        parts.pop_back();
                    } else if (!absolute) {
                        parts.push_back(string{".."});
                    }
                    continue;
                }
                parts.push_back(part);
            }
            string out;
            if (absolute) {
                out += '/';
            }
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) {
                    out += '/';
                }
                out += parts[i];
            }
            return out;
        }

        bool match_segment(const string_view pattern, const string_view text) {
            size_t p = 0;
            size_t t = 0;
            size_t star = string_view::npos;
            size_t mark = 0;
            while (t < text.size()) {
                if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
                    ++p;
                    ++t;
                } else if (p < pattern.size() && pattern[p] == '*') {
                    star = p++;
                    mark = t;
                } else if (star != string_view::npos) {
                    p = star + 1;
                    t = ++mark;
                } else {
                    return false;
                }
            }
            while (p < pattern.size() && pattern[p] == '*') {
                ++p;
            }
            return p == pattern.size();
        }

        bool match_segments(const vector<string>& pattern, const size_t pi, const vector<string>& text,
                            const size_t ti) {
            if (pi == pattern.size()) {
                return ti == text.size();
            }
            if (pattern[pi] == "**") {
                for (size_t skip = ti; skip <= text.size(); ++skip) {
                    if (match_segments(pattern, pi + 1, text, skip)) {
                        return true;
                    }
                }
                return false;
            }
            if (ti >= text.size()) {
                return false;
            }
            return match_segment(pattern[pi].view(), text[ti].view()) && match_segments(pattern, pi + 1, text, ti + 1);
        }
    } // namespace

    bool path_is_within(const string& workspace, const string_view path) {
        const string root = normalize_path(resolve_path(workspace, string_view{}).view());
        const string target = normalize_path(resolve_path(workspace, path).view());
        if (root.empty()) {
            return true;
        }
        if (target == root) {
            return true;
        }
        if (target.size() <= root.size()) {
            return false;
        }
        if (target.view().substr(0, root.size()) != root.view()) {
            return false;
        }
        const char next = target[root.size()];
        return next == '/' || next == '\\';
    }

    bool glob_match(const string_view pattern, const string_view text) {
        const vector<string> pattern_parts = split_path(pattern);
        const vector<string> text_parts = split_path(text);
        if (pattern_parts.empty()) {
            return text_parts.empty();
        }
        return match_segments(pattern_parts, 0, text_parts, 0);
    }

    void tool_registry::add(tool_spec spec, tool_handler handler) {
        specs_.push_back(move(spec));
        handlers_.push_back(move(handler));
    }

    bool tool_registry::has(const string_view name) const {
        for (const auto& spec: specs_) {
            if (spec.name.view() == name) {
                return true;
            }
        }
        return false;
    }

    tool_result tool_registry::invoke(const string_view name, const string_view arguments,
                                      const string& workspace) const {
        for (size_t i = 0; i < specs_.size(); ++i) {
            if (specs_[i].name.view() != name) {
                continue;
            }
            optional<unique_ptr<json_value>> parsed = json_parser{string{arguments}}.try_parse();
            if (!parsed.has_value() || parsed->get() == nullptr) {
                return tool_result{false, "invalid JSON arguments for " + string{name}};
            }
            try {
                return handlers_[i](*parsed->get(), workspace);
            } catch (const exception& e) {
                return tool_result{false, string{"tool failed: "} + e.what()};
            }
        }
        return tool_result{false, "unknown tool: " + string{name}};
    }

    tool_registry tool_registry::make_builtin() {
        tool_registry registry;
        registry.add(tool_spec{"read_file", "Read a UTF-8 text file with line numbers", schema_read}, run_read);
        registry.add(tool_spec{"write_file", "Create or overwrite a text file", schema_write}, run_write);
        registry.add(tool_spec{"edit_file", "Replace an exact substring in a file", schema_edit}, run_edit);
        registry.add(tool_spec{"glob", "List files matching a glob pattern", schema_glob}, run_glob);
        registry.add(tool_spec{"grep", "Search file contents with a regular expression", schema_grep}, run_grep);
        registry.add(tool_spec{"bash", "Run a shell command inside the workspace", schema_bash}, run_bash);
        registry.add(tool_spec{"todo_write", "Replace the session todo list", schema_todo}, run_todo);
        registry.add(tool_spec{"web_fetch", "Fetch a URL and return its body", schema_fetch}, run_fetch);
        return registry;
    }
} // namespace newave::agent
