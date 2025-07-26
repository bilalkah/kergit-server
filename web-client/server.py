#!/usr/bin/env python3
"""
Simple HTTP server for serving the web-based chat client.
Run this script to start a local web server that serves the chat interface.
"""

import http.server
import socketserver
import os
import sys
from pathlib import Path

class ChatServer(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Add CORS headers for development
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        super().end_headers()

    def do_OPTIONS(self):
        # Handle preflight requests
        self.send_response(200)
        self.end_headers()

def main():
    # Get the directory where this script is located
    script_dir = Path(__file__).parent.absolute()
    
    # Change to the script directory
    os.chdir(script_dir)
    
    # Default port
    port = 8080
    
    # Check if port is provided as command line argument
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print(f"Invalid port number: {sys.argv[1]}")
            sys.exit(1)
    
    # Create server
    with socketserver.TCPServer(("", port), ChatServer) as httpd:
        print(f"🚀 Web chat client server started!")
        print(f"📱 Open your browser and go to: http://localhost:{port}")
        print(f"📁 Serving files from: {script_dir}")
        print(f"🔗 Make sure your C++ chat server is running on ws://localhost:9001")
        print(f"⏹️  Press Ctrl+C to stop the server")
        print()
        
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n👋 Server stopped by user")
            httpd.shutdown()

if __name__ == "__main__":
    main() 