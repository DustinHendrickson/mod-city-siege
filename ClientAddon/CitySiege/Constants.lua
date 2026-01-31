--[[
    City Siege Addon - Constants
    Contains all constant data for cities, maps, and configuration
]]

local ADDON_NAME = "CitySiege"

-- City IDs (must match server-side enum)
CitySiege_Cities = {
    STORMWIND = 0,
    IRONFORGE = 1,
    DARNASSUS = 2,
    EXODAR = 3,
    ORGRIMMAR = 4,
    UNDERCITY = 5,
    THUNDERBLUFF = 6,
    SILVERMOON = 7,
}

-- City data structure
CitySiege_CityData = {
    [CitySiege_Cities.STORMWIND] = {
        name = "Stormwind",
        displayName = "Stormwind City",
        faction = "Alliance",
        mapID = 0,
        areaID = 1519,
        centerX = -8913.23,
        centerY = 554.633,
        centerZ = 93.7944,
        spawnX = -9161.16,
        spawnY = 353.365,
        spawnZ = 88.117,
        iconTexture = "Interface\\Icons\\INV_Misc_Tournaments_banner_Human",
        color = {r = 0.0, g = 0.5, b = 1.0},
    },
    [CitySiege_Cities.IRONFORGE] = {
        name = "Ironforge",
        displayName = "Ironforge",
        faction = "Alliance",
        mapID = 0,
        areaID = 1537,
        centerX = -4981.25,
        centerY = -881.542,
        centerZ = 501.660,
        spawnX = -5174.09,
        spawnY = -594.361,
        spawnZ = 397.853,
        iconTexture = "Interface\\Icons\\INV_Misc_Tournaments_banner_Dwarf",
        color = {r = 0.8, g = 0.6, b = 0.3},
    },
    [CitySiege_Cities.DARNASSUS] = {
        name = "Darnassus",
        displayName = "Darnassus",
        faction = "Alliance",
        mapID = 1,
        areaID = 1657,
        centerX = 9947.52,
        centerY = 2482.73,
        centerZ = 1316.21,
        spawnX = 9887.36,
        spawnY = 1856.49,
        spawnZ = 1317.14,
        iconTexture = "Interface\\Icons\\INV_Misc_Tournaments_banner_NightElf",
        color = {r = 0.5, g = 0.0, b = 1.0},
    },
    [CitySiege_Cities.EXODAR] = {
        name = "Exodar",
        displayName = "The Exodar",
        faction = "Alliance",
        mapID = 530,
        areaID = 3557,
        centerX = -3864.92,
        centerY = -11643.7,
        centerZ = -137.644,
        spawnX = -4080.80,
        spawnY = -12193.2,
        spawnZ = 1.712,
        iconTexture = "Interface\\Icons\\Achievement_Character_Draenei_Male",
        color = {r = 0.0, g = 0.8, b = 1.0},
    },
    [CitySiege_Cities.ORGRIMMAR] = {
        name = "Orgrimmar",
        displayName = "Orgrimmar",
        faction = "Horde",
        mapID = 1,
        areaID = 1637,
        centerX = 1633.75,
        centerY = -4439.39,
        centerZ = 15.4396,
        spawnX = 1114.96,
        spawnY = -4374.63,
        spawnZ = 25.813,
        iconTexture = "Interface\\Icons\\INV_Misc_Tournaments_banner_Orc",
        color = {r = 1.0, g = 0.0, b = 0.0},
    },
    [CitySiege_Cities.UNDERCITY] = {
        name = "Undercity",
        displayName = "Undercity",
        faction = "Horde",
        mapID = 0,
        areaID = 1497,
        centerX = 1633.75,
        centerY = 240.167,
        centerZ = -43.1034,
        spawnX = 1982.26,
        spawnY = 226.674,
        spawnZ = 35.951,
        iconTexture = "Interface\\Icons\\INV_Misc_Tournaments_banner_Scourge",
        color = {r = 0.5, g = 1.0, b = 0.0},
    },
    [CitySiege_Cities.THUNDERBLUFF] = {
        name = "ThunderBluff",
        displayName = "Thunder Bluff",
        faction = "Horde",
        mapID = 1,
        areaID = 1638,
        centerX = -1043.11,
        centerY = 285.809,
        centerZ = 135.165,
        spawnX = -1558.61,
        spawnY = -5.071,
        spawnZ = 5.384,
        iconTexture = "Interface\\Icons\\INV_Misc_Tournaments_banner_Tauren",
        color = {r = 0.8, g = 0.4, b = 0.0},
    },
    [CitySiege_Cities.SILVERMOON] = {
        name = "Silvermoon",
        displayName = "Silvermoon City",
        faction = "Horde",
        mapID = 530,
        areaID = 3487,
        centerX = 9338.74,
        centerY = -7277.27,
        centerZ = 13.7014,
        spawnX = 9230.47,
        spawnY = -6962.67,
        spawnZ = 5.004,
        iconTexture = "Interface\\Icons\\INV_Misc_Tournaments_banner_BloodElf",
        color = {r = 1.0, g = 0.8, b = 0.0},
    },
}

-- Siege status constants
CitySiege_SiegeStatus = {
    INACTIVE = 0,
    PREPARING = 1,
    ACTIVE = 2,
    ENDING = 3,
}

-- Chat command list
CitySiege_Commands = {
    {cmd = "start", args = "[city]", desc = "Start a siege in the specified city", security = 2},
    {cmd = "stop", args = "[city] [faction]", desc = "Stop an active siege", security = 2},
    {cmd = "cleanup", args = "[city]", desc = "Clean up siege NPCs", security = 2},
    {cmd = "status", args = "", desc = "Display status of all active sieges", security = 2},
    {cmd = "info", args = "", desc = "Display detailed siege information", security = 2},
    {cmd = "reload", args = "", desc = "Reload siege configuration", security = 3},
}

-- Default configuration
CitySiege_DefaultConfig = {
    minimap = {
        hide = false,
        minimapPos = 220,
        lock = false,
    },
    ui = {
        showInCombat = true,
        autoHide = false,
        scale = 1.0,
        alpha = 1.0,
        locked = false,
    },
    notifications = {
        siegeStart = true,
        siegeEnd = true,
        phaseChange = true,
        soundEnabled = true,
    },
    map = {
        showPlayers = true,
        showNPCs = true,
        showWaypoints = true,
        updateInterval = 1.0,
        iconScale = 1.0,
    },
}

-- Color constants
CitySiege_Colors = {
    ALLIANCE = {r = 0.2, g = 0.4, b = 1.0},
    HORDE = {r = 1.0, g = 0.1, b = 0.1},
    NEUTRAL = {r = 0.8, g = 0.8, b = 0.8},
    SUCCESS = {r = 0.0, g = 1.0, b = 0.0},
    WARNING = {r = 1.0, g = 0.8, b = 0.0},
    ERROR = {r = 1.0, g = 0.0, b = 0.0},
}

-- Icon paths
CitySiege_Icons = {
    MINIMAP = "Interface\\Icons\\Achievement_BG_winWSG",
    MAIN = "Interface\\Icons\\Achievement_Boss_PrinceTenris",
    ALLIANCE_FLAG = "Interface\\WorldStateFrame\\AllianceFlag",
    HORDE_FLAG = "Interface\\WorldStateFrame\\HordeFlag",
    PLAYER_DOT = "Interface\\Minimap\\POIIcons",
    NPC_DOT = "Interface\\Minimap\\ObjectIcons",
}

-- Helper function to get city by name
function CitySiege_GetCityByName(name)
    if not name then return nil end
    name = string.lower(name)
    
    for id, data in pairs(CitySiege_CityData) do
        if string.lower(data.name) == name or string.lower(data.displayName) == name then
            return id, data
        end
    end
    return nil
end

-- Helper function to get city color string
function CitySiege_GetCityColorString(cityID)
    local data = CitySiege_CityData[cityID]
    if not data then return "|cFFFFFFFF" end
    
    local r = math.floor(data.color.r * 255)
    local g = math.floor(data.color.g * 255)
    local b = math.floor(data.color.b * 255)
    
    return string.format("|cFF%02X%02X%02X", r, g, b)
end

-- Helper function to format faction color
function CitySiege_GetFactionColor(faction)
    if faction == "Alliance" then
        return CitySiege_Colors.ALLIANCE
    elseif faction == "Horde" then
        return CitySiege_Colors.HORDE
    else
        return CitySiege_Colors.NEUTRAL
    end
end
