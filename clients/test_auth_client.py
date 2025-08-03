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

    async def test_invalid_auth_type(self):
        """Test invalid authentication type"""
        print(f"\n🔹 Testing invalid authentication type")
        
        message = {
            "type": "auth",
            "auth_type": "invalid_type",
            "username": "test",
            "password": "test"
        }
        
        response = await self.send_message(message)
        if response:
            if not response.get("success"):
                print(f"✓ Invalid auth type correctly rejected: {response.get('error')}")
                return True
            else:
                print(f"✗ Invalid auth type was incorrectly accepted")
                return False
        return False

    async def test_missing_credentials(self):
        """Test missing username/password"""
        print(f"\n🔹 Testing missing credentials")
        
        message = {
            "type": "auth",
            "auth_type": "login",
            "username": "",
            "password": ""
        }
        
        response = await self.send_message(message)
        if response:
            if not response.get("success"):
                print(f"✓ Missing credentials correctly rejected: {response.get('error')}")
                return True
            else:
                print(f"✗ Missing credentials were incorrectly accepted")
                return False
        return False

    async def disconnect(self):
        """Disconnect from the server"""
        if self.websocket:
            await self.websocket.close()
            print("✓ Disconnected from server")

    async def run_comprehensive_test(self):
        """Run a comprehensive test suite"""
        print("🚀 Starting authentication system test suite")
        print("=" * 50)
        
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
            # Test login with newly registered user
            print("\n📝 Testing login with newly registered user:")
            await self.test_login(test_username, test_password)
            
            # Test duplicate registration
            print("\n📝 Testing duplicate registration:")
            await self.test_registration(test_username, "different_password", "different@email.com")
        
        # Test login with wrong password
        print("\n📝 Testing login with wrong password:")
        await self.test_login(test_username, "wrong_password")
        
        # Test login with non-existent user
        print("\n📝 Testing login with non-existent user:")
        await self.test_login("nonexistent_user", "any_password")
        
        # Test weak password registration
        print("\n📝 Testing weak password registration:")
        await self.test_registration("weakpassuser", "123", "weak@example.com")
        
        # Test edge cases
        print("\n📝 Testing edge cases:")
        await self.test_invalid_auth_type()
        await self.test_missing_credentials()
        
        await self.disconnect()
        
        print("\n" + "=" * 50)
        print("🏁 Test suite completed")
        return True

async def main():
    """Main function to run the test client"""
    if len(sys.argv) > 1:
        uri = sys.argv[1]
    else:
        uri = "ws://localhost:9001"
    
    client = AuthTestClient(uri)
    
    try:
        await client.run_comprehensive_test()
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
