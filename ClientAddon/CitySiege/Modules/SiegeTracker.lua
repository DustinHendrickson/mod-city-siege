--[[
    City Siege Addon - Siege Tracker
    Tracks active sieges, stats, and participating entities
]]

CitySiege_SiegeTracker = {}
local Tracker = CitySiege_SiegeTracker

-- Local data
local activeSieges = {}
local updateTimer = nil
local lastUpdate = 0

function Tracker:Initialize()
    -- Load saved sieges
    activeSieges = CitySiege_Config:GetActiveSieges()
    
    -- Start update timer
    self:StartUpdateTimer()
    
    CitySiege_Utils:Debug("Siege Tracker initialized")
end

function Tracker:StartUpdateTimer()
    if updateTimer then return end
    
    local updateInterval = CitySiege_Config:GetMapSettings().updateInterval or 1.0
    
    -- Use OnUpdate frame for 3.3.5a compatibility
    local frame = CreateFrame("Frame")
    local elapsed = 0
    frame:SetScript("OnUpdate", function(self, delta)
        elapsed = elapsed + delta
        if elapsed >= updateInterval then
            elapsed = 0
            Tracker:Update()
        end
    end)
    
    updateTimer = frame
end

function Tracker:StopUpdateTimer()
    if updateTimer then
        updateTimer:SetScript("OnUpdate", nil)
        updateTimer = nil
    end
end

function Tracker:Update()
    local currentTime = time()
    if currentTime - lastUpdate < 0.5 then
        return -- Throttle updates
    end
    
    lastUpdate = currentTime
    
    -- Update each active siege
    for cityID, siegeData in pairs(activeSieges) do
        self:UpdateSiege(cityID, siegeData)
    end
    
    -- Notify UI to refresh
    if CitySiege_MainFrame and CitySiege_MainFrame.UpdateSiegeDisplay then
        CitySiege_MainFrame:UpdateSiegeDisplay()
    end
    
    if CitySiege_MapDisplay and CitySiege_MapDisplay.UpdatePositions then
        CitySiege_MapDisplay:UpdatePositions()
    end
end

function Tracker:UpdateSiege(cityID, siegeData)
    if not siegeData then return end
    
    -- Update elapsed time
    if siegeData.startTime then
        siegeData.elapsedTime = GetTime() - siegeData.startTime
    end
    
    -- Update phase timer
    if siegeData.phaseStartTime then
        siegeData.phaseElapsed = GetTime() - siegeData.phaseStartTime
    end
    
    -- Save updated data
    activeSieges[cityID] = siegeData
    CitySiege_Config:SaveActiveSiege(cityID, siegeData)
end

function Tracker:AddSiege(cityID, siegeData)
    if not cityID or not siegeData then return end
    
    local cityData = CitySiege_CityData[cityID]
    if not cityData then return end
    
    -- Initialize siege data
    siegeData.cityID = cityID
    siegeData.cityName = cityData.name
    siegeData.startTime = siegeData.startTime or GetTime()
    siegeData.status = siegeData.status or "Active"
    siegeData.phase = siegeData.phase or 1
    siegeData.phaseStartTime = siegeData.phaseStartTime or GetTime()
    
    -- Set default values
    siegeData.attackers = siegeData.attackers or {}
    siegeData.defenders = siegeData.defenders or {}
    siegeData.npcs = siegeData.npcs or {}
    siegeData.objectives = siegeData.objectives or {}
    
    siegeData.stats = siegeData.stats or {
        attackerKills = 0,
        defenderKills = 0,
        attackerDeaths = 0,
        defenderDeaths = 0,
        npcKills = 0,
        objectivesCaptured = 0,
    }
    
    activeSieges[cityID] = siegeData
    CitySiege_Config:SaveActiveSiege(cityID, siegeData)
    
    -- Notify
    if CitySiege_Config:IsNotificationEnabled("siegeStart") then
        local color = CitySiege_GetCityColorString(cityID)
        CitySiege_Utils:Print(string.format("Siege started in %s%s|r!", color, cityData.displayName))
        
        if CitySiege_Config:IsSoundEnabled() then
            PlaySound("PVPTHROUGHQUEUE")
        end
    end
    
    -- Update UI
    self:Update()
    
    CitySiege_Utils:Debug("Added siege for " .. cityData.name)
end

function Tracker:RemoveSiege(cityID, winner)
    if not cityID then return end
    
    local siegeData = activeSieges[cityID]
    if not siegeData then return end
    
    local cityData = CitySiege_CityData[cityID]
    
    -- Notify
    if CitySiege_Config:IsNotificationEnabled("siegeEnd") then
        local color = CitySiege_GetCityColorString(cityID)
        local message = string.format("Siege ended in %s%s|r!", color, cityData.displayName)
        
        if winner then
            message = message .. string.format(" Winner: %s", winner)
        end
        
        CitySiege_Utils:Print(message)
        
        if CitySiege_Config:IsSoundEnabled() then
            PlaySound("QUESTCOMPLETE")
        end
    end
    
    -- Update statistics
    CitySiege_Config:IncrementStat("siegesParticipated", 1)
    
    -- Remove from active sieges
    activeSieges[cityID] = nil
    CitySiege_Config:RemoveActiveSiege(cityID)
    
    -- Update UI
    self:Update()
    
    CitySiege_Utils:Debug("Removed siege for " .. cityData.name)
end

function Tracker:GetSiege(cityID)
    return activeSieges[cityID]
end

function Tracker:GetAllSieges()
    return activeSieges
end

function Tracker:IsSiegeActive(cityID)
    return activeSieges[cityID] ~= nil
end

function Tracker:UpdatePhase(cityID, phase)
    local siegeData = activeSieges[cityID]
    if not siegeData then return end
    
    siegeData.phase = phase
    siegeData.phaseStartTime = GetTime()
    
    CitySiege_Config:SaveActiveSiege(cityID, siegeData)
    
    -- Notify
    if CitySiege_Config:IsNotificationEnabled("phaseChange") then
        local cityData = CitySiege_CityData[cityID]
        local color = CitySiege_GetCityColorString(cityID)
        CitySiege_Utils:Print(string.format("%s%s|r siege entered Phase %d", color, cityData.displayName, phase))
    end
    
    self:Update()
end

function Tracker:UpdateStats(cityID, statType, value)
    local siegeData = activeSieges[cityID]
    if not siegeData or not siegeData.stats then return end
    
    if siegeData.stats[statType] ~= nil then
        siegeData.stats[statType] = siegeData.stats[statType] + (value or 1)
        CitySiege_Config:SaveActiveSiege(cityID, siegeData)
    end
end

function Tracker:AddParticipant(cityID, participantData, side)
    local siegeData = activeSieges[cityID]
    if not siegeData then return end
    
    side = side or "attackers"
    
    if not siegeData[side] then
        siegeData[side] = {}
    end
    
    siegeData[side][participantData.guid or participantData.name] = participantData
    CitySiege_Config:SaveActiveSiege(cityID, siegeData)
end

function Tracker:RemoveParticipant(cityID, guid, side)
    local siegeData = activeSieges[cityID]
    if not siegeData then return end
    
    side = side or "attackers"
    
    if siegeData[side] and siegeData[side][guid] then
        siegeData[side][guid] = nil
        CitySiege_Config:SaveActiveSiege(cityID, siegeData)
    end
end

function Tracker:GetParticipants(cityID, side)
    local siegeData = activeSieges[cityID]
    if not siegeData then return {} end
    
    side = side or "attackers"
    return siegeData[side] or {}
end

function Tracker:AddNPC(cityID, npcData)
    local siegeData = activeSieges[cityID]
    if not siegeData then return end
    
    if not siegeData.npcs then
        siegeData.npcs = {}
    end
    
    siegeData.npcs[npcData.guid] = npcData
    CitySiege_Config:SaveActiveSiege(cityID, siegeData)
end

function Tracker:RemoveNPC(cityID, guid)
    local siegeData = activeSieges[cityID]
    if not siegeData or not siegeData.npcs then return end
    
    if siegeData.npcs[guid] then
        siegeData.npcs[guid] = nil
        CitySiege_Config:SaveActiveSiege(cityID, siegeData)
    end
end

function Tracker:GetNPCs(cityID)
    local siegeData = activeSieges[cityID]
    if not siegeData then return {} end
    
    return siegeData.npcs or {}
end

function Tracker:RequestStatusUpdate()
    -- Send command to get status (silent operation)
    SendChatMessage(".citysiege status", "GUILD")
end

function Tracker:CheckCitySiege(cityID)
    -- Check if there's an active siege in this city
    local siegeData = activeSieges[cityID]
    
    if siegeData then
        -- Show notification that player is in an active siege zone
        local cityData = CitySiege_CityData[cityID]
        local color = CitySiege_GetCityColorString(cityID)
        CitySiege_Utils:Print(string.format("You are in %s%s|r which is under siege!", color, cityData.displayName))
        
        -- Auto-show main frame if configured
        if CitySiege_MainFrame and not CitySiege_Config:GetUISettings().autoHide then
            CitySiege_MainFrame:Show()
            CitySiege_MainFrame:SelectCity(cityID)
        end
    end
end

function Tracker:ParseSystemMessage(message)
    -- Parse system messages for siege-related events
    -- This is a placeholder - actual implementation would depend on server message format
    
    if string.find(message, "siege") or string.find(message, "Siege") then
        CitySiege_Utils:Debug("Siege-related system message: " .. message)
        
        -- Try to parse city name and event type
        -- Example: "A siege has started in Stormwind!"
        -- Example: "The siege of Orgrimmar has ended!"
        
        -- This would need to match the actual server message format
    end
end

function Tracker:Clear()
    activeSieges = {}
    CitySiege_Config:ClearActiveSieges()
    self:Update()
end
