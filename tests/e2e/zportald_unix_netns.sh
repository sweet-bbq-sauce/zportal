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
HTTP_LOG="/tmp/${TEST_ID}-http.log"
PAYLOAD_DIR="/tmp/${TEST_ID}-payload"
SOURCE_PAYLOAD="${PAYLOAD_DIR}/payload.bin"
DOWNLOADED_PAYLOAD="${PAYLOAD_DIR}/downloaded.bin"

SERVER_PID=
CLIENT_PID=
HTTP_PID=

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

    echo "--- http log ---"
    if [[ -f ${HTTP_LOG} ]]; then
        cat "${HTTP_LOG}"
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
    if [[ -n ${HTTP_PID} ]]; then
        kill "${HTTP_PID}" 2>/dev/null
        wait "${HTTP_PID}" 2>/dev/null
    fi

    ip netns del "${NS_SERVER}" 2>/dev/null
    ip netns del "${NS_CLIENT}" 2>/dev/null
    rm -rf "${PAYLOAD_DIR}"
    rm -f "${SOCKET_PATH}" "${SERVER_LOG}" "${CLIENT_LOG}" "${HTTP_LOG}"
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

mkdir -p "${PAYLOAD_DIR}"
python3 -c 'from pathlib import Path; Path("'"${SOURCE_PAYLOAD}"'").write_bytes(bytes(range(256)) * 256)'

ip netns exec "${NS_SERVER}" python3 -m http.server \
    --bind 10.88.0.1 \
    --directory "${PAYLOAD_DIR}" \
    18080 \
    >"${HTTP_LOG}" 2>&1 &
HTTP_PID=$!

wait_for "HTTP server through tunnel" ip netns exec "${NS_CLIENT}" python3 -c \
    'import urllib.request; urllib.request.urlopen("http://10.88.0.1:18080/payload.bin", timeout=1).close()'

ip netns exec "${NS_CLIENT}" python3 -c \
    'import urllib.request; urllib.request.urlretrieve("http://10.88.0.1:18080/payload.bin", "'"${DOWNLOADED_PAYLOAD}"'")'

SOURCE_SHA=$(sha256sum "${SOURCE_PAYLOAD}" | awk '{ print $1 }')
DOWNLOADED_SHA=$(sha256sum "${DOWNLOADED_PAYLOAD}" | awk '{ print $1 }')

if [[ ${SOURCE_SHA} != "${DOWNLOADED_SHA}" ]]; then
    echo "HTTP payload checksum mismatch" >&2
    echo "source:     ${SOURCE_SHA}" >&2
    echo "downloaded: ${DOWNLOADED_SHA}" >&2
    exit 1
fi
echo "HTTP payload transfer passed: ${DOWNLOADED_SHA}"

if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo "server exited unexpectedly" >&2
    exit 1
fi
if ! kill -0 "${CLIENT_PID}" 2>/dev/null; then
    echo "client exited unexpectedly" >&2
    exit 1
fi

echo "zportald e2e smoke passed"
