#!/usr/bin/env bash
set -euo pipefail

# Use a job-local source list: keep runner sources intact and retain apt signature checks.
install_ci_dependencies() {
    local suite=$1 arch=$2
    shift 2
    local primary fallback mirror
    case "$arch" in
        amd64) primary=https://archive.ubuntu.com/ubuntu; fallback=https://mirrors.edge.kernel.org/ubuntu ;;
        arm64) primary=https://ports.ubuntu.com/ubuntu-ports; fallback=https://mirrors.mit.edu/ubuntu-ports ;;
        *) echo "Unsupported Ubuntu architecture: $arch" >&2; return 1 ;;
    esac
    local sources
    sources=$(mktemp)
    local -a apt_options=(
        -o "Dir::Etc::sourcelist=$sources" -o Dir::Etc::sourceparts=-
        -o APT::Update::Error-Mode=any -o Acquire::Retries=0
        -o Acquire::http::Timeout=10 -o Acquire::https::Timeout=10
    )
    for mirror in "$primary" "$fallback"; do
        echo "Checking Ubuntu $suite ($arch) at $mirror"
        if ! curl --fail --silent --show-error --location --proto '=https' \
            --connect-timeout 5 --max-time 15 "$mirror/dists/$suite/InRelease" -o /dev/null; then
            echo "Mirror unavailable; trying the next source" >&2
            continue
        fi
        printf 'deb [arch=%s signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] %s %s main universe\n' \
            "$arch" "$mirror" "$suite" > "$sources"
        printf 'deb [arch=%s signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] %s %s-updates main universe\n' \
            "$arch" "$mirror" "$suite" >> "$sources"
        printf 'deb [arch=%s signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] %s %s-security main universe\n' \
            "$arch" "$mirror" "$suite" >> "$sources"
        if timeout 60s sudo apt-get "${apt_options[@]}" update &&
            timeout 180s sudo apt-get "${apt_options[@]}" install -y --no-install-recommends "$@"; then
            rm -f "$sources"
            return 0
        fi
        echo "Dependency installation failed at $mirror; trying the next source" >&2
    done
    rm -f "$sources"
    echo "Both Ubuntu sources failed; stopping before compilation" >&2
    return 1
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    # shellcheck source=/dev/null
    . /etc/os-release
    install_ci_dependencies "$VERSION_CODENAME" "$(dpkg --print-architecture)" "$@"
fi
