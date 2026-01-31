--[[
    City Siege Addon - Utility Functions
    Common helper functions used throughout the addon
]]

local ADDON_NAME = "CitySiege"

CitySiege_Utils = {}
local Utils = CitySiege_Utils

-- Deep copy a table (since WoW 3.3.5 doesn't have CopyTable globally)
function Utils:DeepCopy(orig)
    local orig_type = type(orig)
    local copy
    if orig_type == 'table' then
        copy = {}
        for orig_key, orig_value in next, orig, nil do
            copy[Utils:DeepCopy(orig_key)] = Utils:DeepCopy(orig_value)
        end
        setmetatable(copy, Utils:DeepCopy(getmetatable(orig)))
    else
        copy = orig
    end
    return copy
end

-- Safe table copy for simple tables
function Utils:CopyTable(source)
    if type(source) ~= "table" then
        return source
    end
    
    local copy = {}
    for k, v in pairs(source) do
        if type(v) == "table" then
            copy[k] = self:CopyTable(v)
        else
            copy[k] = v
        end
    end
    return copy
end

-- Format time in seconds to readable string
function Utils:FormatTime(seconds)
    if not seconds or seconds < 0 then
        return "0s"
    end
    
    local hours = math.floor(seconds / 3600)
    local mins = math.floor((seconds % 3600) / 60)
    local secs = math.floor(seconds % 60)
    
    if hours > 0 then
        return string.format("%dh %dm %ds", hours, mins, secs)
    elseif mins > 0 then
        return string.format("%dm %ds", mins, secs)
    else
        return string.format("%ds", secs)
    end
end

-- Format large numbers with separators
function Utils:FormatNumber(num)
    if not num then return "0" end
    
    local formatted = tostring(num)
    local k
    
    while true do
        formatted, k = string.gsub(formatted, "^(-?%d+)(%d%d%d)", '%1,%2')
        if k == 0 then
            break
        end
    end
    
    return formatted
end

-- Color text
function Utils:ColorText(text, r, g, b)
    if type(r) == "table" then
        b = r.b or 1
        g = r.g or 1
        r = r.r or 1
    end
    
    r = math.floor((r or 1) * 255)
    g = math.floor((g or 1) * 255)
    b = math.floor((b or 1) * 255)
    
    return string.format("|cFF%02X%02X%02X%s|r", r, g, b, text)
end

-- Print message to chat
function Utils:Print(msg, r, g, b)
    if r then
        msg = self:ColorText(msg, r, g, b)
    end
    DEFAULT_CHAT_FRAME:AddMessage("|cFF16C3F2[City Siege]|r " .. msg)
end

-- Print debug message (only if debug enabled)
function Utils:Debug(msg)
    -- Debug disabled
end

-- Calculate distance between two points
function Utils:GetDistance(x1, y1, x2, y2)
    local dx = x2 - x1
    local dy = y2 - y1
    return math.sqrt(dx * dx + dy * dy)
end

-- Calculate 3D distance
function Utils:GetDistance3D(x1, y1, z1, x2, y2, z2)
    local dx = x2 - x1
    local dy = y2 - y1
    local dz = z2 - z1
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

-- Get player faction
function Utils:GetPlayerFaction()
    local faction = UnitFactionGroup("player")
    return faction or "Neutral"
end

-- Check if player is in a city
function Utils:IsPlayerInCity(cityID)
    local cityData = CitySiege_CityData[cityID]
    if not cityData then return false end
    
    local mapID = GetCurrentMapAreaID()
    if mapID ~= cityData.areaID then
        return false
    end
    
    return true
end

-- Get current map coordinates
function Utils:GetPlayerMapPosition()
    local x, y = GetPlayerMapPosition("player")
    if x == 0 and y == 0 then
        return nil, nil
    end
    return x, y
end

-- Round number to decimals
function Utils:Round(num, decimals)
    local mult = 10^(decimals or 0)
    return math.floor(num * mult + 0.5) / mult
end

-- Check if value is in table
function Utils:TableContains(table, value)
    for _, v in pairs(table) do
        if v == value then
            return true
        end
    end
    return false
end

-- Get table size
function Utils:TableSize(t)
    local count = 0
    for _ in pairs(t) do
        count = count + 1
    end
    return count
end

-- Parse chat command arguments
function Utils:ParseArgs(text)
    local args = {}
    for arg in string.gmatch(text, "%S+") do
        table.insert(args, arg)
    end
    return args
end

-- Create a simple tooltip
function Utils:ShowTooltip(frame, title, ...)
    GameTooltip:SetOwner(frame, "ANCHOR_RIGHT")
    GameTooltip:SetText(title, 1, 1, 1)
    
    for i = 1, select("#", ...) do
        local text = select(i, ...)
        if text then
            GameTooltip:AddLine(text, nil, nil, nil, true)
        end
    end
    
    GameTooltip:Show()
end

-- Hide tooltip
function Utils:HideTooltip()
    GameTooltip:Hide()
end

-- Play sound
function Utils:PlaySound(soundFile)
    if CitySiege_Config and CitySiege_Config:IsSoundEnabled() then
        PlaySoundFile(soundFile)
    end
end

-- Send addon message
function Utils:SendAddonMessage(prefix, message, chatType, target)
    if not prefix or not message then return end
    chatType = chatType or "PARTY"
    SendAddonMessage(prefix, message, chatType, target)
end

-- Escape special characters for patterns
function Utils:EscapePattern(str)
    return string.gsub(str, "([%^%$%(%)%%%.%[%]%*%+%-%?])", "%%%1")
end

-- Create frame backdrop
function Utils:SetBackdrop(frame, r, g, b, a)
    frame:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 16,
        insets = {left = 4, right = 4, top = 4, bottom = 4},
    })
    frame:SetBackdropColor(r or 0, g or 0, b or 0, a or 0.8)
    frame:SetBackdropBorderColor(1, 1, 1, 1)
end

-- Format percentage
function Utils:FormatPercent(value, total)
    if not total or total == 0 then return "0%" end
    local percent = (value / total) * 100
    return string.format("%.1f%%", percent)
end

-- Global alias for compatibility
CopyTable = CitySiege_Utils.CopyTable
