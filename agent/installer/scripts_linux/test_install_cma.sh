#!/bin/bash
#===============================================================================
# Test Script for install_cma.sh
#===============================================================================
#
# Description:
#   This script tests all the argument parsing and validation logic
#   in install_cma.sh without actually installing anything.
#
# Usage:
#   ./test_install_cma.sh <path_to_install_cma.sh>
#
# Example:
#   ./test_install_cma.sh ./install_cma.sh
#   ./test_install_cma.sh /path/to/install_cma.sh
#
#===============================================================================

set -u

# Colors for output
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly BLUE='\033[0;34m'
readonly CYAN='\033[0;36m'
readonly NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Script under test (passed as first argument)
SCRIPT_UNDER_TEST="${1:-}"

# Temp directory for test files
TEST_TEMP_DIR=""

#===============================================================================
# TEST UTILITIES
#===============================================================================

setup() {
    # Create temp directory for test files
    TEST_TEMP_DIR=$(mktemp -d)
    
    # Create dummy certificate files for testing
    echo "dummy cert content" > "${TEST_TEMP_DIR}/ca.crt"
    echo "dummy cert content" > "${TEST_TEMP_DIR}/public.crt"
    echo "dummy key content" > "${TEST_TEMP_DIR}/private.key"
    echo "dummy check config" > "${TEST_TEMP_DIR}/custom_check.json"
    
    echo -e "${CYAN}=== Test Environment Setup ===${NC}"
    echo "Temp directory: ${TEST_TEMP_DIR}"
    echo ""
}

cleanup() {
    if [[ -n "${TEST_TEMP_DIR}" && -d "${TEST_TEMP_DIR}" ]]; then
        rm -rf "${TEST_TEMP_DIR}"
    fi
}

# Run a test case
# Args: test_name, expected_result (0=pass, 1=fail), script_args...
run_test() {
    local test_name="$1"
    local expected_exit="$2"
    shift 2
    local args=("$@")
    
    TESTS_RUN=$((TESTS_RUN + 1))
    
    echo -e "${BLUE}[TEST ${TESTS_RUN}]${NC} ${test_name}"
    
    # Run the script and capture output
    local output
    local actual_exit
    if [[ ${#args[@]} -eq 0 ]]; then
        output=$(bash "${SCRIPT_UNDER_TEST}" 2>&1)
        actual_exit=$?
    else
        output=$(bash "${SCRIPT_UNDER_TEST}" "${args[@]}" 2>&1)
        actual_exit=$?
    fi
    
    # For dry-run tests, check if it would pass validation
    # The script exits with 0 for --help, and various codes for errors
    
    if [[ ${expected_exit} -eq 0 && ${actual_exit} -eq 0 ]]; then
        echo -e "  ${GREEN}✓ PASSED${NC} (exit code: ${actual_exit})"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    elif [[ ${expected_exit} -ne 0 && ${actual_exit} -ne 0 ]]; then
        echo -e "  ${GREEN}✓ PASSED${NC} (expected failure, exit code: ${actual_exit})"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${RED}✗ FAILED${NC} (expected exit: ${expected_exit}, actual: ${actual_exit})"
        echo -e "  Output: ${output}" | head -5
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Run a test and check for specific error message
run_test_with_message() {
    local test_name="$1"
    local expected_exit="$2"
    local expected_message="$3"
    shift 3
    local args=("$@")
    
    TESTS_RUN=$((TESTS_RUN + 1))
    
    echo -e "${BLUE}[TEST ${TESTS_RUN}]${NC} ${test_name}"
    
    local output
    local actual_exit

    if [[ ${#args[@]} -eq 0 ]]; then
        output=$(bash "${SCRIPT_UNDER_TEST}" 2>&1)
        actual_exit=$?
    else
        output=$(bash "${SCRIPT_UNDER_TEST}" "${args[@]}" 2>&1)
        actual_exit=$?
    fi
    
    local exit_match=false
    local message_match=false
    
    if [[ ${expected_exit} -eq 0 && ${actual_exit} -eq 0 ]] || \
       [[ ${expected_exit} -ne 0 && ${actual_exit} -ne 0 ]]; then
        exit_match=true
    fi
    
    if [[ "${output}" == *"${expected_message}"* ]]; then
        message_match=true
    fi
    
    if [[ "${exit_match}" == "true" && "${message_match}" == "true" ]]; then
        echo -e "  ${GREEN}✓ PASSED${NC} (exit: ${actual_exit}, message found)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    elif [[ "${exit_match}" == "true" && "${message_match}" == "false" ]]; then
        echo -e "  ${YELLOW}⚠ PARTIAL${NC} (exit code correct, but message '${expected_message}' not found)"
        echo -e "  Output: ${output}" | head -3
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    else
        echo -e "  ${RED}✗ FAILED${NC} (expected exit: ${expected_exit}, actual: ${actual_exit})"
        echo -e "  Expected message: ${expected_message}"
        echo -e "  Output: ${output}" | head -5
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

print_section() {
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

print_summary() {
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}TEST SUMMARY${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    echo -e "Total tests run: ${TESTS_RUN}"
    echo -e "${GREEN}Tests passed: ${TESTS_PASSED}${NC}"
    echo -e "${RED}Tests failed: ${TESTS_FAILED}${NC}"
    echo ""
    
    if [[ ${TESTS_FAILED} -eq 0 ]]; then
        echo -e "${GREEN}╔═══════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║       ALL TESTS PASSED! 🎉            ║${NC}"
        echo -e "${GREEN}╚═══════════════════════════════════════╝${NC}"
        return 0
    else
        echo -e "${RED}╔═══════════════════════════════════════╗${NC}"
        echo -e "${RED}║       SOME TESTS FAILED! ❌           ║${NC}"
        echo -e "${RED}╚═══════════════════════════════════════╝${NC}"
        return 1
    fi
}

#===============================================================================
# TEST CASES
#===============================================================================

test_help_and_basic() {
    print_section "1. HELP AND BASIC ARGUMENT TESTS"
    
    # Test help flag
    run_test "Help flag (-h)" 0 -h
    run_test "Help flag (--help)" 0 --help
}

test_required_params() {
    print_section "2. REQUIRED PARAMETERS TESTS"
    
    # Missing required parameters
    run_test_with_message "Missing all required params" 1 "Missing required parameter" 
    run_test_with_message "Missing endpoint" 1 "Missing required parameter: --endpoint" -t "mytoken"
    run_test_with_message "Missing token" 1 "Missing required parameter: --token" -e "localhost:4317"
    
    # Valid minimal configuration
    run_test "Valid minimal config (endpoint + token)" 0 \
        -e "192.168.1.100:4317" -t "my-auth-token" -d
}

test_endpoint_validation() {
    print_section "3. ENDPOINT VALIDATION TESTS"
    
    # Valid endpoints
    run_test "Valid endpoint: IP:port" 0 \
        -e "192.168.1.100:4317" -t "token" -d
    run_test "Valid endpoint: hostname:port" 0 \
        -e "poller.example.com:4317" -t "token" -d
    run_test "Valid endpoint: localhost:port" 0 \
        -e "localhost:8080" -t "token" -d
    run_test "Valid endpoint: with underscore" 0 \
        -e "my_host.example.com:4317" -t "token" -d
    run_test "Valid endpoint: with hyphen" 0 \
        -e "my-host.example.com:4317" -t "token" -d
    run_test "Valid endpoint: port 1 (min)" 0 \
        -e "localhost:1" -t "token" -d
    run_test "Valid endpoint: port 65535 (max)" 0 \
        -e "localhost:65535" -t "token" -d
    
    # Invalid endpoints
    run_test_with_message "Invalid endpoint: no port" 1 "Invalid endpoint" \
        -e "192.168.1.100" -t "token" -d
    run_test_with_message "Invalid endpoint: empty port" 1 "Invalid endpoint" \
        -e "192.168.1.100:" -t "token" -d
    run_test_with_message "Invalid endpoint: empty host" 1 "Invalid endpoint" \
        -e ":4317" -t "token" -d
    run_test_with_message "Invalid endpoint: non-numeric port" 1 "Invalid endpoint" \
        -e "localhost:abc" -t "token" -d
    run_test_with_message "Invalid endpoint: port with letters" 1 "Invalid endpoint" \
        -e "localhost:431a" -t "token" -d
    run_test_with_message "Invalid endpoint: port 0" 1 "Invalid endpoint" \
        -e "localhost:0" -t "token" -d
    run_test_with_message "Invalid endpoint: port > 65535" 1 "Invalid endpoint" \
        -e "localhost:65536" -t "token" -d
    run_test_with_message "Invalid endpoint: port 99999" 1 "Invalid endpoint" \
        -e "localhost:99999" -t "token" -d
    run_test_with_message "Invalid endpoint: special chars in host" 1 "Invalid endpoint" \
        -e "host@example.com:4317" -t "token" -d
    run_test_with_message "Invalid endpoint: space in host" 1 "Invalid endpoint" \
        -e "my host:4317" -t "token" -d
}

test_encryption_modes() {
    print_section "4. ENCRYPTION MODE TESTS"
    
    # Valid encryption modes
    run_test "Encryption: no" 0 \
        -e "localhost:4317" -t "token" -c no -d
    run_test "Encryption: insecure" 0 \
        -e "localhost:4317" -t "token" -c insecure -d
    run_test "Encryption: full" 0 \
        -e "localhost:4317" -t "token" -c full -d
    
    # Invalid encryption modes
    run_test_with_message "Invalid encryption: none" 1 "Invalid encryption mode" \
        -e "localhost:4317" -t "token" -c none -d
    run_test_with_message "Invalid encryption: tls" 1 "Invalid encryption mode" \
        -e "localhost:4317" -t "token" -c tls -d
    run_test_with_message "Invalid encryption: ssl" 1 "Invalid encryption mode" \
        -e "localhost:4317" -t "token" -c ssl -d
    run_test_with_message "Invalid encryption: random" 1 "Invalid encryption mode" \
        -e "localhost:4317" -t "token" -c random -d
}

test_reverse_mode() {
    print_section "5. REVERSE MODE TESTS"
    
    # Reverse mode without encryption (should work)
    run_test "Reverse mode without encryption" 0 \
        -e "0.0.0.0:4317" -t "token" -r -d
    
    # Reverse mode with encryption requires cert and key
    run_test_with_message "Reverse mode + encryption full: missing cert" 1 "Public certificate path" \
        -e "0.0.0.0:4317" -t "token" -r -c full -d
    
    run_test_with_message "Reverse mode + encryption full: missing key" 1 "Private key path" \
        -e "0.0.0.0:4317" -t "token" -r -c full \
        -C "${TEST_TEMP_DIR}/public.crt" -d

    # Reverse mode with encryption requires cert and key
    run_test_with_message "Reverse mode + encryption insecure: missing cert" 1 "Public certificate path" \
        -e "0.0.0.0:4317" -t "token" -r -c insecure -d
    
    run_test_with_message "Reverse mode + encryption insecure: missing key" 1 "Private key path" \
        -e "0.0.0.0:4317" -t "token" -r -c insecure \
        -C "${TEST_TEMP_DIR}/public.crt" -d
    
    # Reverse mode with encryption and valid cert/key
    run_test "Reverse mode + encryption: with cert and key" 0 \
        -e "0.0.0.0:4317" -t "token" -r -c full \
        -C "${TEST_TEMP_DIR}/public.crt" \
        -k "${TEST_TEMP_DIR}/private.key" -d
    
    # Reverse mode with non-existent cert file
    run_test_with_message "Reverse mode: non-existent cert file" 1 "not found" \
        -e "0.0.0.0:4317" -t "token" -r -c full \
        -C "/nonexistent/cert.crt" \
        -k "${TEST_TEMP_DIR}/private.key" -d
    
    # Reverse mode with non-existent key file
    run_test_with_message "Reverse mode: non-existent key file" 1 "not found" \
        -e "0.0.0.0:4317" -t "token" -r -c full \
        -C "${TEST_TEMP_DIR}/public.crt" \
        -k "/nonexistent/private.key" -d
}

test_ca_certificate() {
    print_section "6. CA CERTIFICATE TESTS (Non-Reverse Mode)"
    
    # CA certificate with encryption (valid)
    run_test "Encryption with valid CA cert" 0 \
        -e "localhost:4317" -t "token" -c full \
        -a "${TEST_TEMP_DIR}/ca.crt" -d
    
    # CA certificate with non-existent file
    run_test_with_message "Encryption with non-existent CA cert" 1 "not found" \
        -e "localhost:4317" -t "token" -c full \
        -a "/nonexistent/ca.crt" -d
    
    # CA certificate with non-existent file
    run_test_with_message "Encryption(insecure) with non-existent CA cert" 1 "not found" \
        -e "localhost:4317" -t "token" -c insecure \
        -a "/nonexistent/ca.crt" -d
    
    # CA common name
    run_test "Encryption(full) with CA name" 0 \
        -e "localhost:4317" -t "token" -c full \
        -a "${TEST_TEMP_DIR}/ca.crt" -N "my-ca-name" -d

    # CA common name
    run_test "Encryption(insecure) with CA name" 0 \
        -e "localhost:4317" -t "token" -c insecure \
        -a "${TEST_TEMP_DIR}/ca.crt" -N "my-ca-name" -d
}

test_log_settings() {
    print_section "7. LOG SETTINGS TESTS"
    
    # Log type validation
    run_test "Log type: file (default)" 0 \
        -e "localhost:4317" -t "token" -T file -d
    run_test "Log type: stdout" 0 \
        -e "localhost:4317" -t "token" -T stdout -d

    run_test_with_message "Invalid log type: syslog" 1 "Invalid log type" \
        -e "localhost:4317" -t "token" -T syslog -d
    run_test_with_message "Invalid log type: eventlog" 1 "Invalid log type" \
        -e "localhost:4317" -t "token" -T eventlog -d
    
    # Log level validation
    run_test_with_message "Invalid log level: off" 1 "Invalid log level" \
        -e "localhost:4317" -t "token" -l off -d

    run_test "Log level: critical" 0 \
        -e "localhost:4317" -t "token" -l critical -d
    run_test "Log level: error" 0 \
        -e "localhost:4317" -t "token" -l error -d
    run_test "Log level: warning" 0 \
        -e "localhost:4317" -t "token" -l warning -d
    run_test "Log level: info" 0 \
        -e "localhost:4317" -t "token" -l info -d
    run_test "Log level: debug" 0 \
        -e "localhost:4317" -t "token" -l debug -d
    run_test "Log level: trace" 0 \
        -e "localhost:4317" -t "token" -l trace -d

    run_test_with_message "Invalid log level: verbose" 1 "Invalid log level" \
        -e "localhost:4317" -t "token" -l verbose -d
    run_test_with_message "Invalid log level: warn" 1 "Invalid log level" \
        -e "localhost:4317" -t "token" -l warn -d
    
    # Log file path validation
    run_test "Valid log file path" 0 \
        -e "localhost:4317" -t "token" -T file \
        -L "/var/log/agent.log" -d
    run_test_with_message "Invalid log file: relative path" 1 "must be an absolute path" \
        -e "localhost:4317" -t "token" -T file \
        -L "logs/agent.log" -d
    run_test_with_message "Invalid log file: no leading slash" 1 "must be an absolute path" \
        -e "localhost:4317" -t "token" -T file \
        -L "agent.log" -d
}

test_max_file_settings() {
    print_section "8. MAX FILE SIZE AND MAX NUMBER TESTS"
    
    # Valid max file size
    run_test "Valid max-file-size: 10 MB" 0 \
        -e "localhost:4317" -t "token" -M 10 -d
    run_test "Valid max-file-size: 1 MB" 0 \
        -e "localhost:4317" -t "token" -M 1 -d
    
    # Invalid max file size
    run_test_with_message "Invalid max-file-size: 0" 1 "must be greater than 0" \
        -e "localhost:4317" -t "token" -M 0 -d
    run_test_with_message "Invalid max-file-size: negative" 1 "must be a positive integer" \
        -e "localhost:4317" -t "token" -M -100 -d
    run_test_with_message "Invalid max-file-size: non-numeric" 1 "must be a positive integer" \
        -e "localhost:4317" -t "token" -M abc -d
    run_test_with_message "Invalid max-file-size: with suffix" 1 "must be a positive integer" \
        -e "localhost:4317" -t "token" -M 10MB -d
    
    # Valid max number
    run_test "Valid max-number: 5" 0 \
        -e "localhost:4317" -t "token" -m 5 -d
    run_test "Valid max-number: 100" 0 \
        -e "localhost:4317" -t "token" -m 100 -d
    
    # Invalid max number
    run_test_with_message "Invalid max-number: 0" 1 "must be greater than 0" \
        -e "localhost:4317" -t "token" -m 0 -d
    run_test_with_message "Invalid max-number: negative" 1 "must be a positive integer" \
        -e "localhost:4317" -t "token" -m -5 -d
}

test_custom_check_file() {
    print_section "9. CUSTOM CHECK FILE TESTS"
    
    # Valid custom check file
    run_test "Valid custom check file" 0 \
        -e "localhost:4317" -t "token" \
        -x "${TEST_TEMP_DIR}/custom_check.json" -d
    
    # Non-existent custom check file
    run_test_with_message "Non-existent custom check file" 1 "not found" \
        -e "localhost:4317" -t "token" \
        -x "/nonexistent/check.json" -d
}

test_hostname() {
    print_section "11. HOSTNAME TESTS"
    
    # Valid hostnames
    run_test "Custom hostname" 0 \
        -e "localhost:4317" -t "token" -n "my-server-01" -d
    run_test "Hostname with dots" 0 \
        -e "localhost:4317" -t "token" -n "web.server.prod" -d
    run_test "Hostname with underscores" 0 \
        -e "localhost:4317" -t "token" -n "web_server_01" -d
    
    # No hostname (should use system default)
    run_test "No hostname (uses system default)" 0 \
        -e "localhost:4317" -t "token" -d
}

test_unknown_arguments() {
    print_section "14. UNKNOWN ARGUMENT TESTS"
    
    run_test_with_message "Unknown short option" 1 "Unknown option" \
        -e "localhost:4317" -t "token" -z -d
    run_test_with_message "Unknown long option" 1 "Unknown option" \
        -e "localhost:4317" -t "token" --unknown -d
    run_test_with_message "Typo in option" 1 "Unknown option" \
        -e "localhost:4317" -t "token" --encription full -d
}
#===============================================================================
# Test JSON Output Generation
#===============================================================================


# Helper function to run test and check JSON output
run_json_test() {
    local test_name="$1"
    local expected_field="$2"
    local expected_value="$3"
    shift 3
    local args=("$@")
    
    TESTS_RUN=$((TESTS_RUN + 1))
    
    echo -e "${BLUE}[TEST ${TESTS_RUN}]${NC} ${test_name}"
    
    local output
    local actual_exit
    output=$(bash "${SCRIPT_UNDER_TEST}" --output-config "${args[@]}" 2>&1)
    actual_exit=$?
    
    if [[ ${actual_exit} -ne 0 ]]; then
        echo -e "  ${RED}✗ FAILED${NC} (script exited with error: ${actual_exit})"
        echo -e "  Output: ${output}" | head -3
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
    
    # Check if expected field and value exist in JSON output
    if [[ "${output}" == *"\"${expected_field}\""*"${expected_value}"* ]]; then
        echo -e "  ${GREEN}✓ PASSED${NC} (found ${expected_field}: ${expected_value})"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${RED}✗ FAILED${NC} (expected ${expected_field}: ${expected_value})"
        echo -e "  Output: ${output}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Helper function to check field is NOT in JSON
run_json_absent_test() {
    local test_name="$1"
    local absent_field="$2"
    shift 2
    local args=("$@")
    
    TESTS_RUN=$((TESTS_RUN + 1))
    
    echo -e "${BLUE}[TEST ${TESTS_RUN}]${NC} ${test_name}"
    
    local output
    local actual_exit
    output=$(bash "${SCRIPT_UNDER_TEST}" --output-config "${args[@]}" 2>&1)
    actual_exit=$?
    
    if [[ ${actual_exit} -ne 0 ]]; then
        echo -e "  ${RED}✗ FAILED${NC} (script exited with error: ${actual_exit})"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
    
    if [[ "${output}" != *"\"${absent_field}\""* ]]; then
        echo -e "  ${GREEN}✓ PASSED${NC} (field '${absent_field}' correctly absent)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "  ${RED}✗ FAILED${NC} (field '${absent_field}' should not be present)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}
    

test_output_config_generation() {
    print_section "15. JSON CONFIGURATION GENERATION TESTS"
    
    echo -e "${CYAN}--- Required Fields Tests ---${NC}"
    
    # Test required fields are present
    run_json_test "JSON contains endpoint" "endpoint" "localhost:4317" \
        -e "localhost:4317" -t "token"
    
    run_json_test "JSON contains host" "host" "my-server" \
        -e "localhost:4317" -t "token" -n "my-server"
    
    run_json_test "JSON contains token" "token" "my-secret-token" \
        -e "localhost:4317" -t "my-secret-token"
    
    run_json_test "JSON contains log_level" "log_level" "debug" \
        -e "localhost:4317" -t "token" -l debug
    
    run_json_test "JSON contains log_type" "log_type" "stdout" \
        -e "localhost:4317" -t "token" -T stdout
    
    run_json_test "JSON contains encryption" "encryption" "no" \
        -e "localhost:4317" -t "token" -c no
    
    run_json_test "JSON contains encryption" "encryption" "insecure" \
        -e "localhost:4317" -t "token" -c insecure

    run_json_test "JSON contains encryption" "encryption" "full" \
        -e "localhost:4317" -t "token" -c full
    
    echo -e "${CYAN}--- Log Settings Tests ---${NC}"
    
    # Test log file settings
    run_json_test "JSON contains log_file when log_type=file" "log_file" "/var/log/test.log" \
        -e "localhost:4317" -t "token" -T file -L "/var/log/test.log"
    
    run_json_test "JSON contains log_max_file_size" "log_max_file_size" "10485760" \
        -e "localhost:4317" -t "token" -T file -M 10485760
    
    run_json_test "JSON contains log_max_files" "log_max_files" "5" \
        -e "localhost:4317" -t "token" -T file -m 5
    
    run_json_absent_test "JSON omits log_file when log_type=stdout" "log_file" \
        -e "localhost:4317" -t "token" -T stdout
    
    echo -e "${CYAN}--- Encryption Mode Tests ---${NC}"
    
    # Test encryption settings
    run_json_test "JSON encryption: full" "encryption" "full" \
        -e "localhost:4317" -t "token" -c full
    
    run_json_test "JSON encryption: insecure" "encryption" "insecure" \
        -e "localhost:4317" -t "token" -c insecure
    
    run_json_test "JSON encryption: no" "encryption" "no" \
        -e "localhost:4317" -t "token" -c no
    
    echo -e "${CYAN}--- Reverse Mode Tests ---${NC}"
    
    # Test reverse mode
    run_json_test "JSON reversed_grpc_streaming: false (default)" "reversed_grpc_streaming" "false" \
        -e "localhost:4317" -t "token"
    
    run_json_test "JSON reversed_grpc_streaming: true" "reversed_grpc_streaming" "true" \
        -e "localhost:4317" -t "token" -r \
        -c full -C "${TEST_TEMP_DIR}/public.crt" -k "${TEST_TEMP_DIR}/private.key"
    
    echo -e "${CYAN}--- Certificate Tests (Agent-Initiated) ---${NC}"
    
    # Test CA certificate (non-reverse mode)
    run_json_test "JSON contains ca_certificate in non-reverse mode" "ca_certificate" "${TEST_TEMP_DIR}/ca.crt" \
        -e "localhost:4317" -t "token" -c full -a "${TEST_TEMP_DIR}/ca.crt"
    
    run_json_test "JSON contains ca_name" "ca_name" "MyCentralCA" \
        -e "localhost:4317" -t "token" -c full -a "${TEST_TEMP_DIR}/ca.crt" -N "MyCentralCA"
    
    run_json_absent_test "JSON omits certificate in non-reverse mode" "certificate" \
        -e "localhost:4317" -t "token" -c full -a "${TEST_TEMP_DIR}/ca.crt"
    
    echo -e "${CYAN}--- Certificate Tests (Poller-Initiated/Reverse) ---${NC}"
    
    # Test cert/key in reverse mode
    run_json_test "JSON contains certificate in reverse mode" "certificate" "${TEST_TEMP_DIR}/public.crt" \
        -e "localhost:4317" -t "token" -r -c full \
        -C "${TEST_TEMP_DIR}/public.crt" -k "${TEST_TEMP_DIR}/private.key"
    
    run_json_test "JSON contains private_key in reverse mode" "private_key" "${TEST_TEMP_DIR}/private.key" \
        -e "localhost:4317" -t "token" -r -c full \
        -C "${TEST_TEMP_DIR}/public.crt" -k "${TEST_TEMP_DIR}/private.key"
    
    run_json_absent_test "JSON omits ca_certificate in reverse mode" "ca_certificate" \
        -e "localhost:4317" -t "token" -r -c full \
        -C "${TEST_TEMP_DIR}/public.crt" -k "${TEST_TEMP_DIR}/private.key"
    
    echo -e "${CYAN}--- Optional Fields Tests ---${NC}"
    
    # Test fingerprint
    run_json_test "JSON contains fingerprint" "fingerprint" "sha256:abcdef123456" \
        -e "localhost:4317" -t "token" -f "sha256:abcdef123456"
    
    run_json_absent_test "JSON omits fingerprint when not specified" "fingerprint" \
        -e "localhost:4317" -t "token"
    
    # Test custom check file
    run_json_test "JSON contains check_file" "check_file" "${TEST_TEMP_DIR}/custom_check.json" \
        -e "localhost:4317" -t "token" -x "${TEST_TEMP_DIR}/custom_check.json"
    
    run_json_absent_test "JSON omits check_file when not specified" "check_file" \
        -e "localhost:4317" -t "token"
    
    echo -e "${CYAN}--- Full Configuration Scenarios ---${NC}"
    
}

#===============================================================================
# MAIN
#===============================================================================

show_usage() {
    echo "Usage: $0 <path_to_install_cma.sh>"
    echo ""
    echo "Example:"
    echo "  $0 ./install_cma.sh"
    echo "  $0 /path/to/install_cma.sh"
    echo ""
}

main() {
    echo ""
    echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║     CENTREON MONITORING AGENT INSTALL SCRIPT TEST SUITE       ║${NC}"
    echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    # Check if script path was provided
    if [[ -z "${SCRIPT_UNDER_TEST}" ]]; then
        echo -e "${RED}ERROR: Missing required argument: path to install_cma.sh${NC}"
        echo ""
        show_usage
        exit 1
    fi
    
    # Check if script exists
    if [[ ! -f "${SCRIPT_UNDER_TEST}" ]]; then
        echo -e "${RED}ERROR: Script not found: ${SCRIPT_UNDER_TEST}${NC}"
        echo ""
        show_usage
        exit 1
    fi
    
    echo "Script under test: ${SCRIPT_UNDER_TEST}"
    
    # Setup test environment
    setup
    
    # Trap to cleanup on exit
    trap cleanup EXIT
    
    # Run all test suites
    test_help_and_basic
    test_required_params
    test_endpoint_validation
    test_encryption_modes
    test_reverse_mode
    test_ca_certificate
    test_log_settings
    test_max_file_settings
    test_custom_check_file
    test_hostname
    test_unknown_arguments
    
    test_output_config_generation
    
    # Print summary
    print_summary
    
    # Return appropriate exit code
    if [[ ${TESTS_FAILED} -gt 0 ]]; then
        exit 1
    fi
    exit 0
}

# Run main
main "$@"
