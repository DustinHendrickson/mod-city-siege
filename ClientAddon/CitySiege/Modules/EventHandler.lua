--[[
    City Siege Addon - Event Handler
    Handles game events and parses server communications
]]

CitySiege_EventHandler = {}
local EventHandler = CitySiege_EventHandler

function EventHandler:Initialize()
    -- Register for addon communication (3.3.5 compatible)
    if RegisterAddonMessagePrefix then
        RegisterAddonMessagePrefix("CitySiege")
    end
    
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
    
    if command == "REQUEST_MAP" then
        -- Format: REQUEST_MAP:cityID
        -- Client is requesting map data, execute the server command
        local parts = {}
        for part in string.gmatch(message, "([^:]+)") do
            table.insert(parts, part)
        end
        
        if #parts >= 2 then
            local cityID = tonumber(parts[2])
            -- Use SendChatMessage instead of RunMacroText to avoid permission issues
            -- This sends the command to the server which is available to all players (SEC_PLAYER)
            SendChatMessage(".citysiege mapdata " .. cityID, "GUILD")
        end
        return
        
    elseif command == "START" then
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

            -- Map percentages for the muster point and the throne, so the
            -- map can mark both before the first route update arrives.
            if #parts >= 17 and parts[13] == "MAP" then
                coords.spawnMX = tonumber(parts[14])
                coords.spawnMY = tonumber(parts[15])
                coords.leaderMX = tonumber(parts[16])
                coords.leaderMY = tonumber(parts[17])
            end
            
            self:HandleSiegeStart(cityID, faction, coords)
        end
        
    elseif command == "UPDATE" then
        -- Format: UPDATE:cityId:phase:attackers:defenders:elapsed:remaining:leaderHealth:leaderName:WP:count:x:y:z...
        local parts = {}
        for part in string.gmatch(message, "([^:]+)") do
            table.insert(parts, part)
        end
        
        if #parts >= 9 then
            local cityID = tonumber(parts[2])
            local phase = tonumber(parts[3])
            local attackerCount = tonumber(parts[4])
            local defenderCount = tonumber(parts[5])
            local elapsed = tonumber(parts[6])
            local remaining = tonumber(parts[7])
            local leaderHealth = tonumber(parts[8])
            local leaderName = parts[9] or "Unknown Leader"
            
            -- Parse waypoints, attacker positions, defender positions, bot positions
            local data = {
                waypoints = {},
                attackerPositions = {},
                defenderPositions = {},
                attackerBots = {},
                defenderBots = {}
            }
            
            -- Section markers begin right after leaderName (part 9), so the
            -- first one is part 10. Scanning from 12 skipped the WP block
            -- entirely and left the map with no route to draw.
            local i = 10
            while i <= #parts do
                local section = parts[i]
                
                if section == "WP" then
                    -- Waypoints
                    i = i + 1
                    local wpCount = tonumber(parts[i]) or 0
                    i = i + 1
                    -- Each waypoint is x:y:z:mx:my - world position followed
                    -- by the server's map percentage for it.
                    for j = 1, wpCount do
                        if i + 4 <= #parts then
                            table.insert(data.waypoints, {
                                x = tonumber(parts[i]),
                                y = tonumber(parts[i + 1]),
                                z = tonumber(parts[i + 2]),
                                mx = tonumber(parts[i + 3]),
                                my = tonumber(parts[i + 4]),
                            })
                            i = i + 5
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
                else
                    i = i + 1
                end
            end
            
            self:HandleSiegeUpdate(cityID, phase, attackerCount, defenderCount, elapsed, remaining, leaderHealth, leaderName, data)
        end
        
    elseif command == "ROUTE" then
        -- Format: ROUTE:cityID:token:seq:total:count:mx:my:mx:my...
        -- The client refuses addon messages over 255 bytes, so the route
        -- arrives in pieces that are reassembled in HandleRouteChunk.
        local parts = {}
        for part in string.gmatch(message, "([^:]+)") do
            table.insert(parts, part)
        end

        if #parts >= 6 then
            local cityID = tonumber(parts[2])
            local token = tonumber(parts[3])
            local seq = tonumber(parts[4])
            local total = tonumber(parts[5])
            local count = tonumber(parts[6]) or 0

            local nodes = {}
            local i = 7
            for j = 1, count do
                if i + 1 <= #parts then
                    table.insert(nodes, {
                        mx = tonumber(parts[i]),
                        my = tonumber(parts[i + 1]),
                    })
                    i = i + 2
                end
            end

            self:HandleRouteChunk(cityID, token, seq, total, nodes)
        end

    elseif command == "END" then
        -- Format: END:cityId:winner
        local cityID, winner = string.match(message, "^END:(%d+):(%w+)")
        if cityID then
            self:HandleSiegeEnd(tonumber(cityID), winner)
        end
        
    elseif command == "MAP_DATA" then
        -- Format: MAP_DATA:cityID:WP:count:x:y:z...:LEADER:x:y:z
        local parts = {}
        for part in string.gmatch(message, "([^:]+)") do
            table.insert(parts, part)
        end
        
        if #parts >= 2 then
            local cityID = tonumber(parts[2])
            
            local data = {
                waypoints = {},
                muster = nil,
                leaderPos = nil,
            }
            
            local i = 3
            while i <= #parts do
                local section = parts[i]
                
                if section == "WP" then
                    -- Waypoints
                    i = i + 1
                    local wpCount = tonumber(parts[i]) or 0
                    i = i + 1
                    for j = 1, wpCount do
                        if i + 4 <= #parts then
                            table.insert(data.waypoints, {
                                x = tonumber(parts[i]),
                                y = tonumber(parts[i + 1]),
                                z = tonumber(parts[i + 2]),
                                mx = tonumber(parts[i + 3]),
                                my = tonumber(parts[i + 4]),
                            })
                            i = i + 5
                        end
                    end
                    
                elseif section == "LEADER" or section == "MUSTER" then
                    -- x:y:z:mx:my, same shape as a waypoint
                    if i + 5 <= #parts then
                        local pos = {
                            x = tonumber(parts[i + 1]),
                            y = tonumber(parts[i + 2]),
                            z = tonumber(parts[i + 3]),
                            mx = tonumber(parts[i + 4]),
                            my = tonumber(parts[i + 5]),
                        }
                        if section == "LEADER" then
                            data.leaderPos = pos
                        else
                            data.muster = pos
                        end
                        i = i + 6
                    else
                        i = i + 1
                    end
                else
                    i = i + 1
                end
            end
            
            -- Send to MapDisplay
            if CitySiege_MapDisplay then
                CitySiege_MapDisplay:UpdateMapData(cityID, data)
            end
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

-- Partial routes, keyed by city, until every chunk of one send has arrived.
local pendingRoutes = {}

function EventHandler:HandleRouteChunk(cityID, token, seq, total, nodes)
    if not cityID or not token or not seq or not total or total < 1 then return end

    local pending = pendingRoutes[cityID]
    if not pending or pending.token ~= token then
        -- A new send supersedes whatever was half-assembled before.
        pending = { token = token, total = total, chunks = {}, received = 0 }
        pendingRoutes[cityID] = pending
    end

    if not pending.chunks[seq] then
        pending.chunks[seq] = nodes
        pending.received = pending.received + 1
    end

    if pending.received < pending.total then
        return
    end

    local waypoints = {}
    for index = 1, pending.total do
        for _, node in ipairs(pending.chunks[index] or {}) do
            table.insert(waypoints, node)
        end
    end
    pendingRoutes[cityID] = nil

    CitySiege_Utils:Debug(string.format("Route for city %d assembled: %d waypoints in %d chunk(s)",
        cityID, #waypoints, total))

    if CitySiege_SiegeTracker then
        local siegeData = CitySiege_SiegeTracker:GetSiege(cityID)
        if siegeData then
            siegeData.waypoints = waypoints
            CitySiege_SiegeTracker:UpdateSiege(cityID, siegeData)
        end
    end

    if CitySiege_MapDisplay then
        CitySiege_MapDisplay:UpdateMapData(cityID, { waypoints = waypoints })
    end

    if CitySiege_MainFrame and CitySiege_MainFrame.UpdateSiegeDisplay then
        CitySiege_MainFrame:UpdateSiegeDisplay()
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

function EventHandler:HandleSiegeUpdate(cityID, phase, attackerCount, defenderCount, elapsed, remaining, leaderHealth, leaderName, data)
    if not cityID then return end
    
    CitySiege_Utils:Debug(string.format("Siege update: city=%d, phase=%d, atk=%d, def=%d, time=%d, remaining=%d, leaderHP=%.1f, leader=%s", 
        cityID, phase or 0, attackerCount or 0, defenderCount or 0, elapsed or 0, remaining or 0, leaderHealth or 100, leaderName or "Unknown"))
    
    if CitySiege_SiegeTracker then
        local siegeData = CitySiege_SiegeTracker:GetSiege(cityID)
        if siegeData then
            siegeData.phase = phase
            siegeData.attackerCount = attackerCount
            siegeData.defenderCount = defenderCount
            siegeData.elapsedTime = elapsed
            siegeData.remaining = remaining
            siegeData.leaderHealth = leaderHealth
            siegeData.leaderName = leaderName
            -- Stamped so the UI can count the timer down between server packets.
            siegeData.syncTime = GetTime()
            
            -- Update position data if provided. The route arrives separately
            -- (ROUTE chunks), so an update without one must not wipe it.
            if data then
                if data.waypoints and #data.waypoints > 0 then
                    siegeData.waypoints = data.waypoints
                end
                siegeData.attackerPositions = data.attackerPositions or {}
                siegeData.defenderPositions = data.defenderPositions or {}
                siegeData.attackerBots = data.attackerBots or {}
                siegeData.defenderBots = data.defenderBots or {}
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
                leaderName = leaderName,
                status = "Active",
                startTime = GetTime() - (elapsed or 0),
                syncTime = GetTime(),
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
    
    -- SendAddonMessage doesn't exist in 3.3.5, use chat commands instead
    if SendAddonMessage then
        SendAddonMessage("CitySiege", message, "GUILD")
    else
        -- Fallback: Use chat command (server should handle this)
        SendChatMessage(".citysiege " .. message, "GUILD")
    end
end
