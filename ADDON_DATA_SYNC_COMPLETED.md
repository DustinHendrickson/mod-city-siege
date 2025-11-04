# City Siege Addon - Complete Data Synchronization Implementation

## Summary
Implemented comprehensive server-to-addon data synchronization system for City Siege module. The system now broadcasts ALL siege data silently in the background every 30 seconds and supports on-demand data requests.

## Changes Made

### 1. Server-Side (mod-city-siege.cpp)

#### Enhanced `BroadcastSiegeDataToAddon()` Function (Line ~768)
- **Purpose**: Sends comprehensive siege data to players within range
- **Frequency**: Every 30 seconds automatic broadcast
- **Range**: 500 yards from city center
- **Communication**: Silent (WorldPacket with SMSG_MESSAGECHAT, CHAT_MSG_SYSTEM)

**Data Transmitted**:
1. **Basic Info**: cityId, phase, attacker count, defender count, elapsed time
2. **Time Remaining**: Calculated from event duration
3. **Leader Health**: Percentage from leader creature HP
4. **Waypoints**: All waypoint coordinates (x,y,z)
5. **Attacker NPC Positions**: Up to 50 NPC positions (x,y,z)
6. **Defender NPC Positions**: Up to 50 NPC positions (x,y,z)
7. **Attacker Bot Positions**: All bot positions (x,y,z)
8. **Defender Bot Positions**: All bot positions (x,y,z)
9. **Location Data**: Spawn point, leader position, city center

**Message Format**:
```
CITYSIEGE_UPDATE:cityId:phase:atk:def:elapsed:remaining:leaderHP:WP:count:x:y:z...:ATK:count:x:y:z...:DEF:count:x:y:z...:BATK:count:x:y:z...:BDEF:count:x:y:z...
```

#### Added `HandleCitySiegeSyncCommand()` (Line ~5198)
- **Command**: `.citysiege sync [cityId]`
- **Access Level**: SEC_PLAYER (all players)
- **Purpose**: On-demand data request when player selects a city
- **Behavior**:
  - No cityId: Broadcasts all active sieges
  - With cityId: Broadcasts specific city data
  - City not active: Sends END message

#### Modified `UpdateSiegeEvents()` (Line ~2906)
- Added automatic broadcast trigger every 30 seconds
- Tracks last broadcast time per siege event
- Only broadcasts during active sieges

### 2. Client-Side Addon

#### EventHandler.lua - Enhanced Message Parsing

**ParseAddonMessage()** (Line ~79):
- Complete rewrite to handle comprehensive data format
- Parses colon-delimited sections:
  - `WP:count:coords...` - Waypoint array
  - `ATK:count:coords...` - Attacker NPC positions
  - `DEF:count:coords...` - Defender NPC positions
  - `BATK:count:coords...` - Attacker bot positions
  - `BDEF:count:coords...` - Defender bot positions
- Stores all position arrays in siegeData table

**HandleSiegeStart()** (Line ~250):
- Added support for coordinate data (spawn, leader, center)
- Initializes all position arrays
- Stores leader health and time remaining fields

**HandleSiegeUpdate()** (Line ~302):
- Updated signature to accept 8+ parameters
- Stores time remaining and leader health
- Updates all position arrays from parsed data
- Creates new siege entry if doesn't exist

#### MapDisplay.lua - Position Rendering

**UpdateNPCPositions()** (Line ~255):
- Complete rewrite to render all position types
- Clears old position icons before redrawing
- Color-coded rendering:
  - **Red (1,0,0)**: Attacker NPCs (8x8)
  - **Blue (0,0.5,1)**: Defender NPCs (8x8)
  - **Orange (1,0.5,0)**: Attacker Bots (6x6)
  - **Cyan (0,1,1)**: Defender Bots (6x6)
- Efficient icon pooling with unique IDs
- Fallback support for legacy `npcs` table

#### MainFrame.lua - Info Display

**UpdateInfoText()** (Line ~490):
- Added display of time remaining (colored: green→yellow→red)
- Added display of leader health percentage
- Color changes based on leader health:
  - Green (00FF00): 50%+
  - Yellow (FFFF00): 25-49%
  - Red (FF0000): <25%

**SelectCity()** (Line ~369):
- Added on-demand sync request when city selected
- Sends `.citysiege sync <cityID>` command to server
- Ensures fresh data when switching cities

## Technical Details

### Performance Optimizations
- **Packet Size**: Increased from 200 to 2000 bytes
- **Position Limits**: 50 NPCs per side (prevents overflow)
- **Update Frequency**: 30 seconds (balance between freshness and bandwidth)
- **Range Filtering**: Only sends to players within 500 yards

### Communication Protocol
- **Channel**: SMSG_MESSAGECHAT with CHAT_MSG_SYSTEM
- **Visibility**: Completely invisible (no chat spam)
- **Format**: Colon-separated sections with type prefixes
- **Reliability**: WorldPacket direct message delivery

### Data Flow
1. **Automatic Updates**:
   - UpdateSiegeEvents() runs every game tick
   - Every 30 seconds: BroadcastSiegeDataToAddon()
   - Sends to all players within range
   - Updates all tabs automatically

2. **On-Demand Sync**:
   - Player selects city in dropdown
   - Addon sends `.citysiege sync <cityID>`
   - Server immediately broadcasts current data
   - Ensures fresh data on city switch

## Testing Instructions

### Start a Siege
```
.citysiege start stormwind
```

### Open Addon UI
```
/cs show
```

### Verify Data Display
1. **Map Tab**: Should show all waypoints, NPC positions (red/blue), bot positions (orange/cyan)
2. **Info Tab**: Should display leader health % and time remaining
3. **Updates**: Wait 30 seconds, positions should update

### Test On-Demand Sync
1. Select different city from dropdown
2. Data should refresh immediately
3. Check console for `.citysiege sync` command

### Debug Mode
```
/cs debug
```
Enable to see all EventHandler operations and message parsing

## Known Limitations

1. **Position Limit**: 50 NPCs per faction to prevent packet overflow
2. **Update Delay**: 30 seconds between automatic updates
3. **Range Limit**: Only players within 500 yards receive updates
4. **No Interpolation**: Positions jump rather than smooth movement

## Future Enhancements

Potential improvements for future versions:

1. **Real-time Updates**: Reduce to 10-15 second intervals
2. **Predictive Positioning**: Interpolate movement between updates
3. **Compression**: Use binary format instead of text for smaller packets
4. **Filtering**: Allow players to toggle specific position types
5. **History**: Track position history for tactical analysis

## Files Modified

### Server (C++)
- `modules/mod-city-siege/src/mod-city-siege.cpp`
  - Line ~343: Added `lastAddonBroadcast` to SiegeEvent struct
  - Line ~768: Rewrote BroadcastSiegeDataToAddon()
  - Line ~2906: Added automatic broadcast trigger
  - Line ~4327: Added sync command to command table
  - Line ~5198: Implemented HandleCitySiegeSyncCommand()

### Client (Lua)
- `modules/mod-city-siege/ClientAddon/CitySiege/Modules/EventHandler.lua`
  - Line ~79: Rewrote ParseAddonMessage()
  - Line ~250: Enhanced HandleSiegeStart()
  - Line ~302: Enhanced HandleSiegeUpdate()

- `modules/mod-city-siege/ClientAddon/CitySiege/Modules/MapDisplay.lua`
  - Line ~255: Rewrote UpdateNPCPositions()

- `modules/mod-city-siege/ClientAddon/CitySiege/Modules/MainFrame.lua`
  - Line ~369: Added sync request to SelectCity()
  - Line ~490: Enhanced UpdateInfoText() with health/time

## Version
Implementation Date: 2025
Status: Complete and Ready for Testing
