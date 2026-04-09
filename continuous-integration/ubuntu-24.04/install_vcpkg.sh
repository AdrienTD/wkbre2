#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT_DIR=$(cd "${SCRIPT_DIR}/../.." && pwd)

mkdir --parents "$REPO_ROOT_DIR/build/3rdParty"
cd "$REPO_ROOT_DIR/build/3rdParty"
git clone https://github.com/microsoft/vcpkg.git
cd -

"$REPO_ROOT_DIR/build/3rdParty/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
