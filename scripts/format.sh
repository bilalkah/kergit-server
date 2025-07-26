#!/bin/bash
set -e

# Change to the actual workspace directory (your source code)
if [[ -n "${BUILD_WORKSPACE_DIRECTORY}" ]]; then
    cd "${BUILD_WORKSPACE_DIRECTORY}"
else
    echo "Warning: BUILD_WORKSPACE_DIRECTORY not set, using current directory"
fi

echo "Working in directory: $(pwd)"

# Format C++ files with clang-format using your .clang-format file
echo "=== Formatting C++ files ==="
cpp_files=$(find . -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | \
    grep -v "^./bazel-" | \
    grep -v "^./external/" | \
    grep -v "^./third_party/")

if [[ -n "$cpp_files" ]]; then
    echo "Formatting $(echo "$cpp_files" | wc -l) C++ files..."
    echo "$cpp_files" | xargs clang-format -i
    echo "✓ Formatted C++ files"
else
    echo "No C++ files found"
fi

# Format Bazel files with buildifier
echo ""
echo "=== Formatting BUILD files ==="
build_files=$(find . -name "BUILD" -o -name "BUILD.bazel" -o -name "*.bzl" | \
    grep -v "^./bazel-" | \
    grep -v "^./external/" | \
    grep -v "^./third_party/")

if [[ -n "$build_files" ]]; then
    if command -v buildifier-linux-amd64 &> /dev/null; then
        echo "Formatting $(echo "$build_files" | wc -l) BUILD files..."
        echo "$build_files" | xargs buildifier-linux-amd64
        echo "✓ Formatted BUILD files with buildifier-linux-amd64"
    else
        echo "✗ buildifier-linux-amd64 not found in PATH, skipping BUILD file formatting"
    fi
else
    echo "No BUILD files found"
fi

echo ""
echo "🎉 Formatting complete!"
