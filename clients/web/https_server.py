#!/usr/bin/env python3
"""
Simple HTTPS server for serving the web-based chat client.

Usage:
    python3 serve_https.py [port] [certfile] [keyfile]

Defaults:
    port     = 8443
    certfile = ./certs/cert.pem
    keyfile  = ./certs/key.pem
"""

import http.server
import ssl
import os
import sys
from pathlib import Path

class ChatServer(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Add CORS headers for development
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        # Disable caching so changes are picked up without hard refresh
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def do_OPTIONS(self):
        # Handle preflight requests
        self.send_response(200)
        self.end_headers()

def main():
    script_dir = Path(__file__).parent.absolute()
    os.chdir(script_dir)

    # Defaults
    port = 8443
    certfile = script_dir / "../../certs/cert.pem"
    keyfile = script_dir / "../../certs/key.pem"
    print(certfile)
    print(keyfile)
    # Optional CLI args: port, certfile, keyfile
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print(f"Invalid port number: {sys.argv[1]}")
            sys.exit(1)
    if len(sys.argv) > 2:
        certfile = Path(sys.argv[2]).absolute()
    if len(sys.argv) > 3:
        keyfile = Path(sys.argv[3]).absolute()

    if not certfile.exists() or not keyfile.exists():
        print(f"❌ TLS cert or key not found.")
        print(f"   Expected cert: {certfile}")
        print(f"   Expected key : {keyfile}")
        sys.exit(1)

    handler_cls = ChatServer

    # ThreadingHTTPServer is nicer for multiple requests
    httpd = http.server.ThreadingHTTPServer(("", port), handler_cls)

    # Wrap socket with TLS
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile=str(certfile), keyfile=str(keyfile))
    httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

    print(f"🚀 Web chat client server (HTTPS) started!")
    print(f"📱 Open your browser at: https://localhost:{port}")
    print(f"📁 Serving files from: {script_dir}")
    print(f"🔐 Using cert: {certfile}")
    print(f"🔐 Using key : {keyfile}")
    print(f"🛰️ Make sure your C++ chat server is running on wss://localhost:9001")
    print(f"⏹️  Press Ctrl+C to stop the server\n")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n👋 Server stopped by user")
        httpd.shutdown()
    except Exception as e:
        print(f"❌ Error: {e}")
        httpd.shutdown()

if __name__ == "__main__":
    main()
