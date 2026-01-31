# Required Libraries for City Siege Addon

This addon requires the following external libraries to function properly. These libraries are standard in the WoW addon community and should be placed in the `Libs` folder.

## Required Libraries

### Core Libraries

1. **LibStub** (Required)
   - Download: https://www.curseforge.com/wow/addons/libstub
   - Extract to: `CitySiege/Libs/LibStub/`
   - Files needed: `LibStub.lua`

2. **CallbackHandler-1.0** (Required)
   - Download: https://www.curseforge.com/wow/addons/callbackhandler
   - Extract to: `CitySiege/Libs/CallbackHandler-1.0/`
   - Files needed: `CallbackHandler-1.0.lua`, `CallbackHandler-1.0.xml`

### Ace3 Libraries (Required)

3. **AceAddon-3.0**
   - Part of Ace3: https://www.curseforge.com/wow/addons/ace3
   - Extract to: `CitySiege/Libs/AceAddon-3.0/`
   - Files needed: `AceAddon-3.0.lua`, `AceAddon-3.0.xml`

4. **AceEvent-3.0**
   - Part of Ace3
   - Extract to: `CitySiege/Libs/AceEvent-3.0/`
   - Files needed: `AceEvent-3.0.lua`, `AceEvent-3.0.xml`

5. **AceDB-3.0**
   - Part of Ace3
   - Extract to: `CitySiege/Libs/AceDB-3.0/`
   - Files needed: `AceDB-3.0.lua`, `AceDB-3.0.xml`

6. **AceConsole-3.0**
   - Part of Ace3
   - Extract to: `CitySiege/Libs/AceConsole-3.0/`
   - Files needed: `AceConsole-3.0.lua`, `AceConsole-3.0.xml`

7. **AceGUI-3.0**
   - Part of Ace3
   - Extract to: `CitySiege/Libs/AceGUI-3.0/`
   - Files needed: All AceGUI files and widgets

8. **AceConfig-3.0**
   - Part of Ace3
   - Extract to: `CitySiege/Libs/AceConfig-3.0/`
   - Files needed: All AceConfig files

### Data Broker Libraries (Required)

9. **LibDataBroker-1.1**
   - Download: https://www.curseforge.com/wow/addons/libdatabroker-1-1
   - Extract to: `CitySiege/Libs/LibDataBroker-1.1/`
   - Files needed: `LibDataBroker-1.1.lua`

10. **LibDBIcon-1.0**
    - Download: https://www.curseforge.com/wow/addons/libdbicon-1-0
    - Extract to: `CitySiege/Libs/LibDBIcon-1.0/`
    - Files needed: `LibDBIcon-1.0.lua`

## Installation Instructions

### Option 1: Download Ace3 Package
The easiest method is to download the complete Ace3 package which includes most required libraries:

1. Download Ace3 from: https://www.curseforge.com/wow/addons/ace3
2. Extract the `Ace3` folder
3. Copy all library folders from `Ace3/Libs/` to `CitySiege/Libs/`
4. The Ace3 package typically includes:
   - LibStub
   - CallbackHandler-1.0
   - AceAddon-3.0
   - AceEvent-3.0
   - AceDB-3.0
   - AceConsole-3.0
   - AceGUI-3.0
   - AceConfig-3.0
   - And other useful libraries

### Option 2: Individual Downloads
Download each library individually from CurseForge or WoWInterface and place them in the correct folders.

### Option 3: Use Existing Libraries
If you already have addons that include these libraries (like DBM, BigWigs, etc.), you can:
1. Copy the library folders from those addons
2. Paste them into `CitySiege/Libs/`
3. Make sure file paths match what's in `CitySiege.toc`

## Verification

After installation, your `CitySiege/Libs/` folder should contain:
```
Libs/
├── LibStub/
│   └── LibStub.lua
├── CallbackHandler-1.0/
│   ├── CallbackHandler-1.0.lua
│   └── CallbackHandler-1.0.xml
├── AceAddon-3.0/
│   ├── AceAddon-3.0.lua
│   └── AceAddon-3.0.xml
├── AceEvent-3.0/
│   ├── AceEvent-3.0.lua
│   └── AceEvent-3.0.xml
├── AceDB-3.0/
│   ├── AceDB-3.0.lua
│   └── AceDB-3.0.xml
├── AceConsole-3.0/
│   ├── AceConsole-3.0.lua
│   └── AceConsole-3.0.xml
├── AceGUI-3.0/
│   ├── AceGUI-3.0.lua
│   ├── AceGUI-3.0.xml
│   └── widgets/ (all widget files)
├── AceConfig-3.0/
│   ├── AceConfig-3.0.lua
│   ├── AceConfig-3.0.xml
│   ├── AceConfigRegistry-3.0/
│   ├── AceConfigDialog-3.0/
│   └── AceConfigCmd-3.0/
├── LibDataBroker-1.1/
│   └── LibDataBroker-1.1.lua
└── LibDBIcon-1.0/
    └── LibDBIcon-1.0.lua
```

## Troubleshooting

### "LibStub" not found error
- Ensure `LibStub.lua` is in `CitySiege/Libs/LibStub/`
- Check that the path in `CitySiege.toc` matches the actual folder structure

### Addon won't load
- Verify all libraries are present
- Check WoW error logs (install BugSack addon to see Lua errors)
- Ensure folder names match exactly (case-sensitive)

### Minimap button doesn't work
- LibDataBroker and LibDBIcon must be correctly installed
- Check that `LibDataBroker-1.1.lua` and `LibDBIcon-1.0.lua` exist

## Alternative: All-in-One Package

For convenience, you may want to create a complete package that includes all libraries:

1. Create a `CitySiege-Complete` folder
2. Include all libraries in the package
3. Distribute as a single download
4. Users only need to extract one folder

## License Notes

All Ace3 libraries are distributed under their own licenses. Please respect the original authors' licenses when distributing this addon.

- Ace3: BSD License
- LibDataBroker: Public Domain
- LibDBIcon: GPLv2 or later

## Version Compatibility

These libraries are compatible with WoW 3.3.5a (WotLK). Make sure to download versions specifically for WotLK/3.3.5, not retail or newer expansions.

## Support

If you encounter issues with library installation:
1. Check file paths in `CitySiege.toc`
2. Verify library versions are for WoW 3.3.5
3. Look for Lua errors using `/console scriptErrors 1`
4. Use BugSack addon for detailed error messages

---

**Note**: This addon will not function without these libraries. They provide essential functionality for addon development including event handling, database management, and UI creation.
