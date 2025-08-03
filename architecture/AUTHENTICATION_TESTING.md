# Authentication System Testing Guide

## Overview
This document describes how to test the authentication system that has been integrated into the serverless communication platform.

## Authentication Command Usage

### 1. User Registration
To register a new user, send a JSON message with the following structure:

```json
{
  "type": "auth",
  "auth_type": "register",
  "username": "testuser",
  "password": "password123",
  "email": "testuser@example.com"
}
```

**Expected Response (Success):**
```json
{
  "type": "auth_response",
  "success": true,
  "user_id": "user_abc123",
  "message": "Authentication successful"
}
```

**Expected Response (Error):**
```json
{
  "type": "auth_response",
  "success": false,
  "error": "Username already exists"
}
```

### 2. User Login
To login an existing user, send a JSON message with the following structure:

```json
{
  "type": "auth",
  "auth_type": "login", 
  "username": "testuser",
  "password": "password123"
}
```

**Expected Response (Success):**
```json
{
  "type": "auth_response",
  "success": true,
  "user_id": "user_abc123",
  "message": "Authentication successful"
}
```

**Expected Response (Error):**
```json
{
  "type": "auth_response",
  "success": false,
  "error": "Invalid username or password"
}
```

## Default Test Account
The system comes with a pre-configured admin account for testing:
- **Username:** admin
- **Password:** admin123
- **Email:** admin@example.com

## Security Features Implemented

### 1. Password Security
- **Minimum Length:** 6 characters (reduced for demo, should be 12+ in production)
- **Hashing:** Simple hash for demo (use bcrypt in production)
- **Salt:** Basic salt implementation (enhance in production)

### 2. User Management
- **Unique Usernames:** System prevents duplicate usernames
- **User ID Generation:** Automatic generation of unique user IDs
- **Session Tracking:** User state maintained during WebSocket connection

### 3. Input Validation
- **Required Fields:** Username and password are mandatory
- **Empty Field Check:** System rejects empty credentials
- **Error Handling:** Comprehensive error messages for different failure cases

## Integration with Existing Commands

The authentication system is integrated with the existing command structure:
- **Command Type:** "auth" 
- **Command Handler:** AuthenticateCommand class
- **Registration:** Integrated in ChatServerApp::setup_commands()

## Testing Procedure

### Manual Testing with WebSocket Client

1. **Connect to Server:**
   ```
   ws://localhost:9001
   ```

2. **Test Registration:**
   ```json
   {"type":"auth","auth_type":"register","username":"newuser","password":"newpass123","email":"user@test.com"}
   ```

3. **Test Login:**
   ```json
   {"type":"auth","auth_type":"login","username":"newuser","password":"newpass123"}
   ```

4. **Test Invalid Login:**
   ```json
   {"type":"auth","auth_type":"login","username":"newuser","password":"wrongpass"}
   ```

5. **Test Duplicate Registration:**
   ```json
   {"type":"auth","auth_type":"register","username":"newuser","password":"anotherpass","email":"user2@test.com"}
   ```

### Expected Behavior

1. **Successful Registration:**
   - User is registered in the system
   - User is automatically logged in
   - User ID is generated and returned
   - Success response is sent

2. **Successful Login:**
   - User credentials are validated
   - User session is established
   - User ID is returned
   - Success response is sent

3. **Failed Authentication:**
   - Appropriate error message is returned
   - User is not logged in
   - No sensitive information is leaked

## Security Considerations for Production

### 1. Password Security
- Use bcrypt or Argon2 for password hashing
- Implement stronger password requirements
- Add password complexity validation
- Consider password expiration policies

### 2. Session Management
- Implement JWT tokens for stateless authentication
- Add session expiration and refresh tokens
- Implement secure session storage
- Add session invalidation on logout

### 3. Rate Limiting
- Implement login attempt rate limiting
- Add IP-based blocking for suspicious activity
- Implement CAPTCHA for repeated failures
- Log authentication attempts for monitoring

### 4. Transport Security
- Use WSS (WebSocket Secure) instead of WS
- Implement TLS 1.3 or higher
- Add certificate pinning
- Use secure headers

### 5. Data Validation
- Implement comprehensive input sanitization
- Add SQL injection protection (if using database)
- Validate all user inputs
- Implement CSRF protection

## File Structure

```
server/commands/
├── AuthenticateCommand.h     # Authentication command header
├── AuthenticateCommand.cpp   # Authentication command implementation
├── ICommand.h               # Base command interface
└── AllCommands.h           # Command includes

server/
├── ChatServerApp.h         # Server application header
├── ChatServerApp.cpp       # Server application (auth command registration)
└── BUILD                   # Build configuration

security/
├── Authentication.h        # Authentication service header
├── Authentication.cpp      # Authentication service implementation
└── BUILD                  # Security build configuration
```

## Error Codes and Messages

| Error Code | Message | Description |
|------------|---------|-------------|
| AUTH_001 | "Username and password required" | Missing credentials |
| AUTH_002 | "User not found" | Username doesn't exist |
| AUTH_003 | "Invalid password" | Wrong password provided |
| AUTH_004 | "Username already exists" | Duplicate registration attempt |
| AUTH_005 | "Password must be at least 6 characters" | Weak password |
| AUTH_006 | "Invalid authentication type" | Unknown auth_type |

## Future Enhancements

1. **Database Integration:** Replace in-memory storage with persistent database
2. **JWT Tokens:** Implement proper session tokens
3. **OAuth Integration:** Add third-party authentication providers
4. **Multi-Factor Authentication:** Add 2FA support
5. **Account Recovery:** Implement password reset functionality
6. **User Profiles:** Extend user model with additional fields
7. **Role-Based Access:** Implement user roles and permissions
8. **Audit Logging:** Add comprehensive authentication logging
