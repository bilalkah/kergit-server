#!/usr/bin/env python3
"""
Simple WebSocket client to test the authentication system.
This script demonstrates how to connect to the server and test authentication commands.
"""

import asyncio
import websockets
import json
import sys

class AuthTestClient:
    def __init__(self, uri="ws://localhost:9001"):
        self.uri = uri
        self.websocket = None

    async def connect(self):
        """Connect to the WebSocket server"""
        try:
            self.websocket = await websockets.connect(self.uri)
            print(f"✓ Connected to {self.uri}")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False

    async def send_message(self, message):
        """Send a JSON message to the server"""
        if not self.websocket:
            print("✗ Not connected to server")
            return None
        
        try:
            await self.websocket.send(json.dumps(message))
            response = await self.websocket.recv()
            return json.loads(response)
        except Exception as e:
            print(f"✗ Error sending message: {e}")
            return None

    async def test_registration(self, username, password, email=""):
        """Test user registration"""
        print(f"\n🔹 Testing registration for user: {username}")
        
        message = {
            "type": "auth",
            "auth_type": "register",
            "username": username,
            "password": password,
            "email": email
        }
        
        response = await self.send_message(message)
        if response:
            if response.get("success"):
                print(f"✓ Registration successful. User ID: {response.get('user_id')}")
                return True
            else:
                print(f"✗ Registration failed: {response.get('error')}")
                return False
        return False

    async def test_login(self, username, password):
        """Test user login"""
        print(f"\n🔹 Testing login for user: {username}")
        
        message = {
            "type": "auth",
            "auth_type": "login",
            "username": username,
            "password": password
        }
        
        response = await self.send_message(message)
        if response:
            if response.get("success"):
                print(f"✓ Login successful. User ID: {response.get('user_id')}")
                return True
            else:
                print(f"✗ Login failed: {response.get('error')}")
                return False
        return False

    async def disconnect(self):
        """Disconnect from the server"""
        if self.websocket:
            await self.websocket.close()
            print("✓ Disconnected from server")

    async def run_basic_test(self):
        """Run a basic test suite"""
        print("🚀 Starting authentication system test")
        print("=" * 40)
        
        if not await self.connect():
            return False

        # Test with default admin account
        print("\n📝 Testing default admin account:")
        await self.test_login("admin", "admin123")
        
        # Test registration of new user
        test_username = "testuser123"
        test_password = "testpass123"
        test_email = "test@example.com"
        
        print("\n📝 Testing new user registration:")
        reg_success = await self.test_registration(test_username, test_password, test_email)
        
        if reg_success:
            print("\n📝 Testing login with newly registered user:")
            await self.test_login(test_username, test_password)
        
        await self.disconnect()
        
        print("\n" + "=" * 40)
        print("🏁 Test completed")
        return True

async def main():
    """Main function to run the test client"""
    if len(sys.argv) > 1:
        uri = sys.argv[1]
    else:
        uri = "ws://localhost:9001"
    
    client = AuthTestClient(uri)
    
    try:
        await client.run_basic_test()
    except KeyboardInterrupt:
        print("\n\n⏹️  Test interrupted by user")
        await client.disconnect()
    except Exception as e:
        print(f"\n\n💥 Unexpected error: {e}")
        await client.disconnect()

if __name__ == "__main__":
    print("🔐 Authentication System Test Client")
    print("This script tests the WebSocket authentication system")
    print("Make sure the server is running on ws://localhost:9001")
    print("\nPress Ctrl+C to stop the test at any time\n")
    
    asyncio.run(main())
