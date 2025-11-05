--[[
    City Siege Addon - Core
    Main addon initialization and event handling
]]

local ADDON_NAME, ADDON_NAMESPACE = ...

-- Create main addon object
CitySiege = LibStub("AceAddon-3.0"):NewAddon("CitySiege", "AceConsole-3.0", "AceEvent-3.0")
local Core = CitySiege

-- Local references
local L = setmetatable({}, {
    __index = function(t, k)
        return k
    end
})

-- Initialization
function Core:OnInitialize()
    -- Set up database with defaults
    self.db = LibStub("AceDB-3.0"):New("CitySiegeDB", {
        profile = CitySiege_Utils:CopyTable(CitySiege_DefaultConfig),
        global = {
            activeSieges = {},
            statistics = {
                siegesParticipated = 0,
                siegesWon = 0,
                siegesLost = 0,
                totalKills = 0,
                totalDeaths = 0,
            },
        },
    }, true)
    
    -- Initialize config
    CitySiege_Config:Initialize(self.db)
    
    -- Register chat commands
    self:RegisterChatCommand("citysiege", "SlashCommand")
    self:RegisterChatCommand("cs", "SlashCommand")
    
    -- Create minimap button
    if CitySiege_MinimapButton then
        CitySiege_MinimapButton:Initialize()
    end
    
    -- Print welcome message
    CitySiege_Utils:Print("Addon loaded! Type |cFFFFFF00/cs|r or |cFFFFFF00/citysiege|r for commands.")
end

function Core:OnEnable()
    -- Register events
    self:RegisterEvent("PLAYER_ENTERING_WORLD")
    self:RegisterEvent("PLAYER_LEAVING_WORLD")
    self:RegisterEvent("ZONE_CHANGED_NEW_AREA")
    self:RegisterEvent("CHAT_MSG_SYSTEM")
    self:RegisterEvent("PLAYER_REGEN_DISABLED") -- Enter combat
    self:RegisterEvent("PLAYER_REGEN_ENABLED")  -- Leave combat
    
    -- Register addon message prefix
    RegisterAddonMessagePrefix("CitySiege")
    
    -- Add chat filter to hide CitySiege messages
    ChatFrame_AddMessageEventFilter("CHAT_MSG_SYSTEM", function(self, event, message, ...)
        if message and (string.find(message, "CitySiege") or string.find(message, "UPDATE:") or string.find(message, "START:") or string.find(message, "MAP_DATA:")) then
            return true -- Filter it out (hide it)
        end
        return false -- Show it
    end)
    
    -- Initialize modules
    if CitySiege_EventHandler then
        CitySiege_EventHandler:Initialize()
    end
    
    if CitySiege_SiegeTracker then
        CitySiege_SiegeTracker:Initialize()
    end
    
    if CitySiege_MainFrame then
        CitySiege_MainFrame:Initialize()
    end
    
    -- Check if we're in a siege city
    self:CheckCurrentZone()
end

function Core:OnDisable()
    -- Clean up
    if CitySiege_MainFrame and CitySiege_MainFrame.Hide then
        CitySiege_MainFrame:Hide()
    end
end

-- Event Handlers
function Core:PLAYER_ENTERING_WORLD()
    self:CheckCurrentZone()
    
    -- Request siege status update after 2 seconds
    if CitySiege_SiegeTracker then
        local frame = CreateFrame("Frame")
        local elapsed = 0
        frame:SetScript("OnUpdate", function(self, delta)
            elapsed = elapsed + delta
            if elapsed >= 2 then
                self:SetScript("OnUpdate", nil)
                CitySiege_SiegeTracker:RequestStatusUpdate()
            end
        end)
    end
end

function Core:PLAYER_LEAVING_WORLD()
    -- Save current state
end

function Core:ZONE_CHANGED_NEW_AREA()
    self:CheckCurrentZone()
end

function Core:PLAYER_REGEN_DISABLED()
    -- Entered combat
    if CitySiege_MainFrame and not CitySiege_Config:GetUISettings().showInCombat then
        if CitySiege_MainFrame.Hide then
            CitySiege_MainFrame:Hide()
        end
    end
end

function Core:PLAYER_REGEN_ENABLED()
    -- Left combat
    if CitySiege_MainFrame and CitySiege_Config:GetUISettings().showInCombat then
        if CitySiege_MainFrame.wasShownBeforeCombat then
            CitySiege_MainFrame:Show()
        end
    end
end

function Core:CHAT_MSG_SYSTEM(event, message)
    -- Check if this is a CitySiege addon message (should be invisible)
    if message and string.match(message, "^CitySiege\t") then
        -- Extract the actual message after the tab
        local addonMessage = string.match(message, "^CitySiege\t(.+)$")
        if addonMessage and CitySiege_EventHandler then
            CitySiege_EventHandler:ParseAddonMessage(addonMessage)
        end
        -- Don't return - let it continue to parse as system message
    end
    
    -- Monitor system messages for siege-related announcements
    if CitySiege_SiegeTracker then
        CitySiege_SiegeTracker:ParseSystemMessage(message)
    end
    
    -- Also let EventHandler parse it
    if CitySiege_EventHandler and CitySiege_EventHandler.OnChatMessage then
        CitySiege_EventHandler:OnChatMessage(message)
    end
end

-- Slash command handler
function Core:SlashCommand(input)
    local args = CitySiege_Utils:ParseArgs(input)
    local cmd = string.lower(args[1] or "")
    
    if cmd == "" or cmd == "help" then
        self:ShowHelp()
    elseif cmd == "show" then
        self:ToggleMainFrame()
    elseif cmd == "hide" then
        if CitySiege_MainFrame then
            CitySiege_MainFrame:Hide()
        end
    elseif cmd == "config" or cmd == "settings" then
        self:OpenSettings()
    elseif cmd == "minimap" then
        self:ToggleMinimap()
    elseif cmd == "status" then
        self:RequestStatus()
    elseif cmd == "sync" then
        -- Force sync with server (silent operation)
        self:RequestStatus()
        -- Also manually create siege data if we know there's one
        local cityID = tonumber(args[2])
        if cityID and CitySiege_EventHandler then
            CitySiege_EventHandler:ParseAddonMessage("UPDATE:" .. cityID .. ":2:0:0:0")
        end
    elseif cmd == "start" then
        self:SendCommand("start", args[2])
    elseif cmd == "stop" then
        self:SendCommand("stop", args[2], args[3])
    elseif cmd == "cleanup" then
        self:SendCommand("cleanup", args[2])
    elseif cmd == "info" then
        self:SendCommand("info")
    elseif cmd == "reload" then
        self:SendCommand("reload")
    elseif cmd == "reset" then
        self:ResetAddon()
    elseif cmd == "debug" then
        self:ToggleDebug()
    elseif cmd == "testmap" then
        -- Enable test mode for map visualization
        if CitySiege_MainFrame then
            local cityID = tonumber(args[2]) or CitySiege_Cities.STORMWIND
            CitySiege_Utils:Print("Opening addon with test city " .. cityID)
            
            -- Show the main frame first
            CitySiege_MainFrame:Show()
            
            -- Then select the city (will trigger data fetch)
            CitySiege_MainFrame:SelectCity(cityID)
            
            -- Simulate siege data after delay
            if CitySiege_EventHandler then
                local frame = CreateFrame("Frame")
                local elapsed = 0
                frame:SetScript("OnUpdate", function(self, delta)
                    elapsed = elapsed + delta
                    if elapsed >= 0.5 then
                        self:SetScript("OnUpdate", nil)
                        CitySiege_EventHandler:ParseAddonMessage("START:" .. cityID .. ":Horde")
                        
                        -- Wait another second then send UPDATE
                        local frame2 = CreateFrame("Frame")
                        local elapsed2 = 0
                        frame2:SetScript("OnUpdate", function(self, delta)
                            elapsed2 = elapsed2 + delta
                            if elapsed2 >= 1 then
                                self:SetScript("OnUpdate", nil)
                                CitySiege_EventHandler:ParseAddonMessage("UPDATE:" .. cityID .. ":2:25:30:120")
                            end
                        end)
                    end
                end)
            end
        end
    elseif cmd == "testoff" then
        -- Disable test mode
        if CitySiege_SiegeTracker then
            CitySiege_SiegeTracker:Clear()
            CitySiege_Utils:Print("Test data cleared")
        end
    else
        CitySiege_Utils:Print("Unknown command. Type |cFFFFFF00/cs help|r for available commands.")
    end
end

function Core:ShowHelp()
    CitySiege_Utils:Print("=== City Siege Commands ===")
    CitySiege_Utils:Print("|cFFFFFF00/cs show|r - Toggle main window")
    CitySiege_Utils:Print("|cFFFFFF00/cs config|r - Open settings")
    CitySiege_Utils:Print("|cFFFFFF00/cs minimap|r - Toggle minimap button")
    CitySiege_Utils:Print("|cFFFFFF00/cs status|r - Show siege status")
    CitySiege_Utils:Print("|cFFFFFF00/cs start [city]|r - Start a siege (GM only)")
    CitySiege_Utils:Print("|cFFFFFF00/cs stop [city] [faction]|r - Stop a siege (GM only)")
    CitySiege_Utils:Print("|cFFFFFF00/cs cleanup [city]|r - Clean up siege NPCs (GM only)")
    CitySiege_Utils:Print("|cFFFFFF00/cs info|r - Show detailed info (GM only)")
    CitySiege_Utils:Print("|cFFFFFF00/cs reload|r - Reload config (Admin only)")
    CitySiege_Utils:Print("|cFFFFFF00/cs reset|r - Reset addon settings")
    CitySiege_Utils:Print("|cFF00FF00/cs testmap [cityID]|r - Enable test mode with demo data")
    CitySiege_Utils:Print("|cFF00FF00/cs testoff|r - Disable test mode")
end

function Core:ToggleMainFrame()
    if CitySiege_MainFrame then
        if CitySiege_MainFrame:IsShown() then
            CitySiege_MainFrame:Hide()
        else
            CitySiege_MainFrame:Show()
        end
    end
end

function Core:OpenSettings()
    if CitySiege_SettingsPanel then
        CitySiege_SettingsPanel:Show()
    else
        CitySiege_Utils:Print("Settings panel not available.")
    end
end

function Core:ToggleMinimap()
    local settings = CitySiege_Config:GetMinimapSettings()
    settings.hide = not settings.hide
    
    if CitySiege_MinimapButton then
        if settings.hide then
            CitySiege_MinimapButton:Hide()
            CitySiege_Utils:Print("Minimap button hidden. Use |cFFFFFF00/cs minimap|r to show it again.")
        else
            CitySiege_MinimapButton:Show()
            CitySiege_Utils:Print("Minimap button shown.")
        end
    end
end

function Core:RequestStatus()
    SendChatMessage(".citysiege status", "GUILD")
    -- Silent operation - no user-facing message
end

function Core:SendCommand(cmd, arg1, arg2)
    local command = ".citysiege " .. cmd
    if arg1 then command = command .. " " .. arg1 end
    if arg2 then command = command .. " " .. arg2 end
    
    SendChatMessage(command, "GUILD")
    CitySiege_Utils:Print("Command sent: " .. command)
end

function Core:ResetAddon()
    StaticPopup_Show("CITYSIEGE_RESET_CONFIRM")
end

function Core:ToggleDebug()
    if not self.db.profile.debug then
        self.db.profile.debug = false
    end
    self.db.profile.debug = not self.db.profile.debug
    
    if self.db.profile.debug then
        CitySiege_Utils:Print("Debug mode enabled.")
    else
        CitySiege_Utils:Print("Debug mode disabled.")
    end
end

function Core:CheckCurrentZone()
    -- Check if player is in a siege city
    for cityID, cityData in pairs(CitySiege_CityData) do
        if CitySiege_Utils:IsPlayerInCity(cityID) then
            -- Player is in a city, check if there's an active siege
            if CitySiege_SiegeTracker then
                CitySiege_SiegeTracker:CheckCitySiege(cityID)
            end
            return
        end
    end
end

-- Static popup dialogs
StaticPopupDialogs["CITYSIEGE_RESET_CONFIRM"] = {
    text = "Are you sure you want to reset all City Siege addon settings?",
    button1 = "Yes",
    button2 = "No",
    OnAccept = function()
        CitySiege_Config:ResetAll()
        CitySiege_Utils:Print("All settings have been reset. Please reload your UI.")
    end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
    preferredIndex = 3,
}

-- API Functions for other modules
function Core:GetActiveSieges()
    return CitySiege_Config:GetActiveSieges()
end

function Core:GetCityData(cityID)
    return CitySiege_CityData[cityID]
end

function Core:IsSiegeActive(cityID)
    local sieges = self:GetActiveSieges()
    return sieges[cityID] ~= nil
end
