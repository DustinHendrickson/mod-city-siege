# City Siege Addon - UI Layout Fixes & Server Communication

## UI Improvements Made

### 1. Command Panel Layout ✅
**Fixed:**
- Added proper container sections for city selector and buttons
- Improved spacing: 42px between buttons (was 40px)
- Better padding: 25px from top, organized sections
- Larger, more readable buttons: 220x28 (was 200x25)
- Improved font rendering with outline
- Centered layout with clear visual hierarchy

**New Structure:**
```
Title (25px from top)
    ↓ 20px
City Section (container)
├─ Label: "Select City"
└─ Dropdown (8px below label)
    ↓ 15px
Buttons Container (6 buttons, 42px spacing)
├─ Start Siege
├─ Stop Siege
├─ Cleanup NPCs
├─ Show Status
├─ Detailed Info
└─ Reload Config
    ↓ auto
Warning text (25px from bottom)
```

### 2. Tab System Overhaul ✅
**Problem:** Character tab textures were stretched and looked terrible

**Solution:**
- Replaced `UI-Character-Tab-*` with proper button templates
- Using `Interface\\Buttons\\UI-Panel-Button-*` textures
- Proper texture coordinates (0, 0.625, 0, 0.6875) to prevent stretching
- Color-coded text: White for selected, Light grey for inactive
- Larger tabs: 110x28 (was 100x25) with 115px spacing
- Smooth state transitions

**Visual Result:**
- Clean, professional button appearance
- No more stretched/distorted textures
- Clear selected state
- Better spacing between tabs

### 3. Overall Layout Improvements
- Content area: 850x600 (increased from 830x550)
- Proper padding: 10px from frame edges, 5px internal
- Grey backgrounds for better visual separation
- Consistent spacing throughout

## Server Communication Issues

### Current Status
The mod-city-siege server module **does NOT** have built-in addon communication via:
- `SendAddonMessage()`
- SMSG packets
- Channel broadcasts

### What Exists
The module has:
- `.citysiege start/stop/cleanup/status/info/reload` commands
- Chat message outputs (PSendSysMessage)
- Internal siege data structures
- Active siege tracking

### Solutions

#### Option 1: Parse Chat Messages (Current Approach)
The addon can parse chat output from commands:
```lua
-- EventHandler.lua
function EventHandler:CHAT_MSG_SYSTEM(message)
    if message:match("City Siege") then
        -- Parse siege status
        self:ParseSiegeStatus(message)
    end
end
```

**Pros:** No server changes needed
**Cons:** Unreliable, brittle, depends on message format

#### Option 2: Add Addon Communication to Module (Recommended)
Modify `mod-city-siege.cpp` to broadcast siege data:

```cpp
// In HandleCitySiegeStatusCommand or UpdateSiegeEvent
void BroadcastSiegeDataToAddons(CitySiegeEvent& event)
{
    std::ostringstream ss;
    ss << "SIEGE:" << event.cityId << ":"
       << event.phase << ":"
       << event.attackerCount << ":"
       << event.defenderCount;
    
    WorldPacket data;
    data.Initialize(SMSG_ADDON_MESSAGE);
    data << "CitySiege";
    data << ss.str();
    
    // Send to all players in the city or worldwide
    sWorld->SendGlobalMessage(&data);
}
```

**Pros:** Reliable, proper implementation, real-time updates
**Cons:** Requires C++ module changes

#### Option 3: Add Custom Commands for Addon
Add special command that outputs structured data:

```cpp
// .citysiege addondata
// Outputs: "ADDON_DATA:cityId:phase:attackers:defenders:..."
```

### Data Needed by Addon

The addon needs access to:

1. **Active Sieges List**
   - City ID
   - Status (active/paused/completed)
   - Phase (1-4)
   - Duration/elapsed time
   - Attacking faction

2. **Participant Counts**
   - Number of attackers
   - Number of defenders
   - Number of NPCs per side

3. **Map Visualization** (for Map tab)
   - Player positions (x, y, z coordinates)
   - NPC positions
   - Waypoint data (already in Constants.lua)
   - Current waypoint progress

4. **Statistics** (for Stats tab)
   - Kill counts per side
   - Player-specific stats if available

### Recommendation

**For immediate use:** Use test mode (`/cs testmap`) to visualize features

**For production:** I recommend modifying the server module to add:

1. **Broadcast function** that sends addon messages when:
   - Siege starts/stops
   - Phase changes
   - Every 5-10 seconds for position updates

2. **Data packet format:**
   ```
   PREFIX: "CitySiege"
   
   SIEGE_START: "START:cityId:faction"
   SIEGE_UPDATE: "UPDATE:cityId:phase:attackers:defenders:kills_atk:kills_def"
   SIEGE_END: "END:cityId:winner"
   POSITION_UPDATE: "POS:guid:x:y:z:type" (type = ATTACKER/DEFENDER/NPC)
   ```

3. **Addon registration:**
   ```lua
   -- In Core.lua OnInitialize
   RegisterAddonMessagePrefix("CitySiege")
   
   -- In EventHandler.lua
   function EventHandler:CHAT_MSG_ADDON(prefix, message, channel, sender)
       if prefix == "CitySiege" then
           self:ParseAddonMessage(message)
       end
   end
   ```

## Current Workaround

Until server communication is implemented, the addon uses:

1. **Test Mode** - `/cs testmap` generates demo data
2. **Manual Updates** - Commands trigger manual refresh
3. **Local State** - SiegeTracker maintains local state

## Files Modified

1. `CommandPanel.lua` - Layout improvements, better spacing
2. `MainFrame.lua` - Tab system overhaul, proper button textures
3. `MapDisplay.lua` - Already supports visualization when data exists
4. `Info/Stats tabs` - Show helpful content even without siege data

## Testing

Current features work without server data:
- ✅ UI layout and spacing
- ✅ Tab system and navigation
- ✅ Command buttons (send commands, but no feedback)
- ✅ Test mode map visualization (`/cs testmap`)
- ✅ Info tab (shows city list, explanations)
- ✅ Stats tab (shows template, updates when data exists)

Features that need server data:
- ❌ Real-time siege status
- ❌ Actual player/NPC positions on map
- ❌ Live participant counts
- ❌ Kill statistics
- ❌ Win/loss tracking

## Next Steps

1. **Immediate:** Test current UI improvements in-game
2. **Short-term:** Implement chat message parsing for basic status
3. **Long-term:** Add proper addon communication to server module
