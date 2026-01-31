# City Siege Addon - Installation Guide

Complete step-by-step installation instructions for the City Siege WoW 3.3.5 addon.

## Prerequisites

- World of Warcraft 3.3.5a (WotLK) client
- AzerothCore server with City Siege module installed
- Approximately 5-10 MB of free space

## Step 1: Locate Your WoW AddOns Folder

Find your World of Warcraft installation directory. Common locations:

### Windows
```
C:\Program Files (x86)\World of Warcraft\Interface\AddOns\
```
or
```
C:\Games\World of Warcraft\Interface\AddOns\
```

### macOS
```
/Applications/World of Warcraft/Interface/AddOns/
```

### Linux (Wine)
```
~/.wine/drive_c/Program Files (x86)/World of Warcraft/Interface/AddOns/
```

If the `AddOns` folder doesn't exist, create it.

## Step 2: Install the Addon

### Method A: Copy the Complete Folder
1. Locate the `CitySiege` folder in this directory:
   ```
   azerothcore-wotlk/modules/mod-city-siege/ClientAddon/CitySiege/
   ```

2. Copy the entire `CitySiege` folder to your WoW `AddOns` directory

3. Your final structure should be:
   ```
   World of Warcraft/
   └── Interface/
       └── AddOns/
           └── CitySiege/
               ├── CitySiege.toc
               ├── Core.lua
               ├── Config.lua
               ├── Constants.lua
               ├── Utils.lua
               ├── Templates.xml
               ├── Libs/
               ├── Locales/
               └── Modules/
   ```

## Step 3: Verify Installation (Libraries Included!)

**Good news!** All required libraries are included in the addon package. No additional downloads needed!

1. Start World of Warcraft
2. At the character selection screen, click "AddOns" in the bottom-left
3. Look for "City Siege" in the addon list
4. Ensure it's checked/enabled
5. Check that it shows as "Loaded" or "Load on demand"

### Addon List Should Show:
```
☑ City Siege
  Version: 1.0.0
  Status: Loaded
```

## Step 4: First Launch

1. Log into your character
2. You should see a welcome message:
   ```
   [City Siege] Addon loaded! Type /cs or /citysiege for commands.
   ```

3. Look for the City Siege minimap button (siege tower icon)

4. Test the addon:
   ```
   /cs help
   ```

## Step 6: Configuration

### Initial Setup
1. Right-click the minimap button or type `/cs config`
2. Configure your preferences:
   - UI scale and transparency
   - Notification settings
   - Map display options
   - Minimap button position

### Recommended Settings for First Use
- **UI Scale**: 1.0 (default)
- **Show in Combat**: Enabled
- **Notifications**: All enabled
- **Map Updates**: 1.0 second interval

## Troubleshooting

### Problem: Addon doesn't appear in the addon list

**Solution:**
1. Verify folder name is exactly `CitySiege` (case-sensitive on some systems)
2. Check that `CitySiege.toc` exists in the folder
3. Ensure the interface version in TOC is `30300`
4. Restart WoW completely (not just reload UI)

### Problem: Addon loads but shows errors

**Solution:**
1. Check that all required libraries are installed
2. Install BugSack addon to see detailed errors:
   ```
   /bugsack
   ```
3. Verify library folders match the paths in `CitySiege.toc`
4. Make sure you downloaded WotLK (3.3.5) versions, not retail

### Problem: Minimap button doesn't appear

**Solution:**
1. Type `/cs minimap` to toggle it
2. Check if it's hidden in settings
3. Verify LibDataBroker and LibDBIcon are installed
4. Reset minimap button position in settings

### Problem: Commands don't work

**Solution:**
1. Verify you have GM/Admin permissions on the server
2. Check that the server has the City Siege module enabled
3. Try using the full command: `.citysiege status` in chat
4. Ensure you're connected to a server with the module

### Problem: Map doesn't show positions

**Solution:**
1. Ensure a siege is actually active in the selected city
2. Check Map Display settings (toggles must be enabled)
3. Verify server-side module is broadcasting position data
4. Try reducing the update interval in settings

## Uninstallation

To remove the addon:

1. Delete the `CitySiege` folder from your AddOns directory
2. Delete the saved variables file:
   ```
   World of Warcraft/WTF/Account/<YourAccount>/SavedVariables/CitySiegeDB.lua
   ```
3. Reload UI or restart WoW

## Updating

To update to a newer version:

1. Delete the old `CitySiege` folder
2. Install the new version following the installation steps
3. Your settings will be preserved (stored in SavedVariables)

**Note:** Always backup `CitySiegeDB.lua` before updating if you want to preserve statistics.

## Server Requirements

This addon requires:
- AzerothCore server
- City Siege module (mod-city-siege) installed and enabled
- Server-side addon communication (optional but recommended for real-time updates)

Contact your server administrator if the module isn't available.

## Getting Help

If you need assistance:

1. **In-game help**: `/cs help`
2. **Documentation**: Read the main README.md
3. **Libraries**: Check LIBRARIES_README.md in the Libs folder
4. **Server issues**: Contact your server administrator
5. **Bug reports**: Check GitHub issues or create a new one

## Additional Resources

- **Addon Development**: https://wowwiki-archive.fandom.com/wiki/AddOns
- **Ace3 Documentation**: https://www.wowace.com/projects/ace3
- **WoW API**: https://wowwiki-archive.fandom.com/wiki/World_of_Warcraft_API

## Quick Start Commands

Once installed, try these commands:

```
/cs                  # Show main window
/cs help             # List all commands
/cs config           # Open settings
/cs status           # Request siege status
```

## Performance Tips

For optimal performance:
- Set map update interval to 1-2 seconds
- Disable NPC positions if not needed
- Use auto-hide in non-siege zones
- Adjust UI scale if experiencing low FPS

---

**Installation Complete!** You're ready to experience City Siege battles with enhanced visualization and control.

**Questions?** Type `/cs help` in-game or consult the main README.md file.

---

*Version: 1.0.0*  
*Compatible with: WoW 3.3.5a (12340)*  
*Part of: AzerothCore City Siege Module*
