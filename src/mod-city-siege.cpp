/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "Log.h"
#include "Configuration/Config.h"
#include "Common.h"
#include "World.h"
#include "Map.h"
#include "MapMgr.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "Language.h"
#include "ScriptedCreature.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cmath>

// -----------------------------------------------------------------------------
// CONFIGURATION VARIABLES
// -----------------------------------------------------------------------------

// Module enable/disable
static bool g_CitySiegeEnabled = true;
static bool g_DebugMode = false;

// Timer settings (in seconds for internal use)
static uint32 g_TimerMin = 120 * 60;  // 120 minutes default
static uint32 g_TimerMax = 240 * 60;  // 240 minutes default
static uint32 g_EventDuration = 30 * 60; // 30 minutes default

// Event settings
static bool g_AllowMultipleCities = false;
static uint32 g_AnnounceRadius = 500;
static uint32 g_MinimumLevel = 1;

// City enable/disable flags
static std::unordered_map<std::string, bool> g_CityEnabled;

// Spawn counts
static uint32 g_SpawnCountMinions = 15;
static uint32 g_SpawnCountElites = 5;
static uint32 g_SpawnCountMiniBosses = 2;
static uint32 g_SpawnCountLeaders = 1;

// Creature entries
static uint32 g_CreatureAllianceMinion = 14425;
static uint32 g_CreatureAllianceElite = 14428;
static uint32 g_CreatureAllianceMiniBoss = 14762;
static uint32 g_CreatureAllianceLeader = 16441;
static uint32 g_CreatureHordeMinion = 14423;
static uint32 g_CreatureHordeElite = 14426;
static uint32 g_CreatureHordeMiniBoss = 14763;
static uint32 g_CreatureHordeLeader = 16440;

// Aggro settings
static bool g_AggroPlayers = true;
static bool g_AggroNPCs = true;

// Cinematic settings
static uint32 g_CinematicDelay = 45; // seconds
static uint32 g_YellFrequency = 30;  // seconds

// Reward settings
static bool g_RewardOnDefense = true;
static uint32 g_RewardHonor = 100;
static uint32 g_RewardGold = 500000; // 50 gold in copper

// -----------------------------------------------------------------------------
// CITY SIEGE DATA STRUCTURES
// -----------------------------------------------------------------------------

enum CityId
{
    CITY_STORMWIND = 0,
    CITY_IRONFORGE,
    CITY_DARNASSUS,
    CITY_EXODAR,
    CITY_ORGRIMMAR,
    CITY_UNDERCITY,
    CITY_THUNDERBLUFF,
    CITY_SILVERMOON,
    CITY_MAX
};

struct CityData
{
    CityId id;
    std::string name;
    uint32 mapId;
    float centerX;
    float centerY;
    float centerZ;
    uint32 targetLeaderEntry; // Entry ID of the city leader to attack
};

// City definitions with approximate center coordinates
static std::vector<CityData> g_Cities = {
    { CITY_STORMWIND,   "Stormwind",      0,   -8913.23f, 554.633f,  93.7944f, 1748  }, // King Varian Wrynn
    { CITY_IRONFORGE,   "Ironforge",      0,   -4981.25f, -881.542f, 501.660f, 2784  }, // King Magni Bronzebeard
    { CITY_DARNASSUS,   "Darnassus",      1,    9947.52f, 2482.73f,  1316.21f, 7999  }, // Tyrande Whisperwind
    { CITY_EXODAR,      "Exodar",         530, -3864.92f, -11643.7f, -137.644f, 17949 }, // Prophet Velen
    { CITY_ORGRIMMAR,   "Orgrimmar",      1,    1633.75f, -4439.39f, 15.4396f, 4949  }, // Thrall
    { CITY_UNDERCITY,   "Undercity",      0,    1633.75f, 240.167f,  -43.1034f, 10181 }, // Lady Sylvanas Windrunner
    { CITY_THUNDERBLUFF, "Thunder Bluff", 1,   -1043.11f, 285.809f,  135.165f, 3057  }, // Cairne Bloodhoof
    { CITY_SILVERMOON,  "Silvermoon",     530,  9338.74f, -7277.27f, 13.7014f, 16283 }  // Lor'themar Theron
};

struct SiegeEvent
{
    CityId cityId;
    uint32 startTime;
    uint32 endTime;
    bool isActive;
    std::vector<ObjectGuid> spawnedCreatures;
    bool cinematicPhase;
    uint32 lastYellTime;
};

// Active siege events
static std::vector<SiegeEvent> g_ActiveSieges;
static uint32 g_NextSiegeTime = 0;

// -----------------------------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// Forward declarations
void DistributeRewards(const SiegeEvent& event, const CityData& city);

/**
 * @brief Loads the configuration for the City Siege module.
 */
void LoadCitySiegeConfiguration()
{
    g_CitySiegeEnabled = sConfigMgr->GetOption<bool>("CitySiege.Enabled", true);
    g_DebugMode = sConfigMgr->GetOption<bool>("CitySiege.DebugMode", false);

    // Timer settings (convert minutes to seconds)
    g_TimerMin = sConfigMgr->GetOption<uint32>("CitySiege.TimerMin", 120) * 60;
    g_TimerMax = sConfigMgr->GetOption<uint32>("CitySiege.TimerMax", 240) * 60;
    g_EventDuration = sConfigMgr->GetOption<uint32>("CitySiege.EventDuration", 30) * 60;

    // Event settings
    g_AllowMultipleCities = sConfigMgr->GetOption<bool>("CitySiege.AllowMultipleCities", false);
    g_AnnounceRadius = sConfigMgr->GetOption<uint32>("CitySiege.AnnounceRadius", 500);
    g_MinimumLevel = sConfigMgr->GetOption<uint32>("CitySiege.MinimumLevel", 1);

    // City enable/disable flags
    g_CityEnabled["Stormwind"] = sConfigMgr->GetOption<bool>("CitySiege.Stormwind.Enabled", true);
    g_CityEnabled["Ironforge"] = sConfigMgr->GetOption<bool>("CitySiege.Ironforge.Enabled", true);
    g_CityEnabled["Darnassus"] = sConfigMgr->GetOption<bool>("CitySiege.Darnassus.Enabled", true);
    g_CityEnabled["Exodar"] = sConfigMgr->GetOption<bool>("CitySiege.Exodar.Enabled", true);
    g_CityEnabled["Orgrimmar"] = sConfigMgr->GetOption<bool>("CitySiege.Orgrimmar.Enabled", true);
    g_CityEnabled["Undercity"] = sConfigMgr->GetOption<bool>("CitySiege.Undercity.Enabled", true);
    g_CityEnabled["ThunderBluff"] = sConfigMgr->GetOption<bool>("CitySiege.ThunderBluff.Enabled", true);
    g_CityEnabled["Silvermoon"] = sConfigMgr->GetOption<bool>("CitySiege.Silvermoon.Enabled", true);

    // Spawn counts
    g_SpawnCountMinions = sConfigMgr->GetOption<uint32>("CitySiege.SpawnCount.Minions", 15);
    g_SpawnCountElites = sConfigMgr->GetOption<uint32>("CitySiege.SpawnCount.Elites", 5);
    g_SpawnCountMiniBosses = sConfigMgr->GetOption<uint32>("CitySiege.SpawnCount.MiniBosses", 2);
    g_SpawnCountLeaders = sConfigMgr->GetOption<uint32>("CitySiege.SpawnCount.Leaders", 1);

    // Creature entries
    g_CreatureAllianceMinion = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Minion", 14425);
    g_CreatureAllianceElite = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Elite", 14428);
    g_CreatureAllianceMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.MiniBoss", 14762);
    g_CreatureAllianceLeader = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Leader", 16441);
    g_CreatureHordeMinion = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Minion", 14423);
    g_CreatureHordeElite = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Elite", 14426);
    g_CreatureHordeMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.MiniBoss", 14763);
    g_CreatureHordeLeader = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Leader", 16440);

    // Aggro settings
    g_AggroPlayers = sConfigMgr->GetOption<bool>("CitySiege.AggroPlayers", true);
    g_AggroNPCs = sConfigMgr->GetOption<bool>("CitySiege.AggroNPCs", true);

    // Cinematic settings
    g_CinematicDelay = sConfigMgr->GetOption<uint32>("CitySiege.CinematicDelay", 45);
    g_YellFrequency = sConfigMgr->GetOption<uint32>("CitySiege.YellFrequency", 30);

    // Reward settings
    g_RewardOnDefense = sConfigMgr->GetOption<bool>("CitySiege.RewardOnDefense", true);
    g_RewardHonor = sConfigMgr->GetOption<uint32>("CitySiege.RewardHonor", 100);
    g_RewardGold = sConfigMgr->GetOption<uint32>("CitySiege.RewardGold", 500000);

    if (g_DebugMode)
    {
        LOG_INFO("module", "[City Siege] Configuration loaded:");
        LOG_INFO("module", "[City Siege]   Enabled: {}", g_CitySiegeEnabled);
        LOG_INFO("module", "[City Siege]   Timer: {}-{} minutes", g_TimerMin / 60, g_TimerMax / 60);
        LOG_INFO("module", "[City Siege]   Event Duration: {} minutes", g_EventDuration / 60);
    }
}

/**
 * @brief Selects a random city for siege event.
 * @return Pointer to the selected CityData, or nullptr if no cities are available.
 */
CityData* SelectRandomCity()
{
    std::vector<CityData*> availableCities;

    for (auto& city : g_Cities)
    {
        if (g_CityEnabled[city.name])
        {
            // Check if city already has an active siege (if multiple sieges not allowed)
            if (!g_AllowMultipleCities)
            {
                bool alreadyUnderSiege = false;
                for (const auto& siege : g_ActiveSieges)
                {
                    if (siege.isActive && siege.cityId == city.id)
                    {
                        alreadyUnderSiege = true;
                        break;
                    }
                }
                if (!alreadyUnderSiege)
                {
                    availableCities.push_back(&city);
                }
            }
            else
            {
                availableCities.push_back(&city);
            }
        }
    }

    if (availableCities.empty())
    {
        return nullptr;
    }

    uint32 randomIndex = urand(0, availableCities.size() - 1);
    return availableCities[randomIndex];
}

/**
 * @brief Announces a siege event to players.
 * @param city The city being sieged.
 * @param isStart True if siege is starting, false if ending.
 */
void AnnounceSiege(const CityData& city, bool isStart)
{
    std::string message;
    if (isStart)
    {
        message = "|cffff0000[City Siege]|r The city of " + city.name + " is under attack! Defenders are needed!";
    }
    else
    {
        message = "|cff00ff00[City Siege]|r The siege of " + city.name + " has ended!";
    }

    if (g_AnnounceRadius == 0)
    {
        // Announce to the entire world
        sWorld->SendServerMessage(SERVER_MSG_STRING, message.c_str());
    }
    else
    {
        // Announce to players in range
        Map* map = sMapMgr->FindMap(city.mapId, 0);
        if (map)
        {
            Map::PlayerList const& players = map->GetPlayers();
            for (auto itr = players.begin(); itr != players.end(); ++itr)
            {
                if (Player* player = itr->GetSource())
                {
                    if (player->GetDistance(city.centerX, city.centerY, city.centerZ) <= g_AnnounceRadius)
                    {
                        ChatHandler(player->GetSession()).PSendSysMessage(message.c_str());
                    }
                }
            }
        }
    }

    if (g_DebugMode)
    {
        LOG_INFO("module", "[City Siege] {}", message);
    }
}

/**
 * @brief Spawns siege creatures for a city siege event.
 * @param event The siege event to spawn creatures for.
 */
void SpawnSiegeCreatures(SiegeEvent& event)
{
    const CityData& city = g_Cities[event.cityId];
    
    if (g_DebugMode)
    {
        LOG_INFO("module", "[City Siege] Spawning creatures for siege at {}", city.name);
        LOG_INFO("module", "[City Siege]   Minions: {}", g_SpawnCountMinions);
        LOG_INFO("module", "[City Siege]   Elites: {}", g_SpawnCountElites);
        LOG_INFO("module", "[City Siege]   Mini-Bosses: {}", g_SpawnCountMiniBosses);
        LOG_INFO("module", "[City Siege]   Leaders: {}", g_SpawnCountLeaders);
    }

    Map* map = sMapMgr->FindMap(city.mapId, 0);
    if (!map)
    {
        LOG_ERROR("module", "[City Siege] Failed to find map {} for {}", city.mapId, city.name);
        return;
    }

    // Define creature entries based on city faction
    bool isAllianceCity = (event.cityId <= CITY_EXODAR);
    
    // Use configured creature entries
    uint32 minionEntry = isAllianceCity ? g_CreatureAllianceMinion : g_CreatureHordeMinion;
    uint32 eliteEntry = isAllianceCity ? g_CreatureAllianceElite : g_CreatureHordeElite;
    uint32 miniBossEntry = isAllianceCity ? g_CreatureAllianceMiniBoss : g_CreatureHordeMiniBoss;
    uint32 leaderEntry = isAllianceCity ? g_CreatureAllianceLeader : g_CreatureHordeLeader;
    
    // Spawn creatures OUTSIDE the city (further away from center)
    float spawnRadius = 150.0f; // Spawn 150 yards outside city center
    float spawnAngleStep = 2 * M_PI / (g_SpawnCountMinions + g_SpawnCountElites + g_SpawnCountMiniBosses + g_SpawnCountLeaders);
    float currentAngle = 0.0f;

    // Spawn minions outside the city in a circle
    for (uint32 i = 0; i < g_SpawnCountMinions; ++i)
    {
        float x = city.centerX + spawnRadius * cos(currentAngle);
        float y = city.centerY + spawnRadius * sin(currentAngle);
        float z = city.centerZ;
        
        if (Creature* creature = map->SummonCreature(minionEntry, Position(x, y, z, 0)))
        {
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(isAllianceCity ? 84 : 83); // Horde/Alliance hostile
            event.spawnedCreatures.push_back(creature->GetGUID());
            
            if (g_DebugMode)
            {
                LOG_INFO("module", "[City Siege] Spawned minion at ({}, {}, {})", x, y, z);
            }
        }
        currentAngle += spawnAngleStep;
    }

    // Spawn elites outside the city (slightly closer)
    for (uint32 i = 0; i < g_SpawnCountElites; ++i)
    {
        float x = city.centerX + (spawnRadius * 0.9f) * cos(currentAngle);
        float y = city.centerY + (spawnRadius * 0.9f) * sin(currentAngle);
        float z = city.centerZ;
        
        if (Creature* creature = map->SummonCreature(eliteEntry, Position(x, y, z, 0)))
        {
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(isAllianceCity ? 84 : 83);
            event.spawnedCreatures.push_back(creature->GetGUID());
        }
        currentAngle += spawnAngleStep;
    }

    // Spawn mini-bosses outside the city (even closer)
    for (uint32 i = 0; i < g_SpawnCountMiniBosses; ++i)
    {
        float x = city.centerX + (spawnRadius * 0.8f) * cos(currentAngle);
        float y = city.centerY + (spawnRadius * 0.8f) * sin(currentAngle);
        float z = city.centerZ;
        
        if (Creature* creature = map->SummonCreature(miniBossEntry, Position(x, y, z, 0)))
        {
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(isAllianceCity ? 84 : 83);
            event.spawnedCreatures.push_back(creature->GetGUID());
        }
        currentAngle += spawnAngleStep;
    }

    // Spawn faction leaders outside the city (leading the charge)
    for (uint32 i = 0; i < g_SpawnCountLeaders; ++i)
    {
        float x = city.centerX + (spawnRadius * 0.7f) * cos(currentAngle);
        float y = city.centerY + (spawnRadius * 0.7f) * sin(currentAngle);
        float z = city.centerZ;
        
        if (Creature* creature = map->SummonCreature(leaderEntry, Position(x, y, z, 0)))
        {
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(isAllianceCity ? 84 : 83);
            event.spawnedCreatures.push_back(creature->GetGUID());
            
            // Make leader yell on spawn
            creature->Yell("This city will fall before our might!", LANG_UNIVERSAL);
        }
        currentAngle += spawnAngleStep;
    }

    LOG_INFO("module", "[City Siege] Spawned {} total creatures for siege at {}", 
             event.spawnedCreatures.size(), city.name);
}

/**
 * @brief Despawns all creatures from a siege event.
 * @param event The siege event to clean up.
 */
void DespawnSiegeCreatures(SiegeEvent& event)
{
    const CityData& city = g_Cities[event.cityId];
    Map* map = sMapMgr->FindMap(city.mapId, 0);
    
    if (map)
    {
        for (const auto& guid : event.spawnedCreatures)
        {
            if (Creature* creature = ObjectAccessor::GetCreature(*map, guid))
            {
                creature->DespawnOrUnsummon();
            }
        }
    }
    
    event.spawnedCreatures.clear();

    if (g_DebugMode)
    {
        LOG_INFO("module", "[City Siege] Despawned creatures for siege at {}", city.name);
    }
}

/**
 * @brief Starts a new siege event in a random city.
 */
void StartSiegeEvent()
{
    if (!g_CitySiegeEnabled)
    {
        return;
    }

    // Check if we can start a new siege
    if (!g_AllowMultipleCities && !g_ActiveSieges.empty())
    {
        // Check if any siege is still active
        for (const auto& siege : g_ActiveSieges)
        {
            if (siege.isActive)
            {
                return; // Cannot start new siege
            }
        }
    }

    CityData* city = SelectRandomCity();
    if (!city)
    {
        if (g_DebugMode)
        {
            LOG_INFO("module", "[City Siege] No available cities for siege event");
        }
        return;
    }

    uint32 currentTime = time(nullptr);
    SiegeEvent newEvent;
    newEvent.cityId = city->id;
    newEvent.startTime = currentTime;
    newEvent.endTime = currentTime + g_EventDuration;
    newEvent.isActive = true;
    newEvent.cinematicPhase = true;
    newEvent.lastYellTime = currentTime;

    g_ActiveSieges.push_back(newEvent);

    AnnounceSiege(*city, true);
    SpawnSiegeCreatures(g_ActiveSieges.back());

    if (g_DebugMode)
    {
        LOG_INFO("module", "[City Siege] Started siege event at {}", city->name);
    }
}

/**
 * @brief Ends an active siege event.
 * @param event The siege event to end.
 */
void EndSiegeEvent(SiegeEvent& event)
{
    if (!event.isActive)
    {
        return;
    }

    const CityData& city = g_Cities[event.cityId];
    event.isActive = false;

    // Check if defenders won (city leader still alive)
    bool defendersWon = false;
    Map* map = sMapMgr->FindMap(city.mapId, 0);
    if (map)
    {
        // Find city leader by entry
        Map::PlayerList const& players = map->GetPlayers();
        for (auto itr = players.begin(); itr != players.end(); ++itr)
        {
            if (Player* player = itr->GetSource())
            {
                if (Creature* cityLeader = player->FindNearestCreature(city.targetLeaderEntry, 1000.0f, true))
                {
                    defendersWon = true;
                    break;
                }
            }
        }
    }

    DespawnSiegeCreatures(event);
    AnnounceSiege(city, false);

    if (g_RewardOnDefense && defendersWon)
    {
        DistributeRewards(event, city);
    }

    if (g_DebugMode)
    {
        LOG_INFO("module", "[City Siege] Ended siege event at {} - Defenders {}", 
                 city.name, defendersWon ? "won" : "lost");
    }
}

/**
 * @brief Distributes rewards to players who defended the city.
 * @param event The siege event that ended.
 * @param city The city that was defended.
 */
void DistributeRewards(const SiegeEvent& event, const CityData& city)
{
    Map* map = sMapMgr->FindMap(city.mapId, 0);
    if (!map)
    {
        return;
    }

    uint32 rewardedPlayers = 0;
    Map::PlayerList const& players = map->GetPlayers();
    
    for (auto itr = players.begin(); itr != players.end(); ++itr)
    {
        if (Player* player = itr->GetSource())
        {
            // Check if player is in range and appropriate level
            if (player->GetDistance(city.centerX, city.centerY, city.centerZ) <= g_AnnounceRadius &&
                player->getLevel() >= g_MinimumLevel)
            {
                // Award honor
                if (g_RewardHonor > 0)
                {
                    player->RewardHonor(nullptr, 1, g_RewardHonor);
                }
                
                // Award gold
                if (g_RewardGold > 0)
                {
                    player->ModifyMoney(g_RewardGold);
                }
                
                // Send confirmation message
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cff00ff00[City Siege]|r You have been rewarded for defending {}!", 
                    city.name.c_str());
                
                rewardedPlayers++;
            }
        }
    }
    
    if (g_DebugMode)
    {
        LOG_INFO("module", "[City Siege] Rewarded {} players for defending {}", 
                 rewardedPlayers, city.name);
    }
}

/**
 * @brief Updates all active siege events.
 * @param diff Time since last update in milliseconds.
 */
void UpdateSiegeEvents(uint32 diff)
{
    uint32 currentTime = time(nullptr);

    // Update active sieges
    for (auto& event : g_ActiveSieges)
    {
        if (!event.isActive)
        {
            continue;
        }

        // Check if cinematic phase is over
        if (event.cinematicPhase && (currentTime - event.startTime) >= g_CinematicDelay)
        {
            event.cinematicPhase = false;
            if (g_DebugMode)
            {
                LOG_INFO("module", "[City Siege] Cinematic phase ended, combat begins");
            }
            
            // Make creatures aggressive after cinematic phase
            const CityData& city = g_Cities[event.cityId];
            Map* map = sMapMgr->FindMap(city.mapId, 0);
            if (map)
            {
                for (const auto& guid : event.spawnedCreatures)
                {
                    if (Creature* creature = ObjectAccessor::GetCreature(*map, guid))
                    {
                        creature->SetReactState(g_AggroPlayers ? REACT_AGGRESSIVE : REACT_DEFENSIVE);
                        
                        // Primary target: Find and move toward the city leader
                        if (Creature* cityLeader = creature->FindNearestCreature(city.targetLeaderEntry, 1000.0f, true))
                        {
                            // Start moving toward the city leader
                            creature->GetMotionMaster()->MovePoint(0, cityLeader->GetPositionX(), 
                                                                    cityLeader->GetPositionY(), 
                                                                    cityLeader->GetPositionZ());
                            creature->AI()->AttackStart(cityLeader);
                        }
                        
                        // Secondary: If aggressive to NPCs, attack guards on the way
                        if (g_AggroNPCs)
                        {
                            creature->SetReactState(REACT_AGGRESSIVE); // Will aggro any hostile NPCs encountered
                        }
                    }
                }
            }
        }

        // Handle periodic yells
        if ((currentTime - event.lastYellTime) >= g_YellFrequency)
        {
            event.lastYellTime = currentTime;
            
            const CityData& city = g_Cities[event.cityId];
            Map* map = sMapMgr->FindMap(city.mapId, 0);
            if (map)
            {
                // Make siege leaders yell
                for (const auto& guid : event.spawnedCreatures)
                {
                    if (Creature* creature = ObjectAccessor::GetCreature(*map, guid))
                    {
                        uint32 entry = creature->GetEntry();
                        // Only leaders and mini-bosses yell
                        if (entry == g_CreatureAllianceLeader || entry == g_CreatureHordeLeader ||
                            entry == g_CreatureAllianceMiniBoss || entry == g_CreatureHordeMiniBoss)
                        {
                            // Random yells
                            std::vector<std::string> yells = {
                                "Your defenses crumble!",
                                "This city will burn!",
                                "Face your doom!",
                                "None can stand against us!",
                                "Your leaders will fall!"
                            };
                            
                            uint32 randomIndex = urand(0, yells.size() - 1);
                            creature->Yell(yells[randomIndex].c_str(), LANG_UNIVERSAL);
                            break; // Only one creature yells per cycle
                        }
                    }
                }
            }
        }

        // Check if event should end
        if (currentTime >= event.endTime)
        {
            EndSiegeEvent(event);
        }
    }

    // Clean up ended events
    g_ActiveSieges.erase(
        std::remove_if(g_ActiveSieges.begin(), g_ActiveSieges.end(),
            [currentTime](const SiegeEvent& event) {
                return !event.isActive && (currentTime - event.endTime) > 60;
            }),
        g_ActiveSieges.end()
    );

    // Check if it's time to start a new siege
    if (currentTime >= g_NextSiegeTime)
    {
        StartSiegeEvent();
        // Schedule next siege
        uint32 nextDelay = urand(g_TimerMin, g_TimerMax);
        g_NextSiegeTime = currentTime + nextDelay;

        if (g_DebugMode)
        {
            LOG_INFO("module", "[City Siege] Next siege scheduled in {} minutes", nextDelay / 60);
        }
    }
}

// -----------------------------------------------------------------------------
// SCRIPT CLASSES
// -----------------------------------------------------------------------------

/**
 * @brief WorldScript that manages the City Siege system.
 */
class CitySiegeWorldScript : public WorldScript
{
public:
    CitySiegeWorldScript() : WorldScript("CitySiegeWorldScript") { }

    void OnStartup() override
    {
        LOG_INFO("server.loading", "[City Siege] Loading City Siege module...");
        LoadCitySiegeConfiguration();

        if (g_CitySiegeEnabled)
        {
            // Schedule first siege
            uint32 firstDelay = urand(g_TimerMin, g_TimerMax);
            g_NextSiegeTime = time(nullptr) + firstDelay;

            LOG_INFO("server.loading", "[City Siege] Module enabled. First siege in {} minutes", firstDelay / 60);
        }
        else
        {
            LOG_INFO("server.loading", "[City Siege] Module disabled");
        }
    }

    void OnUpdate(uint32 diff) override
    {
        if (!g_CitySiegeEnabled)
        {
            return;
        }

        UpdateSiegeEvents(diff);
    }

    void OnShutdown() override
    {
        // Clean up any active sieges
        for (auto& event : g_ActiveSieges)
        {
            if (event.isActive)
            {
                DespawnSiegeCreatures(event);
            }
        }
        g_ActiveSieges.clear();

        LOG_INFO("server.loading", "[City Siege] Module shutdown complete");
    }
};

// -----------------------------------------------------------------------------
// SCRIPT REGISTRATION
// -----------------------------------------------------------------------------

void Addmod_city_siegeScripts()
{
    new CitySiegeWorldScript();
}
