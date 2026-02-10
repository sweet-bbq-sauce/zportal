#pragma once

#include <cstdint>

namespace zportal {

/*

  |                         UserData64 Datagram                        |
  |   1B   |       2B       |                    5B                    |
  | optype |      bid       |                 NOT USED                 |

*/

class Operation {
  public:
    Operation() noexcept = default;
    explicit Operation(std::uint64_t serialized) noexcept;

    enum class Type : std::uint8_t { NONE, RECV, SEND, READ, WRITE };

    Type get_type() const noexcept;
    std::uint16_t get_bid() const noexcept;

    std::uint64_t serialize() const noexcept;

private:
    Type type_{Type::NONE};
    std::uint16_t bid_{};
};

} // namespace zportal

#include <zportal/tunnel/operation.inl>