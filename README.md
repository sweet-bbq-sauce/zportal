# **ZPortal** - IP tunnel over TCP

[![CI](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/ci.yml/badge.svg)](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/ci.yml)
[![Release](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/release.yml/badge.svg)](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/release.yml)

Educational project exploring Linux TUN interfaces and io_uring-based
transport of IP packets over a TCP stream.

Not a VPN. No encryption. No authentication.
Intended for local labs and learning purposes only.

## Usage

```bash
zportald -n <ifname> -m <MTU> -a <peer address> (-b <bind addr> | -c <connect addr>) [-p <proxy>]-r <seconds> -e <count> -VM
```

- `-n <ifname>` - TUN device name. For example `tun0`, `tun%d`.
- `-m <MTU>` - Device MTU. Range 68-65535.
- `-a <inner address>` - Inner IP4 cidr. For example `10.0.0.1/24`.
- `-b <bind address>` - Server mode.
- `-c <connect address>` - Client mode.
- `-r <seconds>` - Reconnect duration. Has no effect with `-b`.
- `-e <count>` - Error threshold. Has no effect with `-b`.
- `-p <proxy>` - Proxy address. Has no effect with `-b`.
- `-V` - Verbose mode.
- `-M` - Monitor mode.

## Building and installation

### Dependecies

- `liburing-dev`
  
### Build

- `sudo apt update`
- `sudo apt install -y cmake ninja-build g++ liburing-dev iproute2`
- `mkdir build && cd build`
- `cmake .. -G Ninja`
- `ninja`
- `ctest` (optionally)
  