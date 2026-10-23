#include "policy.hpp"
#include "json_util.hpp"
#include "tools.hpp"
#include <NeForce/core/file/json/json_parser.hpp>
namespace newave::agent {
    using namespace neforce;

    policy::policy(const permission_mode mode, string workspace) :
    mode_(mode),
    workspace_(move(workspace)) {}

    policy::category policy::classify(const string_view tool) noexcept {
        if (tool == "write_file" || tool == "edit_file") {
            return category::write;
        }
        if (tool == "bash") {
            return category::exec;
        }
        return category::read;
    }

    decision policy::check(const string_view tool, const string_view arguments) const {
        const category kind = classify(tool);
        if (mode_ == permission_mode::full) {
            return decision::allow;
        }
        if (mode_ == permission_mode::read_only) {
            return kind == category::read ? decision::allow : decision::deny;
        }
        if (kind == category::read) {
            return decision::allow;
        }
        if (kind == category::exec) {
            return decision::ask;
        }

        optional<unique_ptr<json_value>> parsed = json_parser{string{arguments}}.try_parse();
        if (!parsed.has_value() || parsed->get() == nullptr) {
            return decision::ask;
        }
        const string target = member_string(*parsed->get(), "path");
        if (target.empty()) {
            return decision::ask;
        }
        return path_is_within(workspace_, target.view()) ? decision::allow : decision::ask;
    }
} // namespace newave::agent
