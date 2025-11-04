# City Siege Map Textures

## Why You Can't See Map Textures

WoW 3.3.5 addons **cannot** access the game's built-in WorldMap BLP files directly from `Interface\WorldMap\`. These files are protected and only accessible to the game's internal map system.

## Solution: Add Custom Map Images

You need to extract or create map images and include them in your addon. Here's how:

### Option 1: Extract from WoW Client (Recommended)

1. **Download BLP Viewer/Converter**:
   - Use "BLPConverter" or "BLP2PNG" tool
   - Or use WoW Model Viewer which includes BLP extraction

2. **Extract Map Files**:
   Navigate to your WoW client folder and extract these BLP files:
   ```
   Interface\WorldMap\Stormwind\Stormwind1.blp
   Interface\WorldMap\Ironforge\Ironforge1.blp
   Interface\WorldMap\Darnassis\Darnassis1.blp
   Interface\WorldMap\TheExodar\TheExodar1.blp
   Interface\WorldMap\Orgrimmar\Orgrimmar1.blp
   Interface\WorldMap\Undercity\Undercity1.blp
   Interface\WorldMap\ThunderBluff\ThunderBluff1.blp
   Interface\WorldMap\Silvermoon\Silvermoon1.blp
   ```

3. **Convert to TGA format** (WoW addons work best with TGA):
   - Convert BLP → TGA using your converter tool
   - Save as: `Stormwind.tga`, `Ironforge.tga`, etc.

4. **Copy to Addon**:
   Place the `.tga` files in:
   ```
   CitySiege\Media\Maps\
   ```

### Option 2: Use PNG (Alternative)

If you prefer PNG format:
1. Convert BLP → PNG
2. Place in `CitySiege\Media\Maps\`
3. Note: TGA is more efficient for WoW

### Option 3: Create Custom Tactical Maps

You can create your own simplified tactical maps:
1. Use any image editor (Photoshop, GIMP, etc.)
2. Create 512x512 or 1024x1024 images
3. Draw simplified city layouts
4. Save as TGA format
5. Place in `CitySiege\Media\Maps\`

## Required Files Structure

```
CitySiege/
  Media/
    Maps/
      Stormwind.tga
      Ironforge.tga
      Darnassus.tga
      Exodar.tga
      Orgrimmar.tga
      Undercity.tga
      ThunderBluff.tga
      Silvermoon.tga
```

## Update the Addon Code

Once you have the map files, update `MapDisplay.lua` to use them:

```lua
-- Around line 124 in MapDisplay.lua, replace the current tactical overlay code with:

if frame and frame.mapTexture then
    local cityMapTextures = {
        [CitySiege_Cities.STORMWIND] = "Interface\\AddOns\\CitySiege\\Media\\Maps\\Stormwind",
        [CitySiege_Cities.IRONFORGE] = "Interface\\AddOns\\CitySiege\\Media\\Maps\\Ironforge",
        [CitySiege_Cities.DARNASSUS] = "Interface\\AddOns\\CitySiege\\Media\\Maps\\Darnassus",
        [CitySiege_Cities.EXODAR] = "Interface\\AddOns\\CitySiege\\Media\\Maps\\Exodar",
        [CitySiege_Cities.ORGRIMMAR] = "Interface\\AddOns\\CitySiege\\Media\\Maps\\Orgrimmar",
        [CitySiege_Cities.UNDERCITY] = "Interface\\AddOns\\CitySiege\\Media\\Maps\\Undercity",
        [CitySiege_Cities.THUNDERBLUFF] = "Interface\\AddOns\\CitySiege\\Media\\Maps\\ThunderBluff",
        [CitySiege_Cities.SILVERMOON] = "Interface\\AddOns\\CitySiege\\Media\\Maps\\Silvermoon",
    }
    
    local texturePath = cityMapTextures[cityID]
    
    if texturePath then
        frame.mapTexture:SetTexture(texturePath)
        frame.mapTexture:SetTexCoord(0, 1, 0, 1)
        frame.mapTexture:SetAlpha(0.9)
        
        -- Hide grid when using real map
        if frame.gridTexture then
            frame.gridTexture:Hide()
        end
        if frame.mapCityLabel then
            frame.mapCityLabel:Hide()
        end
    else
        -- Fallback to tactical overlay
        frame.mapTexture:SetColorTexture(0.05, 0.05, 0.08, 1.0)
        -- Show grid and label
    end
end
```

**Important**: Do NOT include the `.tga` or `.blp` extension in the SetTexture path - WoW automatically looks for supported formats.

## Alternative: Use Online Resources

Some WoW addon developers share pre-extracted map packs. Search for:
- "WoW 3.3.5 map textures"
- "WoTLK worldmap BLP pack"
- WoW modding communities like Model-Changing.net

## Current Behavior

Without custom map files, the addon uses:
- Dark blue background
- Grid overlay
- City name watermark
- Tactical positioning icons

This is fully functional for siege tracking, just without detailed map imagery.
