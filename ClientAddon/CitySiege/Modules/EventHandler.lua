--[[
    City Siege Addon - Event Handler
    Handles game events and parses server communications
]]

CitySiege_EventHandler = {}
local EventHandler = CitySiege_EventHandler

function EventHandler:Initialize()
    -- Register for addon communication
    RegisterAddonMessagePrefix("CitySiege")
    
    -- Register event frame for chat messages
    if not self.eventFrame then
        self.eventFrame = CreateFrame("Frame")
        self.eventFrame:RegisterEvent("CHAT_MSG_SYSTEM")
        self.eventFrame:SetScript("OnEvent", function(frame, event, ...)
            if event == "CHAT_MSG_SYSTEM" then
                EventHandler:OnChatMessage(...)
            end
        end)
    end
    
    CitySiege_Utils:Debug("Event Handler initialized")
end

-- Handle chat system messages (server uses this for addon data)
function EventHandler:OnChatMessage(message, ...)
    if not message then return end
    
    -- Check if it's a City Siege addon message
    if string.find(message, "^CITYSIEGE_") then
        local data = string.gsub(message, "^CITYSIEGE_", "")
        self:ParseAddonMessage(data)
    end
end

-- Handle addon messages from server (if supported)
function EventHandler:CHAT_MSG_ADDON(prefix, message, channel, sender)
    if prefix ~= "CitySiege" then return end
    
    self:ParseAddonMessage(message)
end

function EventHandler:ParseAddonMessage(message)
    -- Parse structured messages from server
    -- Expected format from C++: "START:cityId:faction" or "UPDATE:cityId:phase:attackers:defenders:elapsed" or "END:cityId:winner"
    
    local command = string.match(message, "^([^:]+)")
    if not command then return end
    
    command = string.upper(command)
    
    if command == "START" then
        -- Format: START:cityId:faction
        local cityID, faction = string.match(message, "^START:(%d+):(%w+)")
        if cityID then
            self:HandleSiegeStart(tonumber(cityID), faction)
        end
    elseif command == "UPDATE" then
        -- Format: UPDATE:cityId:phase:attackers:defenders:elapsed
        local cityID, phase, attackers, defenders, elapsed = string.match(message, "^UPDATE:(%d+):(%d+):(%d+):(%d+):(%d+)")
        if cityID then
            self:HandleSiegeUpdate(tonumber(cityID), tonumber(phase), tonumber(attackers), tonumber(defenders), tonumber(elapsed))
        end
    elseif command == "END" then
        -- Format: END:cityId:winner
        local cityID, winner = string.match(message, "^END:(%d+):(%w+)")
        if cityID then
            self:HandleSiegeEnd(tonumber(cityID), winner)
        end
    elseif command == "POS" then
        -- Format: POS:cityId:guid:x:y:z:type
        local cityID, guid, x, y, z, unitType = string.match(message, "^POS:(%d+):(%d+):([%d%.%-]+):([%d%.%-]+):([%d%.%-]+):(%w+)")
        if cityID and guid then
            self:HandlePositionUpdate(tonumber(cityID), tonumber(guid), tonumber(x), tonumber(y), tonumber(z), unitType)
        end
    end
end

function EventHandler:HandleSiegeStart(cityID, faction)
    if not cityID then return end
    
    CitySiege_Utils:Debug("Siege started at city " .. cityID .. " by " .. (faction or "Unknown"))
    
    local siegeData = {
        cityID = cityID,
        attackingFaction = faction,
        phase = 1,
        status = "Active",
        startTime = time(),
        attackerCount = 0,
        defenderCount = 0,
        npcs = {},
        waypoints = {},
    }
    
    if CitySiege_SiegeTracker then
        CitySiege_SiegeTracker:AddSiege(cityID, siegeData)
    end
    
    -- Update UI
    if CitySiege_MainFrame and CitySiege_MainFrame.UpdateInfo then
        CitySiege_MainFrame:UpdateInfo()
    end
end

function EventHandler:HandleSiegeEnd(cityID, winner)
    if not cityID then return end
    
    CitySiege_Utils:Debug("Siege ended at city " .. cityID .. ", winner: " .. (winner or "Unknown"))
    
    if CitySiege_SiegeTracker then
        CitySiege_SiegeTracker:RemoveSiege(cityID, winner)
    end
    
    -- Update UI
    if CitySiege_MainFrame and CitySiege_MainFrame.UpdateInfo then
        CitySiege_MainFrame:UpdateInfo()
    end
end

function EventHandler:HandleSiegeUpdate(cityID, phase, attackerCount, defenderCount, elapsed)
    if not cityID then return end
    
    CitySiege_Utils:Debug(string.format("Siege update for city %d: phase=%d, attackers=%d, defenders=%d, elapsed=%d", 
        cityID, phase or 0, attackerCount or 0, defenderCount or 0, elapsed or 0))
    
    if CitySiege_SiegeTracker then
        local siegeData = CitySiege_SiegeTracker:GetSiege(cityID)
        if siegeData then
            siegeData.phase = phase
            siegeData.attackerCount = attackerCount
            siegeData.defenderCount = defenderCount
            siegeData.elapsed = elapsed
            CitySiege_SiegeTracker:UpdateSiege(cityID, siegeData)
        end
    end
    
    -- Update UI
    if CitySiege_MainFrame and CitySiege_MainFrame.UpdateInfo then
        CitySiege_MainFrame:UpdateInfo()
    end
end

function EventHandler:HandlePositionUpdate(cityID, guid, x, y, z, unitType)
    if not cityID or not guid then return end
    
    if CitySiege_SiegeTracker then
        local siegeData = CitySiege_SiegeTracker:GetSiege(cityID)
        if siegeData then
            siegeData.npcs = siegeData.npcs or {}
            siegeData.npcs[guid] = {
                x = x,
                y = y,
                z = z,
                type = unitType,
                lastUpdate = time(),
            }
        end
    end
    
    -- Update map display
    if CitySiege_MapDisplay then
        CitySiege_MapDisplay:UpdateNPCPositions(cityID)
    end
end

-- Helper function to send data to server
function EventHandler:SendToServer(command, ...)
    local message = command
    for i = 1, select("#", ...) do
        local arg = select(i, ...)
        message = message .. ":" .. tostring(arg)
    end
    
    SendAddonMessage("CitySiege", message, "GUILD")
end
