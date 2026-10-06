# Shared helpers for CaramOS App Center recipes. Sourced, not executed.
#
# Recipes run as root through caramos-app-center-helper with one argument, install or remove.
# Lines "PROGRESS <0-100>" on stdout become the progress bar; every other line goes to the log.

set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
export PATH=/usr/sbin:/usr/bin:/sbin:/bin

APT_GET=(apt-get -o DPkg::Lock::Timeout=600 -o Dpkg::Options::=--force-confdef -o Dpkg::Options::=--force-confold)

WORK_DIR="$(mktemp -d /tmp/caramos-app-center.XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT

progress() {
    echo "PROGRESS $1"
}

# download URL OUTPUT: fetch over HTTPS with retries; reports 0-80 % while downloading.
download() {
    local url="$1" out="$2" total now pid
    total=$(curl -fsIL --retry 3 --connect-timeout 30 "$url" \
        | tr -d '\r' | awk 'tolower($1)=="content-length:" {v=$2} END {print v+0}')
    curl -fL --retry 3 --connect-timeout 30 --silent --show-error -o "$out" "$url" &
    pid=$!
    while kill -0 "$pid" 2>/dev/null; do
        if [ "${total:-0}" -gt 0 ]; then
            now=$(stat -c %s "$out" 2>/dev/null || echo 0)
            progress $(( now * 80 / total ))
        fi
        sleep 1
    done
    wait "$pid"
}

# install_deb FILE: install a downloaded package and its dependencies (80-100 %).
install_deb() {
    progress 82
    "${APT_GET[@]}" install -y "$1"
    progress 100
}

remove_packages() {
    "${APT_GET[@]}" remove -y "$@"
}

require_action() {
    case "${1:-}" in
        install|remove) ;;
        *) echo "usage: $0 install|remove" >&2; exit 2 ;;
    esac
}
