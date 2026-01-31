--[[
    City Siege Addon - Map Display
    Shows city maps with waypoints, NPCs, and player positions
]]

CitySiege_MapDisplay = {}
local MapDisplay = CitySiege_MapDisplay

local frame = nil
local currentCityID = nil
local icons = {}
local lines = {}
local updateThrottle = 0

function MapDisplay:Create(parent)
    print("CitySiege MapDisplay: Create() called")
    
    if frame then 
        print("CitySiege MapDisplay: Frame already exists, returning existing frame")
        return frame 
    end
    
    print("CitySiege MapDisplay: Creating new frame")
    
    frame = CreateFrame("Frame", "CitySiegeMapDisplay", parent)
    frame:SetAllPoints(parent)
    frame:Show()
    
    -- Title
    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -15)
    title:SetText("City Map")
    frame.titleText = title
    
    -- Map container - 4:3 aspect ratio to match the actual map
    local mapContainer = CreateFrame("Frame", nil, frame)
    mapContainer:SetPoint("CENTER", frame, "CENTER", -15, -10)
    mapContainer:SetSize(460, 345)  -- Slightly smaller, maintain 4:3
    mapContainer:Show()
    frame.mapContainer = mapContainer
    
    -- Map background - transparent
    local mapBg = mapContainer:CreateTexture(nil, "BACKGROUND")
    mapBg:SetAllPoints(mapContainer)
    mapBg:SetTexture(0, 0, 0, 0)  -- Fully transparent
    frame.mapBg = mapBg
    
    -- Map texture - constrain to exact size
    local mapTexture = mapContainer:CreateTexture(nil, "ARTWORK")
    mapTexture:SetAllPoints(mapContainer)
    frame.mapTexture = mapTexture
    
    -- Border
    mapContainer:SetBackdrop({
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = false,
        edgeSize = 16,
        insets = {left = 4, right = 4, top = 4, bottom = 4},
    })
    mapContainer:SetBackdropBorderColor(0.8, 0.8, 0.8, 1)
    
    -- Overlay frame
    local overlay = CreateFrame("Frame", nil, mapContainer)
    overlay:SetAllPoints(mapContainer)
    overlay:Show()
    frame.overlay = overlay
    
    -- Leader info text (displayed above map)
    local leaderInfo = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    leaderInfo:SetPoint("BOTTOM", mapContainer, "TOP", 0, 5)
    leaderInfo:SetJustifyH("CENTER")
    leaderInfo:SetText("")
    leaderInfo:Show()
    frame.leaderInfo = leaderInfo
    
    -- Legend with professional styling
    local legendFrame = CreateFrame("Frame", nil, frame)
    legendFrame:SetSize(450, 28)
    legendFrame:SetPoint("BOTTOM", frame, "BOTTOM", 0, 12)
    CitySiege_Utils:SetBackdrop(legendFrame, 0.05, 0.05, 0.08, 0.85)
    frame.legendFrame = legendFrame
    
    local legend = legendFrame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    legend:SetPoint("CENTER", 0, 0)
    legend:SetFont("Fonts\\FRIZQT__.TTF", 11, "OUTLINE")
    legend:SetJustifyH("CENTER")
    legend:SetText("|TInterface\\TargetingFrame\\UI-RaidTargetingIcons:18:18:0:0:64:64:48:64:16:32|t |cFFFFD700Leader|r     |TInterface\\TargetingFrame\\UI-RaidTargetingIcons:16:16:0:0:64:64:0:16:16:32|t |cFF00FF00Waypoint|r     |TInterface\\TargetingFrame\\UI-RaidTargetingIcons:16:16:0:0:64:64:32:48:16:32|t |cFF16C3F2Spawn|r")
    legend:Show()
    frame.legend = legend
    
    -- Stats text
    local statsText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    statsText:SetPoint("BOTTOMRIGHT", -20, 15)
    statsText:SetJustifyH("RIGHT")
    statsText:SetText("Select a city from the dropdown")
    statsText:Show()
    frame.statsText = statsText
    
    -- No siege message
    local noSiegeText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    noSiegeText:SetPoint("CENTER", mapContainer, "CENTER", 0, 0)
    noSiegeText:SetText("|cFF808080No active siege\n\nSelect a city|r")
    noSiegeText:SetJustifyH("CENTER")
    noSiegeText:Show()
    frame.noSiegeText = noSiegeText
    
    print("CitySiege: MapDisplay created successfully")
    
    return frame
end

function MapDisplay:SetCity(cityID)
    currentCityID = cityID
    
    print("CitySiege MapDisplay: SetCity called with cityID: " .. tostring(cityID))
    
    if not cityID then
        -- Show placeholder when no city selected
        if frame and frame.mapTexture then
            frame.mapTexture:SetTexture(nil)
            frame.mapTexture:SetTexture(0.05, 0.05, 0.08, 1.0)
            
            if frame.gridTexture then
                frame.gridTexture:Show()
            end
            
            if not frame.noMapText then
                frame.noMapText = frame.mapContainer:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
                frame.noMapText:SetPoint("CENTER", frame.mapContainer, "CENTER", 0, 0)
                frame.noMapText:SetTextColor(0.8, 0.6, 0.2, 1)
            end
            
            frame.noMapText:SetText("|cFF808080Select a city from the dropdown above|r")
            frame.noMapText:Show()
            
            if frame.mapCityLabel then
                frame.mapCityLabel:Hide()
            end
        end
        
        if frame and frame.titleText then
            frame.titleText:SetText("City Map")
        end
        return
    end
    
    local cityData = CitySiege_CityData[cityID]
    if not cityData then return end
    
    -- Update title
    if frame and frame.titleText then
        local color = CitySiege_GetCityColorString(cityID)
        frame.titleText:SetText(string.format("%s%s|r Map", color, cityData.displayName))
    end
    
    -- Set city map texture using our custom map files
    if frame and frame.mapTexture then
        print("CitySiege MapDisplay: frame and mapTexture exist")
        
        -- Map city names to file names (use actual folder names)
        local mapFiles = {
            [CitySiege_Cities.STORMWIND] = "Stormwind",
            [CitySiege_Cities.IRONFORGE] = "Ironforge",
            [CitySiege_Cities.DARNASSUS] = "Darnassis",
            [CitySiege_Cities.EXODAR] = "TheExodar",
            [CitySiege_Cities.ORGRIMMAR] = "Ogrimmar",
            [CitySiege_Cities.UNDERCITY] = "Undercity",
            [CitySiege_Cities.THUNDERBLUFF] = "ThunderBluff",
            [CitySiege_Cities.SILVERMOON] = "SilvermoonCity",
        }
        
        print("CitySiege MapDisplay: Looking up map file for cityID: " .. tostring(cityID))
        local mapFile = mapFiles[cityID]
        print("CitySiege MapDisplay: mapFile = " .. tostring(mapFile))
        if mapFile then
            -- BLP files (no extension needed, WoW adds .blp automatically)
            local texturePath = "Interface\\AddOns\\CitySiege\\Media\\Maps\\" .. mapFile
            
            frame.mapTexture:SetTexture(texturePath)
            -- Crop black borders: aggressive crop to remove all black padding
            frame.mapTexture:SetTexCoord(0.01, 0.96, 0.13, 0.78)
            
            -- Hide placeholders
            if frame.mapCityLabel then frame.mapCityLabel:Hide() end
            if frame.noMapText then frame.noMapText:Hide() end
            if frame.noSiegeText then frame.noSiegeText:Hide() end
            
            print("CitySiege: Texture set for " .. cityData.displayName)
        else
            print("CitySiege: No map file found for city ID: " .. tostring(cityID))
            MapDisplay:ShowMapFallback(cityID, cityData, nil)
        end
    else
        print("CitySiege MapDisplay: ERROR - frame or mapTexture is nil")
    end
end

function MapDisplay:ShowMapFallback(cityID, cityData, mapFile)
    if not frame or not frame.mapTexture then return end
    
    frame.mapTexture:SetTexture(nil)
    frame.mapTexture:SetTexture(0.05, 0.05, 0.08, 1.0)
    
    -- Show "no map available" message
    if not frame.noMapText then
        frame.noMapText = frame.mapContainer:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
        frame.noMapText:SetPoint("CENTER", frame.mapContainer, "CENTER", 0, 20)
        frame.noMapText:SetTextColor(0.8, 0.6, 0.2, 1)
    end
    
    if mapFile then
        frame.noMapText:SetText("|cFFFFAA00Map not loaded|r\n\n" .. cityData.displayName .. "\n\n|cFF808080Ensure BLP files are in:\nInterface/AddOns/CitySiege/Media/Maps/\nRestart game after adding files|r")
    else
        frame.noMapText:SetText("|cFFFFAA00No map configured|r\n\n" .. cityData.displayName)
    end
    
    -- Show city name overlay
    if not frame.mapCityLabel then
        frame.mapCityLabel = frame.mapContainer:CreateFontString(nil, "ARTWORK", "GameFontNormalHuge")
        frame.mapCityLabel:SetPoint("TOP", frame.mapContainer, "TOP", 0, -10)
        frame.mapCityLabel:SetAlpha(0.3)
    end
    
    local color = cityData.color
    frame.mapCityLabel:SetTextColor(color.r, color.g, color.b, 0.5)
    frame.mapCityLabel:SetText(cityData.displayName:upper())
end

function MapDisplay:UpdateDisplay()
    if not frame then
        return
    end
    
    if not currentCityID then
        -- Show "no siege" message
        if frame.noSiegeText then
            frame.noSiegeText:Show()
        end
        self:Clear()
        self:UpdateStats(nil)
        return
    end
    
    -- Hide "no siege" message
    if frame.noSiegeText then
        frame.noSiegeText:Hide()
    end
    
    -- Always show city center marker
    local cityData = CitySiege_CityData[currentCityID]
    if cityData then
        self:DrawCityCenter(cityData)
    end
    
    local siegeData = CitySiege_SiegeTracker and CitySiege_SiegeTracker:GetSiege(currentCityID)
    if not siegeData then
        -- Show city layout even without active siege
        self:UpdateStats(nil)
        return
    end
    
    local mapSettings = CitySiege_Config:GetMapSettings()
    
    -- Show/update NPC positions
    if mapSettings.showNPCs then
        self:UpdateNPCPositions(siegeData)
    end
    
    -- Show/update waypoints
    if mapSettings.showWaypoints then
        self:UpdateWaypoints(siegeData)
    end
    
    -- Update stats display
    self:UpdateStats(siegeData)
end

function MapDisplay:DrawCityCenter(cityData)
    if not cityData then return end
    
    -- Draw spawn point marker (green square - attacker spawn)
    local spawnIcon = self:GetOrCreateIcon("spawn_point", "SPAWN")
    if spawnIcon then
        self:PositionIcon(spawnIcon, cityData.spawnX, cityData.spawnY, cityData)
        spawnIcon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
        spawnIcon:SetTexCoord(0.5, 0.75, 0.25, 0.5) -- Green Square (icon 7)
        spawnIcon:SetSize(16, 16)
        spawnIcon:Show()
    end
end

function MapDisplay:UpdateNPCPositions(siegeData)
    if not siegeData then return end
    
    local cityData = CitySiege_CityData[currentCityID]
    if not cityData then return end
    
    -- Clear previous NPC icons
    for id, icon in pairs(icons) do
        if type(id) == "string" and (string.match(id, "^npc_atk") or string.match(id, "^npc_def") or string.match(id, "^bot_atk") or string.match(id, "^bot_def")) then
            icon:Hide()
            icons[id] = nil
        end
    end
    
    -- Draw attacker NPC positions (red circles - raid icon 3)
    if siegeData.attackerPositions then
        for i, pos in ipairs(siegeData.attackerPositions) do
            if pos.x and pos.y then
                local icon = self:GetOrCreateIcon("npc_atk_" .. i, "NPC_ATTACKER")
                self:PositionIcon(icon, pos.x, pos.y, cityData)
                icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
                icon:SetTexCoord(0, 0.25, 0, 0.25) -- Red Circle (icon 3)
                icon:SetSize(10, 10)
                icon:Show()
            end
        end
    end
    
    -- Draw defender NPC positions (blue circles - raid icon 2)
    if siegeData.defenderPositions then
        for i, pos in ipairs(siegeData.defenderPositions) do
            if pos.x and pos.y then
                local icon = self:GetOrCreateIcon("npc_def_" .. i, "NPC_DEFENDER")
                self:PositionIcon(icon, pos.x, pos.y, cityData)
                icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
                icon:SetTexCoord(0.25, 0.5, 0, 0.25) -- Blue Circle (icon 2)
                icon:SetSize(10, 10)
                icon:Show()
            end
        end
    end
    
    -- Draw attacker bot positions (orange diamonds - raid icon 4)
    if siegeData.attackerBots then
        for i, pos in ipairs(siegeData.attackerBots) do
            if pos.x and pos.y then
                local icon = self:GetOrCreateIcon("bot_atk_" .. i, "BOT_ATTACKER")
                self:PositionIcon(icon, pos.x, pos.y, cityData)
                icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
                icon:SetTexCoord(0.5, 0.75, 0, 0.25) -- Orange Diamond (icon 4)
                icon:SetSize(8, 8)
                icon:Show()
            end
        end
    end
    
    -- Draw defender bot positions (purple diamonds - raid icon 1)
    if siegeData.defenderBots then
        for i, pos in ipairs(siegeData.defenderBots) do
            if pos.x and pos.y then
                local icon = self:GetOrCreateIcon("bot_def_" .. i, "BOT_DEFENDER")
                self:PositionIcon(icon, pos.x, pos.y, cityData)
                icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
                icon:SetTexCoord(0.75, 1, 0, 0.25) -- Purple Diamond (icon 1)
                icon:SetSize(8, 8)
                icon:Show()
            end
        end
    end
    
    -- Old code for legacy npcs table
    if siegeData.npcs then
        for guid, npcData in pairs(siegeData.npcs) do
            if npcData.x and npcData.y then
                local icon = self:GetOrCreateIcon(guid, "NPC")
                self:PositionIcon(icon, npcData.x, npcData.y, cityData)
                icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
                
                -- Use raid icons based on side
                if npcData.side == "attacker" then
                    icon:SetTexCoord(0, 0.25, 0, 0.25) -- Red Circle
                elseif npcData.side == "defender" then
                    icon:SetTexCoord(0.25, 0.5, 0, 0.25) -- Blue Circle
                else
                    icon:SetTexCoord(0.5, 0.75, 0, 0.25) -- Orange Diamond
                end
                icon:SetSize(10, 10)
                icon:Show()
            end
        end
    end
end

function MapDisplay:UpdateWaypoints(siegeData)
    if not siegeData or not siegeData.waypoints then return end
    
    local cityData = CitySiege_CityData[currentCityID]
    if not cityData then return end
    
    -- Draw lines between waypoints
    for i = 1, #siegeData.waypoints do
        local wp1 = siegeData.waypoints[i]
        local wp2 = siegeData.waypoints[(i % #siegeData.waypoints) + 1] -- Connect last to first
        
        if wp1 and wp2 and wp1.x and wp1.y and wp2.x and wp2.y then
            self:DrawLine(wp1.x, wp1.y, wp2.x, wp2.y, cityData)
        end
    end
    
    -- Draw waypoint markers (red square raid icons)
    for i, wp in ipairs(siegeData.waypoints) do
        if wp.x and wp.y then
            local icon = self:GetOrCreateIcon("waypoint_" .. i, "WAYPOINT")
            self:PositionIcon(icon, wp.x, wp.y, cityData)
            icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
            icon:SetTexCoord(0, 0.25, 0.25, 0.5) -- Red Square (icon 6)
            icon:SetSize(12, 12)
            icon:Show()
        end
    end
end

function MapDisplay:GetOrCreateIcon(id, iconType)
    if icons[id] then
        return icons[id]
    end
    
    local icon = frame.overlay:CreateTexture(nil, "OVERLAY")
    
    -- Icons will have their texture set by the calling code
    -- Default to raid targeting icons
    icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
    
    local scale = CitySiege_Config:GetMapSettings().iconScale or 1.0
    icon:SetSize(10 * scale, 10 * scale)
    
    icons[id] = icon
    return icon
end

function MapDisplay:PositionIcon(icon, worldX, worldY, cityData)
    if not icon or not frame or not frame.overlay then return end
    
    -- Convert world coordinates to map coordinates
    local mapX, mapY = self:WorldToMap(worldX, worldY, cityData)
    
    local overlayWidth = frame.overlay:GetWidth() or 0
    local overlayHeight = frame.overlay:GetHeight() or 0
    
    -- Safety check
    if overlayWidth <= 0 or overlayHeight <= 0 then
        return
    end
    
    -- Position icons on the full overlay to match WoW map scaling
    local pixelX = mapX * overlayWidth
    local pixelY = (1 - mapY) * overlayHeight
    
    icon:ClearAllPoints()
    icon:SetPoint("CENTER", frame.overlay, "BOTTOMLEFT", pixelX, pixelY)
    icon:Show()
end

function MapDisplay:WorldToMap(worldX, worldY, cityData)
    if not cityData then return 0.5, 0.5 end
    
    -- Per-city calibration offsets for accurate positioning
    local cityOffsets = {
        [0] = {x = 0.12, y = 0.04, scale = 0.35, range = 1.75},  -- Stormwind
        [1] = {x = 0.10, y = 0.05, scale = 0.35, range = 1.75},  -- Ironforge
        [2] = {x = 0.12, y = 0.04, scale = 0.35, range = 1.75},  -- Darnassus
        [3] = {x = 0.12, y = 0.04, scale = 0.35, range = 1.75},  -- Exodar
        [4] = {x = 0.10, y = 0.06, scale = 0.38, range = 1.85},  -- Orgrimmar
        [5] = {x = 0.12, y = 0.04, scale = 0.35, range = 1.75},  -- Undercity
        [6] = {x = 0.12, y = 0.04, scale = 0.35, range = 1.75},  -- Thunder Bluff
        [7] = {x = 0.12, y = 0.04, scale = 0.35, range = 1.75},  -- Silvermoon
    }
    
    -- Get center and spawn coordinates
    local originalCenterX = cityData.centerX
    local originalCenterY = cityData.centerY
    local spawnX = cityData.spawnX
    local spawnY = cityData.spawnY
    
    -- Get city-specific settings
    local cityID = currentCityID or 0
    local offsets = cityOffsets[cityID] or {x = 0.12, y = 0.04, scale = 0.35, range = 1.75}
    
    -- Calculate map range using original center
    local dx = spawnX - originalCenterX
    local dy = spawnY - originalCenterY
    local spawnDist = math.sqrt(dx * dx + dy * dy)
    local mapRange = spawnDist * offsets.range
    if mapRange < 50 then mapRange = 500 end
    
    -- Convert world coordinates to relative positions from center
    local relX = (worldX - originalCenterX) / mapRange
    local relY = (worldY - originalCenterY) / mapRange
    
    -- Rotate 90 degrees counter-clockwise and apply scaling:
    -- Counter-clockwise 90°: newX = -Y, newY = X
    local mapX = 0.5 - (relY * offsets.scale) + 0.05
    local mapY = 0.5 - (relX * offsets.scale) + 0.25
    
    -- Apply city-specific display offsets
    mapX = mapX + offsets.x
    mapY = mapY - offsets.y
    
    -- Clamp to 0-1 range
    mapX = math.max(0, math.min(1, mapX))
    mapY = math.max(0, math.min(1, mapY))
    
    return mapX, mapY
end

function MapDisplay:DrawLine(x1, y1, x2, y2, cityData)
    -- Draw lines by creating small dots along the path
    local mapX1, mapY1 = self:WorldToMap(x1, y1, cityData)
    local mapX2, mapY2 = self:WorldToMap(x2, y2, cityData)
    
    if not frame or not frame.overlay then return end
    
    local overlayWidth = frame.overlay:GetWidth()
    local overlayHeight = frame.overlay:GetHeight()
    
    if not overlayWidth or overlayWidth <= 0 then return end
    if not overlayHeight or overlayHeight <= 0 then return end
    
    -- Position lines on the full overlay to match WoW map scaling
    local pixelX1 = mapX1 * overlayWidth
    local pixelY1 = (1 - mapY1) * overlayHeight
    local pixelX2 = mapX2 * overlayWidth
    local pixelY2 = (1 - mapY2) * overlayHeight
    
    -- Calculate distance and angle
    local dx = pixelX2 - pixelX1
    local dy = pixelY2 - pixelY1
    local distance = math.sqrt(dx * dx + dy * dy)
    
    if distance < 2 then return end
    
    -- Create line segment using a simple texture
    local lineID = string.format("line_%.0f_%.0f", x1, y1)
    local line = lines[lineID]
    
    if not line then
        line = frame.overlay:CreateTexture(nil, "BACKGROUND")
        line:SetTexture("Interface\\Buttons\\WHITE8X8")
        line:SetTexture(0.5, 0.5, 0, 0.4) -- Yellow-ish line
        lines[lineID] = line
    end
    
    -- Set line as a thin horizontal bar (rotation not supported in 3.3.5)
    line:SetSize(distance, 2)
    line:SetPoint("CENTER", frame.overlay, "BOTTOMLEFT", (pixelX1 + pixelX2) / 2, (pixelY1 + pixelY2) / 2)
    line:Show()
end

function MapDisplay:UpdateStats(siegeData)
    if not frame or not frame.statsText then return end
    
    if not siegeData or not siegeData.stats then
        frame.statsText:SetText("")
        return
    end
    
    local stats = siegeData.stats
    local text = string.format(
        "NPCs: %d  Bots: %d\nWaypoints: %d",
        (siegeData.defenderPositions and #siegeData.defenderPositions or 0) + 
        (siegeData.attackerPositions and #siegeData.attackerPositions or 0),
        (siegeData.defenderBots and #siegeData.defenderBots or 0) + 
        (siegeData.attackerBots and #siegeData.attackerBots or 0),
        siegeData.waypoints and #siegeData.waypoints or 0
    )
    
    frame.statsText:SetText(text)
end

function MapDisplay:Clear()
    -- Hide all icons
    for _, icon in pairs(icons) do
        icon:Hide()
    end
    
    -- Hide all lines
    for _, line in pairs(lines) do
        line:Hide()
    end
    
    -- Clear tables but keep the objects for reuse
    -- icons = {}
    -- lines = {}
end

function MapDisplay:UpdatePositions()
    local now = GetTime()
    if now - updateThrottle < 0.5 then
        return -- Throttle updates
    end
    updateThrottle = now
    
    self:UpdateDisplay()
end

function MapDisplay:Show()
    if frame then
        frame:Show()
        self:UpdateDisplay()
    end
end

function MapDisplay:UpdateMapData(cityID, data)
    print("CitySiege MapDisplay: UpdateMapData called")
    print("CitySiege MapDisplay: cityID=" .. tostring(cityID) .. ", currentCityID=" .. tostring(currentCityID))
    print("CitySiege MapDisplay: frame=" .. tostring(frame) .. ", data=" .. tostring(data))
    
    if not frame or not data then 
        print("CitySiege MapDisplay: ERROR - frame or data is nil!")
        return 
    end
    if currentCityID ~= cityID then 
        print("CitySiege MapDisplay: ERROR - cityID mismatch!")
        return 
    end
    
    print("CitySiege MapDisplay: Clearing existing icons")
    -- Clear existing icons
    if frame.mapIcons then
        for _, icon in ipairs(frame.mapIcons) do
            icon:Hide()
        end
    end
    frame.mapIcons = {}
    
    -- Add waypoint icons
    if data.waypoints then
        print("CitySiege MapDisplay: Adding " .. #data.waypoints .. " waypoint icons")
        for i, wp in ipairs(data.waypoints) do
            print(string.format("CitySiege MapDisplay: Creating waypoint icon %d at %.2f, %.2f, %.2f", 
                i, wp.x, wp.y, wp.z))
            local icon = MapDisplay:CreateIcon(wp.x, wp.y, wp.z, "waypoint", i)
            table.insert(frame.mapIcons, icon)
        end
    else
        print("CitySiege MapDisplay: No waypoints in data")
    end
    
    -- Add leader icon
    if data.leaderPos then
        print("CitySiege MapDisplay: Adding leader icon")
        local icon = MapDisplay:CreateIcon(data.leaderPos.x, data.leaderPos.y, data.leaderPos.z, "leader")
        table.insert(frame.mapIcons, icon)
    else
        print("CitySiege MapDisplay: No leader position in data")
    end
    
    print("CitySiege MapDisplay: Total icons created: " .. #frame.mapIcons)
end

function MapDisplay:CreateIcon(x, y, z, iconType, index)
    local icon = frame.overlay:CreateTexture(nil, "OVERLAY")
    
    -- Use raid target icons for better visibility
    icon:SetTexture("Interface\\TargetingFrame\\UI-RaidTargetingIcons")
    
    -- Different icons for different types
    if iconType == "waypoint" then
        icon:SetTexCoord(0, 0.25, 0.25, 0.5) -- Red Square (icon 6)
        icon:SetSize(12, 12)
    elseif iconType == "leader" then
        icon:SetTexCoord(0.75, 1, 0.25, 0.5) -- Skull (icon 8)
        icon:SetSize(20, 20)
    end

    -- Use PositionIcon helper to convert world coords to overlay pixels
    local cityData = CitySiege_CityData[currentCityID]
    if cityData then
        self:PositionIcon(icon, x, y, cityData)
    else
        -- Fallback: center the icon
        icon:ClearAllPoints()
        icon:SetPoint("CENTER", frame.overlay, "CENTER")
    end

    icon:Show()
    return icon
end

-- NOTE: The older WorldToMap(worldX, worldY, cityID) implementation was removed
-- because it returned pixel coordinates and conflicted with the normalized
-- WorldToMap(worldX, worldY, cityData) helper above. Use PositionIcon to
-- place icons consistently on the overlay.

function MapDisplay:Hide()
    if frame then
        frame:Hide()
    end
end

function MapDisplay:GetFrame()
    return frame
end

