#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT_DIR=$(cd "${SCRIPT_DIR}/../.." && pwd)

# elevate privileges
if [[ $EUID -ne 0 ]]; then
  sudo "$0" "$@"
  exit $?
fi

export DEBIAN_FRONTEND=noninteractive
apt-get --quiet update
apt-get --quiet --assume-yes install \
    cmake \
    curl \
    g++ \
    git \
    libbz2-dev \
    libenet-dev \
    libgl-dev \
    libglew-dev \
    libmpg123-dev \
    libopenal-dev \
    libsdl2-dev \
    libxi-dev \
    libxmu-dev \
    ninja-build \
    nlohmann-json3-dev \
    wget \
    zip
