# 🏰 City Siege - WoW Client Addon

<div align="center">

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![WoW](https://img.shields.io/badge/WoW-3.3.5a-orange.svg)
![License](https://img.shields.io/badge/license-AGPL--3.0-green.svg)
![Status](https://img.shields.io/badge/status-stable-brightgreen.svg)

**A comprehensive client-side addon for the AzerothCore City Siege module**

[Installation](#-installation) • [Features](#-features) • [Documentation](#-documentation) • [Support](#-support)

</div>

---

## 📖 Overview

City Siege is a feature-rich World of Warcraft 3.3.5 addon that provides players with an immersive interface for participating in epic city siege battles. Visualize battles in real-time, track participants, manage commands, and monitor your performance - all through an elegant, easy-to-use interface.

### What Makes This Special?

- 🗺️ **Live Battlefield Visualization** - See the battle unfold on an interactive city map
- 🎯 **Smart Tracking** - Real-time player and NPC position updates
- 🎮 **One-Click Commands** - Execute siege commands with simple button clicks
- 📊 **Comprehensive Stats** - Track your performance across all sieges
- ⚙️ **Highly Customizable** - Tailor everything to your preferences
- 🔔 **Smart Notifications** - Never miss an important siege event

---

## ✨ Features

### Interactive City Map Display
- Real-time visualization of siege battles across 8 major cities
- Color-coded player positions (red = attackers, blue = defenders)
- NPC tracking with allegiance indicators
- Waypoint path visualization showing movement routes
- Smooth city switching with saved states
- Performance-optimized updates

### Siege Management Interface
- Quick-access command buttons for GMs/Admins
- City selection dropdown for targeted actions
- Status monitoring and information display
- Real-time siege statistics
- Phase tracking and notifications

### Statistics & Analytics
- Personal performance tracking
- Win/loss records with percentages
- K/D ratios and kill tracking
- Participation history
- Per-siege detailed statistics

### Customization Options
- UI scaling (0.5x to 2.0x)
- Transparency control
- Position locking
- Combat visibility settings
- Notification preferences
- Map display toggles
- Update interval configuration

---

## 🚀 Quick Start

### 1. Install the Addon

```bash
# Copy the CitySiege folder to your WoW AddOns directory
WoW/Interface/AddOns/CitySiege/
```

### 2. Install Required Libraries

Download **Ace3** for WoW 3.3.5:
- Visit: https://www.curseforge.com/wow/addons/ace3
- Download WotLK version
- Copy library folders to `CitySiege/Libs/`

**Required Libraries:**
- LibStub
- CallbackHandler-1.0
- AceAddon-3.0, AceEvent-3.0, AceDB-3.0
- AceConsole-3.0, AceGUI-3.0, AceConfig-3.0
- LibDataBroker-1.1, LibDBIcon-1.0

See [INSTALLATION.md](INSTALLATION.md) for detailed instructions.

### 3. Launch and Configure

1. Enable addon at character select
2. Look for the minimap button (siege tower icon)
3. Click button or type `/cs` to open
4. Configure settings via `/cs config`

---

## 🎮 Usage

### Basic Commands

```
/cs              # Toggle main window
/cs show         # Show main window
/cs config       # Open settings
/cs status       # Request siege status
/cs help         # Show all commands
```

### Minimap Button Interactions

| Action | Result |
|--------|--------|
| **Left-Click** | Toggle main window |
| **Right-Click** | Open settings |
| **Middle-Click** | Update status |

### Main Window Tabs

1. **Commands** - Execute siege management commands
2. **Map** - Visual battlefield display
3. **Info** - Siege details and status
4. **Stats** - Performance statistics

---

## 🏙️ Supported Cities

### Alliance Cities
- ⚔️ **Stormwind City** - Human capital
- 🔨 **Ironforge** - Dwarf capital
- 🌳 **Darnassus** - Night Elf capital
- ✨ **The Exodar** - Draenei capital

### Horde Cities
- 🗡️ **Orgrimmar** - Orc capital
- ☠️ **Undercity** - Forsaken capital
- 🐃 **Thunder Bluff** - Tauren capital
- 🌅 **Silvermoon City** - Blood Elf capital

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [README.md](README.md) | Complete feature guide and usage |
| [INSTALLATION.md](INSTALLATION.md) | Step-by-step setup instructions |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | Quick command and feature reference |
| [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md) | Technical overview and architecture |
| [CHANGELOG.md](CHANGELOG.md) | Version history and updates |
| [Libs/LIBRARIES_README.md](CitySiege/Libs/LIBRARIES_README.md) | Library installation guide |

---

## 🔧 Requirements

### Client Requirements
- **WoW Version**: 3.3.5a (12340)
- **Disk Space**: ~10 MB (with libraries)
- **Memory**: Minimal (<5 MB RAM)

### Server Requirements
- **Core**: AzerothCore
- **Module**: mod-city-siege (enabled)
- **Optional**: Addon communication support

---

## ⚙️ Configuration

Access settings via `/cs config` or right-click minimap button.

### Key Settings

**UI Customization**
- Scale: 0.5x to 2.0x
- Alpha: 30% to 100%
- Lock position
- Combat visibility

**Map Display**
- Update interval: 0.5s to 5.0s
- Show players/NPCs/waypoints
- Icon scaling

**Notifications**
- Siege start/end alerts
- Phase change notifications
- Sound effects

---

## 📊 Statistics Tracking

The addon automatically tracks:

- 📈 Sieges participated in
- 🏆 Sieges won / lost
- 💯 Win rate percentage
- ⚔️ Total kills
- 💀 Total deaths
- 📉 K/D ratio

View your stats in the **Stats** tab of the main window.

---

## 🎨 Interface Preview

### Color Coding
- 🔵 **Blue** - Defenders / Alliance
- 🔴 **Red** - Attackers / Horde
- 🟡 **Yellow** - NPCs / Objectives
- ⚪ **White** - Waypoint paths
- 🟢 **Green** - Friendly forces
- 🟠 **Orange** - Enemy forces

---

## 🐛 Troubleshooting

### Common Issues

**Addon won't load**
- Verify folder structure is correct
- Check that libraries are installed
- Ensure TOC file interface version is `30300`

**No minimap button**
- Type `/cs minimap` to toggle visibility
- Check if hidden in settings
- Verify LibDBIcon is installed

**Map is empty**
- Ensure siege is active in selected city
- Check that map display toggles are enabled
- Verify server module is functioning

**Commands don't work**
- Commands require GM/Admin permissions
- Check server logs for errors
- Ensure module is enabled server-side

See [INSTALLATION.md](INSTALLATION.md) for detailed troubleshooting.

---

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

### Development

See [DEVELOPMENT_SUMMARY.md](DEVELOPMENT_SUMMARY.md) for:
- Architecture overview
- Code organization
- API documentation
- Development guidelines

---

## 📜 License

This project is licensed under the GNU Affero General Public License v3.0.

See [LICENSE](LICENSE) for details.

### External Libraries
- **Ace3**: BSD License
- **LibDataBroker**: Public Domain
- **LibDBIcon**: GPLv2+

---

## 🙏 Credits

### Development
- **AzerothCore Team** - Core server development
- **Module Authors** - City Siege server module
- **Addon Developer** - Client interface

### Special Thanks
- Ace3 Development Team
- WoW AddOn Community
- Beta testers and contributors
- Server administrators

---

## 📞 Support

### Getting Help

1. **Documentation** - Read the guides in this folder
2. **In-game** - Type `/cs help`
3. **Server Discord** - Ask in your server's support channels
4. **GitHub Issues** - Report bugs or request features

### Resources

- **AzerothCore**: https://www.azerothcore.org/
- **Module Repository**: https://github.com/azerothcore/mod-city-siege
- **WoW API**: https://wowwiki-archive.fandom.com/

---

## 🚧 Roadmap

### Planned Features
- Heat maps showing combat intensity
- Siege replay system
- Achievement integration
- Guild leaderboards
- Advanced statistics graphs
- Custom alert sounds
- Mobile companion app

### Under Consideration
- 3D objective indicators
- Cross-realm coordination
- Tournament mode
- Spectator mode
- Video recording integration

---

## 📈 Project Status

| Component | Status |
|-----------|--------|
| Core Framework | ✅ Complete |
| Map Display | ✅ Complete |
| Siege Tracking | ✅ Complete |
| Command Interface | ✅ Complete |
| Settings Panel | ✅ Complete |
| Statistics | ✅ Complete |
| Documentation | ✅ Complete |
| Localization (enUS) | ✅ Complete |
| Other Locales | 🚧 In Progress |
| Server Integration | ⚠️ Optional |

---

## 🏆 Highlights

- **3,500+** lines of well-documented code
- **7** independent, modular components
- **150+** functions and methods
- **21** files across multiple categories
- **8** supported cities
- **10+** configuration options
- **5,000+** words of documentation

---

## 💡 Pro Tips

1. **Performance**: Set update interval to 2 seconds for smooth gameplay
2. **Visibility**: Increase UI scale to 1.2 for better readability
3. **Focus**: Use city dropdown to track specific sieges
4. **Efficiency**: Create a macro with `/cs` for quick access
5. **Safety**: Backup your SavedVariables before major updates

---

## 🎯 Use Cases

### For Players
- Monitor active sieges across all cities
- Track your performance and improve
- Get notified of important events
- Visualize battlefield positions
- Quick access to siege information

### For GMs/Admins
- One-click siege management
- Visual monitoring of siege status
- Quick command execution
- Real-time participant tracking
- Easy troubleshooting

### For Guilds
- Coordinate siege participation
- Track guild performance
- Share strategies based on map data
- Organize city defense/attack
- Monitor enemy movements

---

<div align="center">

## ⭐ Enjoy City Siege!

**Made with ❤️ for the AzerothCore Community**

*Version 1.0.0 | Compatible with WoW 3.3.5a*

[⬆ Back to Top](#-city-siege---wow-client-addon)

</div>
