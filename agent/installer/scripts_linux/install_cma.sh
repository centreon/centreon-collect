#!/bin/bash
#===============================================================================
# Centreon Monitoring Agent (CMA) Automated Installation Script
#===============================================================================
#
# Description:
#   This script automates Step 3 (Host Preparation) of the Centreon CMA setup.
#   It handles OS detection, repository configuration, agent installation,
#   and configuration file creation.
#
# Supported Operating Systems:
#   - Alma Linux 8 / 9
#   - RHEL 8 / 9
#   - Oracle Linux 8 / 9
#   - Debian 11 / 12
#   - Ubuntu 22.04 / 24.04
#
# Usage:
#   ./install_cma.sh [OPTIONS]
#
# Options:
#   -e, --endpoint       Poller endpoint (IP:PORT or DNS:PORT) [REQUIRED]
#   -t, --token          Authentication token [REQUIRED]
#   -n, --hostname       Host name as defined in Centreon (default: system hostname)
#   -v, --version        Centreon version (24.10 or 25.10, default: 24.10)
#   -c, --encryption     Encryption mode: full, insecure, or no (default: no)
#   -r, --reverse        Enable poller-initiated (reversed) connection mode
#   -a, --ca-cert        Path to CA certificate file (required if encryption=full/insecure, non-reverse)
#   -N, --ca-name        CA common name (optional, used with encryption=full/insecure)
#   -C, --cert           Path to public certificate file (required if encryption=full/insecure, reverse mode)
#   -k, --key            Path to private key file (required if encryption=full/insecure, reverse mode)
#   -f, --fingerprint    Certificate fingerprint for validation
#   -T, --log-type       Log type: file, stdout (default: file)
#   -L, --log-file       Log file path (default: /var/log/centreon-monitoring-agent/centagent.log)
#   -l, --log-level      Log level: off, critical, error, warning, info, debug, trace (default: info)
#   -M, --max-file-size  Maximum log file size in bytes (used with log-type=file)
#   -m, --max-number     Maximum number of log files for rotation (used with log-type=file)
#   -x, --custom-check   Path to custom check configuration file
#   -p, --install-plugins Install Centreon plugins (flag, no value needed)
#   -d, --dry-run        Show what would be done without making changes
#   -h, --help           Display this help message
#
# Examples:
#   # Basic installation (agent connects to poller)
#   ./install_cma.sh -e "192.168.1.100:4317" -t "my-auth-token"
#
#   # Installation with plugins and custom hostname
#   ./install_cma.sh -e "poller.example.com:4317" -t "my-token" -n "web-server-01" -p
#
#   # Installation with full encryption (agent-initiated)
#   ./install_cma.sh -e "192.168.1.100:4317" -t "my-token" -c full -a /path/to/ca.crt
#
#   # Installation with reverse mode and TLS (poller-initiated)
#   ./install_cma.sh -e "0.0.0.0:4317" -t "my-token" -r -c full -C /path/to/cert.crt -k /path/to/key.key
#
#   # Installation with custom logging settings
#   ./install_cma.sh -e "192.168.1.100:4317" -t "my-token" -T file -L /var/log/agent.log -l debug -M 10 -m 5
#
#===============================================================================

set -euo pipefail

#===============================================================================
# CONFIGURATION VARIABLES
#===============================================================================

# Script metadata
readonly SCRIPT_NAME="$(basename "$0")"
readonly SCRIPT_VERSION="1.0.0"

# Default values
DEFAULT_CENTREON_VERSION="25.10"
DEFAULT_ENCRYPTION="no"
DEFAULT_LOG_LEVEL="warning"
DEFAULT_LOG_TYPE="file"
DEFAULT_LOG_FILE="/var/log/centreon-monitoring-agent/centagent.log"
DEFAULT_MAX_FILE_SIZE="10"
DEFAULT_MAX_NUMBER="3"

# Configuration paths
readonly CONFIG_DIR="/etc/centreon-monitoring-agent"
readonly CONFIG_FILE="${CONFIG_DIR}/centagent.json"

# Colors for output
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly BLUE='\033[0;34m'
readonly NC='\033[0m' # No Color

# Global variables (set by command line arguments)
ENDPOINT=""
TOKEN=""
HOSTNAME_CENTREON=""
CENTREON_VERSION="${DEFAULT_CENTREON_VERSION}"
ENCRYPTION="${DEFAULT_ENCRYPTION}"
REVERSE_MODE=false
CA_CERT=""
CA_COMMON_NAME=""
PUBLIC_CERT=""
PRIVATE_KEY=""
FINGERPRINT=""
LOG_TYPE="${DEFAULT_LOG_TYPE}"
LOG_FILE="${DEFAULT_LOG_FILE}"
LOG_LEVEL="${DEFAULT_LOG_LEVEL}"
MAX_FILE_SIZE="${DEFAULT_MAX_FILE_SIZE}"
MAX_NUMBER="${DEFAULT_MAX_NUMBER}"
CUSTOM_CHECK_FILE=""
INSTALL_PLUGINS=false
DRY_RUN=false
OUTPUT_CONFIG=false

# Detected OS information
OS_ID=""
OS_VERSION=""
OS_VERSION_MAJOR=""
PKG_MANAGER=""

#===============================================================================
# UTILITY FUNCTIONS
#===============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

log_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1"
}

die() {
    log_error "$1"
    exit 1
}

show_help() {
    cat << EOF
Centreon Monitoring Agent (CMA) Automated Installation Script v${SCRIPT_VERSION}

USAGE:
    ${SCRIPT_NAME} [OPTIONS]

REQUIRED OPTIONS:
    -e, --endpoint <IP:PORT>      Poller endpoint (e.g., 192.168.1.100:4317)
    -t, --token <TOKEN>           Authentication token from Centreon

OPTIONAL OPTIONS:
    -n, --hostname <NAME>         Host name as defined in Centreon (default: system hostname)
    -v, --version <VERSION>       Centreon version: 24.10 or 25.10 (default: ${DEFAULT_CENTREON_VERSION})
    -c, --encryption <MODE>       Encryption mode: full, insecure, or no (default: ${DEFAULT_ENCRYPTION})
    -r, --reverse                 Enable poller-initiated (reversed) connection mode
    -a, --ca-cert <PATH>          Path to CA certificate file (used with encryption=full/insecure)
    -N, --ca-name <NAME>          CA common name (optional, used with encryption=full/insecure)
    -C, --cert <PATH>             Path to public certificate file (required for TLS in reverse mode)
    -k, --key <PATH>              Path to private key file (required for TLS in reverse mode)
    -f, --fingerprint <STRING>    Certificate fingerprint for validation
    -T, --log-type <TYPE>         Log type: file, stdout (default: ${DEFAULT_LOG_TYPE})
    -L, --log-file <PATH>         Log file path (default: ${DEFAULT_LOG_FILE})
    -l, --log-level <LEVEL>       Log level: off, critical, error, warning, info, debug, trace (default: ${DEFAULT_LOG_LEVEL})
    -M, --max-file-size <BYTES>   Maximum log file size in bytes (used with log-type=file)
    -m, --max-number <NUM>        Maximum number of log files for rotation
    -x, --custom-check <PATH>     Path to custom check configuration file
    -p, --install-plugins         Install Centreon plugins
    -d, --dry-run                 Show what would be done without making changes
    -h, --help                    Display this help message

EXAMPLES:
    # Basic installation
    ${SCRIPT_NAME} -e "192.168.1.100:4317" -t "my-auth-token"

    # Installation with plugins and custom hostname
    ${SCRIPT_NAME} -e "poller.example.com:4317" -t "my-token" -n "web-server-01" -p

    # Installation with full encryption (agent-initiated)
    ${SCRIPT_NAME} -e "192.168.1.100:4317" -t "my-token" -c full -a /path/to/ca.crt

    # Installation with reverse mode and TLS (poller-initiated)
    ${SCRIPT_NAME} -e "0.0.0.0:4317" -t "my-token" -r -c full -C /path/to/cert.crt -k /path/to/key.key

    # Installation with custom logging settings
    ${SCRIPT_NAME} -e "192.168.1.100:4317" -t "my-token" -T file -L /var/log/agent.log -l debug -M 10485760 -m 5

SUPPORTED OPERATING SYSTEMS:
    - Alma Linux 8 / 9
    - RHEL 8 / 9
    - Oracle Linux 8 / 9
    - Debian 11 / 12
    - Ubuntu 22.04 / 24.04

EOF
}

#===============================================================================
# VALIDATION FUNCTIONS
#===============================================================================

validate_root() {
    if [[ $EUID -ne 0 ]]; then
        die "This script must be run as root. Please use sudo or run as root user."
    fi
}

validate_required_params() {
    local errors=0

    # Validate endpoint is provided
    if [[ -z "${ENDPOINT}" ]]; then
        log_error "Missing required parameter: --endpoint"
        errors=$((errors + 1))
    else
        # Validate endpoint format (host:port)
        validate_endpoint_format "${ENDPOINT}" || errors=$((errors + 1))
    fi

    # Validate encryption-related parameters based on mode
    if [[ "${ENCRYPTION}" == "full" || "${ENCRYPTION}" == "insecure" ]]; then
        if [[ "${REVERSE_MODE}" == "true" ]]; then
            # Reverse (poller-initiated) mode requires cert and key
            if [[ -z "${PUBLIC_CERT}" ]]; then
                log_error "Public certificate path (--cert) is required when encryption is '${ENCRYPTION}' in reverse mode"
                errors=$((errors + 1))
            elif [[ ! -f "${PUBLIC_CERT}" ]]; then
                log_error "Public certificate file not found: ${PUBLIC_CERT}"
                errors=$((errors + 1))
            fi

            if [[ -z "${PRIVATE_KEY}" ]]; then
                log_error "Private key path (--key) is required when encryption is '${ENCRYPTION}' in reverse mode"
                errors=$((errors + 1))
            elif [[ ! -f "${PRIVATE_KEY}" ]]; then
                log_error "Private key file not found: ${PRIVATE_KEY}"
                errors=$((errors + 1))
            fi
        else
            # Non-reverse (agent-initiated) mode - CA is optional but validate if provided
            if [[ -n "${CA_CERT}" && ! -f "${CA_CERT}" ]]; then
                log_error "CA certificate file not found: ${CA_CERT}"
                errors=$((errors + 1))
            fi
        fi
    fi

    # Validate log type and related parameters
    if [[ "${LOG_TYPE}" == "file" ]]; then
        if [[ -z "${LOG_FILE}" ]]; then
            log_error "Log file path (--log-file) is required when log-type is 'file'"
            errors=$((errors + 1))
        else
            validate_path_format "${LOG_FILE}" "log file" || errors=$((errors + 1))
        fi
    fi

    # Validate max-file-size is a positive integer if provided
    if [[ -n "${MAX_FILE_SIZE}" ]]; then
        validate_positive_integer "${MAX_FILE_SIZE}" "max-file-size" || errors=$((errors + 1))
    fi

    # Validate max-number is a positive integer if provided
    if [[ -n "${MAX_NUMBER}" ]]; then
        validate_positive_integer "${MAX_NUMBER}" "max-number" || errors=$((errors + 1))
    fi

    # Validate custom check file exists if provided
    if [[ -n "${CUSTOM_CHECK_FILE}" && ! -f "${CUSTOM_CHECK_FILE}" ]]; then
        log_error "Custom check file not found: ${CUSTOM_CHECK_FILE}"
        errors=$((errors + 1))
    fi

    if [[ ${errors} -gt 0 ]]; then
        echo ""
        exit 1
    fi
}

# Validates endpoint format (host:port)
# Returns 0 if valid, 1 if invalid
validate_endpoint_format() {
    local endpoint="$1"
    local host port

    # Check for colon separator
    if [[ "${endpoint}" != *":"* ]]; then
        log_error "Invalid endpoint format: '${endpoint}'. Expected format: 'host:port' (e.g., '192.168.1.100:4317')"
        return 1
    fi

    # Extract host and port
    host="${endpoint%:*}"
    port="${endpoint##*:}"

    # Validate host is not empty
    if [[ -z "${host}" ]]; then
        log_error "Invalid endpoint: missing host in '${endpoint}'"
        return 1
    fi

    # Validate host contains only allowed characters (alphanumeric, dots, hyphens, underscores)
    if [[ ! "${host}" =~ ^[a-zA-Z0-9._-]+$ ]]; then
        log_error "Invalid endpoint: host '${host}' contains invalid characters. Allowed: alphanumeric, '.', '-', '_'"
        return 1
    fi

    # Validate port is not empty
    if [[ -z "${port}" ]]; then
        log_error "Invalid endpoint: missing port in '${endpoint}'"
        return 1
    fi

    # Validate port is numeric
    if [[ ! "${port}" =~ ^[0-9]+$ ]]; then
        log_error "Invalid endpoint: port '${port}' must contain only digits (0-9)"
        return 1
    fi

    # Validate port range (1-65535)
    if [[ "${port}" -lt 1 || "${port}" -gt 65535 ]]; then
        log_error "Invalid endpoint: port '${port}' must be between 1 and 65535"
        return 1
    fi

    return 0
}

# Validates that a path is in proper format (absolute path for Linux)
# Returns 0 if valid, 1 if invalid
validate_path_format() {
    local path="$1"
    local description="${2:-path}"

    # Check if path is absolute (starts with /)
    if [[ ! "${path}" =~ ^/ ]]; then
        log_error "Invalid ${description}: '${path}' must be an absolute path (starting with /)"
        return 1
    fi

    # Check for invalid characters in path
    if [[ "${path}" =~ [[:cntrl:]] ]]; then
        log_error "Invalid ${description}: '${path}' contains invalid control characters"
        return 1
    fi

    return 0
}

# Validates that a value is a positive integer
# Returns 0 if valid, 1 if invalid
validate_positive_integer() {
    local value="$1"
    local param_name="${2:-value}"

    if [[ ! "${value}" =~ ^[0-9]+$ ]]; then
        log_error "Invalid ${param_name}: '${value}' must be a positive integer"
        return 1
    fi

    if [[ "${value}" -eq 0 ]]; then
        log_error "Invalid ${param_name}: '${value}' must be greater than 0"
        return 1
    fi

    return 0
}

validate_log_level() {
    local level="$1"
    case "${level}" in
        critical|error|warning|info|debug|trace)
            return 0
            ;;
        *)
            die "Invalid log level: ${level}. Must be one of: off, critical, error, warning, info, debug, trace"
            ;;
    esac
}

validate_log_type() {
    local log_type="$1"
    case "${log_type}" in
        file|stdout)
            return 0
            ;;
        *)
            die "Invalid log type: ${log_type}. Must be 'file' or 'stdout'"
            ;;
    esac
}

validate_encryption() {
    local mode="$1"
    case "${mode}" in
        full|insecure|no)
            return 0
            ;;
        *)
            die "Invalid encryption mode: ${mode}. Must be 'full', 'insecure', or 'no'"
            ;;
    esac
}

validate_centreon_version() {
    local version="$1"
    case "${version}" in
        24.10|25.10)
            return 0
            ;;
        *)
            die "Invalid Centreon version: ${version}. Must be '24.10' or '25.10'"
            ;;
    esac
}

#===============================================================================
# OS DETECTION FUNCTIONS
#===============================================================================

detect_os() {
    log_info "Detecting operating system..."

    if [[ ! -f /etc/os-release ]]; then
        die "Cannot detect OS: /etc/os-release not found"
    fi

    # Source the os-release file
    # shellcheck source=/dev/null
    . /etc/os-release

    OS_ID="${ID}"
    OS_VERSION="${VERSION_ID}"
    OS_VERSION_MAJOR="${VERSION_ID%%.*}"

    log_info "Detected: ${PRETTY_NAME:-${OS_ID} ${OS_VERSION}}"

    # Validate supported OS
    validate_os_support

    # Determine package manager
    determine_package_manager
}

validate_os_support() {
    local supported=false

    case "${OS_ID}" in
        almalinux|rhel|ol)
            if [[ "${OS_VERSION_MAJOR}" == "8" || "${OS_VERSION_MAJOR}" == "9" ]]; then
                supported=true
            fi
            ;;
        debian)
            if [[ "${OS_VERSION_MAJOR}" == "11" || "${OS_VERSION_MAJOR}" == "12" ]]; then
                supported=true
            fi
            ;;
        ubuntu)
            if [[ "${OS_VERSION}" == "22.04" || "${OS_VERSION}" == "24.04" ]]; then
                supported=true
            fi
            ;;
    esac

    if [[ "${supported}" != "true" ]]; then
        die "Unsupported operating system: ${OS_ID} ${OS_VERSION}
            Supported systems:
            - Alma/RHEL/Oracle 8 or 9
            - Debian 11 or 12
            - Ubuntu 22.04 or 24.04"
    fi

    log_info "Operating system is supported"
}

determine_package_manager() {
    case "${OS_ID}" in
        almalinux|rhel|ol)
            if command -v dnf &> /dev/null; then
                PKG_MANAGER="dnf"
            elif command -v yum &> /dev/null; then
                PKG_MANAGER="yum"
            else
                die "Neither dnf nor yum found on this system"
            fi
            ;;
        debian|ubuntu)
            if command -v apt &> /dev/null; then
                PKG_MANAGER="apt"
            else
                die "apt-get not found on this system"
            fi
            ;;
        *)
            die "Cannot determine package manager for OS: ${OS_ID}"
            ;;
    esac

    log_info "Package manager: ${PKG_MANAGER}"
}

#===============================================================================
# REPOSITORY CONFIGURATION FUNCTIONS
#===============================================================================

# Base URL for all Centreon packages
readonly CENTREON_PACKAGES_BASE_URL="https://packages.centreon.com"
readonly CENTREON_GPG_KEY_URL="https://apt-key.centreon.com"

# Build the repository URL based on OS type and version
# Usage: build_repo_url <os_type> <centreon_version> <el_version|codename>
# os_type: "rpm" for RHEL-based, "apt" for Debian, "ubuntu" for Ubuntu
build_repo_url() {
    local os_type="$1"
    local version="$2"
    local distro_id="$3"

    case "${os_type}" in
        rpm)
            # Format: rpm-standard/<version>/el<X>/centreon-<version>.repo
            echo "${CENTREON_PACKAGES_BASE_URL}/rpm-standard/${version}/el${distro_id}/centreon-${version}.repo"
            ;;
        apt|ubuntu)
            local prefix
            [[ "${os_type}" == "ubuntu" ]] && prefix="ubuntu-standard" || prefix="apt-standard"
            
            # 24.10: <prefix>-24.10-stable <codename> main
            # 25.10: <prefix>/ <codename>-25.10-stable main
            if [[ "${version}" == "24.10" ]]; then
                echo "deb ${CENTREON_PACKAGES_BASE_URL}/${prefix}-${version}-stable ${distro_id} main"
            else
                echo "deb ${CENTREON_PACKAGES_BASE_URL}/${prefix}/ ${distro_id}-${version}-stable main"
            fi
            ;;
    esac
}

# Get the codename for Debian/Ubuntu distributions
get_distro_codename() {
    case "${OS_ID}" in
        debian)
            case "${OS_VERSION_MAJOR}" in
                11) echo "bullseye" ;;
                12) echo "bookworm" ;;
                13) echo "trixie" ;;
            esac
            ;;
        ubuntu)
            case "${OS_VERSION}" in
                22.04) echo "jammy" ;;
                24.04) echo "noble" ;;
            esac
            ;;
    esac
}

configure_rhel_repos() {
    local el_version="${OS_VERSION_MAJOR}"
    local repo_url

    repo_url=$(build_repo_url "rpm" "${CENTREON_VERSION}" "${el_version}")
    
    log_info "Configuring Centreon repositories for EL${el_version}..."

    if [[ "${DRY_RUN}" == "true" ]]; then
        log_info "[DRY-RUN] Would configure Centreon repo: ${repo_url}"
        return 0
    fi

    # Install dnf-plugins-core if not present
    ${PKG_MANAGER} install -y dnf-plugins-core hostname 2>/dev/null || true

    # Add Centreon repository
    ${PKG_MANAGER} config-manager --add-repo "${repo_url}" \
        || die "Failed to add Centreon repository"

    log_info "Centreon repository configured successfully"
}

configure_debian_ubuntu_repos() {
    local codename
    local standard
    local source_repo

    codename=$(get_distro_codename)

    [[ "${OS_ID}" == "ubuntu" ]] && standard="ubuntu" || standard="apt"
    source_repo=$(build_repo_url "${standard}" "${CENTREON_VERSION}" "${codename}")

    log_info "Configuring Centreon repositories for ${OS_ID} ${codename}..."
    log_info "Adding repository: ${source_repo}"
    
    if [[ "${DRY_RUN}" == "true" ]]; then
        log_info "[DRY-RUN] Would configure Centreon repo for ${OS_ID} ${codename}"
        return 0
    fi

    # Install prerequisites
    apt-get update -qq
    apt-get -y -qq install lsb-release gpg wget hostname

    wget -O- "${CENTREON_GPG_KEY_URL}" | gpg --dearmor | tee /etc/apt/trusted.gpg.d/centreon.gpg > /dev/null 2>&1

    echo "${source_repo}" > /etc/apt/sources.list.d/centreon.list

    # Update package lists
    apt-get update -qq

    log_info "Centreon repository configured successfully"
}

configure_plugins_repo_rhel() {
    local el_version="${OS_VERSION_MAJOR}"
    local plugins_base_url="${CENTREON_PACKAGES_BASE_URL}/rpm-plugins/el${el_version}"
    local gpg_key_url="https://yum-gpg.centreon.com/RPM-GPG-KEY-CES"

    log_info "Configuring Centreon plugins repository for EL${el_version}..."

    if [[ "${DRY_RUN}" == "true" ]]; then
        log_info "[DRY-RUN] Would configure Centreon plugins repo for EL${el_version}"
        return 0
    fi

    # Install dnf-plugins-core and EPEL repository
    ${PKG_MANAGER} install -y dnf-plugins-core epel-release 2>/dev/null || true

    # Enable PowerTools/CRB
    if [[ "${el_version}" == "8" ]]; then
        ${PKG_MANAGER} config-manager --set-enabled powertools 2>/dev/null || true
    elif [[ "${el_version}" == "9" ]]; then
        ${PKG_MANAGER} config-manager --set-enabled crb 2>/dev/null || true
    fi

    # Create plugins repository file
    cat > /etc/yum.repos.d/centreon-plugins.repo << EOF
[centreon-plugins-stable]
name=Centreon plugins repository.
baseurl=${plugins_base_url}/stable/\$basearch/
enabled=1
gpgcheck=1
gpgkey=${gpg_key_url}
module_hotfixes=1

[centreon-plugins-stable-noarch]
name=Centreon plugins repository.
baseurl=${plugins_base_url}/stable/noarch/
enabled=1
gpgcheck=1
gpgkey=${gpg_key_url}
module_hotfixes=1

[centreon-plugins-testing]
name=Centreon plugins repository. (UNSUPPORTED)
baseurl=${plugins_base_url}/testing/\$basearch/
enabled=0
gpgcheck=1
gpgkey=${gpg_key_url}
module_hotfixes=1

[centreon-plugins-testing-noarch]
name=Centreon plugins repository. (UNSUPPORTED)
baseurl=${plugins_base_url}/testing/noarch/
enabled=0
gpgcheck=1
gpgkey=${gpg_key_url}
module_hotfixes=1

[centreon-plugins-unstable]
name=Centreon plugins repository. (UNSUPPORTED)
baseurl=${plugins_base_url}/unstable/\$basearch/
enabled=0
gpgcheck=1
gpgkey=${gpg_key_url}
module_hotfixes=1

[centreon-plugins-unstable-noarch]
name=Centreon plugins repository. (UNSUPPORTED)
baseurl=${plugins_base_url}/unstable/noarch/
enabled=0
gpgcheck=1
gpgkey=${gpg_key_url}
module_hotfixes=1
EOF

    log_info "Centreon plugins repository configured successfully"
}

configure_plugins_repo_debian() {
    local codename
    local plugins_prefix
    local plugins_repo

    codename=$(get_distro_codename)
    
    # Debian uses apt-plugins-stable, Ubuntu uses ubuntu-plugins-stable
    [[ "${OS_ID}" == "ubuntu" ]] && plugins_prefix="ubuntu-plugins-stable" || plugins_prefix="apt-plugins-stable"
    plugins_repo="deb ${CENTREON_PACKAGES_BASE_URL}/${plugins_prefix}/ ${codename} main"

    log_info "Configuring Centreon plugins repository for ${OS_ID} ${codename}..."

    if [[ "${DRY_RUN}" == "true" ]]; then
        log_info "[DRY-RUN] Would configure Centreon plugins repo for ${OS_ID} ${codename}"
        return 0
    fi

    # Add plugins repository (GPG key already imported during main repo setup)
    echo "${plugins_repo}" > /etc/apt/sources.list.d/centreon-plugins.list

    apt-get update -qq

    log_info "Centreon plugins repository configured successfully"
}

#===============================================================================
# INSTALLATION FUNCTIONS
#===============================================================================

install_cma_agent() {
    log_info "Installing Centreon Monitoring Agent..."

    if [[ "${DRY_RUN}" == "true" ]]; then
        log_info "[DRY-RUN] Would install centreon-monitoring-agent package"
        return 0
    fi

    case "${PKG_MANAGER}" in
        dnf|yum)
            configure_rhel_repos
            ${PKG_MANAGER} install -y centreon-monitoring-agent \
                || die "Failed to install centreon-monitoring-agent"
            ;;
        apt)
            configure_debian_ubuntu_repos
            apt install -y centreon-monitoring-agent \
                || die "Failed to install centreon-monitoring-agent"
            ;;
    esac

    # Verify installation
    if ! command -v centagent &> /dev/null; then
        die "Installation verification failed: centagent command not found"
    fi

    log_info "Centreon Monitoring Agent installed successfully"
}

install_centreon_plugins() {
    if [[ "${INSTALL_PLUGINS}" != "true" ]]; then
        return 0
    fi

    log_info "Installing Centreon plugins..."

    if [[ "${DRY_RUN}" == "true" ]]; then
        log_info "[DRY-RUN] Would install Centreon plugins"
        return 0
    fi

    case "${PKG_MANAGER}" in
        dnf|yum)
            configure_plugins_repo_rhel
            ${PKG_MANAGER} install -y centreon-plugin-Operatingsystems-Linux-Local.noarch \
                || log_warn "Failed to install Linux local plugin (may already be installed)"
            ;;
        apt)
            configure_plugins_repo_debian
            apt-get install -y centreon-plugin-operatingsystems-linux-local \
                || log_warn "Failed to install Linux local plugin (may already be installed)"
            ;;
    esac

    log_info "Centreon plugins installed successfully"
}

#===============================================================================
# CONFIGURATION FUNCTIONS
#===============================================================================

create_config_file() {
    log_info "Creating configuration file: ${CONFIG_FILE}"

    # Use system hostname if not specified
    local host_name="${HOSTNAME_CENTREON:-$(hostname)}"

    if [[ "${DRY_RUN}" == "true" ]]; then
        log_info "[DRY-RUN] Would create configuration file with:"
        log_info "  - Endpoint: ${ENDPOINT}"
        log_info "  - Hostname: ${host_name}"
        log_info "  - Encryption: ${ENCRYPTION}"
        log_info "  - Reverse Mode: ${REVERSE_MODE}"
        log_info "  - Log Type: ${LOG_TYPE}"
        log_info "  - Log File: ${LOG_FILE}"
        log_info "  - Log Level: ${LOG_LEVEL}"
        [[ -n "${MAX_FILE_SIZE}" ]] && log_info "  - Max File Size: ${MAX_FILE_SIZE}"
        [[ -n "${MAX_NUMBER}" ]] && log_info "  - Max Number: ${MAX_NUMBER}"
        [[ -n "${CUSTOM_CHECK_FILE}" ]] && log_info "  - Custom Check File: ${CUSTOM_CHECK_FILE}"
        return 0
    fi

    # Check if token is missing before generating config
    if [[ -z "${TOKEN}" ]]; then
        log_warn "Authentication token is missing"
        echo -n "Please enter the authentication token: "
        read -r TOKEN
        
        # Validate token after user input
        if [[ -z "${TOKEN}" ]]; then
            die "Installation stopped: Authentication token is required"
        fi
        log_info "Token provided successfully"
    fi

    # Create config directory if it doesn't exist
    mkdir -p "${CONFIG_DIR}"

    # Generate and write configuration JSON
    generate_config_json > "${CONFIG_FILE}"

    # Set proper permissions
    chmod 0640 "${CONFIG_FILE}"
    chown centreon-monitoring-agent:centreon-monitoring-agent "${CONFIG_FILE}" 2>/dev/null || \
        chown root:root "${CONFIG_FILE}"

    log_info "Configuration file created successfully"
}

# Generate configuration JSON string (used for testing and actual config creation)
generate_config_json() {
    local host_name="${HOSTNAME_CENTREON:-$(hostname)}"
    local config_json
    
    config_json='{'
    config_json+=$'\n'"    \"endpoint\": \"${ENDPOINT}\","
    config_json+=$'\n'"    \"host\": \"${host_name}\","
    config_json+=$'\n'"    \"log_type\": \"${LOG_TYPE}\","
    config_json+=$'\n'"    \"log_level\": \"${LOG_LEVEL}\","

    # Add log file settings if log_type is file
    if [[ "${LOG_TYPE}" == "file" ]]; then
        config_json+=$'\n'"    \"log_file\": \"${LOG_FILE}\","
        if [[ -n "${MAX_FILE_SIZE}" ]]; then
            config_json+=$'\n'"    \"log_max_file_size\": ${MAX_FILE_SIZE},"
        fi
        if [[ -n "${MAX_NUMBER}" ]]; then
            config_json+=$'\n'"    \"log_max_files\": ${MAX_NUMBER},"
        fi
    fi

    config_json+=$'\n'"    \"encryption\": \"${ENCRYPTION}\","
    config_json+=$'\n'"    \"reversed_grpc_streaming\": ${REVERSE_MODE},"

    if [[ "${ENCRYPTION}" == "full" || "${ENCRYPTION}" == "insecure" ]]; then
        if [[ "${REVERSE_MODE}" == "true" ]]; then
            if [[ -n "${PUBLIC_CERT}" ]]; then
                config_json+=$'\n'"    \"certificate\": \"${PUBLIC_CERT}\","
            fi
            if [[ -n "${PRIVATE_KEY}" ]]; then
                config_json+=$'\n'"    \"private_key\": \"${PRIVATE_KEY}\","
            fi
        else
            if [[ -n "${CA_CERT}" ]]; then
                config_json+=$'\n'"    \"ca_certificate\": \"${CA_CERT}\","
            fi
        fi
        if [[ -n "${CA_COMMON_NAME}" ]]; then
            config_json+=$'\n'"    \"ca_name\": \"${CA_COMMON_NAME}\","
        fi
    fi

    if [[ -n "${FINGERPRINT}" ]]; then
        config_json+=$'\n'"    \"fingerprint\": \"${FINGERPRINT}\","
    fi

    if [[ -n "${CUSTOM_CHECK_FILE}" ]]; then
        config_json+=$'\n'"    \"check_file\": \"${CUSTOM_CHECK_FILE}\","
    fi

    config_json="${config_json%,}"

    if [[ -n "${TOKEN}" ]]; then
        config_json+=","
        config_json+=$'\n'"    \"token\": \"${TOKEN}\""
    fi

    config_json+=$'\n'"}"
    
    echo "${config_json}"
}

configure_service() {
    log_info "Configuring centagent service..."

    if [[ "${DRY_RUN}" == "true" ]]; then
        log_info "[DRY-RUN] Would enable and start centagent service"
        return 0
    fi

    # Reload systemd
    systemctl daemon-reload

    # Enable service
    systemctl enable centagent

    # Start/restart service
    systemctl restart centagent

    # Check service status
    if systemctl is-active --quiet centagent; then
        log_info "centagent service is running"
    else
        log_warn "centagent service may not be running correctly. Check with: systemctl status centagent"
    fi
}

#===============================================================================
# MAIN SCRIPT
#===============================================================================

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -e|--endpoint)
                ENDPOINT="$2"
                shift 2
                ;;
            -t|--token)
                TOKEN="$2"
                shift 2
                ;;
            -n|--hostname)
                HOSTNAME_CENTREON="$2"
                shift 2
                ;;
            -c|--encryption)
                ENCRYPTION="$2"
                validate_encryption "${ENCRYPTION}"
                shift 2
                ;;
            -r|--reverse)
                REVERSE_MODE=true
                shift
                ;;
            -a|--ca-cert)
                CA_CERT="$2"
                shift 2
                ;;
            -N|--ca-name)
                CA_COMMON_NAME="$2"
                shift 2
                ;;
            -C|--cert)
                PUBLIC_CERT="$2"
                shift 2
                ;;
            -k|--key)
                PRIVATE_KEY="$2"
                shift 2
                ;;
            -f|--fingerprint)
                FINGERPRINT="$2"
                shift 2
                ;;
            -T|--log-type)
                LOG_TYPE="$2"
                validate_log_type "${LOG_TYPE}"
                shift 2
                ;;
            -L|--log-file)
                LOG_FILE="$2"
                shift 2
                ;;
            -l|--log-level)
                LOG_LEVEL="$2"
                validate_log_level "${LOG_LEVEL}"
                shift 2
                ;;
            -M|--max-file-size)
                MAX_FILE_SIZE="$2"
                shift 2
                ;;
            -m|--max-number)
                MAX_NUMBER="$2"
                shift 2
                ;;
            -x|--custom-check)
                CUSTOM_CHECK_FILE="$2"
                shift 2
                ;;
            -v|--version)
                CENTREON_VERSION="$2"
                validate_centreon_version "${CENTREON_VERSION}"
                shift 2
                ;;
            -p|--install-plugins)
                INSTALL_PLUGINS=true
                shift
                ;;
            -d|--dry-run)
                DRY_RUN=true
                shift
                ;;
            --output-config)
                OUTPUT_CONFIG=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                die "Unknown option: $1. Use --help for usage information."
                ;;
        esac
    done
}

print_summary() {
    echo ""
    echo "========================================"
    echo "Installation Summary"
    echo "========================================"
    echo "Endpoint:        ${ENDPOINT}"
    echo "Hostname:        ${HOSTNAME_CENTREON:-$(hostname)}"
    echo "Centreon Ver:    ${CENTREON_VERSION}"
    echo "Encryption:      ${ENCRYPTION}"
    echo "Reverse Mode:    ${REVERSE_MODE}"
    echo "Log Type:        ${LOG_TYPE}"
    echo "Log File:        ${LOG_FILE}"
    echo "Log Level:       ${LOG_LEVEL}"
    [[ -n "${MAX_FILE_SIZE}" ]] && echo "Max File Size:   ${MAX_FILE_SIZE}"
    [[ -n "${MAX_NUMBER}" ]] && echo "Max Log Files:   ${MAX_NUMBER}"
    [[ -n "${CA_CERT}" ]] && echo "CA Certificate:  ${CA_CERT}"
    [[ -n "${CA_COMMON_NAME}" ]] && echo "CA Common Name:  ${CA_COMMON_NAME}"
    [[ -n "${PUBLIC_CERT}" ]] && echo "Public Cert:     ${PUBLIC_CERT}"
    [[ -n "${PRIVATE_KEY}" ]] && echo "Private Key:     ${PRIVATE_KEY}"
    [[ -n "${FINGERPRINT}" ]] && echo "Fingerprint:     ${FINGERPRINT}"
    [[ -n "${CUSTOM_CHECK_FILE}" ]] && echo "Custom Check:    ${CUSTOM_CHECK_FILE}"
    echo "Install Plugins: ${INSTALL_PLUGINS}"
    echo "Config File:     ${CONFIG_FILE}"
    echo "========================================"
    echo ""
}

main() {
    # Parse command line arguments first
    parse_arguments "$@"

    # If --output-config is specified, just output JSON and exit (for testing)
    if [[ "${OUTPUT_CONFIG}" == "true" ]]; then
        # Validate required parameters
        validate_required_params
        # Output JSON config to stdout
        generate_config_json
        exit 0
    fi

    echo ""
    echo "========================================"
    echo "Centreon Monitoring Agent Installer"
    echo "Version: ${SCRIPT_VERSION}"
    echo "========================================"
    echo ""

    # Validate root privileges
    validate_root

    # Validate required parameters
    validate_required_params

    # Print installation summary
    print_summary

    # Detect operating system
    detect_os

    # Install CMA agent
    install_cma_agent

    # Install plugins if requested
    install_centreon_plugins

    # Create configuration file
    create_config_file

    # Configure and start service
    configure_service

    echo ""
    log_info "========================================"
    log_info "Installation completed successfully!"
    log_info "========================================"

}

# Run main function with all arguments
main "$@"
