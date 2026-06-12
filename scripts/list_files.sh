#!/bin/bash

# Change to the actual workspace directory (your source code)
if [[ -n "${BUILD_WORKSPACE_DIRECTORY}" ]]; then
    cd "${BUILD_WORKSPACE_DIRECTORY}"
else
    echo "Warning: BUILD_WORKSPACE_DIRECTORY not set, using current directory"
fi

echo "=== Working directory: $(pwd) ==="
echo ""

echo "=== C++ files that would be formatted ==="
cpp_files=$(find . -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | \
    grep -v "^./bazel-" | \
    grep -v "^./external/" | \
    grep -v "^./third_party/")

if [[ -n "$cpp_files" ]]; then
    echo "$cpp_files" | sort
    echo ""
    echo "Total C++ files: $(echo "$cpp_files" | wc -l)"
else
    echo "No C++ files found"
fi

echo ""
echo "=== BUILD files that would be formatted ==="
build_files=$(find . -name "BUILD" -o -name "BUILD.bazel" -o -name "*.bzl" | \
    grep -v "^./bazel-" | \
    grep -v "^./external/" | \
    grep -v "^./third_party/")

if [[ -n "$build_files" ]]; then
    echo "$build_files" | sort
    echo ""
    echo "Total BUILD files: $(echo "$build_files" | wc -l)"
else
    echo "No BUILD files found"
fi

echo ""
echo "=== Tools check ==="
if command -v clang-format &> /dev/null; then
    echo "✓ clang-format found: $(which clang-format)"
else
    echo "✗ clang-format not found"
fi

if command -v buildifier-linux-amd64 &> /dev/null; then
    echo "✓ buildifier found: $(which buildifier-linux-amd64)"
else
    echo "✗ buildifier not found"
fi

if [[ -f ".clang-format" ]]; then
    echo "✓ .clang-format file exists"
else
    echo "✗ .clang-format file not found"
fi