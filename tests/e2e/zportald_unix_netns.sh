#!/usr/bin/env bash
set -Eeuo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <path-to-zportald>" >&2
    exit 2
fi

if [[ ${EUID} -ne 0 ]]; then
    echo "This test needs root privileges for TUN and network namespaces. Run it with sudo." >&2
    exit 2
fi

ZPORTALD=$1
if [[ ! -x ${ZPORTALD} ]]; then
    echo "zportald is not executable: ${ZPORTALD}" >&2
    exit 2
fi

TEST_ID="zportal-e2e-$$"
NS_SERVER="${TEST_ID}-server"
NS_CLIENT="${TEST_ID}-client"
SOCKET_PATH="/tmp/${TEST_ID}.sock"
SERVER_LOG="/tmp/${TEST_ID}-server.log"
CLIENT_LOG="/tmp/${TEST_ID}-client.log"

SERVER_PID=
CLIENT_PID=

dump_logs() {
    echo "--- server log ---"
    if [[ -f ${SERVER_LOG} ]]; then
        cat "${SERVER_LOG}"
    else
        echo "<missing>"
    fi

    echo "--- client log ---"
    if [[ -f ${CLIENT_LOG} ]]; then
        cat "${CLIENT_LOG}"
    else
        echo "<missing>"
    fi
}

cleanup() {
    set +e

    if [[ -n ${SERVER_PID} ]]; then
        kill "${SERVER_PID}" 2>/dev/null
        wait "${SERVER_PID}" 2>/dev/null
    fi
    if [[ -n ${CLIENT_PID} ]]; then
        kill "${CLIENT_PID}" 2>/dev/null
        wait "${CLIENT_PID}" 2>/dev/null
    fi

    ip netns del "${NS_SERVER}" 2>/dev/null
    ip netns del "${NS_CLIENT}" 2>/dev/null
    rm -f "${SOCKET_PATH}" "${SERVER_LOG}" "${CLIENT_LOG}"
}

on_exit() {
    status=$?
    if [[ ${status} -ne 0 ]]; then
        dump_logs
    fi
    cleanup
    exit "${status}"
}

trap on_exit EXIT

wait_for() {
    local description=$1
    shift

    for _ in $(seq 1 100); do
        if "$@" >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.1
    done

    echo "Timed out waiting for ${description}" >&2
    return 1
}

ip netns add "${NS_SERVER}"
ip netns add "${NS_CLIENT}"

ip netns exec "${NS_SERVER}" "${ZPORTALD}" \
    -n zpte2e0 \
    -m 1400 \
    -a 10.88.0.1/24 \
    -b "unix:${SOCKET_PATH}" \
    >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!

wait_for "server Unix socket" test -S "${SOCKET_PATH}"

ip netns exec "${NS_CLIENT}" "${ZPORTALD}" \
    -n zpte2e1 \
    -m 1400 \
    -a 10.88.0.2/24 \
    -c "unix:${SOCKET_PATH}" \
    >"${CLIENT_LOG}" 2>&1 &
CLIENT_PID=$!

wait_for "server TUN device" ip netns exec "${NS_SERVER}" ip link show zpte2e0
wait_for "client TUN device" ip netns exec "${NS_CLIENT}" ip link show zpte2e1

ip netns exec "${NS_CLIENT}" timeout 10 ping -c 3 -W 2 -I zpte2e1 10.88.0.1

if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo "server exited unexpectedly" >&2
    exit 1
fi
if ! kill -0 "${CLIENT_PID}" 2>/dev/null; then
    echo "client exited unexpectedly" >&2
    exit 1
fi

echo "zportald e2e smoke passed"
