#include "sse.hpp"
namespace newave::agent {
    using namespace neforce;

    void sse_parser::feed(const string_view chunk, const function<void(string_view)>& on_event) {
        buffer_.append(chunk);
        size_t start = 0;
        while (start < buffer_.size()) {
            size_t end = start;
            while (end < buffer_.size() && buffer_[end] != '\n') {
                ++end;
            }
            if (end >= buffer_.size()) {
                break;
            }
            size_t line_end = end;
            if (line_end > start && buffer_[line_end - 1] == '\r') {
                --line_end;
            }
            const string_view line{buffer_.view().substr(start, line_end - start)};
            start = end + 1;

            if (line.empty()) {
                if (!data_.empty()) {
                    on_event(data_.view());
                    data_.clear();
                }
                continue;
            }
            if (line[0] == ':') {
                continue;
            }
            if (line.view(0, 5) == "data:") {
                string_view payload = line.substr(5);
                if (!payload.empty() && payload[0] == ' ') {
                    payload = payload.tail(1);
                }
                if (!data_.empty()) {
                    data_.push_back('\n');
                }
                data_.append(payload);
            }
        }
        if (start > 0) {
            buffer_ = string{buffer_.view().substr(start)};
        }
    }

    void sse_parser::reset() {
        buffer_.clear();
        data_.clear();
    }
} // namespace newave::agent
