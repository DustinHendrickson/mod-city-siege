# City Siege Client Addon

A comprehensive World of Warcraft 3.3.5 (WotLK) addon for the AzerothCore City Siege module.

## Features

### 🗺️ Interactive City Map Display
- Real-time visualization of siege battles
- Player position tracking (attackers shown in red, defenders in blue)
- NPC position markers (color-coded by allegiance)
- Waypoint path visualization showing NPC movement routes
- Switchable between different sieged cities
- Zoom and pan controls

### 📊 Siege Information & Statistics
- View all active sieges across different cities
- Real-time siege status updates
- Phase information and duration tracking
- Participant counts (attackers vs defenders)
- Personal statistics tracking:
  - Sieges participated in
  - Win/loss record
  - Kill/death ratios
  - Overall performance metrics

### 🎮 Command Interface
- Easy-to-use button interface for all siege commands
- Quick access to GM/Admin commands:
  - Start Siege
  - Stop Siege
  - Cleanup NPCs
  - Show Status
  - Reload Configuration
- City selection dropdown for targeted commands
- Visual feedback for command execution

### ⚙️ Comprehensive Settings
- **Minimap Button**: Show/hide, lock/unlock position
- **UI Settings**: Scale, transparency, lock window, combat visibility
- **Notifications**: Configurable alerts for siege events with sound effects
- **Map Display**: Toggle players, NPCs, waypoints; adjust update intervals

### 🔔 Event Notifications
- Siege start/end announcements
- Phase change notifications
- Zone entry alerts when entering an active siege
- Optional sound effects
- Color-coded messages by faction

## Installation

### Method 1: Manual Installation
1. Download the `CitySiege` folder from the `ClientAddon` directory
2. Copy it to your WoW installation: `World of Warcraft/Interface/AddOns/`
3. Restart WoW or reload UI (`/reload`)

### Method 2: Direct Path
Copy the entire addon folder to:
```
C:\Program Files (x86)\World of Warcraft\Interface\AddOns\CitySiege\
```

## Usage

### Basic Commands
- `/cs` or `/citysiege` - Main command
- `/cs show` - Toggle main window
- `/cs config` - Open settings panel
- `/cs minimap` - Toggle minimap button
- `/cs status` - Request siege status
- `/cs help` - Show all available commands

### GM/Admin Commands (Server-side)
These commands are executed through the addon but require appropriate permissions:
- `/cs start [city]` - Start a siege in specified city
- `/cs stop [city] [faction]` - Stop an active siege
- `/cs cleanup [city]` - Clean up siege NPCs
- `/cs info` - Display detailed siege information
- `/cs reload` - Reload server-side configuration

### Interface Navigation

#### Main Window Tabs
1. **Commands Tab**: Quick access to all siege management commands
2. **Map Tab**: Visual representation of the siege battlefield
3. **Info Tab**: Detailed information about active sieges
4. **Stats Tab**: Personal performance statistics

#### City Selection
Use the dropdown menu at the top to switch between:
- All Active Sieges (overview mode)
- Individual cities (Stormwind, Orgrimmar, Ironforge, etc.)

#### Minimap Button
- **Left-Click**: Toggle main window
- **Right-Click**: Open settings
- **Middle-Click**: Request status update
- **Tooltip**: Shows active sieges at a glance

## Configuration

### UI Customization
Access settings via `/cs config` or right-click minimap button:

- **Scale**: Adjust overall UI size (0.5x to 2.0x)
- **Alpha**: Set window transparency (30% to 100%)
- **Lock**: Prevent accidental window movement
- **Combat Behavior**: Show/hide UI during combat

### Map Settings
- **Update Interval**: How frequently positions refresh (0.5s to 5s)
- **Icon Scale**: Size of player/NPC markers
- **Display Toggles**: Show/hide specific elements

### Notifications
Configure which events trigger notifications:
- Siege start/end
- Phase changes
- Sound effects

## Supported Cities

The addon tracks sieges in all major faction cities:

### Alliance Cities
- **Stormwind City** - Human capital
- **Ironforge** - Dwarf capital
- **Darnassus** - Night Elf capital
- **The Exodar** - Draenei capital

### Horde Cities
- **Orgrimmar** - Orc capital
- **Undercity** - Forsaken capital
- **Thunder Bluff** - Tauren capital
- **Silvermoon City** - Blood Elf capital

## Technical Details

### Dependencies
The addon uses the following libraries (included):
- **Ace3**: Core addon framework
  - AceAddon-3.0
  - AceEvent-3.0
  - AceDB-3.0
  - AceConsole-3.0
  - AceConfig-3.0
  - AceGUI-3.0
- **LibStub**: Library management
- **LibDataBroker-1.1**: Data sharing
- **LibDBIcon-1.0**: Minimap button support

### Saved Variables
- `CitySiegeDB` - Stores all settings and persistent data

### Performance
- Optimized update intervals to minimize CPU usage
- Throttled position updates
- Efficient event handling
- Minimal memory footprint

## Troubleshooting

### Addon Not Loading
1. Check that the folder structure is correct:
   ```
   AddOns/CitySiege/CitySiege.toc
   AddOns/CitySiege/Core.lua
   (etc.)
   ```
2. Verify the TOC file interface version matches your client (30300)
3. Ensure all library dependencies are present

### Minimap Button Missing
1. Check if it's hidden: `/cs minimap`
2. Reset position in settings if stuck off-screen
3. Disable and re-enable in settings panel

### Map Not Showing Positions
1. Verify siege is active in selected city
2. Check Map Display settings (toggles enabled)
3. Ensure update interval isn't set too high
4. Server-side addon communication must be working

### Commands Not Working
- Commands require GM/Admin permissions on the server
- Verify you're sending commands to the correct chat channel
- Check server console for error messages

## Known Limitations

1. **Map Accuracy**: Position markers use estimated coordinate conversion; exact positioning requires server-side waypoint data
2. **Real-time Updates**: Position updates depend on server communication frequency
3. **WoW 3.3.5 API**: Some modern addon features aren't available in this client version
4. **Rotation**: Line rotation for waypoint paths is simplified due to API limitations

## Future Enhancements

Planned features for future versions:
- 3D arrow indicators for objectives
- Heat maps showing combat intensity
- Replay functionality for completed sieges
- Achievement integration
- Cross-realm siege coordination
- Mobile companion app data export

## Support

For issues, suggestions, or contributions:
1. Check existing GitHub issues
2. Server logs for server-side problems
3. Lua error capture for client-side issues
4. Provide detailed reproduction steps

## Credits

- **Development**: AzerothCore Community
- **Framework**: Ace3 Development Team
- **Testing**: City Siege module contributors
- **Special Thanks**: All server administrators and players

## License

This addon is part of the AzerothCore City Siege module project.
Licensed under GNU Affero General Public License v3.0.

---

**Version**: 1.0.0  
**Compatible With**: WoW 3.3.5a (12340)  
**Last Updated**: 2025  
**Module**: mod-city-siege

## Quick Reference

### Keyboard Shortcuts
- `ESC` - Close windows
- `Left-Click + Drag` - Move windows (when unlocked)

### Color Codes
- 🔵 **Blue** - Defenders / Alliance
- 🔴 **Red** - Attackers / Horde
- 🟡 **Yellow** - NPCs / Objectives
- ⚪ **White** - Waypoints
- 🟢 **Green** - Friendly forces
- 🟠 **Orange** - Enemy forces

### Status Indicators
- **Active** - Siege currently in progress
- **Preparing** - Siege about to begin
- **Ending** - Siege concluding
- **Inactive** - No siege in this city

---

**Need help?** Type `/cs help` in-game or consult the server documentation.
