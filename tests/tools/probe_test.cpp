#include <ios>
#include <iostream>

#include <gtest/gtest.h>

#include <zportal/tools/probe.hpp>

using namespace zportal;

TEST(Probe, ProbeReadMultishot) {
    bool result;
    EXPECT_NO_THROW(result = zportal::support_check::read_multishot());
    std::cout << std::boolalpha << "Is read multishot supported on this host? " << result << std::endl;
}

TEST(Probe, ProbeRecvMultishot) {
    bool result;
    EXPECT_NO_THROW(result = zportal::support_check::recv_multishot());
    std::cout << std::boolalpha << "Is recv multishot supported on this host? " << result << std::endl;
}