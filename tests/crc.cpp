#include <string>

#include <gtest/gtest.h>

#include <zportal/crc.hpp>

using namespace zportal;

TEST(CRC, CRC32C) {
    const std::string data = "zportal";
    EXPECT_EQ(crc32c({reinterpret_cast<const std::byte*>(data.data()), data.size()}), 0xa9d08df5u);
}