# **ZPortal** - IP tunnel over TCP

[![CI](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/ci.yml/badge.svg)](https://github.com/sweet-bbq-sauce/zportal/actions/workflows/ci.yml)

## Usage

```bash
zportald -n <ifname> -m <MTU> -a <peer address> (-b <bind addr> | -c <connect addr>) [-p <proxy>]
```

- `-n <ifname>` - TUN device name. For example `tun0`, `tun%d`. Required.
- `-m <MTU>` - Device MTU. Range 0-4294967295. Required.
- `-a <inner address>` - Inner IP4 address as cidr. For example `10.0.0.1/24` Required.
- `-b <bind address>` - Server mode. `-c` can not be set.
- `-c <connect address>` - Client mode. `-b` can not be set.
- `-p <proxy>` - Proxy address. Optional.

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
  