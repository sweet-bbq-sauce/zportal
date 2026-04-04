#pragma once

#include <cstdint>

#include <zportal/iouring/cqe.hpp>

namespace zportal {

/*

  |                         UserData64 Datagram                        |
  |                                 8B                                 |
  |   1B   |                            7B                             |
  | optype |                         NOT USED                          |

*/

class Operation {
  public:
    Operation() noexcept = default;
    explicit Operation(std::uint64_t serialized) noexcept;
    explicit Operation(const Cqe& cqe) noexcept;

    enum class Type : std::uint8_t { NONE, RECV, SEND, READ, WRITE, SIGNAL };

    Type get_type() const noexcept;
    void set_type(Type type) noexcept;

    void parse(std::uint64_t serialized) noexcept;
    std::uint64_t serialize() const noexcept;

  private:
    Type type_{Type::NONE};
};

} // namespace zportal

#include <zportal/session/operation.inl>