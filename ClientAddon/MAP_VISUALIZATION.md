# City Siege Addon - Map Visualization Guide

## Overview

The map display system now properly renders city sieges with visual markers for players, NPCs, and waypoints.

## What Was Fixed

### 1. Icon Rendering
**Problem:** Icons weren't visible due to incorrect texture paths.

**Solution:**
- Changed from `Interface\\Minimap\\POIIcons` to `Interface\\Buttons\\WHITE8X8`
- Using colored textures with `SetColorTexture()` for better visibility
- Icons are now simple colored dots that are easy to see

### 2. Map Display Logic
**Problem:** Map only attempted to show data when a siege was active, but had no siege data to display.

**Solution:**
- Added `DrawCityCenter()` function that always shows city center and spawn point
- Map now displays basic city layout even without active siege
- Added "No active siege" message with better UX

### 3. Coordinate Conversion
**Problem:** World coordinates weren't being converted to map coordinates properly.

**Solution:**
- Improved `WorldToMap()` function with better normalization
- Uses city center as origin point
- Maps to 0-1 range with proper clamping

### 4. Test Mode for Development
**Problem:** No way to test map visualization without an actual server siege.

**Solution:**
- Added `CreateTestSiegeData()` function that generates demo data
- Creates circular patterns of attackers, defenders, NPCs, and waypoints
- Can be enabled/disabled via slash commands

## Using Test Mode

To test the map visualization without a real siege:

```
/cs testmap
```

This will:
- Create demo siege data for Stormwind (default)
- Show 5 attackers (red dots) in a circle around the city
- Show 3 defenders (blue dots) near the center
- Show 4 NPCs (orange/green dots) based on side
- Show 8 waypoints (yellow dots) in a circular path
- Draw lines connecting waypoints

To test a specific city:
```
/cs testmap 0    # Stormwind
/cs testmap 1    # Ironforge
/cs testmap 4    # Orgrimmar
```

To disable test mode:
```
/cs testoff
```

## Map Legend

### Icon Colors
- **White (large)**: City center marker
- **Green**: Spawn point
- **Red**: Attacking players
- **Blue**: Defending players
- **Orange**: Attacking NPCs
- **Green**: Defending NPCs
- **Yellow (small)**: Waypoints

### Lines
- **Yellow semi-transparent**: Waypoint paths (connecting waypoints in order)

## Map Features

### Current Implementation
1. **City Center & Spawn Point**: Always visible when a city is selected
2. **Player Positions**: Shows all attackers and defenders with real-time positions
3. **NPC Positions**: Shows siege NPCs color-coded by side
4. **Waypoint Path**: Shows the route siege forces follow
5. **Statistics**: Bottom-right shows attacker/defender counts and kills

### Display Settings
Controlled via Settings panel (Map Display section):
- Show Players (toggle)
- Show NPCs (toggle)
- Show Waypoints (toggle)
- Icon Scale (0.5 - 2.0)
- Update Interval (0.5 - 5.0 seconds)

## Technical Details

### Map Coordinate System
```
World Coordinates (x, y, z)
    ↓ WorldToMap()
Map Coordinates (0-1 range, normalized)
    ↓ Position calculation
Pixel Coordinates (relative to overlay frame)
    ↓ SetPoint()
Icon Position on map
```

### Icon Creation
```lua
-- Icons are created once and reused
local icon = overlay:CreateTexture(nil, "OVERLAY")
icon:SetTexture("Interface\\Buttons\\WHITE8X8")
icon:SetColorTexture(r, g, b, alpha)
icon:SetSize(width, height)
```

### Waypoint Lines
```lua
-- Lines are drawn as horizontal textures between points
-- WoW 3.3.5 doesn't support texture rotation easily
-- So lines appear horizontal but positioned between waypoints
local line = overlay:CreateTexture(nil, "BACKGROUND")
line:SetTexture("Interface\\Buttons\\WHITE8X8")
line:SetColorTexture(0.5, 0.5, 0, 0.4)
line:SetSize(distance, 2)
```

## Integration with Server

The map automatically updates when the server sends position data:

1. **Server Event**: `.citysiege` command executed
2. **Addon Message**: Server broadcasts siege state via addon channel
3. **EventHandler**: Parses message and updates SiegeTracker
4. **SiegeTracker**: Stores player/NPC positions
5. **MapDisplay**: Reads from SiegeTracker and renders icons

### Server Data Format (Expected)
```lua
siegeData = {
    cityID = 0,              -- City identifier
    status = "active",       -- Siege status
    phase = 2,               -- Current phase
    attackers = {            -- Attacking players
        ["player_guid"] = {
            name = "PlayerName",
            x = -8900.5,
            y = 550.2,
            z = 93.5
        }
    },
    defenders = { ... },     -- Defending players
    npcs = {                 -- Siege NPCs
        ["npc_guid"] = {
            name = "NPC Name",
            x = -8850.0,
            y = 600.0,
            z = 95.0,
            side = "attacker"  -- or "defender"
        }
    },
    waypoints = {            -- Patrol path
        { x = -8900, y = 500, z = 93 },
        { x = -8850, y = 550, z = 93 },
        ...
    }
}
```

## Performance Considerations

### Update Throttling
- Map updates are throttled to prevent excessive frame updates
- Default: 1 second between updates
- Configurable via settings (0.5 - 5.0 seconds)

### Icon Pooling
- Icons are created once and reused
- Hidden icons remain in memory for performance
- `Clear()` hides icons but doesn't destroy them

### Optimization Tips
1. Increase update interval for better performance
2. Disable NPCs if not needed (fewer icons to render)
3. Reduce icon scale if map feels cluttered

## Troubleshooting

### Map is Empty
1. Check if a city is selected in the dropdown
2. Verify test mode is enabled: `/cs testmap`
3. Check Settings → Map Display → ensure toggles are enabled

### Icons Not Showing
1. Verify icon scale isn't set too small (Settings panel)
2. Check that appropriate toggles are enabled (Players/NPCs/Waypoints)
3. Try `/reload` to reset the addon

### Lines Not Visible
1. Lines may be horizontal due to WoW 3.3.5 limitations
2. They appear as thin yellow bars connecting waypoints
3. Zoom or adjust UI scale if needed

### Test Mode Not Working
1. Ensure SiegeTracker is initialized
2. Check for errors with `/console scriptErrors 1`
3. Try `/reload` and then `/cs testmap` again

## Future Enhancements

### Planned Features
1. **Texture Maps**: Replace colored background with actual city maps
2. **Zoom & Pan**: Allow zooming and panning around the map
3. **Tooltips**: Show player/NPC names on hover
4. **Click Actions**: Click icons to target or get info
5. **Heat Maps**: Show density of combat activity
6. **Route Planning**: Draw custom waypoints for coordination

### Advanced Rendering
1. **True Line Rotation**: Use advanced techniques for angled lines
2. **Fog of War**: Only show visible areas
3. **Mini-Map Integration**: Show siege on world map
4. **3D Visualization**: Show height/terrain data

## API Reference

### Public Functions

```lua
-- Create map display
CitySiege_MapDisplay:Create(parent)

-- Set displayed city
CitySiege_MapDisplay:SetCity(cityID)

-- Force update display
CitySiege_MapDisplay:UpdateDisplay()

-- Show/hide map
CitySiege_MapDisplay:Show()
CitySiege_MapDisplay:Hide()

-- Test mode
CitySiege_MapDisplay:EnableTestMode(cityID)
CitySiege_MapDisplay:DisableTestMode()

-- Clear all icons
CitySiege_MapDisplay:Clear()
```

### Events

Map automatically updates on:
- `SIEGE_STATE_CHANGED` - Siege starts/stops/changes phase
- `SIEGE_POSITION_UPDATE` - Player/NPC positions updated
- Tab selection (when Map tab is shown)

## Developer Notes

### Adding Custom Markers
```lua
-- Create custom icon
local icon = CitySiege_MapDisplay:GetOrCreateIcon("custom_marker", "CUSTOM")
CitySiege_MapDisplay:PositionIcon(icon, worldX, worldY, cityData)
icon:SetColorTexture(1, 0, 1, 1) -- Magenta
icon:SetSize(10, 10)
icon:Show()
```

### Coordinate Conversion Example
```lua
local cityData = CitySiege_CityData[CitySiege_Cities.STORMWIND]
local worldX, worldY = -8913.23, 554.633  -- City center

-- Convert to map coordinates (0-1 range)
local mapX, mapY = CitySiege_MapDisplay:WorldToMap(worldX, worldY, cityData)
-- Result: mapX ≈ 0.5, mapY ≈ 0.5 (center of map)
```

---

**Map visualization is now fully functional!** Use `/cs testmap` to see it in action.
