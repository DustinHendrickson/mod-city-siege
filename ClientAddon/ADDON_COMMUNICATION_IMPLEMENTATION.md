# Addon Communication Implementation

## Overview
This document describes the implementation of server-to-client communication for the City Siege addon. The module now broadcasts siege data to clients, allowing the addon to display real-time information without test data.

## Status: ✅ IMPLEMENTED

## Changes Made

### Server Side (C++)

#### 1. Added Broadcast Functions (`mod-city-siege.cpp` ~line 760)

Two new functions were added to broadcast siege data to clients:

```cpp
void BroadcastSiegeDataToAddon(const SiegeEvent& event, const std::string& messageType)
```
- Broadcasts siege events (START, UPDATE, END) to all players within 500 yards of the siege
- Sends data via `PSendSysMessage` prefixed with `CITYSIEGE_`
- Message formats:
  - **START**: `CITYSIEGE_START:cityId:faction`
    - Example: `CITYSIEGE_START:0:Horde`
  - **UPDATE**: `CITYSIEGE_UPDATE:cityId:phase:attackers:defenders:elapsed`
    - Example: `CITYSIEGE_UPDATE:0:2:15:8:450`
  - **END**: `CITYSIEGE_END:cityId:winner`
    - Example: `CITYSIEGE_END:0:unknown`

```cpp
void BroadcastPositionUpdate(const SiegeEvent& event, ObjectGuid guid, float x, float y, float z, const std::string& unitType)
```
- Broadcasts NPC/player positions for map visualization
- Message format:
  - **POS**: `CITYSIEGE_POS:cityId:guid:x:y:z:type`
    - Example: `CITYSIEGE_POS:0:123456:1234.56:-4321.12:50.25:ATTACKER`

#### 2. Integration Points

**Siege Start** (~line 2095):
```cpp
g_ActiveSieges.push_back(newEvent);
BroadcastSiegeDataToAddon(g_ActiveSieges.back(), "START");
```

**Siege Updates** (~line 4033):
```cpp
sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, statusMsg);
BroadcastSiegeDataToAddon(event, "UPDATE");
```
- Updates broadcast every 5 minutes during active siege phase

**Siege End** (~line 2148):
```cpp
DespawnSiegeCreatures(event);
AnnounceSiege(city, false);
BroadcastSiegeDataToAddon(event, "END");
```

### Client Side (Lua)

#### 1. Event Listener (`EventHandler.lua`)

**System Message Interceptor**:
```lua
function EventHandler:Initialize()
    -- Creates event frame to intercept CHAT_MSG_SYSTEM messages
    -- Filters for messages starting with "CITYSIEGE_"
    -- Strips prefix and passes to ParseAddonMessage()
end
```

**Message Parser**:
```lua
function EventHandler:ParseAddonMessage(message)
    -- Uses string.match() to extract command and data
    -- Supports: START, UPDATE, END, POS commands
    -- Calls appropriate handler functions
end
```

**Handler Functions**:
- `HandleSiegeStart(cityID, faction)` - Creates new siege entry in tracker
- `HandleSiegeUpdate(cityID, phase, attackers, defenders, elapsed)` - Updates siege state
- `HandleSiegeEnd(cityID, winner)` - Removes siege from tracker
- `HandlePositionUpdate(cityID, guid, x, y, z, unitType)` - Updates NPC positions

#### 2. Test Mode Removed (`MapDisplay.lua`)

Deleted functions:
- `CreateTestSiegeData()` - Generated fake siege data
- `EnableTestMode()` - Injected test data
- `DisableTestMode()` - Cleared test data

The addon now **only displays real server data**.

## Data Flow

```
Server Module (C++)
    ↓
StartSiegeEvent() / UpdateSiegeEvents() / EndSiegeEvent()
    ↓
BroadcastSiegeDataToAddon(event, "START"|"UPDATE"|"END")
    ↓
PSendSysMessage("CITYSIEGE_START:0:Horde")
    ↓
Client receives CHAT_MSG_SYSTEM
    ↓
EventHandler:OnChatMessage() filters "CITYSIEGE_" prefix
    ↓
EventHandler:ParseAddonMessage() extracts command & data
    ↓
HandleSiegeStart/Update/End() modifies SiegeTracker data
    ↓
UI updates automatically (MainFrame, MapDisplay, etc.)
```

## Message Format Specification

### START Message
Format: `CITYSIEGE_START:cityId:faction`
- `cityId`: Integer (0-7 for the 8 cities)
- `faction`: String ("Alliance" or "Horde")

Example: `CITYSIEGE_START:0:Horde` (Horde attacking Stormwind)

### UPDATE Message
Format: `CITYSIEGE_UPDATE:cityId:phase:attackers:defenders:elapsed`
- `cityId`: Integer city ID
- `phase`: Integer (1-4)
- `attackers`: Integer count of attacking NPCs
- `defenders`: Integer count of defending NPCs
- `elapsed`: Integer seconds since siege started

Example: `CITYSIEGE_UPDATE:0:2:15:8:450` (Phase 2, 15 attackers, 8 defenders, 7.5 minutes elapsed)

### END Message
Format: `CITYSIEGE_END:cityId:winner`
- `cityId`: Integer city ID
- `winner`: String ("unknown" currently, can be expanded)

Example: `CITYSIEGE_END:0:unknown`

### POS Message (Future Enhancement)
Format: `CITYSIEGE_POS:cityId:guid:x:y:z:type`
- `cityId`: Integer city ID
- `guid`: Integer GUID of unit
- `x, y, z`: Float coordinates
- `type`: String ("ATTACKER", "DEFENDER", "NPC")

Example: `CITYSIEGE_POS:0:123456:1234.56:-4321.12:50.25:ATTACKER`

## Testing Instructions

### 1. Compile the Module
```bash
./acore.sh compiler build
```

### 2. Install the Addon
Copy `ClientAddon/CitySiege/` to `WoW/Interface/AddOns/CitySiege/`

### 3. Start a Siege
```
.citysiege start 0
```
(0 = Stormwind, see city IDs in Constants.lua)

### 4. Verify Communication
Open the addon UI and check:
- **Info Tab**: Should show "Status: Active" with real siege data
- **Stats Tab**: Should show phase, attacker/defender counts
- **Map Tab**: Should display city layout (positions require POS messages)
- **Debug Mode**: Enable in Config.lua to see debug messages

### 5. Watch Updates
Wait 5 minutes - addon should receive UPDATE message and refresh stats automatically

### 6. End the Siege
```
.citysiege stop 0
```
Addon should receive END message and clear the siege display

## Broadcast Frequency

| Event Type | Frequency | Trigger |
|------------|-----------|---------|
| START | Once | When `.citysiege start` is executed |
| UPDATE | Every 5 minutes | During active siege phase (not RP phase) |
| END | Once | When siege ends (time expired or leader killed) |
| POS | Not implemented yet | Would be every 5-10 seconds for real-time tracking |

## Future Enhancements

### Position Broadcasting
Add to `UpdateSiegeEvents()` to send positions every 10 seconds:
```cpp
// Add after status announcement block
static uint32 lastPositionBroadcast = 0;
if (currentTime - lastPositionBroadcast >= 10) {
    lastPositionBroadcast = currentTime;
    
    // Broadcast attacker positions
    for (const auto& guid : event.spawnedCreatures) {
        Creature* creature = map->GetCreature(guid);
        if (creature && creature->IsAlive()) {
            BroadcastPositionUpdate(event, guid, 
                creature->GetPositionX(), 
                creature->GetPositionY(), 
                creature->GetPositionZ(), 
                "ATTACKER");
        }
    }
    
    // Broadcast defender positions
    for (const auto& guid : event.spawnedDefenders) {
        Creature* creature = map->GetCreature(guid);
        if (creature && creature->IsAlive()) {
            BroadcastPositionUpdate(event, guid, 
                creature->GetPositionX(), 
                creature->GetPositionY(), 
                creature->GetPositionZ(), 
                "DEFENDER");
        }
    }
}
```

### Winner Determination
Modify `EndSiegeEvent()` to pass winner info to END message:
```cpp
std::string winner = defendersWon ? "Defenders" : "Attackers";
std::ostringstream ss;
ss << "END:" << static_cast<uint32>(event.cityId) << ":" << winner;
```

### Waypoint Broadcasting
Send waypoint data when siege starts so map can display the path:
```cpp
// Add to StartSiegeEvent after BroadcastSiegeDataToAddon(START)
for (size_t i = 0; i < city->waypoints.size(); i++) {
    const Waypoint& wp = city->waypoints[i];
    std::ostringstream wpMsg;
    wpMsg << "WAYPOINT:" << static_cast<uint32>(event.cityId) << ":" 
          << i << ":" << wp.x << ":" << wp.y << ":" << wp.z;
    // Broadcast wpMsg
}
```

## Known Limitations

1. **No Channel Validation**: Messages use CHAT_MSG_SYSTEM, not true addon channels (SMSG_ADDON_MESSAGE)
2. **Range Limited**: Only players within 500 yards receive messages
3. **No Persistence**: If player logs in during active siege, they don't receive START message
4. **Position Updates**: Not implemented yet (map won't show real-time NPC positions)
5. **Winner Info**: END message doesn't specify which side won

## Security Considerations

- Messages use one-way communication (server → client)
- No client input is processed for siege state
- Players cannot spoof siege events
- Range limit prevents spam to distant players

## Debugging

Enable debug mode in addon:
```lua
-- Config.lua
CitySiegeConfig.debugMode = true
```

Then check for messages in chat:
```
[City Siege] Siege started at city 0 by Horde
[City Siege] Sent addon message to PlayerName: CITYSIEGE_START:0:Horde
```

Server-side debug (if g_DebugMode enabled):
```
[City Siege] Sent addon message to PlayerName: CITYSIEGE_START:0:Horde
```

## Conclusion

The addon communication system is now fully functional for basic siege tracking. Players can see:
- ✅ When sieges start
- ✅ Which faction is attacking
- ✅ Current phase and participant counts
- ✅ When sieges end

No more test data - all information comes directly from the server module in real-time.

For full map visualization with NPC positions, implement the position broadcasting enhancement described above.
