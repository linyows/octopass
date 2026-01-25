#!/bin/bash
# Integration test script for octopass
# This script tests installed octopass package using environment variables:
#   OCTOPASS_TOKEN        - GitHub Personal Access Token (required for API tests)
#   OCTOPASS_ORGANIZATION - GitHub Organization name (required for API tests)
#   OCTOPASS_TEAM         - GitHub Team slug (required for API tests)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}PASS${NC}: $1"
}

fail() {
    echo -e "${RED}FAIL${NC}: $1"
    exit 1
}

skip() {
    echo -e "${YELLOW}SKIP${NC}: $1"
}

echo "=== Octopass Integration Tests ==="
echo ""

# Check if octopass is installed
if ! command -v octopass &> /dev/null; then
    fail "octopass command not found. Please install the package first."
fi

OCTOPASS=$(command -v octopass)
echo "Using octopass: $OCTOPASS"

# Setup config file for environment variable override
CONFIG_FILE="/etc/octopass.conf"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Creating minimal config file at $CONFIG_FILE"
    cat > "$CONFIG_FILE" << 'EOF'
# Minimal config - values are overridden by environment variables
Token = ""
Organization = ""
Team = ""
EOF
    chmod 600 "$CONFIG_FILE"
fi
echo "Config file: $CONFIG_FILE"

# Check environment variables for API tests
API_TESTS_ENABLED=true
if [ -z "$OCTOPASS_TOKEN" ]; then
    echo -e "${YELLOW}WARNING${NC}: OCTOPASS_TOKEN not set - API tests will be skipped"
    API_TESTS_ENABLED=false
fi
if [ -z "$OCTOPASS_ORGANIZATION" ]; then
    echo -e "${YELLOW}WARNING${NC}: OCTOPASS_ORGANIZATION not set - API tests will be skipped"
    API_TESTS_ENABLED=false
fi
if [ -z "$OCTOPASS_TEAM" ]; then
    echo -e "${YELLOW}WARNING${NC}: OCTOPASS_TEAM not set - API tests will be skipped"
    API_TESTS_ENABLED=false
fi

if [ "$API_TESTS_ENABLED" = true ]; then
    echo "API tests enabled for: $OCTOPASS_ORGANIZATION/$OCTOPASS_TEAM"
fi
echo ""

# Test CLI version
echo "Testing CLI version command..."
VERSION_OUTPUT=$("$OCTOPASS" --version 2>&1 || true)
if [[ "$VERSION_OUTPUT" == *"octopass"* ]]; then
    pass "version command works"
    echo "  Output: $VERSION_OUTPUT"
else
    fail "version command"
fi

# Test CLI help
echo ""
echo "Testing CLI help command..."
HELP_OUTPUT=$("$OCTOPASS" --help 2>&1 || true)
if [[ "$HELP_OUTPUT" == *"Usage:"* ]]; then
    pass "help command works"
else
    fail "help command"
fi

# Find and test NSS library
echo ""
echo "Testing NSS library..."
NSS_LIB=$(find /usr/lib* -name "libnss_octopass.so*" 2>/dev/null | head -1)
if [ -n "$NSS_LIB" ]; then
    echo "Found NSS library: $NSS_LIB"

    if command -v nm &> /dev/null; then
        SYMBOLS=$(nm -D "$NSS_LIB" 2>/dev/null || true)

        required_symbols=(
            "_nss_octopass_setpwent"
            "_nss_octopass_endpwent"
            "_nss_octopass_getpwent_r"
            "_nss_octopass_getpwnam_r"
            "_nss_octopass_getpwuid_r"
            "_nss_octopass_setgrent"
            "_nss_octopass_endgrent"
            "_nss_octopass_getgrent_r"
            "_nss_octopass_getgrnam_r"
            "_nss_octopass_getgrgid_r"
            "_nss_octopass_setspent"
            "_nss_octopass_endspent"
            "_nss_octopass_getspent_r"
            "_nss_octopass_getspnam_r"
        )

        all_found=true
        for sym in "${required_symbols[@]}"; do
            if echo "$SYMBOLS" | grep -q "$sym"; then
                echo "  PASS: $sym found"
            else
                echo "  FAIL: $sym not found"
                all_found=false
            fi
        done

        if $all_found; then
            pass "All required NSS symbols found"
        else
            fail "Some NSS symbols missing"
        fi
    else
        skip "nm command not available"
    fi
else
    fail "NSS library not found"
fi

echo ""
echo "=== API Integration Tests ==="
echo ""

if [ "$API_TESTS_ENABLED" = false ]; then
    skip "API tests skipped - environment variables not set"
    echo ""
    echo "To run API tests, set the following environment variables:"
    echo "  export OCTOPASS_TOKEN=<your-github-token>"
    echo "  export OCTOPASS_ORGANIZATION=<your-org>"
    echo "  export OCTOPASS_TEAM=<your-team>"
    echo ""
    echo "=== Basic Integration Tests Passed (API tests skipped) ==="
    exit 0
fi

# Test passwd command (list all users)
echo "Testing passwd command..."
PASSWD_OUTPUT=$("$OCTOPASS" passwd 2>&1)
if [ $? -eq 0 ] && [ -n "$PASSWD_OUTPUT" ]; then
    # Check output format: name:x:uid:gid:gecos:home:shell
    if echo "$PASSWD_OUTPUT" | grep -qE '^[a-zA-Z0-9_-]+:x:[0-9]+:[0-9]+:'; then
        pass "passwd command returns valid format"
        USER_COUNT=$(echo "$PASSWD_OUTPUT" | wc -l | tr -d ' ')
        echo "  Found $USER_COUNT user(s)"
        # Show first user as example
        FIRST_USER=$(echo "$PASSWD_OUTPUT" | head -n1)
        echo "  Example: $FIRST_USER"
    else
        fail "passwd command output format invalid"
    fi
else
    fail "passwd command failed: $PASSWD_OUTPUT"
fi

# Extract a username from passwd output for further tests
TEST_USER=$(echo "$PASSWD_OUTPUT" | head -n1 | cut -d: -f1)
echo "  Using '$TEST_USER' for subsequent tests"

# Test passwd command with specific user
echo ""
echo "Testing passwd command with specific user..."
PASSWD_USER_OUTPUT=$("$OCTOPASS" passwd "$TEST_USER" 2>&1)
if [ $? -eq 0 ] && echo "$PASSWD_USER_OUTPUT" | grep -q "^${TEST_USER}:"; then
    pass "passwd <user> command works"
else
    fail "passwd <user> command failed"
fi

# Test group command
echo ""
echo "Testing group command..."
GROUP_OUTPUT=$("$OCTOPASS" group 2>&1)
if [ $? -eq 0 ] && [ -n "$GROUP_OUTPUT" ]; then
    # Check output format: name:x:gid:members
    if echo "$GROUP_OUTPUT" | grep -qE '^[a-zA-Z0-9_-]+:x:[0-9]+:'; then
        pass "group command returns valid format"
        echo "  Output: $GROUP_OUTPUT"
    else
        fail "group command output format invalid"
    fi
else
    fail "group command failed: $GROUP_OUTPUT"
fi

# Test shadow command
echo ""
echo "Testing shadow command..."
SHADOW_OUTPUT=$("$OCTOPASS" shadow 2>&1)
if [ $? -eq 0 ] && [ -n "$SHADOW_OUTPUT" ]; then
    # Check output format: name:password:...
    if echo "$SHADOW_OUTPUT" | grep -qE '^[a-zA-Z0-9_-]+:!!:'; then
        pass "shadow command returns valid format"
        SHADOW_COUNT=$(echo "$SHADOW_OUTPUT" | wc -l | tr -d ' ')
        echo "  Found $SHADOW_COUNT shadow entry(ies)"
    else
        fail "shadow command output format invalid"
    fi
else
    fail "shadow command failed: $SHADOW_OUTPUT"
fi

# Test keys command (SSH public keys)
echo ""
echo "Testing keys command (SSH public keys for $TEST_USER)..."
KEYS_OUTPUT=$("$OCTOPASS" "$TEST_USER" 2>&1)
if [ $? -eq 0 ]; then
    if [ -n "$KEYS_OUTPUT" ]; then
        # Check if output looks like SSH keys
        if echo "$KEYS_OUTPUT" | grep -qE '^ssh-(rsa|ed25519|ecdsa)'; then
            pass "keys command returns SSH public keys"
            KEY_COUNT=$(echo "$KEYS_OUTPUT" | wc -l | tr -d ' ')
            echo "  Found $KEY_COUNT key(s)"
        else
            skip "keys command returned data but not SSH key format (user may not have public keys)"
        fi
    else
        skip "keys command returned empty (user has no public keys)"
    fi
else
    fail "keys command failed: $KEYS_OUTPUT"
fi

echo ""
echo "=== All Integration Tests Passed ==="
