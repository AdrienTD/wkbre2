#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT_DIR=$(cd "${SCRIPT_DIR}/../.." && pwd)

"$REPO_ROOT_DIR/continuous-integration/ubuntu-24.04/install_dependencies.sh"
"$REPO_ROOT_DIR/continuous-integration/ubuntu-24.04/install_uv.sh"
"$REPO_ROOT_DIR/continuous-integration/ubuntu-24.04/install_vcpkg.sh"
