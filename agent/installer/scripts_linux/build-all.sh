#!/bin/bash
#===============================================================================
# Build all Podman containers for Centreon Monitoring Agent testing
#===============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if podman is installed
if ! command -v podman &> /dev/null; then
    log_error "Podman is not installed. Please install podman first."
    exit 1
fi

# Define containers to build (image_name:containerfile)
CONTAINERS=(
    "cma-almalinux8:Containerfile.almalinux8"
    "cma-almalinux9:Containerfile.almalinux9"
    "cma-rhel8:Containerfile.rhel8"
    "cma-rhel9:Containerfile.rhel9"
    "cma-oraclelinux8:Containerfile.oraclelinux8"
    "cma-oraclelinux9:Containerfile.oraclelinux9"
    "cma-debian11:Containerfile.debian11"
    "cma-debian12:Containerfile.debian12"
    "cma-ubuntu2204:Containerfile.ubuntu2204"
    "cma-ubuntu2404:Containerfile.ubuntu2404"
)

# Parse arguments
BUILD_SPECIFIC=""
if [[ $# -gt 0 ]]; then
    BUILD_SPECIFIC="$1"
fi

# Build containers
build_count=0
fail_count=0

for entry in "${CONTAINERS[@]}"; do
    image_name="${entry%%:*}"
    containerfile="${entry##*:}"
    
    # Skip if building specific container and this isn't it
    if [[ -n "${BUILD_SPECIFIC}" && "${image_name}" != *"${BUILD_SPECIFIC}"* ]]; then
        continue
    fi
    
    log_info "Building ${image_name} from ${containerfile}..."
    
    if podman build -t "${image_name}" -f "${containerfile}" .; then
        log_info "Successfully built ${image_name}"
        build_count=$((build_count + 1))
    else
        log_error "Failed to build ${image_name}"
        fail_count=$((fail_count + 1))
    fi
done


echo ""
log_info "Build complete: ${build_count} succeeded, ${fail_count} failed"

if [[ ${fail_count} -gt 0 ]]; then
    exit 1
fi
