#!/bin/bash
#===============================================================================
# Podman run commands for all CMA test containers
#===============================================================================

# Centreon versions to test
CENTREON_VERSIONS=("24.10" "25.10")

# RHEL-based containers (AlmaLinux, RHEL, Oracle Linux)
RHEL_CONTAINERS=(
    "cma-almalinux8"
    "cma-almalinux9"
    "cma-rhel8"
    "cma-rhel9"
    "cma-oraclelinux8"
    "cma-oraclelinux9"
)

# Debian-based containers (Debian, Ubuntu)
DEBIAN_CONTAINERS=(
    "cma-debian11"
    "cma-debian12"
    "cma-ubuntu2204"
    "cma-ubuntu2404"
)

# Arrays to track results
SUCCESS_CONTAINERS=()
FAILED_CONTAINERS=()
declare -A FAILURE_REASONS

# Function to run and test a container with a specific version
run_container() {
    local container="$1"
    local version="$2"
    local container_type="$3"
    local container_name="${container}-v${version//./}"  # e.g., cma-almalinux8-v2410

    echo "Starting ${container_name} (Centreon ${version})..."

    # Build podman run command based on container type
    local run_cmd="podman run -d --name ${container_name} --privileged --tmpfs /tmp --tmpfs /run"
    
    if [[ "${container_type}" == "debian" ]]; then
        run_cmd+=" --tmpfs /run/lock --cgroupns=host"
    fi
    
    run_cmd+=" -v /sys/fs/cgroup:/sys/fs/cgroup:ro -v $(pwd):/opt/centreon:z ${container}"

    if eval "${run_cmd}" &>/dev/null; then
        # Wait for systemd to fully initialize
        sleep 2

        # Run installation script with version
        if podman exec "${container_name}" /opt/centreon/install_cma.sh -e "host:4317" -t "token" -v "${version}" -p &>/dev/null; then
            # Check if service is running
            if podman exec "${container_name}" systemctl is-active centagent &>/dev/null; then
                echo "✓ ${container_name}: SUCCESS (service running)"
                SUCCESS_CONTAINERS+=("${container_name}")
                #check if the plugin is installed
                if podman exec "${container_name}" ls /usr/lib/centreon/plugins/centreon_linux_local.pl &>/dev/null; then
                    echo "✓ ${container_name}: FULL SUCCESS (plugin installed)"
                    podman rm -f "${container_name}" &>/dev/null
                else
                    echo "✗ ${container_name}: PARTIAL (plugin not installed)"
                    FAILED_CONTAINERS+=("${container_name}")
                    FAILURE_REASONS["${container_name}"]="Plugin not installed"
                fi                
            else
                echo "✗ ${container_name}: PARTIAL (installed but service not running)"
                FAILED_CONTAINERS+=("${container_name}")
                FAILURE_REASONS["${container_name}"]="Service not running after installation"
            fi
        else
            echo "✗ ${container_name}: FAILED (installation error)"
            FAILED_CONTAINERS+=("${container_name}")
            FAILURE_REASONS["${container_name}"]="Installation script failed"
        fi
    else
        echo "✗ ${container_name}: FAILED (container start error)"
        FAILED_CONTAINERS+=("${container_name}")
        FAILURE_REASONS["${container_name}"]="Container failed to start"
    fi
}

echo "Testing CMA installation for versions: ${CENTREON_VERSIONS[*]}"
echo ""

for version in "${CENTREON_VERSIONS[@]}"; do
    echo "=========================================="
    echo "  Testing Centreon version ${version}"
    echo "=========================================="
    echo ""

    echo "Starting RHEL-based containers..."
    for container in "${RHEL_CONTAINERS[@]}"; do
        run_container "${container}" "${version}" "rhel"
    done

    echo ""
    echo "Starting Debian-based containers..."
    for container in "${DEBIAN_CONTAINERS[@]}"; do
        run_container "${container}" "${version}" "debian"
    done
    echo ""
done

echo ""
echo "=========================================="
echo "           SUMMARY"
echo "=========================================="
echo "Total containers: $((${#SUCCESS_CONTAINERS[@]} + ${#FAILED_CONTAINERS[@]}))"
echo "Successful: ${#SUCCESS_CONTAINERS[@]}"
echo "Failed: ${#FAILED_CONTAINERS[@]}"
echo ""

if [ ${#SUCCESS_CONTAINERS[@]} -gt 0 ]; then
    echo "✓ Successful containers:"
    for container in "${SUCCESS_CONTAINERS[@]}"; do
        echo "  - ${container}"
    done
    echo ""
fi

if [ ${#FAILED_CONTAINERS[@]} -gt 0 ]; then
    echo "✗ Failed containers:"
    for container in "${FAILED_CONTAINERS[@]}"; do
        echo "  - ${container}: ${FAILURE_REASONS[${container}]}"
    done
    echo ""
fi



