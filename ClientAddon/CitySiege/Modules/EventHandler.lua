--[[
    City Siege Addon - Event Handler
    Handles game events and parses server communications
]]

CitySiege_EventHandler = {}
local EventHandler = CitySiege_EventHandler

function EventHandler:Initialize()
    -- Register for addon communication
    RegisterAddonMessagePrefix("CitySiege")
    
    -- Note: CHAT_MSG_SYSTEM is handled by Core.lua which calls EventHandler:OnChatMessage
    
    CitySiege_Utils:Debug("Event Handler initialized - listening for server data")
end

-- Handle chat system messages (server uses this for addon data)
function EventHandler:OnChatMessage(message, ...)
    if not message then return end
    
    -- Check for CitySiege tab-separated format (primary)
    if string.find(message, "^CitySiege\t") then
        local data = string.gsub(message, "^CitySiege\t", "")
        self:ParseAddonMessage(data)
        return
    end
    
    -- Check for legacy formats (backup)
    if string.find(message, "^CITYSIEGE_") or string.find(message, "^CITYSIEGE:") then
        local data = string.gsub(message, "^CITYSIEGE[_:]", "")
        self:ParseAddonMessage(data)
        return
    end
    
    -- ALSO parse the .citysiege status output text (for manual status checks)
    if string.find(message, "Active Sieges:") then
        local count = string.match(message, "Active Sieges: (%d+)")
        if count and tonumber(count) > 0 then
            CitySiege_Utils:Debug("Server reports " .. count .. " active siege(s)")
        end
    end
    
    -- Parse siege details from status output
    local cityName, phase, minutes = string.match(message, "%-%s+([%w%s]+)%s+%(%d+%)%s+%-%s+Phase:%s+(%w+)%s+%-%s+(%d+)%s+minutes remaining")
    if cityName and phase and minutes then
        CitySiege_Utils:Debug("Parsed siege: " .. cityName .. ", Phase: " .. phase)
        
        -- Find the city ID
        for id, data in pairs(CitySiege_CityData) do
            if string.find(cityName, data.name) or string.find(data.displayName, cityName) then
                local siegeData = {
                    cityID = id,
                    phase = (phase == "Cinematic" and 1 or 2),
                    attackerCount = 0,
                    defenderCount = 0,
                    elapsedTime = 0,
                    status = "Active",
                    startTime = time(),
                    stats = {
                        attackerKills = 0,
                        defenderKills = 0,
                    },
                }
                
                if CitySiege_SiegeTracker then
                    CitySiege_SiegeTracker:AddSiege(id, siegeData)
                end
                
                if CitySiege_MainFrame then
                    CitySiege_MainFrame:UpdateSiegeDisplay()
                end
                break
            end
        end
    end
end

-- Handle addon messages from server (if supported)
function EventHandler:CHAT_MSG_ADDON(prefix, message, channel, sender)
    if prefix ~= "CitySiege" then return end
    
    self:ParseAddonMessage(message)
end

function EventHandler:ParseAddonMessage(message)
    if not message then return end
    
    local command = string.match(message, "^([^:]+)")
    if not command then return end
    
    command = string.upper(command)
    
    if command == "START" then
        -- Format: START:cityId:faction:spawnX:spawnY:spawnZ:leaderX:leaderY:leaderZ:centerX:centerY:centerZ
        local parts = {}
        for part in string.gmatch(message, "([^:]+)") do
            table.insert(parts, part)
        end
        
        if #parts >= 3 then
            local cityID = tonumber(parts[2])
            local faction = parts[3]
            
            -- Parse coordinates if available
            local coords = {}
            if #parts >= 12 then
                coords.spawnX = tonumber(parts[4])
                coords.spawnY = tonumber(parts[5])
                coords.spawnZ = tonumber(parts[6])
                coords.leaderX = tonumber(parts[7])
                coords.leaderY = tonumber(parts[8])
                coords.leaderZ = tonumber(parts[9])
                coords.centerX = tonumber(parts[10])
                coords.centerY = tonumber(parts[11])
                coords.centerZ = tonumber(parts[12])
            end
            
            self:HandleSiegeStart(cityID, faction, coords)
        end
        
    elseif command == "UPDATE" then
        -- Format: UPDATE:cityId:phase:attackers:defenders:elapsed:remaining:leaderHealth:WP:count:x:y:z...:ATK:count:x:y:z...:DEF:count:x:y:z...:BATK:count:x:y:z...:BDEF:count:x:y:z...
        local parts = {}
        for part in string.gmatch(message, "([^:]+)") do
            table.insert(parts, part)
        end
        
        if #parts >= 8 then
            local cityID = tonumber(parts[2])
            local phase = tonumber(parts[3])
            local attackerCount = tonumber(parts[4])
            local defenderCount = tonumber(parts[5])
            local elapsed = tonumber(parts[6])
            local remaining = tonumber(parts[7])
            local leaderHealth = tonumber(parts[8])
            
            -- Parse waypoints, attacker positions, defender positions, bot positions
            local data = {
                waypoints = {},
                attackerPositions = {},
                defenderPositions = {},
                attackerBots = {},
                defenderBots = {}
            }
            
            local i = 9
            while i <= #parts do
                local section = parts[i]
                
                if section == "WP" then
                    -- Waypoints
                    i = i + 1
                    local wpCount = tonumber(parts[i]) or 0
                    i = i + 1
                    for j = 1, wpCount do
                        if i + 2 <= #parts then
                            table.insert(data.waypoints, {
                                x = tonumber(parts[i]),
                                y = tonumber(parts[i + 1]),
                                z = tonumber(parts[i + 2])
                            })
                            i = i + 3
                        end
                    end
                    
                elseif section == "ATK" then
                    -- Attacker positions
                    i = i + 1
                    local atkCount = tonumber(parts[i]) or 0
                    i = i + 1
                    for j = 1, atkCount do
                        if i + 2 <= #parts then
                            table.insert(data.attackerPositions, {
                                x = tonumber(parts[i]),
                                y = tonumber(parts[i + 1]),
                                z = tonumber(parts[i + 2])
                            })
                            i = i + 3
                        end
                    end
                    
                elseif section == "DEF" then
                    -- Defender positions
                    i = i + 1
                    local defCount = tonumber(parts[i]) or 0
                    i = i + 1
                    for j = 1, defCount do
                        if i + 2 <= #parts then
                            table.insert(data.defenderPositions, {
                                x = tonumber(parts[i]),
                                y = tonumber(parts[i + 1]),
                                z = tonumber(parts[i + 2])
                            })
                            i = i + 3
                        end
                    end
                    
                elseif section == "BATK" then
                    -- Attacker bot positions
                    i = i + 1
                    local botCount = tonumber(parts[i]) or 0
                    i = i + 1
                    for j = 1, botCount do
                        if i + 2 <= #parts then
                            table.insert(data.attackerBots, {
                                x = tonumber(parts[i]),
                                y = tonumber(parts[i + 1]),
                                z = tonumber(parts[i + 2])
                            })
                            i = i + 3
                        end
                    end
                    
                elseif section == "BDEF" then
                    -- Defender bot positions
                    i = i + 1
                    local botCount = tonumber(parts[i]) or 0
                    i = i + 1
                    for j = 1, botCount do
                        if i + 2 <= #parts then
                            table.insert(data.defenderBots, {
                                x = tonumber(parts[i]),
                                y = tonumber(parts[i + 1]),
                                z = tonumber(parts[i + 2])
                            })
                            i = i + 3
                        end
                    end
                else
                    i = i + 1
                end
            end
            
            self:HandleSiegeUpdate(cityID, phase, attackerCount, defenderCount, elapsed, remaining, leaderHealth, data)
        end
        
    elseif command == "END" then
        -- Format: END:cityId:winner
        local cityID, winner = string.match(message, "^END:(%d+):(%w+)")
        if cityID then
            self:HandleSiegeEnd(tonumber(cityID), winner)
        end
    end
end

function EventHandler:HandleSiegeStart(cityID, faction, coords)
    if not cityID then return end
    
    CitySiege_Utils:Debug("Siege started: city=" .. cityID .. ", faction=" .. (faction or "Unknown"))
    
    local siegeData = {
        cityID = cityID,
        attackingFaction = faction,
        phase = 1,
        status = "Active",
        startTime = GetTime(),
        attackerCount = 0,
        defenderCount = 0,
        npcs = {},
        waypoints = {},
        coords = coords or {},
        attackerPositions = {},
        defenderPositions = {},
        attackerBots = {},
        defenderBots = {},
        leaderHealth = 100,
        remaining = 0,
        stats = {
            attackerKills = 0,
            defenderKills = 0,
        },
    }
    
    if CitySiege_SiegeTracker then
        CitySiege_SiegeTracker:AddSiege(cityID, siegeData)
    end
    
    -- Update UI
    if CitySiege_MainFrame then
        if CitySiege_MainFrame.UpdateSiegeDisplay then
            CitySiege_MainFrame:UpdateSiegeDisplay()
        end
        if CitySiege_MainFrame.SelectCity then
            CitySiege_MainFrame:SelectCity(cityID)
        end
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

function EventHandler:HandleSiegeUpdate(cityID, phase, attackerCount, defenderCount, elapsed, remaining, leaderHealth, data)
    if not cityID then return end
    
    CitySiege_Utils:Debug(string.format("Siege update: city=%d, phase=%d, atk=%d, def=%d, time=%d, remaining=%d, leaderHP=%.1f", 
        cityID, phase or 0, attackerCount or 0, defenderCount or 0, elapsed or 0, remaining or 0, leaderHealth or 100))
    
    if CitySiege_SiegeTracker then
        local siegeData = CitySiege_SiegeTracker:GetSiege(cityID)
        if siegeData then
            siegeData.phase = phase
            siegeData.attackerCount = attackerCount
            siegeData.defenderCount = defenderCount
            siegeData.elapsedTime = elapsed
            siegeData.remaining = remaining
            siegeData.leaderHealth = leaderHealth
            
            -- Update position data if provided
            if data then
                siegeData.waypoints = data.waypoints or {}
                siegeData.attackerPositions = data.attackerPositions or {}
                siegeData.defenderPositions = data.defenderPositions or {}
                siegeData.attackerBots = data.attackerBots or {}
                siegeData.defenderBots = data.defenderBots or {}
            end
            
            if not siegeData.stats then
                siegeData.stats = {}
            end
            CitySiege_SiegeTracker:UpdateSiege(cityID, siegeData)
        else
            -- Create new siege data if it doesn't exist
            CitySiege_Utils:Debug("Creating new siege entry for city " .. cityID)
            siegeData = {
                cityID = cityID,
                phase = phase,
                attackerCount = attackerCount,
                defenderCount = defenderCount,
                elapsedTime = elapsed,
                remaining = remaining,
                leaderHealth = leaderHealth,
                status = "Active",
                startTime = GetTime() - (elapsed or 0),
                waypoints = data and data.waypoints or {},
                attackerPositions = data and data.attackerPositions or {},
                defenderPositions = data and data.defenderPositions or {},
                attackerBots = data and data.attackerBots or {},
                defenderBots = data and data.defenderBots or {},
                stats = {
                    attackerKills = 0,
                    defenderKills = 0,
                },
            }
            CitySiege_SiegeTracker:AddSiege(cityID, siegeData)
        end
    end
    
    -- Update UI
    if CitySiege_MainFrame and CitySiege_MainFrame.UpdateSiegeDisplay then
        CitySiege_MainFrame:UpdateSiegeDisplay()
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
