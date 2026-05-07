# ZPortal

[![CI](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/ci.yml/badge.svg)](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/ci.yml)
[![E2E](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/e2e.yml/badge.svg)](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/e2e.yml)
[![Release](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/release.yml/badge.svg)](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/release.yml)

ZPortal is a Linux-only C++23 network tunneling project. It creates a TUN
interface, reads raw IP packets from it, frames them, and transports them over a
TCP stream. On the other side it validates and reconstructs frames, then writes
the packets back into a TUN interface.

The project is intentionally close to Linux system APIs: `/dev/net/tun`,
netlink route configuration, sockets, Unix sockets, SOCKS5 CONNECT, `io_uring`,
provided buffers, multishot operations, RAII file descriptor ownership, and
explicit error propagation with `std::expected`.

It is not a production VPN. There is no encryption, authentication, routing
daemon, privilege separation, or stable public protocol guarantee. The goal is
to demonstrate low-level Linux/C++ work in a real, runnable network program.

## What This Shows

- C++23 code using move-only resource owners, `std::expected`, spans, variants,
  source locations, and RAII around file descriptors and sockets.
- Linux networking: TUN devices, IPv4/IPv6 CIDR handling, netlink interface
  configuration, TCP sockets, Unix domain sockets, and SOCKS5 proxy chaining.
- `io_uring` event-driven I/O with CQE `user_data`, provided buffer rings,
  runtime support checks, and fallback paths for kernel/liburing differences.
- A small binary framing protocol with fixed big-endian headers and CRC32C
  payload validation.
- CI that builds with GCC 14, runs clang-format, clang-tidy analyzer checks,
  unit tests, ASan/UBSan builds, and an end-to-end tunnel smoke test in Linux
  network namespaces.

## Architecture

```text
  +----------+       TCP stream       +----------+
  | zportald | <--------------------> | zportald |
  +----------+                        +----------+
       ^                                    ^
       | raw IP packets                     | raw IP packets
       v                                    v
  +---------+                         +---------+
  | TUN dev |                         | TUN dev |
  +---------+                         +---------+
```

Or with SOCKS5 proxy:
```text
  +----------+    TCP   +---------+       +---------+    TCP   +----------+
  | zportald | <------> | proxy 1 | <---> | proxy n | <------> | zportald |
  +----------+          +---------+       +---------+          +----------+
       ^                                                             ^
       | raw IP packets                                              | raw IP packets
       v                                                             v
  +---------+                                                   +---------+
  | TUN dev |                                                   | TUN dev |
  +---------+                                                   +---------+
```

The daemon has two symmetric data paths:

- `Transmitter`: reads packets from TUN, creates a frame header, computes
  CRC32C, and sends header + payload through the connected stream socket.
- `Receiver`: receives stream bytes, parses frame headers and payloads across
  arbitrary TCP chunk boundaries, validates magic/size/CRC, and writes complete
  packets to TUN with `writev`.

The session loop is completion-driven. `io_uring` completions are tagged with a
small operation enum in CQE `user_data`:

```text
READ  - TUN packet was read and can be queued for socket send
SEND  - socket send completed and the next frame may advance
RECV  - socket bytes were received and can be parsed
WRITE - TUN write completed and receive buffers can be released
TIMEOUT - monitor tick for interface statistics
```

## Design Choices

ZPortal uses TCP deliberately. The project is not trying to beat WireGuard or a
UDP tunnel on throughput; it is exploring what happens when raw IP packets are
carried through infrastructure that already understands stream connections.
That makes the tunnel easy to run over direct TCP, Unix sockets for local tests,
SOCKS5 chains, and potentially restrictive networks where UDP is unavailable or
harder to pass through.

The trade-off is also intentional: tunneling IP over TCP can suffer from
head-of-line blocking and "TCP-over-TCP" behavior when the tunneled traffic is
itself TCP. For this project that cost is acceptable because the interesting
part is transport compatibility, framing, buffer ownership, and backpressure in
a real Linux networking program.

`io_uring` is used because the daemon has multiple independent file descriptors
that should progress from one completion loop: TUN reads/writes, socket
receives/sends, and monitor timeouts. It keeps the core loop completion-driven,
lets CQE `user_data` identify operation types without extra threads, and allows
provided buffer rings/multishot operations where the running kernel and
`liburing` support them. The code also has runtime and compile-time fallbacks,
which keeps the project useful across different Linux versions.

## Wire Format

Each packet is sent as one frame:

```text
0                   4                   8                  12                  16
+-------------------+-------------------+-------------------+-------------------+
| magic 0x5A505254  | flags             | payload size      | CRC32C payload    |
+-------------------+-------------------+-------------------+-------------------+
| payload bytes, length = payload size                                          |
+-------------------------------------------------------------------------------+
```

Header fields are encoded as big-endian `uint32_t` values. `flags` is reserved
for future use. The receiver rejects invalid magic values, zero-sized payloads,
payloads larger than the TUN MTU, and CRC mismatches.

## Backpressure Model

ZPortal currently uses a simple bounded-buffer model around `io_uring` provided
buffers:

- TUN reads and socket receives use buffer groups sized at session creation.
- When the kernel reports `ENOBUFS`, the relevant side enters a cooldown state.
- Reads/receives are armed again once enough queued buffers have been returned.
- Socket sends are serialized: only one `SEND` operation is in flight at a time,
  and partial sends are continued from the saved frame state.

This keeps buffer ownership explicit and easy to reason about, but it is still a
prototype-level policy. There is no configurable packet drop strategy or
advanced fairness between flows.

## Usage

```bash
zportald -n <ifname> -m <mtu> -a <inner-cidr> (-b <bind-address> | -c <connect-address>) [-p <proxy>]...
```

Options:

- `-n <ifname>`: TUN interface name, for example `zpt0` or `tun%d`.
- `-m <mtu>`: TUN MTU. Accepted range is `68..65535`.
- `-a <inner-cidr>`: address assigned to the TUN interface, for example
  `10.10.0.1/24` or `fd00::1/64`.
- `-b <bind-address>`: server mode. Listen, accept one peer, then run the
  tunnel.
- `-c <connect-address>`: client mode. Connect to a peer, optionally through
  SOCKS5 proxies.
- `-p <proxy>`: SOCKS5 proxy hop. Can be repeated to build a proxy chain.
- `-h`: print help.
- `-v`: print version.

Exactly one of `-b` or `-c` must be set.

## Supported Transports

Direct TCP:

```bash
sudo zportald -n zpt0 -m 1400 -a 10.10.0.1/24 -b 0.0.0.0:7000
sudo zportald -n zpt1 -m 1400 -a 10.10.0.2/24 -c 203.0.113.10:7000
```

Unix domain socket, useful for local tests:

```bash
sudo zportald -n zpt0 -m 1400 -a 10.10.0.1/24 -b unix:/tmp/zportal.sock
sudo zportald -n zpt1 -m 1400 -a 10.10.0.2/24 -c unix:/tmp/zportal.sock
```

SOCKS5 chain:

```bash
sudo zportald -n zpt1 -m 1400 -a 10.10.0.2/24 \
  -c 203.0.113.10:7000 \
  -p proxy1.example:1080 \
  -p proxy2.example:1080
```

Address formats accepted by `-b`, `-c`, and `-p`:

```text
192.168.1.10:7000
[2001:db8::1]:7000
example.com:7000
unix:/tmp/zportal.sock
unixa:zportal
```

## Build

Requirements:

- Linux
- CMake 3.23+
- C++23 compiler, tested in CI with GCC 14
- `liburing-dev`
- root privileges or `CAP_NET_ADMIN` for actually creating/configuring TUN
  interfaces

On Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ liburing-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The daemon binary is produced at:

```bash
build/app/zportald
```

Build options:

```bash
cmake -S . -B build -DBUILD_TESTS=OFF
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DWITH_ASAN_UBSAN=ON
cmake -S . -B build -DINSTALL_LIBZPORTAL=ON
```

## Tests

Unit tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

End-to-end tunnel smoke test:

```bash
sudo apt-get install -y iproute2 iputils-ping python3

cmake -S . -B build-e2e -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=OFF -DWITH_ASAN_UBSAN=ON
cmake --build build-e2e -j
sudo tests/e2e/zportald_unix_netns.sh build-e2e/app/zportald
```

The e2e test creates two Linux network namespaces, starts one server and one
client daemon connected through a Unix socket, verifies ICMP over the tunnel,
serves a payload over HTTP through the tunnel, downloads it from the other
namespace, and compares SHA-256 checksums.

## CI And Release

GitHub Actions currently run:

- formatting check with `clang-format-19`
- `clang-tidy-19` analyzer checks as errors
- Release and ASan/UBSan builds
- GoogleTest unit tests
- privileged e2e tunnel smoke test in network namespaces
- tag-based packaging with CPack into `.tar.gz` and `.deb` artifacts

## Project Status

Implemented:

- TUN creation and configuration through ioctl + netlink
- IPv4/IPv6 CIDR parsing and network membership helpers
- TCP and Unix domain socket transports
- SOCKS5 CONNECT proxy chaining
- `io_uring` abstraction and provided buffer groups
- frame encoding/decoding with CRC32C validation
- interface traffic monitor
- unit tests for core utility components
- network namespace e2e test for real packet flow

Known next steps:

- Split stream/frame parsing out of `Receiver` into a separately testable module.
- Add fuzz tests for address parsing and frame parsing.
- Expand unit coverage for address/CIDR parsing, SOCKS5 handshakes, frame
  headers, and error paths.
- Add an explicit queue/drop policy for sustained congestion.
- Decide whether the protocol should stay TCP-oriented or grow a UDP mode.
- Add security features only if the project moves beyond lab/learning use.

## License

MIT. See [LICENSE](LICENSE).
