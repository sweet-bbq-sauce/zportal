#include <gtest/gtest.h>

#include <zportal/tools/support_check.hpp>

using namespace zportal;

TEST(SupportCheck, CheckReadMultishot) {
    const auto result = support_check::read_multishot();
    if (!result) {
        GTEST_SKIP() << result.error().to_string();
    }

    const auto cached_result = support_check::read_multishot();
    ASSERT_TRUE(cached_result) << cached_result.error().to_string();

    EXPECT_EQ(*cached_result, *result);
}

TEST(SupportCheck, CheckRecvMultishot) {
    const auto result = support_check::recv_multishot();
    if (!result) {
        GTEST_SKIP() << result.error().to_string();
    }

    const auto cached_result = support_check::recv_multishot();
    ASSERT_TRUE(cached_result) << cached_result.error().to_string();

    EXPECT_EQ(*cached_result, *result);
}
