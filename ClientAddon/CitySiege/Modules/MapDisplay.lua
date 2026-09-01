--[[
    City Siege Addon - Map Display

    Draws the besieged city on its own world-map art and places every marker
    using map coordinates the server computed through WorldMapArea.dbc - the
    same lookup the game uses to put the player's dot on the world map.

    There is deliberately no calibration in here. Earlier versions projected
    world coordinates through hand-tuned per-city offsets onto a re-encoded
    copy of the map, and nothing lined up because nothing could: the offsets
    were guesses, and the crop applied to the texture was never applied to the
    projection. Now the server says "43.2% across, 61.7% down" and that is
    where the marker goes, on the identical art the game shows for that city.
]]

CitySiege_MapDisplay = {}
local MapDisplay = CitySiege_MapDisplay

-- Blizzard draws a zone map as twelve 256x256 tiles in a 4x3 grid. The map
-- image occupies the top-left 1002x668 of that 1024x768 canvas and the rest
-- of the right-hand and bottom tiles is padding. WorldMapArea.dbc coordinates
-- span the 1002x668 image, so that is the rectangle markers are placed in.
local TILE_COLUMNS = 4
local TILE_ROWS    = 3
local TILE_SIZE    = 256
local MAP_WIDTH    = 1002
local MAP_HEIGHT   = 668

-- Width the map is shown at inside the panel. Height follows from the map's
-- own aspect ratio so nothing is stretched.
local DISPLAY_WIDTH = 460

-- Pixels between dots on the drawn route.
local DOT_SPACING = 6

-- Folder and file stem of each city's tiles under Interface\WorldMap\. These
-- are the client's internal zone names, spelling included. Keyed by the
-- server's city ids (see CitySiege_Cities).
local MAP_FILES = {
    [0] = "Stormwind",
    [1] = "Ironforge",
    [2] = "Darnassis",
    [3] = "TheExodar",
    [4] = "Ogrimmar",
    [5] = "Undercity",
    [6] = "ThunderBluff",
    [7] = "SilvermoonCity",
}

-- Raid target icons, which the legend below also uses.
local RAID_ICONS    = "Interface\\TargetingFrame\\UI-RaidTargetingIcons"
local ICON_MUSTER   = { 0.50, 0.75, 0.25, 0.50 }   -- cross
local ICON_WAYPOINT = { 0.00, 0.25, 0.25, 0.50 }   -- moon
local ICON_LEADER   = { 0.75, 1.00, 0.25, 0.50 }   -- skull

local frame = nil
local currentCityID = nil
local routeCache = {}       -- cityID -> { waypoints, muster, leaderPos } from MAP_DATA
local markers = {}          -- pooled marker textures by key
local waypointMarkers = {}  -- pooled waypoint textures by index
local dots = {}             -- pooled route dots
local updateThrottle = 0

-- ---------------------------------------------------------------------------
-- Construction
-- ---------------------------------------------------------------------------

function MapDisplay:Create(parent)
    if frame then
        return frame
    end

    frame = CreateFrame("Frame", "CitySiegeMapDisplay", parent)
    frame:SetAllPoints(parent)
    frame:Show()

    -- The tab already reads "Battle Map", so this line names the city instead.
    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -6)
    title:SetText("No city selected")
    frame.titleText = title

    local scale = DISPLAY_WIDTH / MAP_WIDTH
    frame.scale = scale

    -- Canvas sized to the map image, so its edges are the map's edges.
    local canvas = CreateFrame("Frame", nil, frame)
    canvas:SetSize(MAP_WIDTH * scale, MAP_HEIGHT * scale)
    canvas:SetPoint("CENTER", frame, "CENTER", 0, -6)
    frame.canvas = canvas

    -- Shown when there is no city to draw.
    local blank = canvas:CreateTexture(nil, "BACKGROUND")
    blank:SetAllPoints(canvas)
    blank:SetTexture(0.05, 0.05, 0.08, 1)
    frame.blank = blank

    -- The tile grid. Right-hand and bottom tiles are trimmed to the map image
    -- so the padding never shows.
    frame.tiles = {}
    for row = 0, TILE_ROWS - 1 do
        for col = 0, TILE_COLUMNS - 1 do
            local width  = math.min(TILE_SIZE, MAP_WIDTH  - col * TILE_SIZE)
            local height = math.min(TILE_SIZE, MAP_HEIGHT - row * TILE_SIZE)

            local tile = canvas:CreateTexture(nil, "ARTWORK")
            tile:SetSize(width * scale, height * scale)
            tile:SetPoint("TOPLEFT", canvas, "TOPLEFT", col * TILE_SIZE * scale, -row * TILE_SIZE * scale)
            tile:SetTexCoord(0, width / TILE_SIZE, 0, height / TILE_SIZE)
            tile:Hide()

            frame.tiles[row * TILE_COLUMNS + col + 1] = tile
        end
    end

    -- Border on its own frame so it sits over the map edge rather than inside it.
    local border = CreateFrame("Frame", nil, canvas)
    border:SetPoint("TOPLEFT", canvas, "TOPLEFT", -5, 5)
    border:SetPoint("BOTTOMRIGHT", canvas, "BOTTOMRIGHT", 5, -5)
    border:SetBackdrop({
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = false,
        edgeSize = 16,
    })
    border:SetBackdropBorderColor(0.8, 0.8, 0.8, 1)
    border:SetFrameLevel(canvas:GetFrameLevel() + 1)

    -- Markers live here, above the tiles and border.
    local overlay = CreateFrame("Frame", nil, canvas)
    overlay:SetAllPoints(canvas)
    overlay:SetFrameLevel(canvas:GetFrameLevel() + 2)
    frame.overlay = overlay

    local legendFrame = CreateFrame("Frame", nil, frame)
    legendFrame:SetSize(470, 26)
    legendFrame:SetPoint("BOTTOM", frame, "BOTTOM", 0, 6)
    frame.legendFrame = legendFrame

    local legend = legendFrame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    legend:SetPoint("CENTER", 0, 0)
    legend:SetJustifyH("CENTER")
    legend:SetText("|T" .. RAID_ICONS .. ":18:18:0:0:64:64:48:64:16:32|t |cFFFFD700Leader|r     "
        .. "|T" .. RAID_ICONS .. ":16:16:0:0:64:64:0:16:16:32|t |cFF00FF00Waypoint|r     "
        .. "|T" .. RAID_ICONS .. ":16:16:0:0:64:64:32:48:16:32|t |cFF16C3F2Muster|r")
    frame.legend = legend

    local statsText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    statsText:SetPoint("BOTTOMRIGHT", canvas, "TOPRIGHT", -2, 4)
    statsText:SetJustifyH("RIGHT")
    statsText:SetText("")
    frame.statsText = statsText

    local placeholder = canvas:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    placeholder:SetPoint("CENTER", canvas, "CENTER", 0, 0)
    placeholder:SetJustifyH("CENTER")
    placeholder:SetText("|cFF808080Select a city from the dropdown above|r")
    frame.placeholder = placeholder

    return frame
end

-- ---------------------------------------------------------------------------
-- Placement
-- ---------------------------------------------------------------------------

-- Pixel offset from the canvas' top-left corner for a map percentage.
local function ToPixels(mx, my)
    local s = frame.scale
    return mx / 100 * MAP_WIDTH * s, my / 100 * MAP_HEIGHT * s
end

-- True if a position carries usable map coordinates. The server sends -1
-- when it could not place something on this city's map.
local function OnMap(pos)
    return pos and pos.mx and pos.my and pos.mx >= 0 and pos.my >= 0
end

local function PlaceMarker(texture, pos)
    if not texture then return end

    if not frame or not OnMap(pos) then
        texture:Hide()
        return
    end

    local px, py = ToPixels(pos.mx, pos.my)
    texture:ClearAllPoints()
    texture:SetPoint("CENTER", frame.overlay, "TOPLEFT", px, -py)
    texture:Show()
end

local function IconScale()
    local settings = CitySiege_Config and CitySiege_Config:GetMapSettings()
    return (settings and settings.iconScale) or 1.0
end

local function GetMarker(key, texCoords, size)
    local texture = markers[key]
    if not texture then
        texture = frame.overlay:CreateTexture(nil, "OVERLAY")
        texture:SetTexture(RAID_ICONS)
        markers[key] = texture
    end

    texture:SetTexCoord(texCoords[1], texCoords[2], texCoords[3], texCoords[4])
    texture:SetSize(size * IconScale(), size * IconScale())
    return texture
end

local function GetWaypointMarker(index)
    local texture = waypointMarkers[index]
    if not texture then
        texture = frame.overlay:CreateTexture(nil, "OVERLAY")
        texture:SetTexture(RAID_ICONS)
        texture:SetTexCoord(ICON_WAYPOINT[1], ICON_WAYPOINT[2], ICON_WAYPOINT[3], ICON_WAYPOINT[4])
        waypointMarkers[index] = texture
    end

    texture:SetSize(10 * IconScale(), 10 * IconScale())
    return texture
end

-- Draws the route as a dotted line between consecutive waypoints. Textures
-- cannot be rotated in this client, so a line is a run of dots.
local function DrawRoute(waypoints)
    local used = 0
    local previous = nil

    for _, wp in ipairs(waypoints) do
        if OnMap(wp) then
            if previous then
                local x1, y1 = ToPixels(previous.mx, previous.my)
                local x2, y2 = ToPixels(wp.mx, wp.my)
                local dx, dy = x2 - x1, y2 - y1
                local length = math.sqrt(dx * dx + dy * dy)
                local steps = math.max(1, math.floor(length / DOT_SPACING))

                for k = 1, steps - 1 do
                    used = used + 1
                    local dot = dots[used]
                    if not dot then
                        dot = frame.overlay:CreateTexture(nil, "ARTWORK")
                        dot:SetTexture(1.0, 0.85, 0.2, 0.9)
                        dot:SetSize(3, 3)
                        dots[used] = dot
                    end

                    local t = k / steps
                    dot:ClearAllPoints()
                    dot:SetPoint("CENTER", frame.overlay, "TOPLEFT", x1 + dx * t, -(y1 + dy * t))
                    dot:Show()
                end
            end

            previous = wp
        end
    end

    for k = used + 1, #dots do
        dots[k]:Hide()
    end
end

local function HideRoute()
    for _, dot in ipairs(dots) do dot:Hide() end
    for _, texture in ipairs(waypointMarkers) do texture:Hide() end
end

-- ---------------------------------------------------------------------------
-- City selection
-- ---------------------------------------------------------------------------

function MapDisplay:SetCity(cityID)
    currentCityID = cityID
    if not frame then return end

    local cityData = cityID and CitySiege_CityData[cityID]
    local mapFile = cityID and MAP_FILES[cityID]

    if not cityData or not mapFile then
        for _, tile in ipairs(frame.tiles) do tile:Hide() end
        frame.blank:Show()
        frame.placeholder:Show()
        frame.titleText:SetText("No city selected")
        self:Clear()
        return
    end

    for index, tile in ipairs(frame.tiles) do
        tile:SetTexture("Interface\\WorldMap\\" .. mapFile .. "\\" .. mapFile .. index)
        tile:Show()
    end

    frame.blank:Hide()
    frame.placeholder:Hide()
    frame.titleText:SetText(string.format("%s%s|r", CitySiege_GetCityColorString(cityID), cityData.displayName))

    self:UpdateDisplay()
end

-- ---------------------------------------------------------------------------
-- Drawing
-- ---------------------------------------------------------------------------

function MapDisplay:UpdateDisplay()
    if not frame then return end

    if not currentCityID then
        self:Clear()
        return
    end

    local siege = CitySiege_SiegeTracker and CitySiege_SiegeTracker:GetSiege(currentCityID)
    local cached = routeCache[currentCityID] or {}

    -- A live siege carries the route it is actually marching; otherwise show
    -- the planned route from the last MAP_DATA reply.
    local waypoints = cached.waypoints or {}
    if siege and siege.waypoints and #siege.waypoints > 0 then
        waypoints = siege.waypoints
    end

    local muster = cached.muster
    local leader = cached.leaderPos
    if siege and siege.coords then
        muster = muster or { mx = siege.coords.spawnMX, my = siege.coords.spawnMY }
        leader = leader or { mx = siege.coords.leaderMX, my = siege.coords.leaderMY }
    end

    local settings = CitySiege_Config:GetMapSettings()
    if settings.showWaypoints then
        DrawRoute(waypoints)

        local shown = 0
        for index, wp in ipairs(waypoints) do
            shown = index
            PlaceMarker(GetWaypointMarker(index), wp)
        end
        for index = shown + 1, #waypointMarkers do
            waypointMarkers[index]:Hide()
        end
    else
        HideRoute()
    end

    PlaceMarker(GetMarker("muster", ICON_MUSTER, 16), muster)
    PlaceMarker(GetMarker("leader", ICON_LEADER, 20), leader)

    if siege then
        frame.statsText:SetText(string.format("|cFFFF4040Under siege|r  -  %d waypoints", #waypoints))
    elseif #waypoints > 0 then
        frame.statsText:SetText(string.format("|cFF808080Planned route|r  -  %d waypoints", #waypoints))
    else
        frame.statsText:SetText("|cFF808080No route received yet|r")
    end
end

-- The server answers a map request with the city's route; keep it so the map
-- can be shown when no siege is running.
function MapDisplay:UpdateMapData(cityID, data)
    if not cityID or not data then return end

    routeCache[cityID] = data

    if frame and currentCityID == cityID then
        self:UpdateDisplay()
    end
end

-- Kept for callers elsewhere in the addon; the server does not stream unit
-- positions, so there is nothing separate to draw.
function MapDisplay:UpdateNPCPositions()
    self:UpdateDisplay()
end

function MapDisplay:Clear()
    if not frame then return end

    HideRoute()
    for _, texture in pairs(markers) do texture:Hide() end
    frame.statsText:SetText("")
end

function MapDisplay:UpdatePositions()
    local now = GetTime()
    if now - updateThrottle < 0.5 then
        return
    end
    updateThrottle = now

    self:UpdateDisplay()
end

-- ---------------------------------------------------------------------------
-- Plumbing
-- ---------------------------------------------------------------------------

function MapDisplay:Show()
    if frame then
        frame:Show()
        self:UpdateDisplay()
    end
end

function MapDisplay:Hide()
    if frame then
        frame:Hide()
    end
end

function MapDisplay:GetFrame()
    return frame
end
