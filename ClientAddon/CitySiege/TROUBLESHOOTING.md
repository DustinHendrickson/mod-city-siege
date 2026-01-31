# City Siege Addon - Troubleshooting Guide

## No Data Showing on Map/Info/Status Tabs

### Quick Test
1. Type `/cs testmap` in chat - this will inject fake siege data
2. Open the main window with `/cs show`
3. Select "Stormwind" from the dropdown
4. Check the Info and Stats tabs

If the UI shows data with `/cs testmap`, the addon UI is working and the issue is server communication.

### Check Server Communication

#### Step 1: Verify EventHandler is Loaded
When you log in, you should see:
```
EventHandler initialized and listening for server messages...
```

If you don't see this, the EventHandler module failed to load.

#### Step 2: Enable Debug Mode
Type `/cs debug` to enable debug logging. You should see more detailed messages.

#### Step 3: Start a Siege on Server
Use the GM command:
```
.citysiege start stormwind
```

You should see messages in your chat like:
```
[RECEIVED] Siege started at city 0 by Horde
```

If you don't see these messages, the server is not sending data to the addon.

### Common Issues

#### 1. Server Not Broadcasting
**Symptom**: No `[RECEIVED]` messages when siege starts
**Cause**: Server's `BroadcastSiegeDataToAddon()` not being called or PSendSysMessage failing
**Fix**: 
- Check server console for "[City Siege] Sent addon message" logs
- Ensure you're within 500 yards of the siege location
- Verify `ChatHandler::PSendSysMessage()` is working

#### 2. Message Format Mismatch
**Symptom**: Server logs show messages sent but addon doesn't receive them
**Cause**: Message prefix doesn't match
**Fix**: 
- Server sends: `CITYSIEGE_START:0:Horde`
- Addon expects: `CITYSIEGE_START:...` or `CITYSIEGE:START:...`
- Both formats are now supported

#### 3. No Active Siege Data
**Symptom**: Messages received but no data on tabs
**Cause**: SiegeTracker not updating UI correctly
**Fix**:
- Use `/cs testmap` to verify UI updates work
- Check that `MainFrame:UpdateSiegeDisplay()` is being called
- Verify cityID matches (Stormwind = 0, Orgrimmar = 4, etc.)

#### 4. Map Shows No Texture
**Symptom**: Map tab is dark/empty
**Cause**: WoW 3.3.5 addons cannot access built-in map BLP files
**Solution**: This is expected! The map uses a tactical overlay (dark background + grid).
- To add custom maps, see `Media/MAP_TEXTURES_README.md`
- The tactical overlay is fully functional for siege tracking

### Manual Testing

You can manually inject messages to test the addon:

```lua
/script CitySiege_EventHandler:ParseAddonMessage("START:0:Horde")
/script CitySiege_EventHandler:ParseAddonMessage("UPDATE:0:2:25:30:120")
/script CitySiege_EventHandler:ParseAddonMessage("END:0:Alliance")
```

### City IDs

```
Stormwind     = 0
Ironforge     = 1
Darnassus     = 2
Exodar        = 3
Orgrimmar     = 4
Undercity     = 5
Thunder Bluff = 6
Silvermoon    = 7
```

### Reload Addon
Type `/reload` to reload the UI and reinitialize all modules.

### Check Lua Errors
Install BugSack addon to catch and display Lua errors that might be preventing the addon from working.

### Still Not Working?

1. Check for Lua errors with `/bugsack show` (requires BugSack addon)
2. Verify the addon is loaded: `/dump IsAddOnLoaded("CitySiege")`
3. Check EventHandler exists: `/dump CitySiege_EventHandler`
4. Try test mode: `/cs testmap 0`
5. Check server console for "[City Siege]" log messages
