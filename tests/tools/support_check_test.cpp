#include <gtest/gtest.h>

#include <zportal/tools/support_check.hpp>

using namespace zportal;

TEST(SupportCheck, CheckReadMultishot) {
    EXPECT_TRUE(zportal::support_check::read_multishot());
}

TEST(SupportCheck, CheckRecvMultishot) {
    EXPECT_TRUE(zportal::support_check::recv_multishot());
}