#!/bin/bash
# Integration test script for octopass-zig

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/zig-out"

echo "=== Octopass Zig Integration Tests ==="
echo ""

# Build the project
echo "Building project..."
cd "$PROJECT_DIR"
zig build

# Check if build succeeded
if [ ! -f "$BUILD_DIR/bin/octopass" ]; then
    echo "ERROR: CLI binary not found at $BUILD_DIR/bin/octopass"
    exit 1
fi

if [ ! -f "$BUILD_DIR/lib/libnss_octopass.so.2.0.0" ]; then
    echo "ERROR: NSS library not found at $BUILD_DIR/lib/libnss_octopass.so.2.0.0"
    exit 1
fi

echo "Build successful!"
echo ""

# Test CLI version
echo "Testing CLI version command..."
VERSION_OUTPUT=$("$BUILD_DIR/bin/octopass" version 2>&1)
if [[ "$VERSION_OUTPUT" == *"octopass"* ]]; then
    echo "PASS: version command works"
    echo "  Output: $VERSION_OUTPUT"
else
    echo "FAIL: version command"
    exit 1
fi

# Test CLI help
echo ""
echo "Testing CLI help command..."
HELP_OUTPUT=$("$BUILD_DIR/bin/octopass" help 2>&1)
if [[ "$HELP_OUTPUT" == *"Usage:"* ]]; then
    echo "PASS: help command works"
else
    echo "FAIL: help command"
    exit 1
fi

# Test NSS library symbols
echo ""
echo "Testing NSS library symbols..."
if command -v nm &> /dev/null; then
    SYMBOLS=$(nm -D "$BUILD_DIR/lib/libnss_octopass.so.2.0.0" 2>/dev/null || true)

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
        echo "PASS: All required NSS symbols found"
    else
        echo "FAIL: Some NSS symbols missing"
        exit 1
    fi
else
    echo "SKIP: nm command not available"
fi

# Run unit tests
echo ""
echo "Running unit tests..."
cd "$PROJECT_DIR"
if zig build test 2>&1; then
    echo "PASS: Unit tests passed"
else
    echo "FAIL: Unit tests failed"
    exit 1
fi

echo ""
echo "=== All Integration Tests Passed ==="
