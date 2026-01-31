# City Siege Addon - Quick Reference Card

## 🚀 Quick Start

1. **Install addon** → Copy `CitySiege` folder to `WoW/Interface/AddOns/`
2. **Install libraries** → Download Ace3 from CurseForge, copy Libs to `CitySiege/Libs/`
3. **Launch WoW** → Enable addon at character select
4. **Open addon** → Click minimap button or type `/cs`

## ⌨️ Essential Commands

| Command | Description |
|---------|-------------|
| `/cs` | Toggle main window |
| `/cs config` | Open settings |
| `/cs minimap` | Toggle minimap button |
| `/cs status` | Request siege status |
| `/cs help` | Show all commands |

## 🖱️ Minimap Button

| Click | Action |
|-------|--------|
| **Left** | Toggle main window |
| **Right** | Open settings |
| **Middle** | Update status |

## 📑 Main Window Tabs

| Tab | What You'll Find |
|-----|------------------|
| **Commands** | Execute GM siege commands with buttons |
| **Map** | Live visualization of siege battlefield |
| **Info** | Active siege details and status |
| **Stats** | Your personal performance statistics |

## 🎨 Color Guide

| Color | Meaning |
|-------|---------|
| 🔵 **Blue** | Defenders / Alliance |
| 🔴 **Red** | Attackers / Horde |
| 🟡 **Yellow** | NPCs / Objectives |
| ⚪ **White** | Waypoint paths |
| 🟢 **Green** | Friendly forces |
| 🟠 **Orange** | Enemy forces |

## 🏙️ Supported Cities

### Alliance
- ⚔️ Stormwind City
- 🔨 Ironforge
- 🌳 Darnassus
- ✨ The Exodar

### Horde
- 🗡️ Orgrimmar
- ☠️ Undercity
- 🐃 Thunder Bluff
- 🌅 Silvermoon City

## ⚙️ Quick Settings

### Most Used Options
- **UI Scale**: Adjust overall size
- **Transparency**: Set window opacity
- **Show in Combat**: Keep UI visible during fights
- **Map Update Speed**: How often positions refresh
- **Notifications**: Enable/disable alerts

### Recommended First-Time Settings
```
✓ UI Scale: 1.0
✓ Show in Combat: ON
✓ All Notifications: ON
✓ Map Update: 1.0 second
✓ Show Players: ON
✓ Show NPCs: ON
```

## 🎮 GM Commands (Require Permissions)

| Command | Syntax | Purpose |
|---------|--------|---------|
| Start | `.citysiege start [city]` | Begin a siege |
| Stop | `.citysiege stop [city] [faction]` | End a siege |
| Cleanup | `.citysiege cleanup [city]` | Remove NPCs |
| Status | `.citysiege status` | Show all sieges |
| Info | `.citysiege info` | Detailed information |
| Reload | `.citysiege reload` | Refresh config |

## 🔧 Troubleshooting Quick Fixes

| Problem | Solution |
|---------|----------|
| **Addon won't load** | Check libraries are installed; verify folder name is `CitySiege` |
| **No minimap button** | Type `/cs minimap` to unhide |
| **Lua errors** | Install BugSack addon; check all Ace3 libs present |
| **Commands fail** | Need GM permissions; check server has module |
| **Map empty** | Ensure siege is active; check map settings toggles |
| **Settings reset** | Check SavedVariables folder wasn't deleted |

## 📊 Statistics Explained

| Stat | What It Tracks |
|------|----------------|
| **Participated** | Number of sieges you've joined |
| **Won/Lost** | Your siege victory record |
| **Win Rate** | Percentage of sieges won |
| **Total Kills** | Enemy players killed |
| **Total Deaths** | Times you died |
| **K/D Ratio** | Kills divided by deaths |

## 🔔 Notification Types

- ✅ **Siege Start** - New siege begins
- ✅ **Siege End** - Siege concludes
- ✅ **Phase Change** - Siege enters new phase
- ✅ **Zone Entry** - You enter an active siege
- 🔊 **Sounds** - Audio alerts (toggle on/off)

## 📂 File Locations

### Addon Files
```
WoW/Interface/AddOns/CitySiege/
```

### Saved Settings
```
WoW/WTF/Account/<Account>/SavedVariables/CitySiegeDB.lua
```

### Screenshots (Auto)
```
WoW/Screenshots/
```

## 🆘 Getting Help

1. **In-game**: Type `/cs help`
2. **Documentation**: Check README.md
3. **Installation**: See INSTALLATION.md
4. **Libraries**: Read LIBRARIES_README.md
5. **Server Issues**: Contact your GM/Admin

## 💡 Pro Tips

1. **Performance**: Set map update to 2 seconds if FPS drops
2. **Visibility**: Increase UI scale to 1.2 for better readability
3. **Focus**: Hide NPC markers if map gets too crowded
4. **Auto-hide**: Enable to clear screen when leaving siege
5. **Lock UI**: Prevent accidental movement during combat
6. **Keybind**: Create macro `/cs` for instant access
7. **Backup**: Export SavedVariables before major updates

## 📱 Macro Examples

### Toggle Addon
```
/cs
```

### Quick Status Check
```
/cs status
```

### Combat Toggle
```
/run CitySiege_Config:SetShowInCombat(not CitySiege_Config:GetUISettings().showInCombat)
/cs show
```

## 🎯 Best Practices

1. **Keep addon updated** - Check for new versions
2. **Backup settings** - Copy SavedVariables file
3. **Report bugs** - Help improve the addon
4. **Configure alerts** - Tailor to your preference
5. **Use map wisely** - Don't rely solely on it
6. **Respect limits** - Don't spam commands
7. **Share feedback** - Help development

## 📞 Support Channels

- **Discord**: Check your server's Discord
- **Forum**: Server website forums
- **GitHub**: Project issues page
- **In-game**: Guild or server GMs

## ⚡ Performance Tips

| Situation | Recommended Setting |
|-----------|---------------------|
| **Low FPS** | Update interval: 2-3s, Scale: 0.8 |
| **High FPS** | Update interval: 0.5-1s, Scale: 1.2 |
| **Large raids** | Hide NPCs, Show players only |
| **Solo play** | Show everything for full awareness |
| **PvP focus** | Disable auto-hide, Lock UI |

## 🎁 Hidden Features

- **Double-click** title bar to minimize (planned)
- **Shift-click** map to set personal waypoint (planned)
- **Alt-click** player name to whisper (planned)
- **Ctrl-click** to focus target (planned)

## 🔮 Coming Soon

- Heat maps for combat zones
- Replay system
- Guild leaderboards
- Achievement integration
- Custom alert sounds
- Advanced statistics

---

## 📋 Checklist for New Users

```
□ Addon installed in AddOns folder
□ Ace3 libraries downloaded and placed
□ Addon enabled at character select
□ Welcome message appeared in chat
□ Minimap button visible
□ Main window opens with /cs
□ Settings configured to preference
□ All tabs accessible
□ Map displays when siege active
□ Notifications working
□ Statistics tracking
```

---

**Version**: 1.0.0 | **Client**: WoW 3.3.5a | **Module**: City Siege

*Print this card or save as bookmark for quick reference!*

---

**Need more help?** Type `/cs help` or read the full README.md
