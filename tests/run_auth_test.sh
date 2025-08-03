#!/bin/bash
# Script to run the authentication test client with dependency management

echo "🔐 Authentication Test Client Runner"
echo "Checking Python dependencies..."

# Check if websockets is installed
if python3 -c "import websockets" 2>/dev/null; then
    echo "✓ websockets library found"
else
    echo "⚠️  Installing websockets library..."
    pip3 install websockets
    if [ $? -eq 0 ]; then
        echo "✓ websockets installed successfully"
    else
        echo "✗ Failed to install websockets. Please install manually:"
        echo "  pip3 install websockets"
        exit 1
    fi
fi

echo "🚀 Starting authentication test..."
echo "Make sure the server is running on ws://localhost:9001"
echo ""

# Run the Python test client
python3 "$(dirname "$0")/auth_test_client.py" "$@"
