#pragma once

#include <cstdint>

namespace zportal {

/*

  |                         UserData64 Datagram                        |
  |                                 8B                                 |
  |   1B   |                            7B                             |
  | optype |                         NOT USED                          |

*/

enum class OperationType : std::uint8_t { NONE, RECV, SEND, READ, WRITE, SIGNAL, TIMEOUT };

class Operation {
  public:
    Operation() noexcept = default;
    explicit Operation(std::uint64_t serialized) noexcept;

    OperationType get_type() const noexcept;
    void set_type(OperationType type) noexcept;

    void parse(std::uint64_t serialized) noexcept;
    std::uint64_t serialize() const noexcept;

  private:
    OperationType type_{OperationType::NONE};
};

} // namespace zportal

#include <zportal/session/operation.inl>