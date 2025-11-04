// Add this after the helper functions section (around line 700)

/**
 * @brief Broadcasts siege data to clients via addon messages
 * @param event The siege event to broadcast
 * @param messageType Type of message (START, UPDATE, END, POSITION)
 */
void BroadcastSiegeDataToAddon(const SiegeEvent& event, const std::string& messageType)
{
    const CityData& city = g_Cities[event.cityId];
    Map* map = sMapMgr->FindMap(city.mapId, 0);
    if (!map)
        return;

    std::ostringstream ss;
    
    if (messageType == "START")
    {
        // Format: START:cityId:faction
        bool isAllianceCity = (event.cityId <= CITY_EXODAR);
        std::string attackingFaction = isAllianceCity ? "Horde" : "Alliance";
        ss << "START:" << static_cast<uint32>(event.cityId) << ":" << attackingFaction;
    }
    else if (messageType == "UPDATE")
    {
        // Format: UPDATE:cityId:phase:attackers:defenders:elapsed
        uint32 attackerCount = event.spawnedCreatures.size();
        uint32 defenderCount = event.spawnedDefenders.size();
        uint32 elapsed = time(nullptr) - event.startTime;
        
        // Determine phase (1-4 based on time elapsed)
        uint32 phase = 1;
        if (!event.cinematicPhase)
        {
            uint32 duration = event.endTime - event.startTime;
            if (elapsed > duration * 0.75f) phase = 4;
            else if (elapsed > duration * 0.5f) phase = 3;
            else if (elapsed > duration * 0.25f) phase = 2;
        }
        
        ss << "UPDATE:" << static_cast<uint32>(event.cityId) << ":" << phase 
           << ":" << attackerCount << ":" << defenderCount << ":" << elapsed;
    }
    else if (messageType == "END")
    {
        // Format: END:cityId:winner
        ss << "END:" << static_cast<uint32>(event.cityId) << ":unknown";
    }
    else if (messageType == "POSITION")
    {
        // Format: POS:cityId:guid:x:y:z:type
        // This would be sent for individual units - called separately for each unit
        return; // Handle position updates separately
    }
    
    std::string message = ss.str();
    
    // Send to all players on the map
    Map::PlayerList const& players = map->GetPlayers();
    for (auto itr = players.begin(); itr != players.end(); ++itr)
    {
        if (Player* player = itr->GetSource())
        {
            WorldPacket data(SMSG_MESSAGECHAT, 200);
            data << uint8(CHAT_MSG_SYSTEM);
            data << uint32(LANG_UNIVERSAL);
            data << uint64(0);  // sender GUID
            data << uint32(0);
            data << uint64(0);  // receiver GUID
            data << uint32(message.length() + 1);
            data << message;
            data << uint8(0);   // chat tag
            
            // Prefix for addon
            std::string addonMessage = "CITYSIEGE:" + message;
            player->GetSession()->SendPacket(&data);
            
            if (g_DebugMode)
            {
                LOG_INFO("server.loading", "[City Siege] Sent addon message to {}: {}", 
                         player->GetName(), addonMessage);
            }
        }
    }
}

/**
 * @brief Broadcasts position updates for siege participants
 * @param event The siege event
 * @param guid The GUID of the unit
 * @param x X coordinate
 * @param y Y coordinate  
 * @param z Z coordinate
 * @param unitType Type of unit (ATTACKER, DEFENDER, NPC)
 */
void BroadcastPositionUpdate(const SiegeEvent& event, ObjectGuid guid, float x, float y, float z, const std::string& unitType)
{
    const CityData& city = g_Cities[event.cityId];
    Map* map = sMapMgr->FindMap(city.mapId, 0);
    if (!map)
        return;
        
    std::ostringstream ss;
    ss << "POS:" << static_cast<uint32>(event.cityId) << ":" 
       << guid.GetRawValue() << ":" 
       << std::fixed << std::setprecision(2) << x << ":" << y << ":" << z 
       << ":" << unitType;
       
    std::string message = ss.str();
    std::string addonMessage = "CITYSIEGE:" + message;
    
    // Send to players near the siege
    Map::PlayerList const& players = map->GetPlayers();
    for (auto itr = players.begin(); itr != players.end(); ++itr)
    {
        if (Player* player = itr->GetSource())
        {
            if (player->GetDistance(city.centerX, city.centerY, city.centerZ) <= 500.0f)
            {
                WorldPacket data(SMSG_MESSAGECHAT, 200);
                data << uint8(CHAT_MSG_SYSTEM);
                data << uint32(LANG_UNIVERSAL);
                data << uint64(0);
                data << uint32(0);
                data << uint64(0);
                data << uint32(addonMessage.length() + 1);
                data << addonMessage;
                data << uint8(0);
                
                player->GetSession()->SendPacket(&data);
            }
        }
    }
}
