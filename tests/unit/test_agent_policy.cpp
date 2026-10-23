#include "agent/policy.hpp"
#include <gtest/gtest.h>
using namespace newave::agent;

TEST(PolicyTest, ReadOnlyAllowsReadsAndDeniesWrites) {
    const policy guard{permission_mode::read_only, "/work"};
    EXPECT_EQ(guard.check("read_file", "{}"), decision::allow);
    EXPECT_EQ(guard.check("grep", "{}"), decision::allow);
    EXPECT_EQ(guard.check("write_file", R"({"path":"a"})"), decision::deny);
    EXPECT_EQ(guard.check("bash", R"({"command":"ls"})"), decision::deny);
}

TEST(PolicyTest, WorkspaceWriteAsksOutside) {
    const policy guard{permission_mode::workspace_write, "/work"};
    EXPECT_EQ(guard.check("write_file", R"({"path":"a/b.txt"})"), decision::allow);
    EXPECT_EQ(guard.check("edit_file", R"({"path":"../work/x"})"), decision::allow);
    EXPECT_EQ(guard.check("write_file", R"({"path":"../../etc/x"})"), decision::ask);
    EXPECT_EQ(guard.check("bash", R"({"command":"ls"})"), decision::ask);
}

TEST(PolicyTest, FullAllowsEverything) {
    const policy guard{permission_mode::full, "/work"};
    EXPECT_EQ(guard.check("bash", "{}"), decision::allow);
    EXPECT_EQ(guard.check("write_file", R"({"path":"/etc/x"})"), decision::allow);
}

TEST(PolicyTest, ClassifiesTools) {
    EXPECT_EQ(policy::classify("read_file"), policy::category::read);
    EXPECT_EQ(policy::classify("glob"), policy::category::read);
    EXPECT_EQ(policy::classify("write_file"), policy::category::write);
    EXPECT_EQ(policy::classify("bash"), policy::category::exec);
}
