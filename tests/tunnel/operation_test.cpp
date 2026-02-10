#include <cstdint>

#include <gtest/gtest.h>

#include <zportal/tunnel/operation.hpp>

using namespace zportal;

TEST(Operation, DecodeFields) {
    const std::uint64_t serialized = 0x0100330000003301ULL;

    Operation operation(serialized);
    EXPECT_EQ(operation.get_type(), Operation::Type::RECV);
    EXPECT_EQ(operation.get_bid(), 0x33);
}

TEST(Operation, SerializeKeepsOnlyOperationBytes) {
    const std::uint64_t serialized = 0x0100330000003301ULL;
    Operation operation(serialized);

    EXPECT_EQ(operation.serialize(), 0x0000000000003301ULL);
}

TEST(Operation, DecodeIgnoresUpperFiveBytes) {
    const std::uint64_t base = 0x0000000000003301ULL;
    const std::uint64_t noisy = 0xA5B6C7D8E9003301ULL;

    Operation from_base(base);
    Operation from_noisy(noisy);

    EXPECT_EQ(from_noisy.get_type(), from_base.get_type());
    EXPECT_EQ(from_noisy.get_bid(), from_base.get_bid());
    EXPECT_EQ(from_noisy.serialize(), from_base.serialize());
}
