# 🌐 Web-Based Chat Client

A modern, responsive web interface for the serverless communication platform. This web client provides a beautiful, user-friendly interface that connects to your C++ WebSocket server.

## ✨ Features

### **Modern User Interface**
- 🎨 Clean, professional design with gradient backgrounds
- 📱 Responsive layout that works on desktop and mobile
- 🌙 Modern UI components with smooth animations
- 📊 Real-time connection status indicators

### **User Experience**
- 🔐 Simple username-based authentication
- 📝 Real-time chat with timestamps
- 👥 Channel management (create/join channels)
- 👤 User presence and online status
- 📞 Voice call interface (ready for WebRTC integration)
- 🔔 Incoming call notifications

### **Interactive Elements**
- 💬 Real-time message updates
- 🎯 Click-to-join channels
- 📞 Click-to-call users
- ⌨️ Keyboard shortcuts (Enter to send)
- 📱 Touch-friendly mobile interface

## 🚀 Quick Start

### **Step 1: Start the C++ Server**
First, make sure your C++ chat server is running:

```bash
# In your main project directory
bazel run //server:server
```

The server should start on `ws://localhost:9001`

### **Step 2: Start the Web Client**
In a new terminal, navigate to the web-client directory and start the web server:

```bash
cd web-client
python3 server.py
```

### **Step 3: Open in Browser**
Open your web browser and go to:
```
http://localhost:8080
```

### **Step 4: Connect and Chat**
1. Enter your username
2. Click "Connect" 
3. Join or create a channel
4. Start chatting!

## 🎯 How to Use

### **Connecting**
1. **Enter Username**: Choose any username you like
2. **Server URL**: Default is `ws://localhost:9001` (change if needed)
3. **Click Connect**: The interface will connect to the server

### **Chatting**
- **Send Messages**: Type in the input box and press Enter or click Send
- **Join Channels**: Click on any channel in the sidebar
- **Create Channels**: Click the "+" button next to "Channels"
- **See Users**: View online users in the sidebar

### **Voice Calls**
- **Start Call**: Click the "Start Call" button or click on a user
- **Receive Call**: Answer incoming call notifications
- **Call Controls**: Use mute, video, and screen share buttons

## 🛠️ Development

### **File Structure**
```
web-client/
├── index.html          # Main HTML interface
├── styles.css          # CSS styling
├── app.js             # JavaScript application logic
├── server.py          # Python HTTP server
└── README.md          # This file
```

### **Customization**

#### **Styling**
Edit `styles.css` to customize the appearance:
- Colors and gradients
- Layout and spacing
- Typography
- Responsive breakpoints

#### **Functionality**
Edit `app.js` to add features:
- New message types
- Additional call controls
- Custom animations
- Enhanced error handling

#### **Server Configuration**
Edit `server.py` to change:
- Default port (currently 8080)
- CORS settings
- File serving options

### **Adding New Features**

#### **New Message Types**
```javascript
// In app.js, add to handleMessage()
case 'new_message_type':
    this.handleNewMessageType(data);
    break;
```

#### **New UI Components**
```html
<!-- In index.html -->
<div class="new-component">
    <!-- Your new component -->
</div>
```

```css
/* In styles.css */
.new-component {
    /* Your styling */
}
```

## 🔧 Technical Details

### **WebSocket Communication**
The web client communicates with the C++ server using WebSocket messages:

```javascript
// Example message format
{
    "type": "chat",
    "text": "Hello, world!"
}
```

### **Message Types**
- `join` - Join a channel
- `chat` - Send a message
- `call_request` - Start a voice call
- `call_accept` - Accept a call
- `call_reject` - Reject a call
- `call_end` - End a call

### **Browser Compatibility**
- ✅ Chrome 60+
- ✅ Firefox 55+
- ✅ Safari 12+
- ✅ Edge 79+
- ✅ Mobile browsers

## 🎨 Design System

### **Color Palette**
- **Primary**: `#667eea` (Blue gradient)
- **Secondary**: `#764ba2` (Purple gradient)
- **Success**: `#48bb78` (Green)
- **Warning**: `#ed8936` (Orange)
- **Error**: `#f56565` (Red)
- **Text**: `#2d3748` (Dark gray)
- **Background**: `#f7fafc` (Light gray)

### **Typography**
- **Font Family**: System fonts (San Francisco, Segoe UI, etc.)
- **Headings**: 600-700 weight
- **Body**: 400-500 weight
- **Small text**: 12-14px

### **Spacing**
- **Small**: 8px
- **Medium**: 16px
- **Large**: 24px
- **Extra Large**: 32px

## 🚀 Future Enhancements

### **Planned Features**
- [ ] User avatars and profiles
- [ ] Message reactions (👍, ❤️, 😂)
- [ ] File attachments
- [ ] Message editing and deletion
- [ ] Dark mode theme
- [ ] Push notifications
- [ ] Offline message storage
- [ ] Voice messages
- [ ] Video calls
- [ ] Screen sharing

### **Technical Improvements**
- [ ] WebRTC integration for real voice calls
- [ ] Service Worker for offline support
- [ ] Progressive Web App (PWA) features
- [ ] End-to-end encryption
- [ ] Message search functionality
- [ ] User preferences and settings

## 🐛 Troubleshooting

### **Common Issues**

#### **Can't Connect to Server**
- Make sure the C++ server is running on `ws://localhost:9001`
- Check if the server URL is correct in the web interface
- Verify no firewall is blocking the connection

#### **Messages Not Appearing**
- Check browser console for JavaScript errors
- Verify WebSocket connection is established
- Make sure you're in a channel

#### **Call Features Not Working**
- Voice calls are currently simulated (WebRTC integration pending)
- Check that the server supports call signaling
- Verify call-related messages are being sent/received

#### **Mobile Issues**
- Ensure responsive design is working
- Check touch events are properly handled
- Verify viewport meta tag is present

### **Debug Mode**
Open browser developer tools (F12) to see:
- WebSocket connection status
- Message logs
- JavaScript errors
- Network requests

## 📱 Mobile Experience

The web client is fully responsive and works great on mobile devices:

- **Touch-friendly buttons** and controls
- **Responsive layout** that adapts to screen size
- **Optimized scrolling** for message history
- **Mobile-optimized call interface**

## 🤝 Contributing

To contribute to the web client:

1. **Fork the repository**
2. **Create a feature branch**
3. **Make your changes**
4. **Test thoroughly**
5. **Submit a pull request**

### **Development Guidelines**
- Follow the existing code style
- Add comments for complex logic
- Test on multiple browsers
- Ensure mobile compatibility
- Update documentation as needed

## 📄 License

This web client is part of the serverless communication platform and follows the same license terms.

---

**Happy chatting! 🎉**

For more information about the serverless communication platform, see the main project README. 