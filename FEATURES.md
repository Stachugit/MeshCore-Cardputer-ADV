# MeshCore-Cardputer Feature List

## ✅ Implemented Features

### Chat Interface
- [x] Chat bubble UI (WhatsApp-style)
- [x] Incoming messages: outline bubbles
- [x] Outgoing messages: filled bubbles
- [x] Channel message sender names above bubbles
- [x] Dynamic text sizing (size 2 for ≤48 chars, size 1 for longer)
- [x] Multi-line message wrapping
- [x] Rounded corners on bubbles
- [x] Message scrolling (FN+; for older, FN+. for newer)
- [x] 150-character message limit with counter
- [x] Emoji filtering for display stability

### Notifications
- [x] Full-screen notification popup
- [x] Sender name display
- [x] Message preview (4 lines max)
- [x] Auto-dismiss after 3 seconds
- [x] "Press any key" hint
- [x] Any key dismisses notification

### Navigation
- [x] Contact list with 3 visible items
- [x] Channel list with 3 visible items
- [x] Settings menu access (hamburger icon ☰)
- [x] Tab switching (Contacts ↔ Channels)
- [x] Keyboard navigation (;/./, arrows)
- [x] Contact/channel selection
- [x] Back navigation (FN+`, OPT)
- [x] Empty list handling

### Search & Filtering
- [x] Real-time contact search
- [x] Real-time channel search
- [x] Case-insensitive filtering
- [x] Type-to-filter in lists
- [x] Backspace to delete characters
- [x] Hold backspace (1.5s) to clear
- [x] Search indicator at bottom
- [x] Filtered item navigation

### Settings Menu
- [x] Hamburger menu icon (☰)
- [x] Settings screen with 3 items
- [x] Brightness control (0-255, ~6% steps)
- [x] Main color selection (18 options)
- [x] Secondary color selection (18 options)
- [x] Save/Back buttons at bottom
- [x] Persistent settings (Preferences API)
- [x] Isolated storage (doesn't affect mesh data)
- [x] Settings icon shows when list empty

### Color Themes
- [x] 18 color options:
  - White, Black, Red, Green, Blue
  - Yellow, Cyan, Magenta, Orange, Pink
  - Purple, Brown, Gray, Light Blue, Light Green
  - Dark Blue, Dark Green, Dark Red
- [x] Main color (foreground/text)
- [x] Secondary color (background)
- [x] Dynamic UI color updates
- [x] Settings icon adapts to theme
- [x] Chat bubbles respect colors
- [x] Brightness control

### Display Management
- [x] Auto-off after 5 minutes
- [x] Wake on key press
- [x] Smart refresh (only when needed)
- [x] Frame-based rendering
- [x] Text filtering (emoji removal)
- [x] Proper text truncation
- [x] Header with MeshCore title
- [x] BLE PIN display

### Input Handling
- [x] Full QWERTY keyboard support
- [x] Space bar support
- [x] Backspace with hold-to-clear
- [x] Enter to send message
- [x] FN key combinations
- [x] OPT button shortcuts
- [x] Input mode toggle
- [x] Character counter

### Message Management
- [x] 50-message chat history buffer
- [x] Per-conversation filtering
- [x] Outgoing/incoming message tracking
- [x] Contact/channel association
- [x] Timestamp tracking
- [x] Circular buffer (FIFO when full)
- [x] Message persistence in RAM

### Channel Features
- [x] Channel list display
- [x] Channel selection
- [x] Channel message history
- [x] Sender name extraction (from "Name: Message")
- [x] Unread indicator (yellow dot)
- [x] Channel filtering/search
- [x] Channel-specific chat view

### Contact Features
- [x] Contact list display
- [x] Contact selection
- [x] Contact message history
- [x] Contact filtering/search
- [x] Direct message chat view
- [x] Name filtering for display

## 🚧 Potential Enhancements (Not Implemented)

### UI Improvements
- [ ] Message timestamps in chat
- [ ] Read receipts/delivery status
- [ ] Typing indicators
- [ ] Group/channel avatars
- [ ] Contact avatars
- [ ] Message reactions
- [ ] Threaded conversations
- [ ] Message editing
- [ ] Message deletion
- [ ] Copy/paste support

### Settings Expansion
- [ ] Font size adjustment
- [ ] Auto-off timeout setting
- [ ] Notification duration setting
- [ ] Sound/vibration settings
- [ ] Language selection
- [ ] Custom color RGB picker
- [ ] Display orientation
- [ ] Screen saver options

### Advanced Features
- [ ] Message drafts
- [ ] Multi-select messages
- [ ] Forward messages
- [ ] Search in messages
- [ ] Message export
- [ ] Screenshot/image support
- [ ] File attachments
- [ ] Voice messages
- [ ] Location sharing

### Network Features
- [ ] Network status indicator
- [ ] Signal strength display
- [ ] Mesh topology view
- [ ] Node list/map
- [ ] Routing statistics
- [ ] Network diagnostics

### Performance
- [ ] Message pagination (load on scroll)
- [ ] Lazy loading for large lists
- [ ] Database for message persistence
- [ ] Flash storage for chat history
- [ ] Image/icon caching

## 🐛 Known Limitations

1. **Message History**: Limited to 50 messages in RAM (not persistent across restarts)
2. **Emoji Support**: Filtered out for display stability
3. **Long Names**: Truncated to 18 characters in lists
4. **Scroll**: Manual scrolling only (no inertia/momentum)
5. **Colors**: Pre-defined palette only (no custom RGB)
6. **Storage**: Settings only (no message persistence)

## 📊 Statistics

- **Total Lines of Code**: ~1700 (UITask.cpp)
- **Color Options**: 18
- **Max Message Length**: 150 characters
- **Chat History**: 50 messages
- **Visible Contacts**: 3 at a time
- **Visible Channels**: 3 at a time
- **Screen Resolution**: 240x135
- **Display Type**: TFT RGB565

---

Last updated: January 6, 2026
