#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#include <M5Cardputer.h>

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS 300000  // 5 minutes
#endif



extern MyMesh the_mesh;

UITask::UITask(mesh::MainBoard* board, BaseSerialInterface* serial_interface)
    : AbstractUITask(board, serial_interface), _display(nullptr),
      _menu_state(MenuScreen::CONTACTS), _next_refresh(0), _auto_off(0),
      _need_refresh(false), _alert_expiry(0), _input_length(0), _input_mode(false),
      _scroll_pos(0), _selected_idx(0), _chat_is_channel(false),
      _chat_history_count(0), _chat_scroll(0), _notification_expiry(0), _has_notification(false),
      _chat_msg_scroll_index(0), _search_filter_length(0), _backspace_hold_start(0), _backspace_was_held(false),
      _settings_selected(false), _settings_menu_idx(0), _settings_item_idx(0),
      _brightness(128), _main_color_idx(0), _secondary_color_idx(1) {
    _alert[0] = '\0';
    _input_buffer[0] = '\0';
    _search_filter[0] = '\0';
    _notification_from[0] = '\0';
    _notification_text[0] = '\0';
    _last_read_channel[0] = '\0';
    memset(&_chat_contact, 0, sizeof(_chat_contact));
    memset(&_chat_channel, 0, sizeof(_chat_channel));
    memset(_chat_history, 0, sizeof(_chat_history));
    memset(_channel_has_unread, 0, sizeof(_channel_has_unread));
}

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
    _display = display;
    _sensors = sensors;
    _node_prefs = node_prefs;
    
    if (_display) {
        _display->turnOn();
    }
    
    // Load settings from Preferences (safe, doesn't affect mesh memory)
    loadSettings();
    
    _menu_state = MenuScreen::CONTACTS;
    _scroll_pos = 0;
    _selected_idx = 0;
    _auto_off = millis() + AUTO_OFF_MILLIS;
    _need_refresh = true;
}

void UITask::loop() {
    M5Cardputer.update();
    
    // Handle keyboard input
    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            
            // If notification is active, any key dismisses it
            if (_has_notification) {
                _has_notification = false;
                _need_refresh = true;
            } else {
                handleKeyPress(status);
            }
            _need_refresh = true;
            
            // Reset auto-off timer
            _auto_off = millis() + AUTO_OFF_MILLIS;
            if (_display && !_display->isOn()) {
                _display->turnOn();
            }
        }
    }
    
    // Check for backspace hold (continuous checking while key is pressed)
    if (_backspace_hold_start > 0) {
        if (M5Cardputer.Keyboard.isChange() && !M5Cardputer.Keyboard.isPressed()) {
            // Key released - reset timer
            _backspace_hold_start = 0;
            _backspace_was_held = false;
        } else if (millis() - _backspace_hold_start > 1500 && !_backspace_was_held) {
            // Held for 500ms - clear buffer/filter
            _backspace_was_held = true;
            if (_menu_state == MenuScreen::CHAT && _input_mode) {
                _input_buffer[0] = '\0';
                _input_length = 0;
            } else if (_menu_state == MenuScreen::CONTACTS || _menu_state == MenuScreen::CHANNELS) {
                _search_filter[0] = '\0';
                _search_filter_length = 0;
                _scroll_pos = 0;
                _selected_idx = 0;
            }
            _need_refresh = true;
        }
    }
    
    // Refresh display only when needed
    if (_display && _display->isOn() && _need_refresh) {
        _need_refresh = false;
        
        // Black background
        _display->startFrame();
        _display->setColor(DisplayDriver::DARK);
        _display->fillRect(0, 0, _display->width(), _display->height());
        
        switch (_menu_state) {
            case MenuScreen::CONTACTS:
                renderContactList();
                break;
                
            case MenuScreen::CHANNELS:
                renderChannelList();
                break;
                
            case MenuScreen::CHAT:
                renderChatScreen();
                break;
                
            case MenuScreen::SETTINGS:
                renderSettingsMenu();
                break;
        }
        
        // Don't show bottom bar in Settings (it has its own)
        if (_menu_state != MenuScreen::SETTINGS) {
            renderBottomBar();
        }
        
        // Show notification popup if active
        if (_has_notification && millis() < _notification_expiry) {
            renderNotification();
        } else if (_has_notification && millis() >= _notification_expiry) {
            _has_notification = false;
        }
        
        _display->endFrame();
    }
    
    // Auto-off display
    if (_display && _display->isOn() && millis() > _auto_off) {
        _display->turnOff();
    }
}

void UITask::renderContactList() {
    // Header bar (0, 0, 240, 28)
    _display->setColor(DisplayDriver::LIGHT);
    _display->drawRect(0, 0, 240, 28);
    
    // Settings icon box (left)
    _display->drawRect(0, 0, 30, 28);
    
    // Draw hamburger menu icon (3 horizontal lines)
    uint16_t main_color = COLORS[_main_color_idx].rgb565;
    uint16_t secondary_color = COLORS[_secondary_color_idx].rgb565;
    
    if (_settings_selected) {
        // Selected: fill background with main color
        M5Cardputer.Display.fillRect(0, 0, 30, 28, main_color);
        // Draw 3 lines in secondary color
        M5Cardputer.Display.fillRect(6, 7, 18, 3, secondary_color);
        M5Cardputer.Display.fillRect(6, 13, 18, 3, secondary_color);
        M5Cardputer.Display.fillRect(6, 19, 18, 3, secondary_color);
    } else {
        // Normal: draw 3 lines in main color
        M5Cardputer.Display.fillRect(6, 7, 18, 3, main_color);
        M5Cardputer.Display.fillRect(6, 13, 18, 3, main_color);
        M5Cardputer.Display.fillRect(6, 19, 18, 3, main_color);
    }
    
    // MeshCore title (center)
    _display->setTextSize(2);
    _display->setCursor(73, 7);
    _display->print("MeshCore");
    
    // BLE PIN (right)
    uint32_t ble_pin = the_mesh.getBLEPin();
    if (ble_pin != 0 && ble_pin != 123456) {
        _display->setTextSize(1);
        char pin[16];
        sprintf(pin, "%lu", ble_pin);
        _display->setCursor(189, 11);
        _display->print(pin);
    }
    
    int num_contacts = the_mesh.getNumContacts();
    
    // Filter contacts by search term
    int filtered_indices[64];
    int filtered_count = 0;
    
    if (_search_filter_length > 0) {
        for (int i = 0; i < num_contacts; i++) {
            ContactInfo contact;
            if (the_mesh.getContactByIdx(i, contact)) {
                // Case-insensitive search
                char lower_name[32];
                char lower_filter[32];
                for (int j = 0; j < 32 && contact.name[j]; j++) {
                    lower_name[j] = tolower(contact.name[j]);
                    lower_name[j+1] = '\0';
                }
                for (int j = 0; j < 32 && _search_filter[j]; j++) {
                    lower_filter[j] = tolower(_search_filter[j]);
                    lower_filter[j+1] = '\0';
                }
                if (strstr(lower_name, lower_filter) != nullptr) {
                    filtered_indices[filtered_count++] = i;
                }
            }
        }
        num_contacts = filtered_count;
    } else {
        // No filter - show all
        for (int i = 0; i < num_contacts; i++) {
            filtered_indices[i] = i;
        }
        filtered_count = num_contacts;
    }
    
    if (num_contacts == 0) {
        _display->setTextSize(2);
        _display->setColor(DisplayDriver::LIGHT);
        const char* msg = "No contacts";
        int msg_width = _display->getTextWidth(msg);
        int msg_x = (240 - msg_width) / 2;
        _display->setCursor(msg_x, 60);
        _display->print(msg);
    } else {
        // Render 3 contact items (y: 27, 54, 81)
        int y_positions[3] = {27, 54, 81};
        
        for (int i = 0; i < 3; i++) {
            int contact_idx = _scroll_pos + i;
            if (contact_idx >= num_contacts) break;
            
            int real_idx = filtered_indices[contact_idx];
            ContactInfo contact;
            if (the_mesh.getContactByIdx(real_idx, contact)) {
                int y = y_positions[i];
                
                // Draw border
                _display->setColor(DisplayDriver::LIGHT);
                _display->drawRect(0, y, 240, 28);
                
                // Fill white if selected
                if (contact_idx == _selected_idx) {
                    _display->fillRect(0, y, 240, 28);
                    _display->setColor(DisplayDriver::DARK);  // Black text
                    // Arrow indicator
                    _display->setCursor(2, y + 7);
                    _display->setTextSize(2);
                    _display->print(">");
                } else {
                    _display->setColor(DisplayDriver::LIGHT);  // White text
                }
                
                _display->setTextSize(2);
                _display->setCursor(16, y + 6);
                
                // Filter and truncate long names - max 18 chars to prevent wrapping
                char filtered_name[32];
                filterDisplayText(contact.name, filtered_name, sizeof(filtered_name));
                char display_name[19];
                strncpy(display_name, filtered_name, 18);
                display_name[18] = '\0';
                _display->print(display_name);
            }
        }
    }
}

void UITask::renderChannelList() {
    // Header bar (0, 0, 240, 28)
    _display->setColor(DisplayDriver::LIGHT);
    _display->drawRect(0, 0, 240, 28);
    
    // Settings icon box (left)
    _display->drawRect(0, 0, 30, 28);
    
    // Draw hamburger menu icon (3 horizontal lines)
    uint16_t main_color = COLORS[_main_color_idx].rgb565;
    uint16_t secondary_color = COLORS[_secondary_color_idx].rgb565;
    
    if (_settings_selected) {
        // Selected: fill background with main color
        M5Cardputer.Display.fillRect(0, 0, 30, 28, main_color);
        // Draw 3 lines in secondary color
        M5Cardputer.Display.fillRect(6, 7, 18, 3, secondary_color);
        M5Cardputer.Display.fillRect(6, 13, 18, 3, secondary_color);
        M5Cardputer.Display.fillRect(6, 19, 18, 3, secondary_color);
    } else {
        // Normal: draw 3 lines in main color
        M5Cardputer.Display.fillRect(6, 7, 18, 3, main_color);
        M5Cardputer.Display.fillRect(6, 13, 18, 3, main_color);
        M5Cardputer.Display.fillRect(6, 19, 18, 3, main_color);
    }
    _display->setTextSize(2);
    _display->setCursor(73, 7);
    _display->print("MeshCore");
    
    // BLE PIN (right)
    uint32_t ble_pin = the_mesh.getBLEPin();
    if (ble_pin != 0 && ble_pin != 123456) {
        _display->setTextSize(1);
        char pin[16];
        sprintf(pin, "%lu", ble_pin);
        _display->setCursor(189, 11);
        _display->print(pin);
    }
    
    // Count and collect channels
    int num_channels = 0;
    ChannelDetails channels[MAX_GROUP_CHANNELS];
    int channel_mesh_idx[MAX_GROUP_CHANNELS]; // Track original mesh indices
    
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        if (the_mesh.getChannel(i, channels[num_channels]) && channels[num_channels].name[0] != '\0') {
            channel_mesh_idx[num_channels] = i;
            num_channels++;
        }
    }
    
    // Filter channels by search term
    int filtered_indices[MAX_GROUP_CHANNELS];
    int filtered_count = 0;
    
    if (_search_filter_length > 0) {
        for (int i = 0; i < num_channels; i++) {
            // Case-insensitive search
            char lower_name[32];
            char lower_filter[32];
            for (int j = 0; j < 32 && channels[i].name[j]; j++) {
                lower_name[j] = tolower(channels[i].name[j]);
                lower_name[j+1] = '\0';
            }
            for (int j = 0; j < 32 && _search_filter[j]; j++) {
                lower_filter[j] = tolower(_search_filter[j]);
                lower_filter[j+1] = '\0';
            }
            if (strstr(lower_name, lower_filter) != nullptr) {
                filtered_indices[filtered_count++] = i;
            }
        }
        num_channels = filtered_count;
    } else {
        // No filter - show all
        for (int i = 0; i < num_channels; i++) {
            filtered_indices[i] = i;
        }
        filtered_count = num_channels;
    }
    
    if (num_channels == 0) {
        _display->setTextSize(2);
        _display->setColor(DisplayDriver::LIGHT);
        const char* msg = "No channels";
        int msg_width = _display->getTextWidth(msg);
        int msg_x = (240 - msg_width) / 2;
        _display->setCursor(msg_x, 60);
        _display->print(msg);
    } else {
        // Render 3 channel items (y: 27, 54, 81)
        int y_positions[3] = {27, 54, 81};
        int visible_count = min(3, num_channels);
        
        for (int i = 0; i < visible_count; i++) {
            int channel_idx = _scroll_pos + i;
            if (channel_idx >= num_channels) break;
            
            int real_idx = filtered_indices[channel_idx];
            ChannelDetails& channel = channels[real_idx];
            int y = y_positions[i];
            
            // Draw border
            _display->setColor(DisplayDriver::LIGHT);
            _display->drawRect(0, y, 240, 28);
            
            // Fill white if selected
            if (channel_idx == _selected_idx) {
                _display->fillRect(0, y, 240, 28);
                _display->setColor(DisplayDriver::DARK);  // Black text
                // Arrow indicator
                _display->setCursor(2, y + 7);
                _display->setTextSize(2);
                //_display->print("#");
            } else {
                _display->setColor(DisplayDriver::LIGHT);  // White text
            }
            
            _display->setTextSize(2);
            _display->setCursor(16, y + 6);
            
            // Filter and truncate long names - max 18 chars to prevent wrapping
            char filtered_name[32];
            filterDisplayText(channel.name, filtered_name, sizeof(filtered_name));
            char display_name[19];
            strncpy(display_name, filtered_name, 18);
            display_name[18] = '\0';
            _display->print(display_name);
            
            // Show unread indicator (dot) if channel has unread messages
            // Use the original mesh index for this channel
            int original_mesh_idx = channel_mesh_idx[real_idx];
            if (_channel_has_unread[original_mesh_idx]) {
                // Draw yellow dot indicator
                _display->setColor(DisplayDriver::YELLOW);
                _display->fillRect(220, y + 11, 6, 6);
            }
        }
    }
}

void UITask::renderChatScreen() {
    // === HEADER BAR === (0, 0, 240, 28)
    _display->setColor(DisplayDriver::LIGHT);
    _display->drawRect(0, 0, 240, 28);
    
    // Back arrow
    _display->setTextSize(2);
    _display->setCursor(4, 7);
    _display->print("<");
    
    // Chat name - centered
    char filtered_name[32];
    char full_name[15];
    if (_chat_is_channel) {
        filterDisplayText(_chat_channel.name, filtered_name, sizeof(filtered_name));
        char name[13];
        strncpy(name, filtered_name, 12);
        name[12] = '\0';
        snprintf(full_name, 15, "#%s", name);
    } else {
        filterDisplayText(_chat_contact.name, filtered_name, sizeof(filtered_name));
        char name[13];
        strncpy(name, filtered_name, 12);
        name[12] = '\0';
        snprintf(full_name, 15, "@%s", name);
    }
    
    int name_width = _display->getTextWidth(full_name);
    int center_x = (240 - name_width) / 2;
    _display->setCursor(center_x, 7);
    _display->print(full_name);
    
    // === SCROLLABLE MESSAGE AREA === (y=30 to y=106)
    // This area must be fully above the input bar
    int msg_area_top = 30;
    int msg_area_bottom = 106;
    int msg_area_height = msg_area_bottom - msg_area_top;
    
    if (_chat_history_count == 0) {
        // No messages - just show empty space
    } else {
        // Get current contact/channel name for filtering
        const char* current_name = _chat_is_channel ? _chat_channel.name : _chat_contact.name;
        
        // First, collect messages for this conversation
        int filtered_indices[MAX_CHAT_MESSAGES];
        int filtered_count = 0;
        for (int i = _chat_history_count - 1; i >= 0; i--) {
            ChatMessage& msg = _chat_history[i];
            if (msg.is_channel == _chat_is_channel && strcmp(msg.contact_or_channel, current_name) == 0) {
                filtered_indices[filtered_count++] = i;
            }
        }
        
        if (filtered_count == 0) {
            _display->setTextSize(1);
            _display->setColor(DisplayDriver::LIGHT);
            _display->setCursor(80, 60);
            _display->print("No messages");
        } else {
            // Clamp scroll index
            if (_chat_msg_scroll_index >= filtered_count) _chat_msg_scroll_index = filtered_count - 1;
            if (_chat_msg_scroll_index < 0) _chat_msg_scroll_index = 0;
            
            // Try to fit messages starting from scroll index
            // scroll_index=0 means show newest messages, scroll_index=N means skip N newest
            int messages_to_show[10]; // Max 10 messages on screen
            int show_count = 0;
            int available_height = msg_area_height; // Full 76px available
            
            // Start from scroll_index and go toward older messages (higher indices)
            for (int idx = _chat_msg_scroll_index; idx < filtered_count && show_count < 10; idx++) {
                int hist_idx = filtered_indices[idx];
                ChatMessage& msg = _chat_history[hist_idx];
                
                // Calculate message height
                char filtered_text[128];
                filterDisplayText(msg.text, filtered_text, sizeof(filtered_text));
                
                // Extract sender name for channel messages
                const char* message_text = filtered_text;
                bool has_name = false;
                char sender_name[32] = "";
                
                if (_chat_is_channel && !msg.is_outgoing) {
                    char* colon_pos = strchr(filtered_text, ':');
                    if (colon_pos != nullptr && (colon_pos - filtered_text) < 30) {
                        int name_len = colon_pos - filtered_text;
                        strncpy(sender_name, filtered_text, name_len);
                        sender_name[name_len] = '\0';
                        message_text = colon_pos + 1;
                        while (*message_text == ' ') message_text++;
                        has_name = true;
                    }
                }
                
                int text_len = strlen(message_text);
                int text_size = (text_len <= 48) ? 2 : 1;
                int chars_per_line = (text_size == 2) ? 16 : 35;
                int line_height = (text_size == 2) ? 16 : 9;
                int padding = (text_size == 2) ? 20 : 12;
                int num_lines = (text_len + chars_per_line - 1) / chars_per_line;
                int bubble_height = (num_lines * line_height) + padding;
                int name_height = has_name ? 10 : 0;
                int total_height = bubble_height + name_height + 2; // +2 for gap
                
                // Check if this message fits
                if (available_height - total_height < 0 && show_count > 0) {
                    // Doesn't fit, stop here (but allow first message even if too big)
                    break;
                }
                
                messages_to_show[show_count++] = hist_idx;
                available_height -= total_height;
            }
            
            // Now render the selected messages from bottom up
            int y = msg_area_bottom - 2;
            
            for (int i = 0; i < show_count; i++) {
                ChatMessage& msg = _chat_history[messages_to_show[i]];
                
                // Filter emojis and non-ASCII characters for display
                char filtered_text[128];
                filterDisplayText(msg.text, filtered_text, sizeof(filtered_text));
            
                // For INCOMING channel messages, extract sender name and message text
                char sender_name[32] = "";
                const char* message_text = filtered_text; // By default, use full text
                
                if (_chat_is_channel && !msg.is_outgoing) {
                    // Look for "Name: Message" pattern
                    char* colon_pos = strchr(filtered_text, ':');
                    if (colon_pos != nullptr && (colon_pos - filtered_text) < 30) {
                        // Extract sender name (before colon)
                        int name_len = colon_pos - filtered_text;
                        strncpy(sender_name, filtered_text, name_len);
                        sender_name[name_len] = '\0';
                        
                        // Point to message text (after colon and space)
                        message_text = colon_pos + 1;
                        while (*message_text == ' ') message_text++; // Skip spaces
                    }
                }
                
                // Calculate bubble dimensions with dynamic text sizing
                int text_len = strlen(message_text);
                
                // Use size 2 for short messages (<=48 chars), size 1 for longer ones
                int text_size;
                int chars_per_line;
                int line_height;
                int padding;
                
                if (text_len <= 48) {
                    text_size = 2;
                    chars_per_line = 16;
                    line_height = 16;
                    padding = 20; // 10px top + 10px bottom
                } else {
                    text_size = 1;
                    chars_per_line = 35;
                    line_height = 9;
                    padding = 12; // 6px top + 6px bottom
                }
                
                int num_lines = (text_len + chars_per_line - 1) / chars_per_line;
                int bubble_height = (num_lines * line_height) + padding;
                int bubble_width = 220;
                int bubble_x = 10;
                
                // Calculate starting position (from bottom up)
                int bubble_y = y - bubble_height;
                
                // For channel messages, add space for sender name above bubble
                int name_y = 0;
                bool has_name = (sender_name[0] != '\0');
                
                if (has_name) {
                    name_y = bubble_y - 10; // Name is 10px above bubble (9px text + 1px gap)
                }
                
                // Draw sender name above bubble if it exists (for channel messages)
                if (has_name) {
                    _display->setTextSize(1);
                    _display->setColor(DisplayDriver::LIGHT);
                    _display->setCursor(bubble_x, name_y);
                    _display->print(sender_name);
                }
                
                if (msg.is_outgoing) {
                    // === OUTGOING - WHITE FILLED ROUNDED BUBBLE ===
                    _display->setColor(DisplayDriver::LIGHT);
                    _display->fillRect(bubble_x, bubble_y, bubble_width, bubble_height);
                    
                    // Draw rounded corners with better effect
                    _display->drawRect(bubble_x, bubble_y, bubble_width, bubble_height);
                    _display->drawRect(bubble_x + 1, bubble_y + 1, bubble_width - 2, bubble_height - 2);
                    _display->drawRect(bubble_x + 2, bubble_y + 2, bubble_width - 4, bubble_height - 4);
                    
                    // Clear corner pixels for rounded effect
                    _display->setColor(DisplayDriver::DARK);
                    _display->fillRect(bubble_x, bubble_y, 3, 3); // Top-left
                    _display->fillRect(bubble_x + bubble_width - 3, bubble_y, 3, 3); // Top-right
                    _display->fillRect(bubble_x, bubble_y + bubble_height - 3, 3, 3); // Bottom-left
                    _display->fillRect(bubble_x + bubble_width - 3, bubble_y + bubble_height - 3, 3, 3); // Bottom-right
                    
                    // Black text with wrapping (dynamic size)
                    _display->setColor(DisplayDriver::DARK);
                    _display->setTextSize(text_size);
                    
                    int text_x = bubble_x + (text_size == 2 ? 10 : 6);
                    int text_y = bubble_y + (text_size == 2 ? 10 : 6);
                    
                    // Draw text with wrapping
                    for (int line = 0; line < num_lines; line++) {
                        int start = line * chars_per_line;
                        int len = min(chars_per_line, text_len - start);
                        if (len <= 0) break;
                        
                        char line_text[40];
                        strncpy(line_text, message_text + start, len);
                        line_text[len] = '\0';
                        
                        _display->setCursor(text_x, text_y + (line * line_height));
                        _display->print(line_text);
                    }
                } else {
                    // === INCOMING - WHITE OUTLINE ROUNDED BUBBLE ===
                    _display->setColor(DisplayDriver::LIGHT);
                    
                    // Draw rounded corners with better effect
                    _display->drawRect(bubble_x, bubble_y, bubble_width, bubble_height);
                    _display->drawRect(bubble_x + 1, bubble_y + 1, bubble_width - 2, bubble_height - 2);
                    _display->drawRect(bubble_x + 2, bubble_y + 2, bubble_width - 4, bubble_height - 4);
                    
                    // Clear corner pixels for rounded effect
                    _display->setColor(DisplayDriver::DARK);
                    _display->fillRect(bubble_x, bubble_y, 3, 3); // Top-left
                    _display->fillRect(bubble_x + bubble_width - 3, bubble_y, 3, 3); // Top-right
                    _display->fillRect(bubble_x, bubble_y + bubble_height - 3, 3, 3); // Bottom-left
                    _display->fillRect(bubble_x + bubble_width - 3, bubble_y + bubble_height - 3, 3, 3); // Bottom-right
                    
                    // White text with wrapping (dynamic size)
                    _display->setColor(DisplayDriver::LIGHT);
                    _display->setTextSize(text_size);
                    
                    int text_x = bubble_x + (text_size == 2 ? 10 : 6);
                    int text_y = bubble_y + (text_size == 2 ? 10 : 6);
                    
                    // Draw text with wrapping
                    for (int line = 0; line < num_lines; line++) {
                        int start = line * chars_per_line;
                        int len = min(chars_per_line, text_len - start);
                        if (len <= 0) break;
                        
                        char line_text[40];
                        strncpy(line_text, message_text + start, len);
                        line_text[len] = '\0';
                        
                        _display->setCursor(text_x, text_y + (line * line_height));
                        _display->print(line_text);
                    }
                }
                
                // Move up for next bubble (with 2px gap)
                if (has_name) {
                    y = name_y - 2;
                } else {
                    y = bubble_y - 2;
                }
            } // End of for loop iterating through messages_to_show
        }
    }
    
    // === FIXED INPUT BAR AT BOTTOM === (107-135)
    // This is ALWAYS at the bottom, messages never overlap it
    _display->setColor(DisplayDriver::LIGHT);
    _display->drawRect(0, 107, 240, 28);
    
    _display->setTextSize(2);
    _display->setColor(DisplayDriver::LIGHT);
    
    if (_input_mode) {
        // Show input text (max 150 chars enforced)
        _display->setCursor(6, 112);
        
        // Show last 19 chars that fit on screen
        char display_buf[20];
        int visible_chars = min(19, _input_length);
        int start = max(0, _input_length - 19);
        strncpy(display_buf, _input_buffer + start, visible_chars);
        display_buf[visible_chars] = '\0';
        
        _display->print(display_buf);
        
        // Blinking cursor
        if ((millis() / 350) % 2 == 0) {
            int cursor_x = 6 + _display->getTextWidth(display_buf);
            if (cursor_x < 190) { // Leave space for counter
                _display->fillRect(cursor_x, 112, 3, 14);
            }
        }
        
        // Character counter in bottom-right corner (small text)
        _display->setTextSize(1);
        char counter[12];
        snprintf(counter, 12, "%d/150", _input_length);
        int counter_width = _display->getTextWidth(counter);
        _display->setCursor(234 - counter_width, 122); // Right-aligned, lower position
        _display->print(counter);
        _display->setTextSize(2); // Restore size
    } else {
        // Placeholder
        _display->setCursor(6, 112);
        _display->print("_");
    }
}

void UITask::renderBottomBar() {
    int bar_y = 108;
    
    // Only show tabs in list views (but not when searching)
    if (_menu_state == MenuScreen::CHAT) {
        // Show back hint
        _display->setTextSize(1);
        _display->setColor(DisplayDriver::YELLOW);
        _display->setCursor(4, bar_y + 4);
        return;
    }
    
    // When searching, show input bar instead of tabs
    if (_search_filter_length > 0) {
        _display->setColor(DisplayDriver::LIGHT);
        _display->drawRect(0, 107, 240, 28);
        
        _display->setTextSize(2);
        _display->setCursor(4, 112);
        
        // Display search filter text
        char filtered_text[32];
        filterDisplayText(_search_filter, filtered_text, sizeof(filtered_text));
        _display->print(filtered_text);
        
        // Cursor
        int cursor_x = 4 + (_search_filter_length * 12);
        if (cursor_x < 235) {
            _display->fillRect(cursor_x, 112, 3, 14);
        }
        return;
    }
    
    // Draw tab bar border (0, 108, 240, 27)
    _display->setColor(DisplayDriver::LIGHT);
    _display->drawRect(0, bar_y, 240, 27);
    
    _display->setTextSize(2);
    
    // Contacts tab (left half: 0-120)
    if (_menu_state == MenuScreen::CONTACTS) {
        // Active - white fill, black text
        _display->setColor(DisplayDriver::LIGHT);
        _display->fillRect(0, bar_y, 120, 27);
        _display->setColor(DisplayDriver::DARK);
        _display->setCursor(13, bar_y + 7);
        _display->print("Contacts");
    } else {
        // Inactive - white text only
        _display->setColor(DisplayDriver::LIGHT);
        _display->setCursor(13, bar_y + 7);
        _display->print("Contacts");
    }
    
    // Channels tab (right half: 120-240)
    if (_menu_state == MenuScreen::CHANNELS) {
        // Active - white fill, black text
        _display->setColor(DisplayDriver::LIGHT);
        _display->fillRect(120, bar_y, 120, 27);
        _display->setColor(DisplayDriver::DARK);
        _display->setCursor(133, bar_y + 7);
        _display->print("Channels");
    } else {
        // Inactive - white text only
        _display->setColor(DisplayDriver::LIGHT);
        _display->setCursor(133, bar_y + 7);
        _display->print("Channels");
    }
}

void UITask::renderSettingsMenu() {
    // Header bar (0, 0, 240, 28) - similar to main menu
    _display->setColor(DisplayDriver::LIGHT);
    _display->drawRect(0, 0, 240, 28);
    
    // Empty settings icon box (left)
    _display->drawRect(0, 0, 30, 28);
    
    // Settings title (center)
    _display->setTextSize(2);
    _display->setCursor(75, 7);
    _display->print("Settings");
    
    // No BLE PIN on right (keeping it minimal)
    
    // Settings list (middle section)
    _display->setTextSize(2);
    int y_start = 35;
    int line_height = 18;
    
    // Brightness setting
    _display->setColor(DisplayDriver::LIGHT);
    if (_settings_item_idx == 0) {
        _display->fillRect(0, y_start - 2, 240, line_height);
        _display->setColor(DisplayDriver::DARK);
    }
    _display->setCursor(10, y_start);
    _display->print("Brightness: ");
    char brightness_str[8];
    sprintf(brightness_str, "%d%%", (_brightness * 100) / 255);
    _display->print(brightness_str);
    
    // Main Color setting
    y_start += line_height + 5;
    _display->setColor(DisplayDriver::LIGHT);
    if (_settings_item_idx == 1) {
        _display->fillRect(0, y_start - 2, 240, line_height);
        _display->setColor(DisplayDriver::DARK);
    }
    _display->setCursor(10, y_start);
    _display->print("Main: ");
    _display->print(COLORS[_main_color_idx].name);
    
    // Secondary Color setting
    y_start += line_height + 5;
    _display->setColor(DisplayDriver::LIGHT);
    if (_settings_item_idx == 2) {
        _display->fillRect(0, y_start - 2, 240, line_height);
        _display->setColor(DisplayDriver::DARK);
    }
    _display->setCursor(10, y_start);
    _display->print("Secondary: ");
    _display->print(COLORS[_secondary_color_idx].name);
    
    // Bottom bar with Save/Back options (similar to Contacts/Channels)
    int bar_y = 108;
    _display->setColor(DisplayDriver::LIGHT);
    _display->drawRect(0, bar_y, 240, 27);
    
    _display->setTextSize(2);
    
    // Save tab (left half: 0-120)
    if (_settings_item_idx == -1 && _settings_menu_idx == 0) {
        // Active - white fill, black text
        _display->setColor(DisplayDriver::LIGHT);
        _display->fillRect(0, bar_y, 120, 27);
        _display->setColor(DisplayDriver::DARK);
        _display->setCursor(35, bar_y + 7);
        _display->print("Save");
    } else {
        // Inactive - white text only
        _display->setColor(DisplayDriver::LIGHT);
        _display->setCursor(35, bar_y + 7);
        _display->print("Save");
    }
    
    // Back tab (right half: 120-240)
    if (_settings_item_idx == -1 && _settings_menu_idx == 1) {
        // Active - white fill, black text
        _display->setColor(DisplayDriver::LIGHT);
        _display->fillRect(120, bar_y, 120, 27);
        _display->setColor(DisplayDriver::DARK);
        _display->setCursor(155, bar_y + 7);
        _display->print("Back");
    } else {
        // Inactive - white text only
        _display->setColor(DisplayDriver::LIGHT);
        _display->setCursor(155, bar_y + 7);
        _display->print("Back");
    }
}

void UITask::renderNotification() {
    // Full screen notification - black background, white text
    _display->setColor(DisplayDriver::DARK);
    _display->fillRect(0, 0, 240, 135); // Fill entire screen with black
    
    // Filter emojis from notification text and sender name
    char filtered_from[32];
    char filtered_text[128];
    filterDisplayText(_notification_from, filtered_from, sizeof(filtered_from));
    filterDisplayText(_notification_text, filtered_text, sizeof(filtered_text));
    
    // Show sender name (size 2, white, centered)
    _display->setColor(DisplayDriver::LIGHT);
    _display->setTextSize(2);
    int from_width = _display->getTextWidth(filtered_from);
    int from_x = (240 - from_width) / 2;
    _display->setCursor(from_x, 30);
    _display->print(filtered_from);
    
    // Show message text (size 2, white, wrapped, centered)
    _display->setTextSize(2);
    int text_len = strlen(filtered_text);
    int chars_per_line = 19; // Size 2 text fits ~19 chars per line
    int y_offset = 55; // Start below sender name
    
    // Calculate number of lines
    int num_lines = (text_len + chars_per_line - 1) / chars_per_line;
    if (num_lines > 4) num_lines = 4; // Max 4 lines
    
    // Draw text lines
    for (int line = 0; line < num_lines; line++) {
        int start = line * chars_per_line;
        int len = min(chars_per_line, text_len - start);
        if (len <= 0) break;
        
        char line_text[20];
        strncpy(line_text, filtered_text + start, len);
        line_text[len] = '\0';
        
        int line_width = _display->getTextWidth(line_text);
        int line_x = (240 - line_width) / 2;
        _display->setCursor(line_x, y_offset + (line * 18));
        _display->print(line_text);
    }
    
    // Show dismiss hint at bottom (size 1)
    _display->setTextSize(1);
    const char* hint = "Press any key";
    int hint_width = _display->getTextWidth(hint);
    int hint_x = (240 - hint_width) / 2;
    _display->setCursor(hint_x, 120);
    _display->print(hint);
}
void UITask::handleKeyPress(Keyboard_Class::KeysState& status) {
    // In chat mode with input active
    if (_menu_state == MenuScreen::CHAT && _input_mode) {
        if (status.enter) {
            sendMessage();
            _input_mode = false;
            _input_buffer[0] = '\0';
            _input_length = 0;
        } else if (status.fn) {
            // Check for FN+` (escape)
            bool has_backtick = false;
            for (auto key : status.word) {
                if (key == '`') has_backtick = true;
            }
            if (has_backtick) {
                // If input is empty, go back to list immediately
                if (_input_length == 0) {
                    _menu_state = _chat_is_channel ? MenuScreen::CHANNELS : MenuScreen::CONTACTS;
                    _input_mode = false;
                    _chat_msg_scroll_index = 0;
                    _search_filter_length = 0;
                    _search_filter[0] = '\0';
                } else {
                    // Otherwise just exit input mode
                    _input_mode = false;
                    _input_buffer[0] = '\0';
                    _input_length = 0;
                }
                return;
            }
        } else if (status.opt) {
            // OPT button also acts as escape
            // If input is empty, go back to list immediately
            if (_input_length == 0) {
                _menu_state = _chat_is_channel ? MenuScreen::CHANNELS : MenuScreen::CONTACTS;
                _input_mode = false;
                _chat_msg_scroll_index = 0;
                _search_filter_length = 0;
                _search_filter[0] = '\0';
            } else {
                // Otherwise just exit input mode
                _input_mode = false;
                _input_buffer[0] = '\0';
                _input_length = 0;
            }
        } else if (status.del) {
            // Backspace - start hold timer and delete one char
            if (_backspace_hold_start == 0) {
                _backspace_hold_start = millis();
                _backspace_was_held = false;
            }
            if (_input_length > 0) {
                _input_length--;
                _input_buffer[_input_length] = '\0';
            }
        } else {
            // Reset backspace hold timer when other keys pressed
            _backspace_hold_start = 0;
            _backspace_was_held = false;
            
            if (status.space) {
                // Enforce 150 character limit
                if (_input_length < 150) {
                    _input_buffer[_input_length++] = ' ';
                    _input_buffer[_input_length] = '\0';
                }
            } else {
                // Add characters from word vector (with 150 char limit)
                for (auto key : status.word) {
                    if (_input_length < 150) {
                        _input_buffer[_input_length++] = key;
                        _input_buffer[_input_length] = '\0';
                    }
                }
            }
        }
    }
    
    // In chat mode but not typing - start typing on any key
    if (_menu_state == MenuScreen::CHAT && !_input_mode) {
        // Check for FN combinations first
        if (status.fn) {
            // FN+` = back to list
            bool has_backtick = false;
            bool has_semicolon = false;
            bool has_period = false;
            
            for (auto key : status.word) {
                if (key == '`') has_backtick = true;
                if (key == ';') has_semicolon = true;
                if (key == '.') has_period = true;
            }
            
            if (has_backtick) {
                _menu_state = _chat_is_channel ? MenuScreen::CHANNELS : MenuScreen::CONTACTS;
                _input_mode = false;
                _input_buffer[0] = '\0';
                _input_length = 0;
                _chat_msg_scroll_index = 0;
                _search_filter_length = 0;
                _search_filter[0] = '\0';
                return;
            } else if (has_semicolon) {
                // FN+; = scroll to show older messages (move view up)
                _chat_msg_scroll_index += 1;
                if (_chat_msg_scroll_index > _chat_history_count - 1) _chat_msg_scroll_index = _chat_history_count - 1;
                _need_refresh = true;
                return;
            } else if (has_period) {
                // FN+. = scroll to show newer messages (move view down)
                _chat_msg_scroll_index -= 1;
                if (_chat_msg_scroll_index < 0) _chat_msg_scroll_index = 0;
                _need_refresh = true;
                return;
            }
        }
        
        // OPT button = back to list
        if (status.opt) {
            _menu_state = _chat_is_channel ? MenuScreen::CHANNELS : MenuScreen::CONTACTS;
            _input_mode = false;
            _input_buffer[0] = '\0';
            _input_length = 0;
            _chat_msg_scroll_index = 0;
            _search_filter_length = 0;
            _search_filter[0] = '\0';
            return;
        }
        
        // Any other key starts input mode
        if (status.word.size() > 0 || status.space) {
            _input_mode = true;
            _input_buffer[0] = '\0';
            _input_length = 0;
            
            // Process the key that started input
            if (status.space) {
                _input_buffer[_input_length++] = ' ';
                _input_buffer[_input_length] = '\0';
            } else {
                for (auto key : status.word) {
                    if (_input_length < 255) {
                        _input_buffer[_input_length++] = key;
                        _input_buffer[_input_length] = '\0';
                    }
                }
            }
        }
        return;
    }
    
    // In CONTACTS or CHANNELS menu - check for navigation first, then filter
    if (_menu_state == MenuScreen::CONTACTS || _menu_state == MenuScreen::CHANNELS) {
        // Check if this is navigation input (FN + keys or direct navigation keys)
        bool has_nav_keys = false;
        if (status.fn) {
            has_nav_keys = true; // FN pressed - this is navigation
        } else {
            // Check for direct navigation keys
            for (auto key : status.word) {
                if (key == ',' || key == '.' || key == '/' || key == ';') {
                    has_nav_keys = true;
                    break;
                }
            }
        }
        
        // If navigation keys detected, skip to navigation handling
        if (has_nav_keys) {
            // Fall through to handleNavigation
        } else if (status.del) {
            // Backspace - start hold timer and delete one char
            if (_backspace_hold_start == 0) {
                _backspace_hold_start = millis();
                _backspace_was_held = false;
            }
            if (_search_filter_length > 0) {
                _search_filter_length--;
                _search_filter[_search_filter_length] = '\0';
                _scroll_pos = 0;
                _selected_idx = 0;
            }
            return;
        } else {
            // Reset backspace hold timer when other keys pressed
            _backspace_hold_start = 0;
            _backspace_was_held = false;
            
            if (status.word.size() > 0) {
                // Add typed characters to filter (limit 30 chars)
                // Ignore navigation keys: , . / ;
                for (auto key : status.word) {
                    if (key != ',' && key != '.' && key != '/' && key != ';') {
                        if (_search_filter_length < 30) {
                            _search_filter[_search_filter_length++] = key;
                            _search_filter[_search_filter_length] = '\0';
                            _scroll_pos = 0;
                            _selected_idx = 0;
                        }
                    }
                }
                return;
            }
        }
    }
    
    // Navigation mode
    handleNavigation(status);
}

void UITask::handleNavigation(Keyboard_Class::KeysState& status) {
    bool up = false, down = false, left = false, right = false, select = false;
    
    for (auto key : status.word) {
        char c = key;
        if (c == ';') up = true;
        if (c == '.') down = true;
        if (c == ',') left = true;
        if (c == '/') right = true;
    }
    
    if (status.enter || status.space) select = true;
    
    switch (_menu_state) {
        case MenuScreen::CONTACTS: {
            int num_contacts = the_mesh.getNumContacts();
            
            // Build filtered list for navigation
            int filtered_indices[64];
            int filtered_count = 0;
            
            if (_search_filter_length > 0) {
                for (int i = 0; i < num_contacts; i++) {
                    ContactInfo contact;
                    if (the_mesh.getContactByIdx(i, contact)) {
                        char lower_name[32];
                        char lower_filter[32];
                        for (int j = 0; j < 32 && contact.name[j]; j++) {
                            lower_name[j] = tolower(contact.name[j]);
                            lower_name[j+1] = '\0';
                        }
                        for (int j = 0; j < 32 && _search_filter[j]; j++) {
                            lower_filter[j] = tolower(_search_filter[j]);
                            lower_filter[j+1] = '\0';
                        }
                        if (strstr(lower_name, lower_filter) != nullptr) {
                            filtered_indices[filtered_count++] = i;
                        }
                    }
                }
                num_contacts = filtered_count;
            } else {
                for (int i = 0; i < num_contacts; i++) {
                    filtered_indices[i] = i;
                }
                filtered_count = num_contacts;
            }
            
            if (left || right) {
                // Switch to channels, deselect settings icon
                _menu_state = MenuScreen::CHANNELS;
                _scroll_pos = 0;
                _selected_idx = 0;
                _settings_selected = false;
                _search_filter_length = 0;
                _search_filter[0] = '\0';
            } else if (up) {
                if (_settings_selected) {
                    // Already on settings icon, wrap to last contact
                    if (num_contacts > 0) {
                        _settings_selected = false;
                        _selected_idx = num_contacts - 1;
                        _scroll_pos = (_selected_idx > 2) ? _selected_idx - 2 : 0;
                    }
                    // If no contacts, stay on settings icon
                } else if (_selected_idx == 0 && num_contacts > 0) {
                    // At first contact, go to settings icon
                    _settings_selected = true;
                } else if (num_contacts > 0) {
                    // Navigate up in list
                    _selected_idx = (_selected_idx > 0) ? _selected_idx - 1 : num_contacts - 1;
                    if (_selected_idx < _scroll_pos) {
                        _scroll_pos = _selected_idx;
                    }
                } else if (num_contacts == 0 && !_settings_selected) {
                    // No contacts but not on settings - go to settings
                    _settings_selected = true;
                }
            } else if (down) {
                if (_settings_selected) {
                    // From settings icon, go to first contact
                    if (num_contacts > 0) {
                        _settings_selected = false;
                        _selected_idx = 0;
                        _scroll_pos = 0;
                    }
                    // If no contacts, stay on settings icon
                } else if (num_contacts > 0) {
                    // Navigate down in list
                    _selected_idx = (_selected_idx < num_contacts - 1) ? _selected_idx + 1 : 0;
                    if (_selected_idx >= _scroll_pos + 3) {
                        _scroll_pos = _selected_idx - 2;
                    }
                    // If wrapped to 0, deselect settings
                    if (_selected_idx == 0 && _selected_idx < _scroll_pos) {
                        _scroll_pos = 0;
                    }
                } else if (num_contacts == 0 && !_settings_selected) {
                    // No contacts but not on settings - go to settings
                    _settings_selected = true;
                }
            } else if (select) {
                if (_settings_selected) {
                    // Open settings menu
                    _menu_state = MenuScreen::SETTINGS;
                    _settings_menu_idx = 0;
                    _settings_item_idx = 0; // Start on first setting
                    _settings_selected = false;
                } else if (num_contacts > 0) {
                    // Open chat with selected contact (use filtered index)
                    int real_idx = filtered_indices[_selected_idx];
                    if (the_mesh.getContactByIdx(real_idx, _chat_contact)) {
                        _menu_state = MenuScreen::CHAT;
                        _chat_is_channel = false;
                        _input_mode = false;
                        _input_buffer[0] = '\0';
                        _input_length = 0;
                        _chat_scroll = 0;
                        _chat_msg_scroll_index = 0; // Reset scroll to newest
                        _search_filter_length = 0;
                        _search_filter[0] = '\0';
                        // Messages are already stored in global history, just filter them
                    }
                }
            }
            break;
        }
            
        case MenuScreen::CHANNELS: {
            // Count and collect channels
            int num_channels = 0;
            ChannelDetails channels[MAX_GROUP_CHANNELS];
            int channel_mesh_idx[MAX_GROUP_CHANNELS];
            
            for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
                if (the_mesh.getChannel(i, channels[num_channels]) && channels[num_channels].name[0] != '\0') {
                    channel_mesh_idx[num_channels] = i;
                    num_channels++;
                }
            }
            
            // Filter channels by search term
            int filtered_indices[MAX_GROUP_CHANNELS];
            int filtered_count = 0;
            
            if (_search_filter_length > 0) {
                for (int i = 0; i < num_channels; i++) {
                    char lower_name[32];
                    char lower_filter[32];
                    for (int j = 0; j < 32 && channels[i].name[j]; j++) {
                        lower_name[j] = tolower(channels[i].name[j]);
                        lower_name[j+1] = '\0';
                    }
                    for (int j = 0; j < 32 && _search_filter[j]; j++) {
                        lower_filter[j] = tolower(_search_filter[j]);
                        lower_filter[j+1] = '\0';
                    }
                    if (strstr(lower_name, lower_filter) != nullptr) {
                        filtered_indices[filtered_count++] = i;
                    }
                }
                num_channels = filtered_count;
            } else {
                for (int i = 0; i < num_channels; i++) {
                    filtered_indices[i] = i;
                }
                filtered_count = num_channels;
            }
            
            if (left || right) {
                // Switch to contacts, deselect settings icon
                _menu_state = MenuScreen::CONTACTS;
                _scroll_pos = 0;
                _selected_idx = 0;
                _settings_selected = false;
                _search_filter_length = 0;
                _search_filter[0] = '\0';
            } else if (up) {
                if (_settings_selected) {
                    // Already on settings icon, wrap to last channel
                    if (num_channels > 0) {
                        _settings_selected = false;
                        _selected_idx = num_channels - 1;
                        _scroll_pos = (_selected_idx > 2) ? _selected_idx - 2 : 0;
                    }
                    // If no channels, stay on settings icon
                } else if (_selected_idx == 0 && num_channels > 0) {
                    // At first channel, go to settings icon
                    _settings_selected = true;
                } else if (num_channels > 0) {
                    // Navigate up in list
                    _selected_idx = (_selected_idx > 0) ? _selected_idx - 1 : num_channels - 1;
                    if (_selected_idx < _scroll_pos) {
                        _scroll_pos = _selected_idx;
                    }
                } else if (num_channels == 0 && !_settings_selected) {
                    // No channels but not on settings - go to settings
                    _settings_selected = true;
                }
            } else if (down) {
                if (_settings_selected) {
                    // From settings icon, go to first channel
                    if (num_channels > 0) {
                        _settings_selected = false;
                        _selected_idx = 0;
                        _scroll_pos = 0;
                    }
                    // If no channels, stay on settings icon
                } else if (num_channels > 0) {
                    // Navigate down in list
                    _selected_idx = (_selected_idx < num_channels - 1) ? _selected_idx + 1 : 0;
                    if (_selected_idx >= _scroll_pos + 3) {
                        _scroll_pos = _selected_idx - 2;
                    }
                    if (_selected_idx == 0 && _selected_idx < _scroll_pos) {
                        _scroll_pos = 0;
                    }
                } else if (num_channels == 0 && !_settings_selected) {
                    // No channels but not on settings - go to settings
                    _settings_selected = true;
                }
            } else if (select) {
                if (_settings_selected) {
                    // Open settings menu
                    _menu_state = MenuScreen::SETTINGS;
                    _settings_menu_idx = 0;
                    _settings_item_idx = 0; // Start on first setting
                    _settings_selected = false;
                } else if (num_channels > 0) {
                    // Open chat with selected channel (use filtered index)
                    int real_idx = filtered_indices[_selected_idx];
                    _chat_channel = channels[real_idx];
                    _menu_state = MenuScreen::CHAT;
                    _chat_is_channel = true;
                    _input_mode = false;
                    _input_buffer[0] = '\0';
                    _input_length = 0;
                    _chat_scroll = 0;
                    _chat_msg_scroll_index = 0;
                    _search_filter_length = 0;
                    _search_filter[0] = '\0'; // Reset scroll to newest
                    // Messages are already stored in global history, just filter them
                    // Mark channel as read
                    int original_mesh_idx = channel_mesh_idx[real_idx];
                    _channel_has_unread[original_mesh_idx] = false;
                    strncpy(_last_read_channel, _chat_channel.name, 31);
                    _last_read_channel[31] = '\0';
                }
            }
            break;
        }
            
        case MenuScreen::SETTINGS: {
            if (up || down) {
                if (_settings_item_idx == -1) {
                    // In bottom bar, go back to settings items
                    _settings_item_idx = 2; // Start on Secondary Color (bottom item)
                } else {
                    // Navigate between settings items (0 = Brightness, 1 = Main Color, 2 = Secondary Color)
                    if (up && _settings_item_idx > 0) {
                        _settings_item_idx--;
                    } else if (down && _settings_item_idx < 2) {
                        _settings_item_idx++;
                    } else if (down && _settings_item_idx == 2) {
                        // Go down from Secondary Color to bottom bar
                        _settings_menu_idx = 0;
                        _settings_item_idx = -1;
                    } else if (up && _settings_item_idx == 0) {
                        // Can't go up from Brightness
                    }
                }
            } else if (left || right) {
                if (_settings_item_idx == 0) {
                    // Adjust brightness
                    int step = 15; // ~6% steps
                    if (left && _brightness > step) {
                        _brightness -= step;
                    } else if (left && _brightness <= step) {
                        _brightness = 0;
                    } else if (right && _brightness < (255 - step)) {
                        _brightness += step;
                    } else if (right) {
                        _brightness = 255;
                    }
                    M5Cardputer.Display.setBrightness(_brightness);
                } else if (_settings_item_idx == 1) {
                    // Change main color
                    if (left && _main_color_idx > 0) {
                        _main_color_idx--;
                    } else if (left) {
                        _main_color_idx = NUM_COLORS - 1;
                    } else if (right && _main_color_idx < NUM_COLORS - 1) {
                        _main_color_idx++;
                    } else if (right) {
                        _main_color_idx = 0;
                    }
                    applyTheme();
                } else if (_settings_item_idx == 2) {
                    // Change secondary color
                    if (left && _secondary_color_idx > 0) {
                        _secondary_color_idx--;
                    } else if (left) {
                        _secondary_color_idx = NUM_COLORS - 1;
                    } else if (right && _secondary_color_idx < NUM_COLORS - 1) {
                        _secondary_color_idx++;
                    } else if (right) {
                        _secondary_color_idx = 0;
                    }
                    applyTheme();
                } else {
                    // In bottom bar, toggle between Save (0) and Back (1)
                    _settings_menu_idx = (_settings_menu_idx == 0) ? 1 : 0;
                }
            } else if (select) {
                if (_settings_item_idx >= 0) {
                    // If on a setting item, Enter goes to bottom bar
                    _settings_menu_idx = 0;
                    _settings_item_idx = -1;
                } else if (_settings_menu_idx == 0) {
                    // Save - write to LittleFS and go back
                    saveSettings();
                    _menu_state = MenuScreen::CONTACTS;
                    _settings_menu_idx = 0;
                    _settings_item_idx = 0;
                } else {
                    // Back - don't save, return to contacts
                    // Reload settings to revert changes
                    loadSettings();
                    _menu_state = MenuScreen::CONTACTS;
                    _settings_menu_idx = 0;
                    _settings_item_idx = 0;
                }
            }
            break;
        }
            
        default:
            break;
    }
}

void UITask::sendMessage() {
    if (_input_length == 0) return;
    
    uint32_t timestamp = rtc_clock.getCurrentTime();
    
    // Add to chat history with contact/channel info
    if (_chat_is_channel) {
        addMessageToHistory(_node_prefs->node_name, _input_buffer, true, _chat_channel.name, true);
    } else {
        addMessageToHistory(_node_prefs->node_name, _input_buffer, true, _chat_contact.name, false);
    }
    
    if (_chat_is_channel) {
        // Send to channel
        the_mesh.sendGroupMessage(timestamp, _chat_channel.channel, 
                                  _node_prefs->node_name, _input_buffer, _input_length);
    } else {
        // Send to contact
        uint32_t expected_ack = 0;
        uint32_t timeout = 0;
        the_mesh.sendMessage(_chat_contact, timestamp, 0, _input_buffer, expected_ack, timeout);
    }
}

void UITask::addMessageToHistory(const char* from_name, const char* text, bool is_outgoing,
                                  const char* contact_or_channel, bool is_channel) {
    if (_chat_history_count >= MAX_CHAT_MESSAGES) {
        // Shift messages down
        for (int i = 0; i < MAX_CHAT_MESSAGES - 1; i++) {
            _chat_history[i] = _chat_history[i + 1];
        }
        _chat_history_count = MAX_CHAT_MESSAGES - 1;
    }
    
    ChatMessage& msg = _chat_history[_chat_history_count];
    strncpy(msg.from_name, from_name, 31);
    msg.from_name[31] = '\0';
    strncpy(msg.text, text, 127);
    msg.text[127] = '\0';
    strncpy(msg.contact_or_channel, contact_or_channel, 31);
    msg.contact_or_channel[31] = '\0';
    msg.is_outgoing = is_outgoing;
    msg.is_channel = is_channel;
    msg.timestamp = millis();
    
    _chat_history_count++;
}

void UITask::filterDisplayText(const char* input, char* output, int max_len) {
    int out_idx = 0;
    int in_idx = 0;
    
    while (input[in_idx] != '\0' && out_idx < max_len - 1) {
        unsigned char c = (unsigned char)input[in_idx];
        
        // Handle UTF-8 sequences (emojis and special characters) - skip them
        if (c >= 0x80) {
            // Skip UTF-8 multi-byte sequences completely
            if (c >= 0xF0) {
                // 4-byte sequence (most emojis)
                in_idx += 4;
            } else if (c >= 0xE0) {
                // 3-byte sequence
                in_idx += 3;
            } else if (c >= 0xC0) {
                // 2-byte sequence
                in_idx += 2;
            } else {
                // Invalid sequence, skip 1 byte
                in_idx += 1;
            }
        }
        // ASCII printable characters (space to ~)
        else if (c >= 0x20 && c <= 0x7E) {
            output[out_idx++] = c;
            in_idx++;
        }
        // Skip control characters
        else {
            in_idx++;
        }
    }
    
    output[out_idx] = '\0';
}

void UITask::showAlert(const char* msg) {
    strncpy(_alert, msg, sizeof(_alert) - 1);
    _alert[sizeof(_alert) - 1] = '\0';
    _alert_expiry = millis() + 2000;
}

void UITask::gotoScreen(MenuScreen screen) {
    _menu_state = screen;
}

uint16_t UITask::getBattMilliVolts() {
    #ifdef PIN_VBAT_READ
        return analogReadMilliVolts(PIN_VBAT_READ) * 2;
    #else
        return 0;
    #endif
}

bool UITask::isButtonPressed() {
    return user_btn.isPressed();
}

void UITask::msgRead(int msgcount) {
    // Mark messages as read
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
    // Check if it's a direct message (not a channel message)
    // Direct messages: path_len <= 2 AND no '#' in name
    // Channel messages: contain channel name or have '#' prefix
    bool is_direct_message = true;
    bool is_channel_msg = false;
    
    // Check if message is from a channel
    ChannelDetails temp_ch;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        if (the_mesh.getChannel(i, temp_ch) && temp_ch.name[0] != '\0') {
            if (strstr(from_name, temp_ch.name) != nullptr) {
                is_direct_message = false;
                is_channel_msg = true;
                break;
            }
        }
    }
    
    // Always save message to history (for all contacts and channels)
    if (is_direct_message) {
        // Save to contact's history
        addMessageToHistory(from_name, text, false, from_name, false);
    } else if (is_channel_msg) {
        // For channel messages, extract channel name and save
        char channel_name[32];
        
        // Try to find which channel this belongs to
        ChannelDetails temp_ch;
        bool found_channel = false;
        for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
            if (the_mesh.getChannel(i, temp_ch) && temp_ch.name[0] != '\0') {
                if (strstr(from_name, temp_ch.name) != nullptr) {
                    strncpy(channel_name, temp_ch.name, 31);
                    channel_name[31] = '\0';
                    found_channel = true;
                    
                    // Mark as unread if not currently viewing this channel
                    if (!(_menu_state == MenuScreen::CHAT && _chat_is_channel && 
                          strcmp(_chat_channel.name, temp_ch.name) == 0)) {
                        _channel_has_unread[i] = true;
                    }
                    break;
                }
            }
        }
        
        if (found_channel) {
            addMessageToHistory(from_name, text, false, channel_name, true);
        }
    }
    
    // Only show notification for direct messages AND only if not currently chatting with that person
    if (is_direct_message) {
        // Check if we're currently in chat with this contact
        bool is_currently_chatting = (_menu_state == MenuScreen::CHAT && 
                                      !_chat_is_channel && 
                                      strcmp(_chat_contact.name, from_name) == 0);
        
        if (!is_currently_chatting) {
            strncpy(_notification_from, from_name, 31);
            _notification_from[31] = '\0';
            strncpy(_notification_text, text, 127);
            _notification_text[127] = '\0';
            _notification_expiry = millis() + 2000; // 2 seconds
            _has_notification = true;
            _need_refresh = true;
            
            // Wake up display if off
            if (_display && !_display->isOn()) {
                _display->turnOn();
            }
            _auto_off = millis() + AUTO_OFF_MILLIS;
        }
    }
    
    _need_refresh = true;
}

void UITask::notify(UIEventType t) {
    // Handle notifications
    _need_refresh = true;
}

void UITask::syncChatHistoryToBLE(int max_messages) {
    // Sync last N messages from chat history to phone via BLE
    if (_chat_history_count == 0) {
        Serial.println("No chat history to sync");
        return;
    }
    
    int start_idx = (_chat_history_count > max_messages) ? 
                    (_chat_history_count - max_messages) : 0;
    
    Serial.printf("Syncing %d messages to BLE (from idx %d to %d)\n", 
                  _chat_history_count - start_idx, start_idx, _chat_history_count - 1);
    
    for (int i = start_idx; i < _chat_history_count; i++) {
        ChatMessage& msg = _chat_history[i];
        
        // ONLY sync INCOMING messages (is_outgoing = false)
        // Outgoing messages are already sent through mesh and will appear on phone naturally
        if (msg.is_outgoing) {
            continue; // Skip outgoing messages
        }
        
        // Find contact or channel info
        if (msg.is_channel) {
            // Find channel by name
            ChannelDetails channel;
            bool found = false;
            for (int ch_idx = 0; ch_idx < MAX_GROUP_CHANNELS; ch_idx++) {
                if (the_mesh.getChannel(ch_idx, channel) && 
                    strcmp(channel.name, msg.contact_or_channel) == 0) {
                    found = true;
                    break;
                }
            }
            
            if (found) {
                the_mesh.queueOutgoingMessageForBLE(NULL, &channel, 
                                                     msg.from_name, msg.text, msg.timestamp);
            }
        } else {
            // Find contact by name
            ContactInfo contact;
            bool found = false;
            
            ContactsIterator iter = the_mesh.startContactsIterator();
            while (iter.hasNext(&the_mesh, contact)) {
                if (strcmp(contact.name, msg.contact_or_channel) == 0) {
                    found = true;
                    break;
                }
            }
            
            if (found) {
                the_mesh.queueOutgoingMessageForBLE(&contact, NULL,
                                                     msg.from_name, msg.text, msg.timestamp);
            }
        }
    }
    
    Serial.println("Chat history sync complete");
}


