# City Siege - Addon Communication Protocol

## Overview

The City Siege module communicates with the client addon using SILENT background messages. Players will NOT see these messages in their chat - the addon intercepts them invisibly.

## Server → Addon Communication

### Broadcast Frequency
- **Every 30 seconds** during active sieges
- On siege START
- On siege END

### Message Format

All messages are prefixed with `CITYSIEGE_` and sent as system messages that the addon intercepts.

#### START Message
```
CITYSIEGE_START:cityId:attackingFaction
```
Example: `CITYSIEGE_START:0:Horde`

**Fields:**
- `cityId`: 0-7 (0=Stormwind, 1=Ironforge, 2=Darnassus, 3=Exodar, 4=Orgrimmar, 5=Undercity, 6=ThunderBluff, 7=Silvermoon)
- `attackingFaction`: "Horde" or "Alliance"

#### UPDATE Message  
```
CITYSIEGE_UPDATE:cityId:phase:attackerCount:defenderCount:elapsedSeconds
```
Example: `CITYSIEGE_UPDATE:0:2:45:30:1200`

**Fields:**
- `cityId`: City identifier (0-7)
- `phase`: Current siege phase (1-4)
  - 1 = Cinematic/Preparation
  - 2 = Early combat (0-25% elapsed)
  - 3 = Mid combat (25-50% elapsed)
  - 4 = Late combat (50%+ elapsed)
- `attackerCount`: Number of attacking NPCs alive
- `defenderCount`: Number of defending NPCs alive
- `elapsedSeconds`: Time since siege started (seconds)

#### END Message
```
CITYSIEGE_END:cityId:winner
```
Example: `CITYSIEGE_END:0:Alliance`

**Fields:**
- `cityId`: City identifier (0-7)
- `winner`: "Alliance", "Horde", or "unknown"

## Addon Tabs & Data Requirements

### Commands Tab
- Uses command buttons - no server data needed
- Player sends `.citysiege start/stop/cleanup` commands

### Map Tab
- **Background texture**: Requires manual map images (see `Media/MAP_TEXTURES_README.md`)
- **City markers**: Uses coordinate data from `Constants.lua`
- **NPC positions**: Would require position broadcast (not yet implemented)
- **Current display**: Tactical overlay with grid

### Info Tab
- **Active sieges**: Populated from UPDATE messages
- **City name**: From cityId lookup
- **Phase**: From UPDATE message
- **Attacker/Defender counts**: From UPDATE message  
- **Elapsed time**: From UPDATE message
- **Status**: "Active" when receiving updates

### Stats Tab
- **Personal statistics**: Stored in addon's saved variables
- **Not populated by server** - tracks player's own participation
- Increments when player joins/leaves sieges

## Implementation Details

### Server-Side (`mod-city-siege.cpp`)

**Function: `BroadcastSiegeDataToAddon()`**
- Location: Line ~768
- Sends SILENT messages using `WorldPacket` with `SMSG_MESSAGECHAT`
- Messages are invisible to players
- Only sent to players within 500 yards of siege center
- Called every 30 seconds from `UpdateSiegeEvents()`

**Broadcast Timer:**
- Added `lastAddonBroadcast` field to `SiegeEvent` struct
- Initialized to 0 when siege starts
- Checked every game update tick
- Broadcasts when 30+ seconds elapsed

### Client-Side (`EventHandler.lua`)

**Message Interception:**
- Registers for `CHAT_MSG_SYSTEM` events
- Checks for `CITYSIEGE_` prefix
- Strips prefix and parses message format
- Routes to appropriate handler (START/UPDATE/END)

**Handlers:**
- `HandleSiegeStart()`: Creates new siege in tracker
- `HandleSiegeUpdate()`: Updates existing siege data
- `HandleSiegeEnd()`: Removes siege from tracker
- All handlers update `MainFrame:UpdateSiegeDisplay()`

## Testing

### Test Command
```lua
/cs testmap 0
```
Injects fake siege data to test UI without server

### Manual Sync
```lua
/cs sync 0
```
Requests server status and creates siege entry

### Debug Mode
```lua
/cs debug
```
Enables debug logging to see parsed messages

## Troubleshooting

### No Data in Tabs
1. Check EventHandler initialized: Look for "Event Handler initialized" in chat
2. Enable debug mode: `/cs debug`
3. Start a siege: `.citysiege start stormwind`
4. Wait 5 seconds for first UPDATE broadcast
5. Check debug output for "Received:" messages

### Messages Visible in Chat
This is a BUG - messages should be invisible. Check that server is using `WorldPacket` broadcast, not `PSendSysMessage()`.

### Settings Button Takes 2 Clicks
Fixed in `SettingsPanel.lua` - `Toggle()` now creates frame before checking `IsShown()`

### Map Shows No Texture
Expected - WoW 3.3.5 addons cannot access built-in map BLP files. The tactical overlay (dark background + grid) is the default display.

## Future Enhancements

- Position broadcasts for individual NPCs
- Player position tracking for map display
- Waypoint path visualization
- Kill/death event broadcasts for real-time stats
- City leader health percentage
- Objective capture progress
