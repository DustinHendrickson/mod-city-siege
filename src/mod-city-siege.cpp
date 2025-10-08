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
#include "CommandScript.h"
#include "Log.h"
#include "Configuration/Config.h"
#include "Common.h"
#include "World.h"
#include "WorldSessionMgr.h"
#include "Map.h"
#include "MapMgr.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "MoveSplineInit.h"
#include "MotionMaster.h"
#include "Language.h"
#include "ScriptedCreature.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cmath>
#include <algorithm>

using namespace Acore::ChatCommands;

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

// Creature entries - Using Mount Hyjal battle units for thematic appropriateness
// Alliance attackers: Footman, Knights, Riflemen, Priests
static uint32 g_CreatureAllianceMinion = 17919;   // Alliance Footman
static uint32 g_CreatureAllianceElite = 17920;    // Alliance Knight  
static uint32 g_CreatureAllianceMiniBoss = 17921; // Alliance Rifleman
static uint32 g_CreatureAllianceLeader = 17928;   // Alliance Priest (commander)
// Horde attackers: Grunts, Tauren Warriors, Headhunters, Shamans
static uint32 g_CreatureHordeMinion = 17932;      // Horde Grunt
static uint32 g_CreatureHordeElite = 17933;       // Tauren Warrior
static uint32 g_CreatureHordeMiniBoss = 17934;    // Horde Headhunter
static uint32 g_CreatureHordeLeader = 17936;      // Horde Shaman (commander)

// Aggro settings
static bool g_AggroPlayers = true;
static bool g_AggroNPCs = true;

// Cinematic settings
static uint32 g_CinematicDelay = 45; // seconds
static uint32 g_YellFrequency = 30;  // seconds

// Reward settings
static bool g_RewardOnDefense = true;
static uint32 g_RewardHonor = 100;
static uint32 g_RewardGoldBase = 5000; // 50 silver in copper at level 1
static uint32 g_RewardGoldPerLevel = 5000; // 0.5 gold per level in copper

// Announcement messages
static std::string g_MessageSiegeStart = "|cffff0000[City Siege]|r The city of {CITYNAME} is under attack! Defenders are needed!";
static std::string g_MessageSiegeEnd = "|cff00ff00[City Siege]|r The siege of {CITYNAME} has ended!";
static std::string g_MessageReward = "|cff00ff00[City Siege]|r You have been rewarded for defending {CITYNAME}!";

// Leader spawn yell
static std::string g_YellLeaderSpawn = "This city will fall before our might!";

// Combat yells (semicolon separated)
static std::string g_YellsCombat = "Your defenses crumble!;This city will burn!;Face your doom!;None can stand against us!;Your leaders will fall!";

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

struct Waypoint
{
    float x;
    float y;
    float z;
};

struct CityData
{
    CityId id;
    std::string name;
    uint32 mapId;
    float centerX;      // City center for announcement radius
    float centerY;
    float centerZ;
    float spawnX;       // Configurable spawn location
    float spawnY;
    float spawnZ;
    float leaderX;      // Configurable leader location
    float leaderY;
    float leaderZ;
    uint32 targetLeaderEntry; // Entry ID of the city leader to attack
    std::vector<Waypoint> waypoints; // Waypoints for creatures to follow to reach the leader
};

// City definitions with approximate center coordinates
static std::vector<CityData> g_Cities = {
    { CITY_STORMWIND,   "Stormwind",      0,   -8913.23f, 554.633f,  93.7944f,  -9161.16f, 353.365f,  88.117f,   -8442.578f, 334.6064f, 122.476685f,  1748  },
    { CITY_IRONFORGE,   "Ironforge",      0,   -4981.25f, -881.542f, 501.660f,  -5174.09f, -594.361f, 397.853f,  -4981.25f, -881.542f, 501.660f,  2784  },
    { CITY_DARNASSUS,   "Darnassus",      1,    9947.52f, 2482.73f,  1316.21f,   9887.36f, 1856.49f,  1317.14f,   9947.52f, 2482.73f,  1316.21f,  7999  },
    { CITY_EXODAR,      "Exodar",         530, -3864.92f, -11643.7f, -137.644f, -4080.80f, -12193.2f, 1.712f,    -3864.92f, -11643.7f, -137.644f, 17949 },
    { CITY_ORGRIMMAR,   "Orgrimmar",      1,    1633.75f, -4439.39f, 15.4396f,   1114.96f, -4374.63f, 25.813f,    1633.75f, -4439.39f, 15.4396f,  4949  },
    { CITY_UNDERCITY,   "Undercity",      0,    1633.75f, 240.167f,  -43.1034f,  1982.26f, 226.674f,  35.951f,    1633.75f, 240.167f,  -43.1034f, 10181 },
    { CITY_THUNDERBLUFF, "Thunder Bluff", 1,   -1043.11f, 285.809f,  135.165f,  -1558.61f, -5.071f,   5.384f,    -1043.11f, 285.809f,  135.165f,  3057  },
    { CITY_SILVERMOON,  "Silvermoon",     530,  9338.74f, -7277.27f, 13.7014f,   9230.47f, -6962.67f, 5.004f,     9338.74f, -7277.27f, 13.7014f,  16283 }
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
    std::unordered_map<ObjectGuid, uint32> creatureWaypointProgress; // Tracks which waypoint each creature is on
};

// Active siege events
static std::vector<SiegeEvent> g_ActiveSieges;
static uint32 g_NextSiegeTime = 0;

// -----------------------------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// Forward declarations
void DistributeRewards(const SiegeEvent& event, const CityData& city, int winningTeam = -1);

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

    // Creature entries - Mount Hyjal battle units
    g_CreatureAllianceMinion = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Minion", 17919);   // Alliance Footman
    g_CreatureAllianceElite = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Elite", 17920);     // Alliance Knight
    g_CreatureAllianceMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.MiniBoss", 17921); // Alliance Rifleman
    g_CreatureAllianceLeader = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Leader", 17928);   // Alliance Priest
    g_CreatureHordeMinion = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Minion", 17932);         // Horde Grunt
    g_CreatureHordeElite = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Elite", 17933);           // Tauren Warrior
    g_CreatureHordeMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.MiniBoss", 17934);     // Horde Headhunter
    g_CreatureHordeLeader = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Leader", 17936);         // Horde Shaman

    // Aggro settings
    g_AggroPlayers = sConfigMgr->GetOption<bool>("CitySiege.AggroPlayers", true);
    g_AggroNPCs = sConfigMgr->GetOption<bool>("CitySiege.AggroNPCs", true);

    // Cinematic settings
    g_CinematicDelay = sConfigMgr->GetOption<uint32>("CitySiege.CinematicDelay", 45);
    g_YellFrequency = sConfigMgr->GetOption<uint32>("CitySiege.YellFrequency", 30);

    // Reward settings
    g_RewardOnDefense = sConfigMgr->GetOption<bool>("CitySiege.RewardOnDefense", true);
    g_RewardHonor = sConfigMgr->GetOption<uint32>("CitySiege.RewardHonor", 100);
    g_RewardGoldBase = sConfigMgr->GetOption<uint32>("CitySiege.RewardGoldBase", 5000);
    g_RewardGoldPerLevel = sConfigMgr->GetOption<uint32>("CitySiege.RewardGoldPerLevel", 5000);

    // Messages
    g_MessageSiegeStart = sConfigMgr->GetOption<std::string>("CitySiege.Message.SiegeStart", 
        "|cffff0000[City Siege]|r The city of {CITYNAME} is under attack! Defenders are needed!");
    g_MessageSiegeEnd = sConfigMgr->GetOption<std::string>("CitySiege.Message.SiegeEnd", 
        "|cff00ff00[City Siege]|r The siege of {CITYNAME} has ended!");
    g_MessageReward = sConfigMgr->GetOption<std::string>("CitySiege.Message.Reward", 
        "|cff00ff00[City Siege]|r You have been rewarded for defending {CITYNAME}!");
    
    // Yells
    g_YellLeaderSpawn = sConfigMgr->GetOption<std::string>("CitySiege.Yell.LeaderSpawn", 
        "This city will fall before our might!");
    g_YellsCombat = sConfigMgr->GetOption<std::string>("CitySiege.Yell.Combat", 
        "Your defenses crumble!;This city will burn!;Face your doom!;None can stand against us!;Your leaders will fall!");

    // Load spawn locations for each city
    g_Cities[CITY_STORMWIND].spawnX = sConfigMgr->GetOption<float>("CitySiege.Stormwind.SpawnX", -9161.16f);
    g_Cities[CITY_STORMWIND].spawnY = sConfigMgr->GetOption<float>("CitySiege.Stormwind.SpawnY", 353.365f);
    g_Cities[CITY_STORMWIND].spawnZ = sConfigMgr->GetOption<float>("CitySiege.Stormwind.SpawnZ", 88.117f);
    
    g_Cities[CITY_IRONFORGE].spawnX = sConfigMgr->GetOption<float>("CitySiege.Ironforge.SpawnX", -5174.09f);
    g_Cities[CITY_IRONFORGE].spawnY = sConfigMgr->GetOption<float>("CitySiege.Ironforge.SpawnY", -594.361f);
    g_Cities[CITY_IRONFORGE].spawnZ = sConfigMgr->GetOption<float>("CitySiege.Ironforge.SpawnZ", 397.853f);
    
    g_Cities[CITY_DARNASSUS].spawnX = sConfigMgr->GetOption<float>("CitySiege.Darnassus.SpawnX", 9887.36f);
    g_Cities[CITY_DARNASSUS].spawnY = sConfigMgr->GetOption<float>("CitySiege.Darnassus.SpawnY", 1856.49f);
    g_Cities[CITY_DARNASSUS].spawnZ = sConfigMgr->GetOption<float>("CitySiege.Darnassus.SpawnZ", 1317.14f);
    
    g_Cities[CITY_EXODAR].spawnX = sConfigMgr->GetOption<float>("CitySiege.Exodar.SpawnX", -4080.80f);
    g_Cities[CITY_EXODAR].spawnY = sConfigMgr->GetOption<float>("CitySiege.Exodar.SpawnY", -12193.2f);
    g_Cities[CITY_EXODAR].spawnZ = sConfigMgr->GetOption<float>("CitySiege.Exodar.SpawnZ", 1.712f);
    
    g_Cities[CITY_ORGRIMMAR].spawnX = sConfigMgr->GetOption<float>("CitySiege.Orgrimmar.SpawnX", 1114.96f);
    g_Cities[CITY_ORGRIMMAR].spawnY = sConfigMgr->GetOption<float>("CitySiege.Orgrimmar.SpawnY", -4374.63f);
    g_Cities[CITY_ORGRIMMAR].spawnZ = sConfigMgr->GetOption<float>("CitySiege.Orgrimmar.SpawnZ", 25.813f);
    
    g_Cities[CITY_UNDERCITY].spawnX = sConfigMgr->GetOption<float>("CitySiege.Undercity.SpawnX", 1982.26f);
    g_Cities[CITY_UNDERCITY].spawnY = sConfigMgr->GetOption<float>("CitySiege.Undercity.SpawnY", 226.674f);
    g_Cities[CITY_UNDERCITY].spawnZ = sConfigMgr->GetOption<float>("CitySiege.Undercity.SpawnZ", 35.951f);
    
    g_Cities[CITY_THUNDERBLUFF].spawnX = sConfigMgr->GetOption<float>("CitySiege.ThunderBluff.SpawnX", -1558.61f);
    g_Cities[CITY_THUNDERBLUFF].spawnY = sConfigMgr->GetOption<float>("CitySiege.ThunderBluff.SpawnY", -5.071f);
    g_Cities[CITY_THUNDERBLUFF].spawnZ = sConfigMgr->GetOption<float>("CitySiege.ThunderBluff.SpawnZ", 5.384f);
    
    g_Cities[CITY_SILVERMOON].spawnX = sConfigMgr->GetOption<float>("CitySiege.Silvermoon.SpawnX", 9230.47f);
    g_Cities[CITY_SILVERMOON].spawnY = sConfigMgr->GetOption<float>("CitySiege.Silvermoon.SpawnY", -6962.67f);
    g_Cities[CITY_SILVERMOON].spawnZ = sConfigMgr->GetOption<float>("CitySiege.Silvermoon.SpawnZ", 5.004f);

    // Load leader locations for each city
    g_Cities[CITY_STORMWIND].leaderX = sConfigMgr->GetOption<float>("CitySiege.Stormwind.LeaderX", -8442.578f);
    g_Cities[CITY_STORMWIND].leaderY = sConfigMgr->GetOption<float>("CitySiege.Stormwind.LeaderY", 334.6064f);
    g_Cities[CITY_STORMWIND].leaderZ = sConfigMgr->GetOption<float>("CitySiege.Stormwind.LeaderZ", 122.476685f);
    
    g_Cities[CITY_IRONFORGE].leaderX = sConfigMgr->GetOption<float>("CitySiege.Ironforge.LeaderX", -4981.25f);
    g_Cities[CITY_IRONFORGE].leaderY = sConfigMgr->GetOption<float>("CitySiege.Ironforge.LeaderY", -881.542f);
    g_Cities[CITY_IRONFORGE].leaderZ = sConfigMgr->GetOption<float>("CitySiege.Ironforge.LeaderZ", 501.660f);
    
    g_Cities[CITY_DARNASSUS].leaderX = sConfigMgr->GetOption<float>("CitySiege.Darnassus.LeaderX", 9947.52f);
    g_Cities[CITY_DARNASSUS].leaderY = sConfigMgr->GetOption<float>("CitySiege.Darnassus.LeaderY", 2482.73f);
    g_Cities[CITY_DARNASSUS].leaderZ = sConfigMgr->GetOption<float>("CitySiege.Darnassus.LeaderZ", 1316.21f);
    
    g_Cities[CITY_EXODAR].leaderX = sConfigMgr->GetOption<float>("CitySiege.Exodar.LeaderX", -3864.92f);
    g_Cities[CITY_EXODAR].leaderY = sConfigMgr->GetOption<float>("CitySiege.Exodar.LeaderY", -11643.7f);
    g_Cities[CITY_EXODAR].leaderZ = sConfigMgr->GetOption<float>("CitySiege.Exodar.LeaderZ", -137.644f);
    
    g_Cities[CITY_ORGRIMMAR].leaderX = sConfigMgr->GetOption<float>("CitySiege.Orgrimmar.LeaderX", 1633.75f);
    g_Cities[CITY_ORGRIMMAR].leaderY = sConfigMgr->GetOption<float>("CitySiege.Orgrimmar.LeaderY", -4439.39f);
    g_Cities[CITY_ORGRIMMAR].leaderZ = sConfigMgr->GetOption<float>("CitySiege.Orgrimmar.LeaderZ", 15.4396f);
    
    g_Cities[CITY_UNDERCITY].leaderX = sConfigMgr->GetOption<float>("CitySiege.Undercity.LeaderX", 1633.75f);
    g_Cities[CITY_UNDERCITY].leaderY = sConfigMgr->GetOption<float>("CitySiege.Undercity.LeaderY", 240.167f);
    g_Cities[CITY_UNDERCITY].leaderZ = sConfigMgr->GetOption<float>("CitySiege.Undercity.LeaderZ", -43.1034f);
    
    g_Cities[CITY_THUNDERBLUFF].leaderX = sConfigMgr->GetOption<float>("CitySiege.ThunderBluff.LeaderX", -1043.11f);
    g_Cities[CITY_THUNDERBLUFF].leaderY = sConfigMgr->GetOption<float>("CitySiege.ThunderBluff.LeaderY", 285.809f);
    g_Cities[CITY_THUNDERBLUFF].leaderZ = sConfigMgr->GetOption<float>("CitySiege.ThunderBluff.LeaderZ", 135.165f);
    
    g_Cities[CITY_SILVERMOON].leaderX = sConfigMgr->GetOption<float>("CitySiege.Silvermoon.LeaderX", 9338.74f);
    g_Cities[CITY_SILVERMOON].leaderY = sConfigMgr->GetOption<float>("CitySiege.Silvermoon.LeaderY", -7277.27f);
    g_Cities[CITY_SILVERMOON].leaderZ = sConfigMgr->GetOption<float>("CitySiege.Silvermoon.LeaderZ", 13.7014f);

    // Load waypoints for each city
    for (auto& city : g_Cities)
    {
        city.waypoints.clear();
        
        // Get waypoint count for this city
        std::string waypointCountKey = "CitySiege." + city.name + ".WaypointCount";
        uint32 waypointCount = sConfigMgr->GetOption<uint32>(waypointCountKey, 0);
        
        if (g_DebugMode)
        {
            LOG_INFO("server.loading", "[City Siege] Loading {} waypoints for {}", waypointCount, city.name);
        }
        
        // Load each waypoint
        for (uint32 i = 0; i < waypointCount; ++i)
        {
            std::string baseKey = "CitySiege." + city.name + ".Waypoint" + std::to_string(i + 1);
            Waypoint wp;
            wp.x = sConfigMgr->GetOption<float>(baseKey + ".X", 0.0f);
            wp.y = sConfigMgr->GetOption<float>(baseKey + ".Y", 0.0f);
            wp.z = sConfigMgr->GetOption<float>(baseKey + ".Z", 0.0f);
            
            // Only add waypoint if coordinates are valid
            if (wp.x != 0.0f || wp.y != 0.0f || wp.z != 0.0f)
            {
                city.waypoints.push_back(wp);
                
                if (g_DebugMode)
                {
                    LOG_INFO("server.loading", "[City Siege]   Waypoint {}: ({}, {}, {})", 
                             i + 1, wp.x, wp.y, wp.z);
                }
            }
        }
    }

    if (g_DebugMode)
    {
        LOG_INFO("server.loading", "[City Siege] Configuration loaded:");
        LOG_INFO("server.loading", "[City Siege]   Enabled: {}", g_CitySiegeEnabled);
        LOG_INFO("server.loading", "[City Siege]   Timer: {}-{} minutes", g_TimerMin / 60, g_TimerMax / 60);
        LOG_INFO("server.loading", "[City Siege]   Event Duration: {} minutes", g_EventDuration / 60);
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
        message = g_MessageSiegeStart;
        size_t pos = message.find("{CITYNAME}");
        if (pos != std::string::npos)
        {
            message.replace(pos, 10, city.name);
        }
    }
    else
    {
        message = g_MessageSiegeEnd;
        size_t pos = message.find("{CITYNAME}");
        if (pos != std::string::npos)
        {
            message.replace(pos, 10, city.name);
        }
    }

    if (g_AnnounceRadius == 0)
    {
        // Announce to the entire world
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message);
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
        LOG_INFO("server.loading", "[City Siege] {}", message);
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
        LOG_INFO("server.loading", "[City Siege] Spawning creatures for siege at {}", city.name);
        LOG_INFO("server.loading", "[City Siege]   Minions: {}", g_SpawnCountMinions);
        LOG_INFO("server.loading", "[City Siege]   Elites: {}", g_SpawnCountElites);
        LOG_INFO("server.loading", "[City Siege]   Mini-Bosses: {}", g_SpawnCountMiniBosses);
        LOG_INFO("server.loading", "[City Siege]   Leaders: {}", g_SpawnCountLeaders);
    }

    Map* map = sMapMgr->FindMap(city.mapId, 0);
    if (!map)
    {
        LOG_ERROR("server.loading", "[City Siege] Failed to find map {} for {}", city.mapId, city.name);
        return;
    }

    // Define creature entries based on city faction
    // If it's an Alliance city, spawn Horde attackers (and vice versa)
    bool isAllianceCity = (event.cityId <= CITY_EXODAR);
    
    // Use configured creature entries - spawn OPPOSITE faction as attackers
    uint32 minionEntry = isAllianceCity ? g_CreatureHordeMinion : g_CreatureAllianceMinion;
    uint32 eliteEntry = isAllianceCity ? g_CreatureHordeElite : g_CreatureAllianceElite;
    uint32 miniBossEntry = isAllianceCity ? g_CreatureHordeMiniBoss : g_CreatureAllianceMiniBoss;
    uint32 leaderEntry = isAllianceCity ? g_CreatureHordeLeader : g_CreatureAllianceLeader;
    
    // Military formation setup - organized ranks like a real army assault
    // Leaders at center, mini-bosses forming command circle, elites in mid-rank, minions in outer perimeter
    float baseRadius = 35.0f;
    
    // === RANK 1: LEADERS (Center/Command Post) ===
    // Leaders spawn at the very center in a tight formation
    float leaderRadius = 3.0f;
    float leaderAngleStep = (2 * M_PI) / std::max(1u, g_SpawnCountLeaders);
    for (uint32 i = 0; i < g_SpawnCountLeaders; ++i)
    {
        float angle = leaderAngleStep * i;
        float x = city.spawnX + leaderRadius * cos(angle);
        float y = city.spawnY + leaderRadius * sin(angle);
        float z = city.spawnZ;
        
        // Get proper ground height
        float groundZ = map->GetHeight(x, y, z + 50.0f, true, 50.0f);
        if (groundZ > INVALID_HEIGHT)
            z = groundZ + 0.5f;
        
        if (Creature* creature = map->SummonCreature(leaderEntry, Position(x, y, z, 0)))
        {
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(35);
            event.spawnedCreatures.push_back(creature->GetGUID());
            creature->Yell(g_YellLeaderSpawn.c_str(), LANG_UNIVERSAL);
        }
    }

    // === RANK 2: MINI-BOSSES (Command Circle) ===
    // Form a protective circle around the leaders
    float miniBossRadius = baseRadius * 0.3f; // ~10.5 yards
    float miniBossAngleStep = (2 * M_PI) / std::max(1u, g_SpawnCountMiniBosses);
    for (uint32 i = 0; i < g_SpawnCountMiniBosses; ++i)
    {
        float angle = miniBossAngleStep * i;
        float x = city.spawnX + miniBossRadius * cos(angle);
        float y = city.spawnY + miniBossRadius * sin(angle);
        float z = city.spawnZ;
        
        float groundZ = map->GetHeight(x, y, z + 50.0f, true, 50.0f);
        if (groundZ > INVALID_HEIGHT)
            z = groundZ + 0.5f;
        
        if (Creature* creature = map->SummonCreature(miniBossEntry, Position(x, y, z, 0)))
        {
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(35);
            event.spawnedCreatures.push_back(creature->GetGUID());
        }
    }

    // === RANK 3: ELITES (Mid-Rank Officers) ===
    // Form the middle rank in an organized formation
    float eliteRadius = baseRadius * 0.6f; // ~21 yards
    float eliteAngleStep = (2 * M_PI) / std::max(1u, g_SpawnCountElites);
    for (uint32 i = 0; i < g_SpawnCountElites; ++i)
    {
        float angle = eliteAngleStep * i;
        float x = city.spawnX + eliteRadius * cos(angle);
        float y = city.spawnY + eliteRadius * sin(angle);
        float z = city.spawnZ;
        
        float groundZ = map->GetHeight(x, y, z + 50.0f, true, 50.0f);
        if (groundZ > INVALID_HEIGHT)
            z = groundZ + 0.5f;
        
        if (Creature* creature = map->SummonCreature(eliteEntry, Position(x, y, z, 0)))
        {
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(35);
            event.spawnedCreatures.push_back(creature->GetGUID());
        }
    }

    // === RANK 4: MINIONS (Front Line / Outer Perimeter) ===
    // Form the outer perimeter - the main fighting force
    float minionRadius = baseRadius; // Full 35 yards
    float minionAngleStep = (2 * M_PI) / std::max(1u, g_SpawnCountMinions);
    for (uint32 i = 0; i < g_SpawnCountMinions; ++i)
    {
        float angle = minionAngleStep * i;
        float x = city.spawnX + minionRadius * cos(angle);
        float y = city.spawnY + minionRadius * sin(angle);
        float z = city.spawnZ;
        
        float groundZ = map->GetHeight(x, y, z + 50.0f, true, 50.0f);
        if (groundZ > INVALID_HEIGHT)
            z = groundZ + 0.5f;
        
        if (Creature* creature = map->SummonCreature(minionEntry, Position(x, y, z, 0)))
        {
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(35);
            event.spawnedCreatures.push_back(creature->GetGUID());
            
            if (g_DebugMode)
            {
                LOG_INFO("server.loading", "[City Siege] Spawned minion at ({}, {}, {})", x, y, z);
            }
        }
    }

    LOG_INFO("server.loading", "[City Siege] Spawned {} total creatures in military formation for siege at {}", 
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
            if (Creature* creature = map->GetCreature(guid))
            {
                creature->DespawnOrUnsummon();
            }
        }
    }
    
    event.spawnedCreatures.clear();

    if (g_DebugMode)
    {
        LOG_INFO("server.loading", "[City Siege] Despawned creatures for siege at {}", city.name);
    }
}

/**
 * @brief Starts a new siege event in a random city.
 */
/**
 * @brief Starts a new siege event.
 * @param targetCityId Optional specific city to siege. If -1, selects random city.
 */
void StartSiegeEvent(int targetCityId = -1)
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

    CityData* city = nullptr;
    
    // If specific city requested, use it
    if (targetCityId >= 0 && targetCityId < CITY_MAX)
    {
        city = &g_Cities[targetCityId];
        
        // Check if city is enabled
        if (!g_CityEnabled[city->name])
        {
            if (g_DebugMode)
            {
                LOG_INFO("server.loading", "[City Siege] Cannot start siege - {} is disabled", city->name);
            }
            return;
        }
    }
    else
    {
        // Select random city
        city = SelectRandomCity();
    }
    
    if (!city)
    {
        if (g_DebugMode)
        {
            LOG_INFO("server.loading", "[City Siege] No available cities for siege event");
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
        LOG_INFO("server.loading", "[City Siege] Started siege event at {}", city->name);
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

    // Determine which faction owns the city
    bool isAllianceCity = (event.cityId == CITY_STORMWIND || event.cityId == CITY_IRONFORGE || 
                          event.cityId == CITY_DARNASSUS || event.cityId == CITY_EXODAR);

    if (g_RewardOnDefense)
    {
        if (defendersWon)
        {
            // Defenders won - reward defending faction (0 = Alliance, 1 = Horde)
            int winningTeam = isAllianceCity ? 0 : 1;
            DistributeRewards(event, city, winningTeam);
        }
        else
        {
            // Attackers won (city leader killed) - reward attacking faction
            int winningTeam = isAllianceCity ? 1 : 0; // Opposite faction
            DistributeRewards(event, city, winningTeam);
        }
    }

    if (g_DebugMode)
    {
        LOG_INFO("server.loading", "[City Siege] Ended siege event at {} - {} won", 
                 city.name, defendersWon ? "Defenders" : "Attackers");
    }
}

/**
 * @brief Distributes rewards to players who defended the city.
 * @param event The siege event that ended.
 * @param city The city that was defended.
 * @param winningTeam The team ID to reward (0=Alliance, 1=Horde, -1=all players)
 */
void DistributeRewards(const SiegeEvent& /*event*/, const CityData& city, int winningTeam)
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
            // If winningTeam is specified, only reward players of that faction
            if (winningTeam != -1 && player->GetTeamId() != winningTeam)
            {
                continue;
            }
            
            // Check if player is in range and appropriate level
            if (player->GetDistance(city.centerX, city.centerY, city.centerZ) <= g_AnnounceRadius &&
                player->GetLevel() >= g_MinimumLevel)
            {
                // Award honor
                if (g_RewardHonor > 0)
                {
                    player->RewardHonor(nullptr, 1, g_RewardHonor);
                }
                
                // Award gold scaled by player level
                if (g_RewardGoldBase > 0 || g_RewardGoldPerLevel > 0)
                {
                    uint32 goldAmount = g_RewardGoldBase + (g_RewardGoldPerLevel * player->GetLevel());
                    player->ModifyMoney(goldAmount);
                }
                
                // Send confirmation message
                std::string rewardMsg = g_MessageReward;
                size_t pos = rewardMsg.find("{CITYNAME}");
                if (pos != std::string::npos)
                {
                    rewardMsg.replace(pos, 10, city.name);
                }
                ChatHandler(player->GetSession()).PSendSysMessage(rewardMsg.c_str());
                
                rewardedPlayers++;
            }
        }
    }
    
    if (g_DebugMode)
    {
        LOG_INFO("server.loading", "[City Siege] Rewarded {} players for the siege of {}", 
                 rewardedPlayers, city.name);
    }
}

/**
 * @brief Updates all active siege events.
 * @param diff Time since last update in milliseconds.
 */
void UpdateSiegeEvents(uint32 /*diff*/)
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
                LOG_INFO("server.loading", "[City Siege] Cinematic phase ended, combat begins");
            }
            
            // Determine the city faction
            const CityData& city = g_Cities[event.cityId];
            bool isAllianceCity = (event.cityId <= CITY_EXODAR);
            
            // Make creatures aggressive after cinematic phase
            Map* map = sMapMgr->FindMap(city.mapId, 0);
            if (map)
            {
                for (const auto& guid : event.spawnedCreatures)
                {
                    if (Creature* creature = map->GetCreature(guid))
                    {
                        // Set proper hostile faction: Horde attacks Alliance cities, Alliance attacks Horde cities
                        creature->SetFaction(isAllianceCity ? 83 : 84); // 83 = Horde, 84 = Alliance
                        
                        // Set react state based on configuration
                        if (g_AggroPlayers && g_AggroNPCs)
                        {
                            creature->SetReactState(REACT_AGGRESSIVE);
                        }
                        else if (g_AggroPlayers)
                        {
                            creature->SetReactState(REACT_DEFENSIVE);
                        }
                        else
                        {
                            creature->SetReactState(REACT_DEFENSIVE);
                        }
                        
                        // Ensure creature is grounded and cannot fly
                        creature->SetDisableGravity(false);
                        creature->SetCanFly(false);
                        creature->SetHover(false);
                        creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING);
                        
                        // Initialize waypoint progress for this creature
                        event.creatureWaypointProgress[guid] = 0;
                        
                        // Determine first destination
                        float destX, destY, destZ;
                        if (!city.waypoints.empty())
                        {
                            // Start with first waypoint
                            destX = city.waypoints[0].x;
                            destY = city.waypoints[0].y;
                            destZ = city.waypoints[0].z;
                        }
                        else
                        {
                            // No waypoints, go directly to leader
                            destX = city.leaderX;
                            destY = city.leaderY;
                            destZ = city.leaderZ;
                        }
                        
                        if (g_DebugMode)
                        {
                            LOG_INFO("server.loading", "[City Siege] Creature {} starting movement to waypoint/leader at ({}, {}, {})", 
                                     creature->GetGUID().ToString(), destX, destY, destZ);
                        }
                        
                        // Use MoveSplineInit for proper pathfinding
                        Movement::MoveSplineInit init(creature);
                        init.MoveTo(destX, destY, destZ, true, true);
                        init.SetWalk(false);
                        init.Launch();
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
                    if (Creature* creature = map->GetCreature(guid))
                    {
                        uint32 entry = creature->GetEntry();
                        // Only leaders and mini-bosses yell
                        if (entry == g_CreatureAllianceLeader || entry == g_CreatureHordeLeader ||
                            entry == g_CreatureAllianceMiniBoss || entry == g_CreatureHordeMiniBoss)
                        {
                            // Parse combat yells from configuration (semicolon separated)
                            std::vector<std::string> yells;
                            std::string yellStr = g_YellsCombat;
                            size_t pos = 0;
                            while ((pos = yellStr.find(';')) != std::string::npos)
                            {
                                std::string yell = yellStr.substr(0, pos);
                                if (!yell.empty())
                                {
                                    yells.push_back(yell);
                                }
                                yellStr.erase(0, pos + 1);
                            }
                            if (!yellStr.empty())
                            {
                                yells.push_back(yellStr);
                            }
                            
                            if (!yells.empty())
                            {
                                uint32 randomIndex = urand(0, yells.size() - 1);
                                creature->Yell(yells[randomIndex].c_str(), LANG_UNIVERSAL);
                            }
                            break; // Only one creature yells per cycle
                        }
                    }
                }
            }
        }

        // Handle waypoint progression - check if creatures have reached their current waypoint
        if (!event.cinematicPhase)
        {
            const CityData& city = g_Cities[event.cityId];
            Map* map = sMapMgr->FindMap(city.mapId, 0);
            if (map)
            {
                for (const auto& guid : event.spawnedCreatures)
                {
                    if (Creature* creature = map->GetCreature(guid))
                    {
                        // Skip if creature is in combat or dead
                        if (creature->IsInCombat() || !creature->IsAlive())
                            continue;
                        
                        // Check if creature has finished its current movement
                        if (creature->movespline->Finalized())
                        {
                            // Get current waypoint index
                            uint32 currentWP = event.creatureWaypointProgress[guid];
                            
                            // Determine next destination
                            float nextX, nextY, nextZ;
                            bool hasNextDestination = false;
                            
                            if (currentWP < city.waypoints.size())
                            {
                                // Move to next waypoint
                                nextX = city.waypoints[currentWP].x;
                                nextY = city.waypoints[currentWP].y;
                                nextZ = city.waypoints[currentWP].z;
                                event.creatureWaypointProgress[guid]++;
                                hasNextDestination = true;
                                
                                if (g_DebugMode)
                                {
                                    LOG_INFO("server.loading", "[City Siege] Creature {} reached waypoint {}, moving to waypoint {} at ({}, {}, {})",
                                             creature->GetGUID().ToString(), currentWP, currentWP + 1, nextX, nextY, nextZ);
                                }
                            }
                            else if (currentWP == city.waypoints.size())
                            {
                                // All waypoints complete, move to leader
                                nextX = city.leaderX;
                                nextY = city.leaderY;
                                nextZ = city.leaderZ;
                                event.creatureWaypointProgress[guid]++;
                                hasNextDestination = true;
                                
                                if (g_DebugMode)
                                {
                                    LOG_INFO("server.loading", "[City Siege] Creature {} completed all waypoints, moving to leader at ({}, {}, {})",
                                             creature->GetGUID().ToString(), nextX, nextY, nextZ);
                                }
                            }
                            
                            // Start movement to next destination
                            if (hasNextDestination)
                            {
                                Movement::MoveSplineInit init(creature);
                                init.MoveTo(nextX, nextY, nextZ, true, true);
                                init.SetWalk(false);
                                init.Launch();
                            }
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
            LOG_INFO("server.loading", "[City Siege] Next siege scheduled in {} minutes", nextDelay / 60);
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
// COMMAND SCRIPT
// -----------------------------------------------------------------------------

/**
 * @brief CommandScript for GM commands to manage City Siege events.
 */
class citysiege_commandscript : public CommandScript
{
public:
    citysiege_commandscript() : CommandScript("citysiege_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable citySiegeCommandTable =
        {
            { "start",   HandleCitySiegeStartCommand,   SEC_GAMEMASTER, Console::No },
            { "stop",    HandleCitySiegeStopCommand,    SEC_GAMEMASTER, Console::No },
            { "cleanup", HandleCitySiegeCleanupCommand, SEC_GAMEMASTER, Console::No },
            { "status",  HandleCitySiegeStatusCommand,  SEC_GAMEMASTER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "citysiege", citySiegeCommandTable }
        };

        return commandTable;
    }

    static bool HandleCitySiegeStartCommand(ChatHandler* handler, Optional<std::string> cityNameArg)
    {
        if (!g_CitySiegeEnabled)
        {
            handler->PSendSysMessage("City Siege module is disabled.");
            return true;
        }

        // Parse city name if provided
        int cityId = -1;
        if (cityNameArg)
        {
            std::string cityName = *cityNameArg;
            std::transform(cityName.begin(), cityName.end(), cityName.begin(), ::tolower);

            for (int i = 0; i < CITY_MAX; ++i)
            {
                std::string compareName = g_Cities[i].name;
                std::transform(compareName.begin(), compareName.end(), compareName.begin(), ::tolower);
                if (compareName == cityName)
                {
                    cityId = i;
                    break;
                }
            }

            if (cityId == -1)
            {
                handler->PSendSysMessage("Invalid city name. Valid cities: Stormwind, Ironforge, Darnassus, Exodar, Orgrimmar, Undercity, Thunderbluff, Silvermoon");
                return true;
            }

            // Check if city is enabled
            if (!g_CityEnabled[g_Cities[cityId].name])
            {
                handler->PSendSysMessage(("City '" + g_Cities[cityId].name + "' is disabled in configuration.").c_str());
                return true;
            }
        }

        // Check if already active
        if (cityId != -1)
        {
            for (const auto& event : g_ActiveSieges)
            {
                if (event.isActive && event.cityId == cityId)
                {
                    handler->PSendSysMessage(("City '" + g_Cities[cityId].name + "' is already under siege!").c_str());
                    return true;
                }
            }
        }

        // Start the siege
        if (cityId == -1)
        {
            StartSiegeEvent(); // Random city
            handler->PSendSysMessage("Started siege event in a random enabled city!");
        }
        else
        {
            StartSiegeEvent(cityId);
            handler->PSendSysMessage(("Started siege event in " + g_Cities[cityId].name + "!").c_str());
        }

        return true;
    }

    static bool HandleCitySiegeStopCommand(ChatHandler* handler, Optional<std::string> cityNameArg, Optional<std::string> factionArg)
    {
        if (g_ActiveSieges.empty())
        {
            handler->PSendSysMessage("No active siege events.");
            return true;
        }

        // Faction is required
        if (!factionArg)
        {
            handler->PSendSysMessage("Usage: .citysiege stop <cityname> <alliance|horde>");
            handler->PSendSysMessage("Specify which faction wins the siege.");
            return true;
        }

        // Parse faction
        std::string factionStr = *factionArg;
        std::transform(factionStr.begin(), factionStr.end(), factionStr.begin(), ::tolower);
        
        bool allianceWins = false;
        if (factionStr == "alliance")
        {
            allianceWins = true;
        }
        else if (factionStr == "horde")
        {
            allianceWins = false;
        }
        else
        {
            handler->PSendSysMessage("Invalid faction. Use 'alliance' or 'horde'.");
            return true;
        }

        // Parse city name
        int cityId = -1;
        if (cityNameArg)
        {
            std::string cityName = *cityNameArg;
            std::transform(cityName.begin(), cityName.end(), cityName.begin(), ::tolower);

            for (int i = 0; i < CITY_MAX; ++i)
            {
                std::string compareName = g_Cities[i].name;
                std::transform(compareName.begin(), compareName.end(), compareName.begin(), ::tolower);
                if (compareName == cityName)
                {
                    cityId = i;
                    break;
                }
            }

            if (cityId == -1)
            {
                handler->PSendSysMessage("Invalid city name.");
                return true;
            }
        }
        else
        {
            handler->PSendSysMessage("Usage: .citysiege stop <cityname> <alliance|horde>");
            return true;
        }

        // Find and stop the siege with winner determination
        bool found = false;
        for (auto& event : g_ActiveSieges)
        {
            if (event.isActive && event.cityId == cityId)
            {
                found = true;
                
                // Determine winning team (0 = Alliance, 1 = Horde)
                int winningTeam = allianceWins ? 0 : 1;
                
                // Distribute rewards to winning faction's players
                DistributeRewards(event, g_Cities[cityId], winningTeam);
                
                std::string winningFaction = allianceWins ? "Alliance" : "Horde";
                handler->PSendSysMessage(("Siege stopped. " + winningFaction + " wins! " + winningFaction + " players have been rewarded.").c_str());
                
                // Clean up
                DespawnSiegeCreatures(event);
                event.isActive = false;
                
                break;
            }
        }

        if (!found)
        {
            handler->PSendSysMessage(("No active siege in " + g_Cities[cityId].name).c_str());
        }
        else
        {
            // Remove inactive events
            g_ActiveSieges.erase(
                std::remove_if(g_ActiveSieges.begin(), g_ActiveSieges.end(),
                    [](const SiegeEvent& event) { return !event.isActive; }),
                g_ActiveSieges.end());
        }

        return true;
    }

    static bool HandleCitySiegeCleanupCommand(ChatHandler* handler, Optional<std::string> cityNameArg)
    {
        int cityId = -1;
        if (cityNameArg)
        {
            std::string cityName = *cityNameArg;
            std::transform(cityName.begin(), cityName.end(), cityName.begin(), ::tolower);

            for (int i = 0; i < CITY_MAX; ++i)
            {
                std::string compareName = g_Cities[i].name;
                std::transform(compareName.begin(), compareName.end(), compareName.begin(), ::tolower);
                if (compareName == cityName)
                {
                    cityId = i;
                    break;
                }
            }

            if (cityId == -1)
            {
                handler->PSendSysMessage("Invalid city name.");
                return true;
            }
        }

        // Cleanup sieges
        int cleanedCount = 0;
        for (auto& event : g_ActiveSieges)
        {
            if (cityId == -1 || event.cityId == cityId)
            {
                DespawnSiegeCreatures(event);
                event.isActive = false;
                handler->PSendSysMessage(("Cleaned up siege creatures in " + g_Cities[event.cityId].name).c_str());
                cleanedCount++;

                if (cityId != -1)
                    break;
            }
        }

        if (cleanedCount == 0)
        {
            handler->PSendSysMessage("No siege events to cleanup.");
        }
        else
        {
            // Remove inactive events
            g_ActiveSieges.erase(
                std::remove_if(g_ActiveSieges.begin(), g_ActiveSieges.end(),
                    [](const SiegeEvent& event) { return !event.isActive; }),
                g_ActiveSieges.end());
        }

        return true;
    }

    static bool HandleCitySiegeStatusCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage("=== City Siege Status ===");
        handler->PSendSysMessage(("Module Enabled: " + std::string(g_CitySiegeEnabled ? "Yes" : "No")).c_str());
        handler->PSendSysMessage(("Active Sieges: " + std::to_string(g_ActiveSieges.size())).c_str());

        if (!g_ActiveSieges.empty())
        {
            handler->PSendSysMessage("--- Active Siege Events ---");
            for (const auto& event : g_ActiveSieges)
            {
                if (event.isActive)
                {
                    uint32 currentTime = time(nullptr);
                    uint32 remaining = event.endTime > currentTime ? (event.endTime - currentTime) : 0;
                    handler->PSendSysMessage(("  " + g_Cities[event.cityId].name + " - " + 
                        std::to_string(event.spawnedCreatures.size()) + " creatures, " + 
                        std::to_string(remaining / 60) + " minutes remaining").c_str());
                }
            }
        }

        if (g_CitySiegeEnabled)
        {
            uint32 currentTime = time(nullptr);
            if (g_NextSiegeTime > currentTime)
            {
                uint32 timeUntilNext = g_NextSiegeTime - currentTime;
                handler->PSendSysMessage(("Next auto-siege in: " + std::to_string(timeUntilNext / 60) + " minutes").c_str());
            }
        }

        return true;
    }
};

// -----------------------------------------------------------------------------
// SCRIPT REGISTRATION
// -----------------------------------------------------------------------------

void Addmod_city_siegeScripts()
{
    new CitySiegeWorldScript();
    new citysiege_commandscript();
}
