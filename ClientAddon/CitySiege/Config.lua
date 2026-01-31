--[[
    City Siege Addon - Configuration
    Manages addon settings and saved variables
]]

local ADDON_NAME = "CitySiege"

CitySiege_Config = {}
local Config = CitySiege_Config

-- Initialize configuration with defaults
function Config:Initialize(db)
    self.db = db
    
    -- Set up defaults if not present
    if not self.db.profile then
        self.db.profile = {}
    end
    
    -- Minimap settings
    if not self.db.profile.minimap then
        self.db.profile.minimap = CopyTable(CitySiege_DefaultConfig.minimap)
    end
    
    -- UI settings
    if not self.db.profile.ui then
        self.db.profile.ui = CopyTable(CitySiege_DefaultConfig.ui)
    end
    
    -- Notification settings
    if not self.db.profile.notifications then
        self.db.profile.notifications = CopyTable(CitySiege_DefaultConfig.notifications)
    end
    
    -- Map settings
    if not self.db.profile.map then
        self.db.profile.map = CopyTable(CitySiege_DefaultConfig.map)
    end
    
    -- Frame positions
    if not self.db.profile.framePositions then
        self.db.profile.framePositions = {}
    end
    
    -- Active sieges cache
    if not self.db.global then
        self.db.global = {}
    end
    if not self.db.global.activeSieges then
        self.db.global.activeSieges = {}
    end
    
    -- Statistics
    if not self.db.global.statistics then
        self.db.global.statistics = {
            siegesParticipated = 0,
            siegesWon = 0,
            siegesLost = 0,
            totalKills = 0,
            totalDeaths = 0,
        }
    end
end

-- Getters
function Config:GetMinimapSettings()
    return self.db.profile.minimap
end

function Config:GetUISettings()
    return self.db.profile.ui
end

function Config:GetNotificationSettings()
    return self.db.profile.notifications
end

function Config:GetMapSettings()
    return self.db.profile.map
end

function Config:GetFramePosition(frameName)
    return self.db.profile.framePositions[frameName]
end

function Config:GetActiveSieges()
    return self.db.global.activeSieges or {}
end

function Config:GetStatistics()
    return self.db.global.statistics
end

-- Setters
function Config:SetMinimapPosition(pos)
    self.db.profile.minimap.minimapPos = pos
end

function Config:SetMinimapHidden(hidden)
    self.db.profile.minimap.hide = hidden
end

function Config:SetMinimapLocked(locked)
    self.db.profile.minimap.lock = locked
end

function Config:SetUIScale(scale)
    self.db.profile.ui.scale = scale
end

function Config:SetUIAlpha(alpha)
    self.db.profile.ui.alpha = alpha
end

function Config:SetUILocked(locked)
    self.db.profile.ui.locked = locked
end

function Config:SetShowInCombat(show)
    self.db.profile.ui.showInCombat = show
end

function Config:SetNotification(key, enabled)
    if self.db.profile.notifications[key] ~= nil then
        self.db.profile.notifications[key] = enabled
    end
end

function Config:SetMapSetting(key, value)
    if self.db.profile.map[key] ~= nil then
        self.db.profile.map[key] = value
    end
end

function Config:SetFramePosition(frameName, point, relativeTo, relativePoint, x, y)
    self.db.profile.framePositions[frameName] = {
        point = point,
        relativeTo = relativeTo,
        relativePoint = relativePoint,
        x = x,
        y = y,
    }
end

function Config:SaveActiveSiege(cityID, siegeData)
    self.db.global.activeSieges[cityID] = siegeData
end

function Config:RemoveActiveSiege(cityID)
    self.db.global.activeSieges[cityID] = nil
end

function Config:ClearActiveSieges()
    self.db.global.activeSieges = {}
end

-- Statistics
function Config:IncrementStat(statName, amount)
    amount = amount or 1
    if self.db.global.statistics[statName] then
        self.db.global.statistics[statName] = self.db.global.statistics[statName] + amount
    end
end

function Config:ResetStatistics()
    self.db.global.statistics = {
        siegesParticipated = 0,
        siegesWon = 0,
        siegesLost = 0,
        totalKills = 0,
        totalDeaths = 0,
    }
end

-- Reset functions
function Config:ResetProfile()
    self.db.profile.minimap = CopyTable(CitySiege_DefaultConfig.minimap)
    self.db.profile.ui = CopyTable(CitySiege_DefaultConfig.ui)
    self.db.profile.notifications = CopyTable(CitySiege_DefaultConfig.notifications)
    self.db.profile.map = CopyTable(CitySiege_DefaultConfig.map)
    self.db.profile.framePositions = {}
end

function Config:ResetAll()
    self:ResetProfile()
    self:ClearActiveSieges()
    self:ResetStatistics()
end

-- Helper functions
function Config:IsMinimapShown()
    return not self.db.profile.minimap.hide
end

function Config:IsNotificationEnabled(key)
    return self.db.profile.notifications[key] == true
end

function Config:IsSoundEnabled()
    return self.db.profile.notifications.soundEnabled == true
end
