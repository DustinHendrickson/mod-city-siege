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
#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cmath>
#include <algorithm>

using namespace Acore::ChatCommands;

// Custom check and searcher for finding creatures by entry without needing WorldObject reference
namespace CitySiege
{
    class CreatureEntryCheck
    {
    public:
        CreatureEntryCheck(uint32 entry) : _entry(entry) {}
        
        bool operator()(Creature* creature) const
        {
            return creature && creature->GetEntry() == _entry;
        }
        
    private:
        uint32 _entry;
    };
    
    // Custom searcher that doesn't require WorldObject for phase checking
    template<typename Check>
    struct SimpleCreatureListSearcher : Acore::ContainerInserter<Creature*>
    {
        Check& _check;
        
        template<typename Container>
        SimpleCreatureListSearcher(Container& container, Check& check)
            : Acore::ContainerInserter<Creature*>(container), _check(check) {}
        
        void Visit(CreatureMapType& m)
        {
            for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                if (_check(itr->GetSource()))
                    this->Insert(itr->GetSource());
            }
        }
        
        template<class NOT_INTERESTED> void Visit(GridRefMgr<NOT_INTERESTED>&) {}
    };
}

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

// Defender settings
static bool g_DefendersEnabled = true;
static uint32 g_DefendersCount = 10;
static uint32 g_CreatureAllianceDefender = 17919;  // Alliance Footman
static uint32 g_CreatureHordeDefender = 17932;     // Horde Grunt

// Level settings for spawned units
static uint32 g_LevelLeader = 80;
static uint32 g_LevelMiniBoss = 80;
static uint32 g_LevelElite = 75;
static uint32 g_LevelMinion = 70;
static uint32 g_LevelDefender = 70;

// Scale settings for spawned units
static float g_ScaleLeader = 1.3f;      // 30% larger
static float g_ScaleMiniBoss = 1.15f;   // 15% larger

// Cinematic settings
static uint32 g_CinematicDelay = 45; // seconds
static uint32 g_YellFrequency = 30;  // seconds

// Respawn settings
static bool g_RespawnEnabled = true;
static uint32 g_RespawnTimeLeader = 300;    // 5 minutes in seconds
static uint32 g_RespawnTimeMiniBoss = 180;  // 3 minutes in seconds
static uint32 g_RespawnTimeElite = 120;     // 2 minutes in seconds
static uint32 g_RespawnTimeMinion = 60;     // 1 minute in seconds
static uint32 g_RespawnTimeDefender = 45;   // 45 seconds

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
    { CITY_STORMWIND,   "Stormwind",      0,   -8913.23f, 554.633f,  93.7944f,  -9161.16f, 353.365f,  88.117f,   -8442.578f, 334.6064f, 122.476685f,  1748,  {} },
    { CITY_IRONFORGE,   "Ironforge",      0,   -4981.25f, -881.542f, 501.660f,  -5174.09f, -594.361f, 397.853f,  -4981.25f, -881.542f, 501.660f,  2784,  {} },
    { CITY_DARNASSUS,   "Darnassus",      1,    9947.52f, 2482.73f,  1316.21f,   9887.36f, 1856.49f,  1317.14f,   9947.52f, 2482.73f,  1316.21f,  7999,  {} },
    { CITY_EXODAR,      "Exodar",         530, -3864.92f, -11643.7f, -137.644f, -4080.80f, -12193.2f, 1.712f,    -3864.92f, -11643.7f, -137.644f, 17949, {} },
    { CITY_ORGRIMMAR,   "Orgrimmar",      1,    1633.75f, -4439.39f, 15.4396f,   1114.96f, -4374.63f, 25.813f,    1633.75f, -4439.39f, 15.4396f,  4949,  {} },
    { CITY_UNDERCITY,   "Undercity",      0,    1633.75f, 240.167f,  -43.1034f,  1982.26f, 226.674f,  35.951f,    1633.75f, 240.167f,  -43.1034f, 10181, {} },
    { CITY_THUNDERBLUFF, "Thunder Bluff", 1,   -1043.11f, 285.809f,  135.165f,  -1558.61f, -5.071f,   5.384f,    -1043.11f, 285.809f,  135.165f,  3057,  {} },
    { CITY_SILVERMOON,  "Silvermoon",     530,  9338.74f, -7277.27f, 13.7014f,   9230.47f, -6962.67f, 5.004f,     9338.74f, -7277.27f, 13.7014f,  16283, {} }
};

struct SiegeEvent
{
    CityId cityId;
    uint32 startTime;
    uint32 endTime;
    bool isActive;
    std::vector<ObjectGuid> spawnedCreatures;
    std::vector<ObjectGuid> spawnedDefenders; // Defender creatures
    bool cinematicPhase;
    uint32 lastYellTime;
    uint32 lastStatusAnnouncement; // For 5-minute countdown announcements
    uint32 cinematicStartTime; // When RP phase started (for pre-battle countdown)
    bool countdown75Announced; // 75% time remaining announced
    bool countdown50Announced; // 50% time remaining announced
    bool countdown25Announced; // 25% time remaining announced
    std::unordered_map<ObjectGuid, uint32> creatureWaypointProgress; // Tracks which waypoint each creature is on (attackers and defenders)
    
    // Respawn tracking: stores creature GUID, entry, and death time
    struct RespawnData
    {
        ObjectGuid guid;
        uint32 entry;
        uint32 deathTime;
        bool isDefender; // Track if this is a defender for correct respawn
    };
    std::vector<RespawnData> deadCreatures; // Creatures waiting to respawn
};

// Active siege events
static std::vector<SiegeEvent> g_ActiveSieges;
static uint32 g_NextSiegeTime = 0;

// Waypoint visualization tracking
static std::unordered_map<uint32, std::vector<ObjectGuid>> g_WaypointVisualizations; // cityId -> vector of creature GUIDs

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

    // Defender settings
    g_DefendersEnabled = sConfigMgr->GetOption<bool>("CitySiege.Defenders.Enabled", true);
    g_DefendersCount = sConfigMgr->GetOption<uint32>("CitySiege.Defenders.Count", 10);
    g_CreatureAllianceDefender = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Defender", 17919);
    g_CreatureHordeDefender = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Defender", 17932);

    // Level settings
    g_LevelLeader = sConfigMgr->GetOption<uint32>("CitySiege.Level.Leader", 80);
    g_LevelMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Level.MiniBoss", 80);
    g_LevelElite = sConfigMgr->GetOption<uint32>("CitySiege.Level.Elite", 75);
    g_LevelMinion = sConfigMgr->GetOption<uint32>("CitySiege.Level.Minion", 70);
    g_LevelDefender = sConfigMgr->GetOption<uint32>("CitySiege.Level.Defender", 70);

    // Scale settings
    g_ScaleLeader = sConfigMgr->GetOption<float>("CitySiege.Scale.Leader", 1.3f);
    g_ScaleMiniBoss = sConfigMgr->GetOption<float>("CitySiege.Scale.MiniBoss", 1.15f);

    // Cinematic settings
    g_CinematicDelay = sConfigMgr->GetOption<uint32>("CitySiege.CinematicDelay", 45);
    g_YellFrequency = sConfigMgr->GetOption<uint32>("CitySiege.YellFrequency", 30);

    // Respawn settings
    g_RespawnEnabled = sConfigMgr->GetOption<bool>("CitySiege.Respawn.Enabled", true);
    g_RespawnTimeLeader = sConfigMgr->GetOption<uint32>("CitySiege.Respawn.LeaderTime", 300);
    g_RespawnTimeMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Respawn.MiniBossTime", 180);
    g_RespawnTimeElite = sConfigMgr->GetOption<uint32>("CitySiege.Respawn.EliteTime", 120);
    g_RespawnTimeMinion = sConfigMgr->GetOption<uint32>("CitySiege.Respawn.MinionTime", 60);
    g_RespawnTimeDefender = sConfigMgr->GetOption<uint32>("CitySiege.Defenders.RespawnTime", 45);

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
            creature->SetLevel(g_LevelLeader);
            creature->SetObjectScale(g_ScaleLeader);
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(35);
            
            // Prevent return to home position after combat
            creature->SetWalk(false);
            creature->GetMotionMaster()->Clear(false);
            creature->GetMotionMaster()->MoveIdle();
            
            // Set home position to spawn location to prevent evading back
            creature->SetHomePosition(x, y, z, 0.0f);
            
            // Enforce ground position immediately after spawn
            creature->UpdateGroundPositionZ(x, y, z);
            
            event.spawnedCreatures.push_back(creature->GetGUID());
            
            // Parse leader spawn yells from configuration (semicolon separated for random selection)
            std::vector<std::string> spawnYells;
            std::string yellStr = g_YellLeaderSpawn;
            size_t pos = 0;
            while ((pos = yellStr.find(';')) != std::string::npos)
            {
                std::string yell = yellStr.substr(0, pos);
                if (!yell.empty())
                {
                    spawnYells.push_back(yell);
                }
                yellStr.erase(0, pos + 1);
            }
            if (!yellStr.empty())
            {
                spawnYells.push_back(yellStr);
            }
            
            // Yell a random spawn message
            if (!spawnYells.empty())
            {
                uint32 randomIndex = urand(0, spawnYells.size() - 1);
                creature->Yell(spawnYells[randomIndex].c_str(), LANG_UNIVERSAL);
            }
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
            creature->SetLevel(g_LevelMiniBoss);
            creature->SetObjectScale(g_ScaleMiniBoss);
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(35);
            
            // Prevent return to home position after combat
            creature->SetWalk(false);
            creature->GetMotionMaster()->Clear(false);
            creature->GetMotionMaster()->MoveIdle();
            
            // Set home position to spawn location to prevent evading back
            creature->SetHomePosition(x, y, z, 0);
            
            // Enforce ground position immediately after spawn
            creature->UpdateGroundPositionZ(x, y, z);
            
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
            creature->SetLevel(g_LevelElite);
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(35);
            
            // Prevent return to home position after combat
            creature->SetWalk(false);
            creature->GetMotionMaster()->Clear(false);
            creature->GetMotionMaster()->MoveIdle();
            
            // Set home position to spawn location to prevent evading back
            creature->SetHomePosition(x, y, z, 0);
            
            // Enforce ground position immediately after spawn
            creature->UpdateGroundPositionZ(x, y, z);
            
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
            creature->SetLevel(g_LevelMinion);
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetFaction(35);
            
            // Prevent return to home position after combat
            creature->SetWalk(false);
            creature->GetMotionMaster()->Clear(false);
            creature->GetMotionMaster()->MoveIdle();
            
            // Set home position to spawn location to prevent evading back
            creature->SetHomePosition(x, y, z, 0);
            creature->GetMotionMaster()->Clear(false);
            creature->GetMotionMaster()->MoveIdle();
            
            // Enforce ground position immediately after spawn
            creature->UpdateGroundPositionZ(x, y, z);
            
            event.spawnedCreatures.push_back(creature->GetGUID());
            
            if (g_DebugMode)
            {
                LOG_INFO("server.loading", "[City Siege] Spawned minion at ({}, {}, {})", x, y, z);
            }
        }
    }

    LOG_INFO("server.loading", "[City Siege] Spawned {} total attacker creatures in military formation for siege at {}", 
             event.spawnedCreatures.size(), city.name);
    
    // === SPAWN DEFENDERS ===
    // Defenders spawn near the leader and march towards the attackers (reverse waypoint order)
    if (g_DefendersEnabled && g_DefendersCount > 0)
    {
        // Determine defender entry based on city faction (same faction as city)
        bool isAllianceCity = (event.cityId <= CITY_EXODAR);
        uint32 defenderEntry = isAllianceCity ? g_CreatureAllianceDefender : g_CreatureHordeDefender;
        
        // Spawn defenders in a formation near the leader position
        float defenderRadius = 15.0f; // Spawn in 15-yard radius around leader
        float defenderAngleStep = (2 * M_PI) / std::max(1u, g_DefendersCount);
        
        for (uint32 i = 0; i < g_DefendersCount; ++i)
        {
            float angle = defenderAngleStep * i;
            float x = city.leaderX + defenderRadius * cos(angle);
            float y = city.leaderY + defenderRadius * sin(angle);
            float z = city.leaderZ;
            
            float groundZ = map->GetHeight(x, y, z + 50.0f, true, 50.0f);
            if (groundZ > INVALID_HEIGHT)
                z = groundZ + 0.5f;
            
            if (Creature* creature = map->SummonCreature(defenderEntry, Position(x, y, z, 0)))
            {
                creature->SetLevel(g_LevelDefender);
                creature->SetDisableGravity(false);
                creature->SetCanFly(false);
                creature->SetHover(false);
                creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
                creature->SetReactState(REACT_PASSIVE);
                creature->SetFaction(35);
                
                // Prevent return to home position after combat
                creature->SetWalk(false);
                creature->GetMotionMaster()->Clear(false);
                creature->GetMotionMaster()->MoveIdle();
                
                // Set home position to spawn location
                creature->SetHomePosition(x, y, z, 0);
                
                // Enforce ground position immediately after spawn
                creature->UpdateGroundPositionZ(x, y, z);
                
                event.spawnedDefenders.push_back(creature->GetGUID());
                
                if (g_DebugMode)
                {
                    LOG_INFO("server.loading", "[City Siege] Spawned defender at ({}, {}, {})", x, y, z);
                }
            }
        }
        
        LOG_INFO("server.loading", "[City Siege] Spawned {} defender creatures for {}", 
                 event.spawnedDefenders.size(), city.name);
    }
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
        
        // Despawn defenders
        for (const ObjectGuid& guid : event.spawnedDefenders)
        {
            if (Creature* creature = map->GetCreature(guid))
            {
                creature->DespawnOrUnsummon();
            }
        }
    }
    
    event.spawnedCreatures.clear();
    event.spawnedDefenders.clear();

    if (g_DebugMode)
    {
        LOG_INFO("server.loading", "[City Siege] Despawned attackers and defenders for siege at {}", city.name);
    }
}

/**
 * @brief Starts a new siege event in a random city.
 */
/**
 * @brief Randomize a position within a radius to prevent creatures from bunching up.
 * @param x Original X coordinate
 * @param y Original Y coordinate
 * @param z Original Z coordinate (will be updated with proper ground height)
 * @param map Map to check ground height
 * @param radius Random radius (default 5.0 yards)
 */
void RandomizePosition(float& x, float& y, float& z, Map* map, float radius = 5.0f)
{
    // Generate random offset within radius
    float angle = frand(0.0f, 2.0f * M_PI);
    float dist = frand(0.0f, radius);
    
    x += dist * cos(angle);
    y += dist * sin(angle);
    
    // Update Z to proper ground height
    if (map)
    {
        float groundZ = map->GetHeight(x, y, z + 50.0f, true, 50.0f);
        if (groundZ > INVALID_HEIGHT)
            z = groundZ + 0.5f;
    }
}

/**
 * @brief Validates and corrects ground position before movement to prevent floating/stuck units.
 * @param x X coordinate
 * @param y Y coordinate  
 * @param z Reference to Z coordinate to adjust
 * @param map Map to check ground height
 * @return true if position is valid, false if position is invalid/unreachable
 */
bool ValidateGroundPosition(float x, float y, float& z, Map* map)
{
    if (!map)
        return false;

    // Get ground height with generous search range
    float groundZ = map->GetHeight(x, y, z + 100.0f, true, 100.0f);
    
    // If ground height is invalid, try searching from below
    if (groundZ <= INVALID_HEIGHT)
    {
        groundZ = map->GetHeight(x, y, z - 50.0f, true, 100.0f);
    }
    
    // Still invalid - position is not reachable
    if (groundZ <= INVALID_HEIGHT)
    {
        return false;
    }
    
    // Clamp Z to be no more than 5 yards from ground (prevent high-altitude floating)
    if (z > groundZ + 5.0f)
    {
        z = groundZ + 0.5f;
    }
    else if (z < groundZ - 2.0f)
    {
        // Too far below ground, raise to ground level
        z = groundZ + 0.5f;
    }
    
    return true;
}

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
    newEvent.lastStatusAnnouncement = currentTime;
    newEvent.cinematicStartTime = currentTime;
    newEvent.countdown75Announced = false;
    newEvent.countdown50Announced = false;
    newEvent.countdown25Announced = false;

    // Announce siege is coming (before RP phase)
    std::string preAnnounce = "|cffff0000[City Siege]|r |cffFFFF00WARNING!|r A siege force is preparing to attack " + city->name + "! The battle will begin in " + std::to_string(g_CinematicDelay) + " seconds. Defenders, prepare yourselves!";
    sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, preAnnounce);

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
void EndSiegeEvent(SiegeEvent& event, int winningTeam = -1)
{
    if (!event.isActive)
    {
        return;
    }

    const CityData& city = g_Cities[event.cityId];
    event.isActive = false;

    // Check if defenders won (city leader still alive)
    bool defendersWon = false;
    bool leaderKilled = false;
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
        
        // If winningTeam was explicitly passed (leader died), mark it
        if (winningTeam != -1)
        {
            defendersWon = false;
            leaderKilled = true;
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

    // Respawn city leader if they were killed during the siege
    if (leaderKilled && map)
    {
        // Search around the leader's throne coordinates directly
        std::list<Creature*> leaderList;
        CitySiege::CreatureEntryCheck check(city.targetLeaderEntry);
        CitySiege::SimpleCreatureListSearcher<CitySiege::CreatureEntryCheck> searcher(leaderList, check);
        Cell::VisitObjects(city.leaderX, city.leaderY, map, searcher, 100.0f);
        
        Creature* existingLeader = nullptr;
        
        // Find the leader at the throne
        for (Creature* leader : leaderList)
        {
            if (leader)
            {
                existingLeader = leader;
                break;
            }
        }
        
        // Respawn the leader
        if (existingLeader)
        {
            if (!existingLeader->IsAlive())
            {
                existingLeader->Respawn();
                
                if (g_DebugMode)
                {
                    LOG_INFO("server.loading", "[City Siege] Respawned city leader {} (entry {}) at {}", 
                             city.name, city.targetLeaderEntry, existingLeader->GetName());
                }
            }
        }
        else
        {
            // Leader doesn't exist in world - log warning
            if (g_DebugMode)
            {
                LOG_WARN("server.loading", "[City Siege] Could not find city leader {} (entry {}) to respawn!", 
                         city.name, city.targetLeaderEntry);
            }
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

        // Countdown announcements during cinematic phase (percentage-based)
        if (event.cinematicPhase)
        {
            const CityData& city = g_Cities[event.cityId];
            uint32 elapsed = currentTime - event.cinematicStartTime;
            uint32 remaining = g_CinematicDelay > elapsed ? g_CinematicDelay - elapsed : 0;
            
            // Calculate percentage of time remaining
            float percentRemaining = g_CinematicDelay > 0 ? (static_cast<float>(remaining) / static_cast<float>(g_CinematicDelay)) * 100.0f : 0.0f;
            
            // Announce at 75%, 50%, and 25% time remaining
            if (percentRemaining <= 75.0f && !event.countdown75Announced)
            {
                event.countdown75Announced = true;
                std::string countdownMsg = "|cffff0000[City Siege]|r |cffFFFF00" + std::to_string(remaining) + " seconds|r until the siege of " + city.name + " begins! Defenders, prepare!";
                sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, countdownMsg);
            }
            else if (percentRemaining <= 50.0f && !event.countdown50Announced)
            {
                event.countdown50Announced = true;
                std::string countdownMsg = "|cffff0000[City Siege]|r |cffFF8800" + std::to_string(remaining) + " seconds|r until the siege of " + city.name + " begins! Defenders, to your posts!";
                sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, countdownMsg);
            }
            else if (percentRemaining <= 25.0f && !event.countdown25Announced)
            {
                event.countdown25Announced = true;
                std::string countdownMsg = "|cffff0000[City Siege]|r |cffFF0000" + std::to_string(remaining) + " seconds|r until the siege of " + city.name + " begins! FINAL WARNING!";
                sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, countdownMsg);
            }
        }

        // Check if cinematic phase is over
        if (event.cinematicPhase && (currentTime - event.startTime) >= g_CinematicDelay)
        {
            event.cinematicPhase = false;
            
            const CityData& city = g_Cities[event.cityId];
            
            // Announce battle has begun!
            std::string battleStart = "|cffff0000[City Siege]|r |cffFF0000THE BATTLE HAS BEGUN!|r The siege of " + city.name + " is now underway! Defenders, to arms!";
            sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, battleStart);
            
            if (g_DebugMode)
            {
                LOG_INFO("server.loading", "[City Siege] Cinematic phase ended, combat begins");
            }
            
            // Determine the city faction
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
                        creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
                        
                        // Force creature to ground level before starting movement
                        float creatureX = creature->GetPositionX();
                        float creatureY = creature->GetPositionY();
                        float creatureZ = creature->GetPositionZ();
                        float groundZ = creature->GetMap()->GetHeight(creatureX, creatureY, creatureZ + 5.0f, true, 50.0f);
                        
                        if (groundZ > INVALID_HEIGHT)
                        {
                            creature->UpdateGroundPositionZ(creatureX, creatureY, groundZ);
                            creature->Relocate(creatureX, creatureY, groundZ, creature->GetOrientation());
                        }
                        
                        // Prevent return to home position after combat - clear motion master
                        creature->SetWalk(false);
                        creature->GetMotionMaster()->Clear(false);
                        creature->GetMotionMaster()->MoveIdle();
                        
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
                        
                        // Randomize position within 5 yards to prevent bunching
                        Map* creatureMap = creature->GetMap();
                        RandomizePosition(destX, destY, destZ, creatureMap, 5.0f);
                        
                        // Update home position before movement to prevent evading
                        creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                        
                        // Use MoveSplineInit for proper pathfinding
                        Movement::MoveSplineInit init(creature);
                        init.MoveTo(destX, destY, destZ, true, true);
                        init.SetWalk(false);
                        init.Launch();
                    }
                }
                
                // Initialize defenders - they move in REVERSE order through waypoints
                for (const auto& guid : event.spawnedDefenders)
                {
                    if (Creature* creature = map->GetCreature(guid))
                    {
                        // Set proper defender faction (same as city faction)
                        creature->SetFaction(isAllianceCity ? 84 : 83); // 84 = Alliance, 83 = Horde
                        creature->SetReactState(REACT_AGGRESSIVE);
                        
                        // Ensure creature is grounded
                        creature->SetDisableGravity(false);
                        creature->SetCanFly(false);
                        creature->SetHover(false);
                        creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
                        
                        // Ground the creature
                        float creatureX = creature->GetPositionX();
                        float creatureY = creature->GetPositionY();
                        float creatureZ = creature->GetPositionZ();
                        float groundZ = creature->GetMap()->GetHeight(creatureX, creatureY, creatureZ + 5.0f, true, 50.0f);
                        
                        if (groundZ > INVALID_HEIGHT)
                        {
                            creature->UpdateGroundPositionZ(creatureX, creatureY, groundZ);
                            creature->Relocate(creatureX, creatureY, groundZ, creature->GetOrientation());
                        }
                        
                        creature->SetWalk(false);
                        creature->GetMotionMaster()->Clear(false);
                        creature->GetMotionMaster()->MoveIdle();
                        
                        // Defenders start at the LAST waypoint (highest index) and go backwards
                        // Set progress to MAX so they start at the end
                        uint32 startWaypoint = city.waypoints.empty() ? 0 : city.waypoints.size();
                        event.creatureWaypointProgress[guid] = startWaypoint + 10000; // Add 10000 to mark as defender
                        
                        // Determine first destination (last waypoint, or spawn point if no waypoints)
                        float destX, destY, destZ;
                        if (!city.waypoints.empty())
                        {
                            // Start at last waypoint and move backwards
                            destX = city.waypoints[city.waypoints.size() - 1].x;
                            destY = city.waypoints[city.waypoints.size() - 1].y;
                            destZ = city.waypoints[city.waypoints.size() - 1].z;
                        }
                        else
                        {
                            // No waypoints, go directly to spawn point
                            destX = city.spawnX;
                            destY = city.spawnY;
                            destZ = city.spawnZ;
                        }
                        
                        // Randomize position to prevent bunching
                        Map* creatureMap = creature->GetMap();
                        RandomizePosition(destX, destY, destZ, creatureMap, 5.0f);
                        
                        // Update home position
                        creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                        
                        // Start movement
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
                        // Track dead creatures for respawning
                        if (!creature->IsAlive())
                        {
                            // Check if this specific creature GUID is already in the dead list (avoid duplicates)
                            bool alreadyTracked = false;
                            for (const auto& deadData : event.deadCreatures)
                            {
                                if (deadData.guid == guid)
                                {
                                    alreadyTracked = true;
                                    break;
                                }
                            }
                            
                            // Add to dead creatures list if not already tracked
                            if (!alreadyTracked && g_RespawnEnabled)
                            {
                                SiegeEvent::RespawnData respawnData;
                                respawnData.guid = guid;
                                respawnData.entry = creature->GetEntry();
                                respawnData.deathTime = currentTime;
                                respawnData.isDefender = false; // This is an attacker
                                event.deadCreatures.push_back(respawnData);
                                
                                if (g_DebugMode)
                                {
                                    LOG_INFO("server.loading", "[City Siege] Attacker {} (entry {}) died, will respawn at siege spawn point in {} seconds",
                                             creature->GetGUID().ToString(), respawnData.entry,
                                             respawnData.entry == g_CreatureAllianceLeader || respawnData.entry == g_CreatureHordeLeader ? g_RespawnTimeLeader :
                                             respawnData.entry == g_CreatureAllianceMiniBoss || respawnData.entry == g_CreatureHordeMiniBoss ? g_RespawnTimeMiniBoss :
                                             respawnData.entry == g_CreatureAllianceElite || respawnData.entry == g_CreatureHordeElite ? g_RespawnTimeElite :
                                             g_RespawnTimeMinion);
                                }
                            }
                            continue;
                        }
                        
                        // IMPORTANT: ALWAYS set home position to current position to prevent evading/returning
                        // This must be done continuously - even during combat - because combat reset can restore original home
                        creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                        
                        // Skip movement updates if creature is currently in combat
                        if (creature->IsInCombat())
                            continue;
                        
                        // Check if creature is currently moving - if so, don't interrupt
                        if (!creature->movespline->Finalized())
                            continue;
                        
                        // Check if creature is currently moving - if so, don't interrupt
                        if (!creature->movespline->Finalized())
                            continue;
                        
                        // Force creature to ground level to prevent floating/clipping
                        float creatureX = creature->GetPositionX();
                        float creatureY = creature->GetPositionY();
                        float creatureZ = creature->GetPositionZ();
                        float groundZ = creature->GetMap()->GetHeight(creatureX, creatureY, creatureZ + 5.0f, true, 50.0f);
                        
                        // If ground Z is valid and creature is significantly off the ground, update position
                        if (groundZ > INVALID_HEIGHT && std::abs(creatureZ - groundZ) > 2.0f)
                        {
                            creature->UpdateGroundPositionZ(creatureX, creatureY, groundZ);
                            creature->Relocate(creatureX, creatureY, groundZ, creature->GetOrientation());
                        }
                        
                        // Continuously enforce ground movement flags
                        creature->SetDisableGravity(false);
                        creature->SetCanFly(false);
                        creature->SetHover(false);
                        creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
                        
                        // Get current waypoint index
                        uint32 currentWP = event.creatureWaypointProgress[guid];
                        
                        // Check if this is a defender (marked with +10000)
                        bool isDefender = (currentWP >= 10000);
                        if (isDefender)
                            currentWP -= 10000; // Remove marker to get actual waypoint
                        
                        // Check if we've reached final destination
                        if (!isDefender && currentWP > city.waypoints.size())
                            continue; // Attacker already at leader
                        if (isDefender && currentWP == 0 && city.waypoints.empty())
                            continue; // Defender at spawn point with no waypoints
                        
                        // Determine current target location
                        float targetX, targetY, targetZ;
                        
                        if (isDefender)
                        {
                            // DEFENDERS: Move backwards through waypoints (high to low), then to spawn
                            if (currentWP > 0 && currentWP <= city.waypoints.size())
                            {
                                // Moving towards a waypoint (backwards)
                                targetX = city.waypoints[currentWP - 1].x;
                                targetY = city.waypoints[currentWP - 1].y;
                                targetZ = city.waypoints[currentWP - 1].z;
                            }
                            else if (currentWP == 0)
                            {
                                // At first waypoint, now go to spawn point
                                targetX = city.spawnX;
                                targetY = city.spawnY;
                                targetZ = city.spawnZ;
                            }
                            else
                            {
                                continue; // Invalid state
                            }
                        }
                        else
                        {
                            // ATTACKERS: Move forwards through waypoints (low to high), then to leader
                            if (currentWP < city.waypoints.size())
                            {
                                targetX = city.waypoints[currentWP].x;
                                targetY = city.waypoints[currentWP].y;
                                targetZ = city.waypoints[currentWP].z;
                            }
                            else if (currentWP == city.waypoints.size())
                            {
                                targetX = city.leaderX;
                                targetY = city.leaderY;
                                targetZ = city.leaderZ;
                            }
                            else
                            {
                                continue;
                            }
                        }
                        
                        // Check distance to current target
                        float dist = creature->GetDistance(targetX, targetY, targetZ);
                        
                        // If creature is far from target (>10 yards) and not moving, resume movement to current target
                        if (dist > 10.0f)
                        {
                            // Randomize target position to prevent bunching
                            Map* creatureMap = creature->GetMap();
                            RandomizePosition(targetX, targetY, targetZ, creatureMap, 5.0f);
                            
                            // Validate ground position before movement
                            if (!ValidateGroundPosition(targetX, targetY, targetZ, creatureMap))
                            {
                                // Position is invalid, skip movement for this cycle
                                if (g_DebugMode)
                                {
                                    LOG_INFO("server.loading", "[City Siege] Creature {} at invalid waypoint position ({}, {}, {}), skipping movement",
                                             creature->GetGUID().ToString(), targetX, targetY, targetZ);
                                }
                                continue;
                            }
                            
                            // Update home position before movement to prevent evading
                            creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                            
                            Movement::MoveSplineInit init(creature);
                            init.MoveTo(targetX, targetY, targetZ, true, true);
                            init.SetWalk(false);
                            init.Launch();
                            continue;
                        }
                        
                        // Creature is close to current target (within 10 yards), consider it reached
                        if (dist <= 10.0f)
                        {
                            float nextX, nextY, nextZ;
                            bool hasNextDestination = false;
                            uint32 nextWP;
                            
                            if (isDefender)
                            {
                                // DEFENDERS: Move backwards (decrement waypoint)
                                if (currentWP > 0)
                                {
                                    nextWP = currentWP - 1;
                                    
                                    if (nextWP > 0)
                                    {
                                        // Move to previous waypoint
                                        nextX = city.waypoints[nextWP - 1].x;
                                        nextY = city.waypoints[nextWP - 1].y;
                                        nextZ = city.waypoints[nextWP - 1].z;
                                        hasNextDestination = true;
                                    }
                                    else
                                    {
                                        // Reached first waypoint, now go to spawn
                                        nextX = city.spawnX;
                                        nextY = city.spawnY;
                                        nextZ = city.spawnZ;
                                        hasNextDestination = true;
                                    }
                                    
                                    nextWP += 10000; // Re-add defender marker
                                }
                            }
                            else
                            {
                                // ATTACKERS: Move forwards (increment waypoint)
                                nextWP = currentWP + 1;
                                
                                if (nextWP < city.waypoints.size())
                                {
                                    // Move to next waypoint
                                    nextX = city.waypoints[nextWP].x;
                                    nextY = city.waypoints[nextWP].y;
                                    nextZ = city.waypoints[nextWP].z;
                                    hasNextDestination = true;
                                }
                                else if (nextWP == city.waypoints.size())
                                {
                                    // All waypoints complete, move to leader
                                    nextX = city.leaderX;
                                    nextY = city.leaderY;
                                    nextZ = city.leaderZ;
                                    hasNextDestination = true;
                                }
                            }
                            
                            // Update progress and start movement to next destination
                            if (hasNextDestination)
                            {
                                event.creatureWaypointProgress[guid] = nextWP;
                                
                                // Randomize next position to prevent bunching
                                Map* creatureMap = creature->GetMap();
                                RandomizePosition(nextX, nextY, nextZ, creatureMap, 5.0f);
                                
                                // Validate ground position before movement
                                if (!ValidateGroundPosition(nextX, nextY, nextZ, creatureMap))
                                {
                                    // Position is invalid, skip movement for this cycle
                                    if (g_DebugMode)
                                    {
                                        LOG_INFO("server.loading", "[City Siege] Creature {} next waypoint position ({}, {}, {}) invalid, skipping",
                                                 creature->GetGUID().ToString(), nextX, nextY, nextZ);
                                    }
                                    continue;
                                }
                                
                                // Update home position before movement to prevent evading
                                creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                                
                                Movement::MoveSplineInit init(creature);
                                init.MoveTo(nextX, nextY, nextZ, true, true);
                                init.SetWalk(false);
                                init.Launch();
                            }
                        }
                    }
                }
                
                // Check defenders for deaths (separate tracking from attackers)
                for (const auto& guid : event.spawnedDefenders)
                {
                    if (Creature* creature = map->GetCreature(guid))
                    {
                        // Track dead defenders for respawning
                        if (!creature->IsAlive())
                        {
                            // Check if this specific defender GUID is already in the dead list (avoid duplicates)
                            bool alreadyTracked = false;
                            for (const auto& deadData : event.deadCreatures)
                            {
                                if (deadData.guid == guid)
                                {
                                    alreadyTracked = true;
                                    break;
                                }
                            }
                            
                            // Add to dead creatures list if not already tracked
                            if (!alreadyTracked && g_RespawnEnabled)
                            {
                                SiegeEvent::RespawnData respawnData;
                                respawnData.guid = guid;
                                respawnData.entry = creature->GetEntry();
                                respawnData.deathTime = currentTime;
                                respawnData.isDefender = true; // This is a defender
                                event.deadCreatures.push_back(respawnData);
                                
                                if (g_DebugMode)
                                {
                                    LOG_INFO("server.loading", "[City Siege] Defender {} (entry {}) died, will respawn near leader position in {} seconds",
                                             creature->GetGUID().ToString(), respawnData.entry, g_RespawnTimeDefender);
                                }
                            }
                            continue;
                        }
                        
                        // IMPORTANT: ALWAYS set home position to current position to prevent evading/returning
                        creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                        
                        // Skip movement updates if creature is currently in combat
                        if (creature->IsInCombat())
                            continue;
                        
                        // Check if creature is currently moving - if so, don't interrupt
                        if (!creature->movespline->Finalized())
                            continue;
                        
                        // Force creature to ground level
                        float creatureX = creature->GetPositionX();
                        float creatureY = creature->GetPositionY();
                        float creatureZ = creature->GetPositionZ();
                        float groundZ = creature->GetMap()->GetHeight(creatureX, creatureY, creatureZ + 5.0f, true, 50.0f);
                        
                        if (groundZ > INVALID_HEIGHT && std::abs(creatureZ - groundZ) > 2.0f)
                        {
                            creature->UpdateGroundPositionZ(creatureX, creatureY, groundZ);
                            creature->Relocate(creatureX, creatureY, groundZ, creature->GetOrientation());
                        }
                        
                        creature->SetDisableGravity(false);
                        creature->SetCanFly(false);
                        creature->SetHover(false);
                        creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
                        
                        // Get current waypoint - defenders have +10000 marker
                        uint32 currentWP = event.creatureWaypointProgress[guid];
                        if (currentWP < 10000)
                            continue; // Not a defender marker, skip
                        
                        currentWP -= 10000; // Remove defender marker
                        
                        // Check if defender has reached spawn point (waypoint 0)
                        if (currentWP == 0 && city.waypoints.empty())
                            continue; // Already at spawn
                        
                        // Defenders move backwards through waypoints
                        float targetX, targetY, targetZ;
                        if (currentWP > 0 && currentWP <= city.waypoints.size())
                        {
                            // Moving towards previous waypoint
                            targetX = city.waypoints[currentWP - 1].x;
                            targetY = city.waypoints[currentWP - 1].y;
                            targetZ = city.waypoints[currentWP - 1].z;
                        }
                        else if (currentWP == 0)
                        {
                            // Go to spawn point
                            targetX = city.spawnX;
                            targetY = city.spawnY;
                            targetZ = city.spawnZ;
                        }
                        else
                        {
                            continue; // Invalid state
                        }
                        
                        // Check distance to target
                        float dist = creature->GetDistance(targetX, targetY, targetZ);
                        
                        // If far from target and not moving, resume movement
                        if (dist > 10.0f)
                        {
                            RandomizePosition(targetX, targetY, targetZ, map, 5.0f);
                            
                            // Validate ground position before movement
                            if (!ValidateGroundPosition(targetX, targetY, targetZ, map))
                            {
                                if (g_DebugMode)
                                {
                                    LOG_INFO("server.loading", "[City Siege] Defender {} at invalid waypoint position, skipping",
                                             creature->GetGUID().ToString());
                                }
                                continue;
                            }
                            
                            creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                            
                            Movement::MoveSplineInit init(creature);
                            init.MoveTo(targetX, targetY, targetZ, true, true);
                            init.SetWalk(false);
                            init.Launch();
                        }
                        // If close to target waypoint, advance to next
                        else if (dist <= 5.0f)
                        {
                            uint32 nextWP;
                            float nextX, nextY, nextZ;
                            
                            if (currentWP > 0)
                            {
                                // Move to previous waypoint
                                nextWP = currentWP - 1;
                                if (nextWP > 0)
                                {
                                    nextX = city.waypoints[nextWP - 1].x;
                                    nextY = city.waypoints[nextWP - 1].y;
                                    nextZ = city.waypoints[nextWP - 1].z;
                                }
                                else
                                {
                                    // Go to spawn point
                                    nextX = city.spawnX;
                                    nextY = city.spawnY;
                                    nextZ = city.spawnZ;
                                }
                            }
                            else
                            {
                                continue; // Already at spawn
                            }
                            
                            // Update progress with defender marker
                            event.creatureWaypointProgress[guid] = nextWP + 10000;
                            
                            RandomizePosition(nextX, nextY, nextZ, map, 5.0f);
                            
                            // Validate ground position before movement
                            if (!ValidateGroundPosition(nextX, nextY, nextZ, map))
                            {
                                if (g_DebugMode)
                                {
                                    LOG_INFO("server.loading", "[City Siege] Defender {} next waypoint position invalid, skipping",
                                             creature->GetGUID().ToString());
                                }
                                continue;
                            }
                            
                            creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                            
                            Movement::MoveSplineInit init(creature);
                            init.MoveTo(nextX, nextY, nextZ, true, true);
                            init.SetWalk(false);
                            init.Launch();
                        }
                    }
                }
            }
        }

        // Handle respawning of dead creatures (only during active siege, not during cinematic)
        if (!event.cinematicPhase && g_RespawnEnabled && !event.deadCreatures.empty())
        {
            const CityData& city = g_Cities[event.cityId];
            Map* map = sMapMgr->FindMap(city.mapId, 0);
            if (map)
            {
                // Check each dead creature to see if it's time to respawn
                for (auto it = event.deadCreatures.begin(); it != event.deadCreatures.end();)
                {
                    const auto& respawnData = *it;
                    
                    // Determine respawn time based on creature type and whether it's a defender
                    uint32 respawnDelay;
                    
                    if (respawnData.isDefender)
                    {
                        // Defenders use their own respawn time
                        respawnDelay = g_RespawnTimeDefender;
                    }
                    else
                    {
                        // Attackers use type-based respawn times
                        respawnDelay = g_RespawnTimeMinion; // Default
                        if (respawnData.entry == g_CreatureAllianceLeader || respawnData.entry == g_CreatureHordeLeader)
                        {
                            respawnDelay = g_RespawnTimeLeader;
                        }
                        else if (respawnData.entry == g_CreatureAllianceMiniBoss || respawnData.entry == g_CreatureHordeMiniBoss)
                        {
                            respawnDelay = g_RespawnTimeMiniBoss;
                        }
                        else if (respawnData.entry == g_CreatureAllianceElite || respawnData.entry == g_CreatureHordeElite)
                        {
                            respawnDelay = g_RespawnTimeElite;
                        }
                    }
                    
                    // Check if enough time has passed
                    if (currentTime >= (respawnData.deathTime + respawnDelay))
                    {
                        // Calculate spawn position based on whether this is a defender or attacker
                        float spawnX, spawnY, spawnZ;
                        
                        if (respawnData.isDefender)
                        {
                            // Defenders respawn near the city leader position
                            spawnX = city.leaderX;
                            spawnY = city.leaderY;
                            spawnZ = city.leaderZ;
                            
                            // Randomize spawn position in a circle around leader (15 yards)
                            float angle = frand(0.0f, 2.0f * M_PI);
                            float dist = frand(10.0f, 15.0f);
                            spawnX += dist * cos(angle);
                            spawnY += dist * sin(angle);
                        }
                        else
                        {
                            // Attackers respawn at the siege spawn point
                            spawnX = city.spawnX;
                            spawnY = city.spawnY;
                            spawnZ = city.spawnZ;
                        }
                        
                        // Get proper ground height at spawn location
                        float groundZ = map->GetHeight(spawnX, spawnY, spawnZ + 50.0f, true, 50.0f);
                        if (groundZ > INVALID_HEIGHT)
                            spawnZ = groundZ + 0.5f;
                        
                        // Respawn the creature
                        if (Creature* creature = map->SummonCreature(respawnData.entry, Position(spawnX, spawnY, spawnZ, 0)))
                        {
                            // Set up the respawned creature
                            bool isAllianceCity = (event.cityId <= CITY_EXODAR);
                            
                            // Set level and scale based on creature type
                            if (respawnData.isDefender)
                            {
                                creature->SetLevel(g_LevelDefender);
                                // Defenders use default scale (1.0)
                            }
                            else
                            {
                                // Determine attacker level and scale by entry
                                if (respawnData.entry == g_CreatureAllianceLeader || respawnData.entry == g_CreatureHordeLeader)
                                {
                                    creature->SetLevel(g_LevelLeader);
                                    creature->SetObjectScale(g_ScaleLeader);
                                }
                                else if (respawnData.entry == g_CreatureAllianceMiniBoss || respawnData.entry == g_CreatureHordeMiniBoss)
                                {
                                    creature->SetLevel(g_LevelMiniBoss);
                                    creature->SetObjectScale(g_ScaleMiniBoss);
                                }
                                else if (respawnData.entry == g_CreatureAllianceElite || respawnData.entry == g_CreatureHordeElite)
                                {
                                    creature->SetLevel(g_LevelElite);
                                    // Elites use default scale (1.0)
                                }
                                else
                                {
                                    creature->SetLevel(g_LevelMinion);
                                    // Minions use default scale (1.0)
                                }
                            }
                            
                            if (respawnData.isDefender)
                            {
                                // Defenders use city faction
                                creature->SetFaction(isAllianceCity ? 84 : 83); // 84 = Alliance, 83 = Horde
                                creature->SetReactState(REACT_AGGRESSIVE);
                            }
                            else
                            {
                                // Attackers use opposing faction
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
                            }
                            
                            // Enforce ground movement
                            creature->SetDisableGravity(false);
                            creature->SetCanFly(false);
                            creature->SetHover(false);
                            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_HOVER);
                            creature->UpdateGroundPositionZ(spawnX, spawnY, spawnZ);
                            
                            // Prevent return to home position after combat - clear motion master
                            creature->SetWalk(false);
                            creature->GetMotionMaster()->Clear(false);
                            creature->GetMotionMaster()->MoveIdle();
                            
                            // Set home position to spawn location to prevent evading back
                            creature->SetHomePosition(spawnX, spawnY, spawnZ, 0);
                            
                            // Replace the old GUID with the new one in appropriate spawned list
                            if (respawnData.isDefender)
                            {
                                for (auto& spawnedGuid : event.spawnedDefenders)
                                {
                                    if (spawnedGuid == respawnData.guid)
                                    {
                                        spawnedGuid = creature->GetGUID();
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                for (auto& spawnedGuid : event.spawnedCreatures)
                                {
                                    if (spawnedGuid == respawnData.guid)
                                    {
                                        spawnedGuid = creature->GetGUID();
                                        break;
                                    }
                                }
                            }
                            
                            // Set waypoint progress and initial movement destination
                            event.creatureWaypointProgress.erase(respawnData.guid); // Remove old GUID
                            
                            float destX, destY, destZ;
                            
                            if (respawnData.isDefender)
                            {
                                // Defenders start at last waypoint and move backwards
                                uint32 startWaypoint = city.waypoints.empty() ? 0 : city.waypoints.size();
                                event.creatureWaypointProgress[creature->GetGUID()] = startWaypoint + 10000; // Add defender marker
                                
                                // Start moving to last waypoint (or spawn point if no waypoints)
                                if (!city.waypoints.empty())
                                {
                                    destX = city.waypoints[city.waypoints.size() - 1].x;
                                    destY = city.waypoints[city.waypoints.size() - 1].y;
                                    destZ = city.waypoints[city.waypoints.size() - 1].z;
                                }
                                else
                                {
                                    destX = city.spawnX;
                                    destY = city.spawnY;
                                    destZ = city.spawnZ;
                                }
                            }
                            else
                            {
                                // Attackers start from waypoint 0 and move forward
                                event.creatureWaypointProgress[creature->GetGUID()] = 0;
                                
                                // Start movement to first waypoint or leader
                                if (!city.waypoints.empty())
                                {
                                    destX = city.waypoints[0].x;
                                    destY = city.waypoints[0].y;
                                    destZ = city.waypoints[0].z;
                                }
                                else
                                {
                                    destX = city.leaderX;
                                    destY = city.leaderY;
                                    destZ = city.leaderZ;
                                }
                            }
                            
                            // Randomize position to prevent bunching on respawn
                            Map* creatureMap = creature->GetMap();
                            RandomizePosition(destX, destY, destZ, creatureMap, 5.0f);
                            
                            // Update home position before movement to prevent evading
                            creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation());
                            
                            Movement::MoveSplineInit init(creature);
                            init.MoveTo(destX, destY, destZ, true, true);
                            init.SetWalk(false);
                            init.Launch();
                            
                            if (g_DebugMode)
                            {
                                LOG_INFO("server.loading", "[City Siege] Respawned {} {} at {} ({}, {}, {}), starting movement to {} waypoint",
                                         respawnData.isDefender ? "defender" : "attacker",
                                         creature->GetGUID().ToString(),
                                         respawnData.isDefender ? "leader position" : "siege spawn point",
                                         spawnX, spawnY, spawnZ,
                                         respawnData.isDefender ? "last" : "first");
                            }
                        }
                        
                        // Remove from dead creatures list
                        it = event.deadCreatures.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }

        // Status announcements every 5 minutes (300 seconds) during active combat
        if (!event.cinematicPhase && (currentTime - event.lastStatusAnnouncement) >= 300)
        {
            event.lastStatusAnnouncement = currentTime;
            
            const CityData& city = g_Cities[event.cityId];
            Map* map = sMapMgr->FindMap(city.mapId, 0);
            
            // Calculate time remaining
            uint32 timeRemaining = event.endTime > currentTime ? event.endTime - currentTime : 0;
            uint32 minutesLeft = timeRemaining / 60;
            
            // Try to get leader health percentage - SEARCH FROM LEADER COORDINATES!
            uint32 leaderHealthPct = 100;
            bool leaderHealthAvailable = false;
            
            if (map)
            {
                // Search around the leader's throne coordinates directly
                std::list<Creature*> leaderList;
                CitySiege::CreatureEntryCheck check(city.targetLeaderEntry);
                CitySiege::SimpleCreatureListSearcher<CitySiege::CreatureEntryCheck> searcher(leaderList, check);
                Cell::VisitObjects(city.leaderX, city.leaderY, map, searcher, 100.0f);
                
                // Find the leader at the throne
                for (Creature* leader : leaderList)
                {
                    if (leader && leader->IsAlive())
                    {
                        leaderHealthPct = leader->GetHealthPct();
                        leaderHealthAvailable = true;
                        break;
                    }
                }
            }
            
            // Build announcement message
            std::string statusMsg = "|cffff0000[City Siege]|r |cffFFFF00STATUS UPDATE:|r ";
            statusMsg += city.name + " siege - ";
            statusMsg += std::to_string(minutesLeft) + " minutes remaining. ";
            
            if (leaderHealthAvailable)
            {
                statusMsg += "Leader health: |cff";
                // Color code based on health
                if (leaderHealthPct > 75)
                    statusMsg += "00FF00"; // Green
                else if (leaderHealthPct > 50)
                    statusMsg += "FFFF00"; // Yellow
                else if (leaderHealthPct > 25)
                    statusMsg += "FF8800"; // Orange
                else
                    statusMsg += "FF0000"; // Red
                    
                statusMsg += std::to_string(leaderHealthPct) + "%|r";
                
                // Add dramatic messages for critical health
                if (leaderHealthPct <= 25)
                {
                    statusMsg += " |cffFF0000CRITICAL!|r The city leader is in grave danger!";
                }
                else if (leaderHealthPct <= 50)
                {
                    statusMsg += " The city leader is under heavy assault!";
                }
            }
            else
            {
                statusMsg += "Leader status: Unknown (not in combat yet)";
            }
            
            // Add time warning if less than 10 minutes left
            if (minutesLeft <= 5 && minutesLeft > 0)
            {
                statusMsg += " |cffFFFF00FINAL MINUTES!|r";
            }
            
            sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, statusMsg);
        }

        // Check if city leader is dead (attackers win)
        if (!event.cinematicPhase)
        {
            const CityData& city = g_Cities[event.cityId];
            Map* map = sMapMgr->FindMap(city.mapId, 0);
            if (map)
            {
                // Search around the leader's throne coordinates directly - no dependency on siege creatures!
                std::list<Creature*> leaderList;
                CitySiege::CreatureEntryCheck check(city.targetLeaderEntry);
                CitySiege::SimpleCreatureListSearcher<CitySiege::CreatureEntryCheck> searcher(leaderList, check);
                Cell::VisitObjects(city.leaderX, city.leaderY, map, searcher, 100.0f);
                
                bool leaderFound = false;
                bool leaderAlive = false;
                
                // Check if we found the leader at the throne
                for (Creature* leader : leaderList)
                {
                    if (leader)
                    {
                        leaderFound = true;
                        leaderAlive = leader->IsAlive();
                        break;
                    }
                }
                
                // Only end siege if we actually FOUND the leader and they are DEAD
                if (leaderFound && !leaderAlive)
                {
                    if (g_DebugMode)
                    {
                        LOG_INFO("server.loading", "[City Siege] City leader killed! Attackers win. Ending siege of {}", city.name);
                    }
                    
                    // Determine winning team: opposite of the city's faction
                    bool isAllianceCity = (event.cityId <= CITY_EXODAR);
                    int winningTeam = isAllianceCity ? 1 : 0; // 0 = Alliance, 1 = Horde
                    
                    EndSiegeEvent(event, winningTeam);
                }
            }
        }

        // Check if event should end (time limit reached - defenders win)
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
            { "start",        HandleCitySiegeStartCommand,        SEC_GAMEMASTER, Console::No },
            { "stop",         HandleCitySiegeStopCommand,         SEC_GAMEMASTER, Console::No },
            { "cleanup",      HandleCitySiegeCleanupCommand,      SEC_GAMEMASTER, Console::No },
            { "status",       HandleCitySiegeStatusCommand,       SEC_GAMEMASTER, Console::No },
            { "testwaypoint", HandleCitySiegeTestWaypointCommand, SEC_GAMEMASTER, Console::No },
            { "waypoints",    HandleCitySiegeWaypointsCommand,    SEC_GAMEMASTER, Console::No }
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
        }
        else
        {
            StartSiegeEvent(cityId);
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

    static bool HandleCitySiegeTestWaypointCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            handler->PSendSysMessage("You must be logged in to use this command.");
            return true;
        }

        Map* map = player->GetMap();
        if (!map)
        {
            handler->PSendSysMessage("Could not get map.");
            return true;
        }

        // Get player position (use actual position, not ground adjusted)
        float x = player->GetPositionX();
        float y = player->GetPositionY();
        float z = player->GetPositionZ();
        
        // Store original for display
        float displayZ = z;

        // Try to find ground near player position for spawning the marker
        float groundZ = map->GetHeight(x, y, z + 10.0f, true, 50.0f);
        if (groundZ <= INVALID_HEIGHT)
        {
            // Try searching from below
            groundZ = map->GetHeight(x, y, z - 10.0f, true, 50.0f);
        }
        
        // Use ground height if found, otherwise use player height
        float spawnZ = (groundZ > INVALID_HEIGHT) ? groundZ : z;

        // Spawn temporary waypoint marker (white spotlight - entry 15631)
        if (Creature* marker = map->SummonCreature(15631, Position(x, y, spawnZ, 0)))
        {
            marker->SetObjectScale(2.5f); // Standard waypoint size
            marker->SetReactState(REACT_PASSIVE);
            marker->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            marker->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            marker->DespawnOrUnsummon(20000); // Despawn after 20 seconds

            handler->PSendSysMessage("Test waypoint marker spawned at your location for 20 seconds.");
            
            // Format coordinates properly
            char coordMsg[256];
            snprintf(coordMsg, sizeof(coordMsg), "Coordinates: X=%.2f, Y=%.2f, Z=%.2f", x, y, displayZ);
            handler->PSendSysMessage(coordMsg);
            
            handler->PSendSysMessage("Copy these coordinates to your mod_city_siege.conf file.");
        }
        else
        {
            handler->PSendSysMessage("Failed to spawn test waypoint marker at this location.");
            
            // Show coordinates anyway
            char coordMsg[256];
            snprintf(coordMsg, sizeof(coordMsg), "Your position: X=%.2f, Y=%.2f, Z=%.2f", x, y, displayZ);
            handler->PSendSysMessage(coordMsg);
            
            handler->PSendSysMessage("This location may not be valid for spawning creatures.");
        }

        return true;
    }

    static bool HandleCitySiegeWaypointsCommand(ChatHandler* handler, Optional<std::string> cityNameArg)
    {
        if (!cityNameArg)
        {
            handler->PSendSysMessage("Usage: .citysiege waypoints <cityname>");
            handler->PSendSysMessage("Shows or hides waypoint visualization for a city.");
            handler->PSendSysMessage("Available cities: Stormwind, Ironforge, Darnassus, Exodar, Orgrimmar, Undercity, ThunderBluff, Silvermoon");
            return true;
        }

        // Parse city name
        std::string cityName = *cityNameArg;
        std::transform(cityName.begin(), cityName.end(), cityName.begin(), ::tolower);

        int cityId = -1;
        for (size_t i = 0; i < g_Cities.size(); ++i)
        {
            std::string checkName = g_Cities[i].name;
            std::transform(checkName.begin(), checkName.end(), checkName.begin(), ::tolower);
            if (checkName == cityName)
            {
                cityId = static_cast<int>(i);
                break;
            }
        }

        if (cityId == -1)
        {
            handler->PSendSysMessage("Unknown city. Use: Stormwind, Ironforge, Darnassus, Exodar, Orgrimmar, Undercity, ThunderBluff, or Silvermoon");
            return true;
        }

        const CityData& city = g_Cities[cityId];
        Map* map = sMapMgr->FindMap(city.mapId, 0);
        if (!map)
        {
            handler->PSendSysMessage("Could not find map for this city.");
            return true;
        }

        // Check if waypoints are already shown for this city
        if (g_WaypointVisualizations.find(cityId) != g_WaypointVisualizations.end())
        {
            // Hide waypoints
            std::vector<ObjectGuid>& visualizations = g_WaypointVisualizations[cityId];
            for (const ObjectGuid& guid : visualizations)
            {
                if (Creature* creature = map->GetCreature(guid))
                {
                    creature->DespawnOrUnsummon(0);
                }
            }
            g_WaypointVisualizations.erase(cityId);
            handler->PSendSysMessage(("Waypoint visualization hidden for " + city.name).c_str());
            return true;
        }

        // Show waypoints - spawn visualization creatures
        std::vector<ObjectGuid> visualizations;

        // Visualize spawn point
        float spawnZ = city.spawnZ;
        float groundZ = map->GetHeight(city.spawnX, city.spawnY, spawnZ + 10.0f, true, 50.0f);
        if (groundZ <= INVALID_HEIGHT)
        {
            groundZ = map->GetHeight(city.spawnX, city.spawnY, spawnZ - 10.0f, true, 50.0f);
        }
        if (groundZ > INVALID_HEIGHT)
            spawnZ = groundZ;

        // Use entry 15631 (spotlight effect) - a tall visual beam
        if (Creature* marker = map->SummonCreature(15631, Position(city.spawnX, city.spawnY, spawnZ, 0)))
        {
            marker->SetObjectScale(3.0f); // Large scale for visibility
            marker->SetReactState(REACT_PASSIVE);
            marker->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            marker->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            visualizations.push_back(marker->GetGUID());
            
            char spawnMsg[256];
            snprintf(spawnMsg, sizeof(spawnMsg), "Spawn Point: X=%.2f, Y=%.2f, Z=%.2f - OK", 
                city.spawnX, city.spawnY, city.spawnZ);
            handler->PSendSysMessage(spawnMsg);
            
            if (g_DebugMode)
            {
                LOG_INFO("module", "[City Siege] Spawned spawn point marker at {}, {}, {}", city.spawnX, city.spawnY, spawnZ);
            }
        }
        else
        {
            char spawnMsg[256];
            snprintf(spawnMsg, sizeof(spawnMsg), "Spawn Point: X=%.2f, Y=%.2f, Z=%.2f - FAILED", 
                city.spawnX, city.spawnY, city.spawnZ);
            handler->PSendSysMessage(spawnMsg);
        }

        // Visualize each waypoint
        handler->PSendSysMessage(("City has " + std::to_string(city.waypoints.size()) + " waypoints configured.").c_str());
        
        int spawnedWaypoints = 0;
        int failedWaypoints = 0;
        
        for (size_t i = 0; i < city.waypoints.size(); ++i)
        {
            float wpX = city.waypoints[i].x;
            float wpY = city.waypoints[i].y;
            float wpZ = city.waypoints[i].z;
            
            // Try to find ground near the waypoint position
            float groundZ = map->GetHeight(wpX, wpY, wpZ + 10.0f, true, 50.0f);
            if (groundZ <= INVALID_HEIGHT)
            {
                // Try searching from below
                groundZ = map->GetHeight(wpX, wpY, wpZ - 10.0f, true, 50.0f);
            }
            
            // Use ground height if found, otherwise use config Z
            float spawnZ = (groundZ > INVALID_HEIGHT) ? groundZ : wpZ;

            if (Creature* marker = map->SummonCreature(15631, Position(wpX, wpY, spawnZ, 0)))
            {
                marker->SetObjectScale(2.5f); // Medium size for waypoints
                marker->SetReactState(REACT_PASSIVE);
                marker->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                marker->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                visualizations.push_back(marker->GetGUID());
                spawnedWaypoints++;
                
                // Format coordinates properly
                char waypointMsg[256];
                snprintf(waypointMsg, sizeof(waypointMsg), "  WP %zu: X=%.2f, Y=%.2f, Z=%.2f - OK", 
                    i + 1, wpX, wpY, wpZ);
                handler->PSendSysMessage(waypointMsg);
                
                if (g_DebugMode)
                {
                    LOG_INFO("module", "[City Siege] Spawned waypoint {} marker at {}, {}, {}", i + 1, wpX, wpY, spawnZ);
                }
            }
            else
            {
                failedWaypoints++;
                
                // Format coordinates properly
                char waypointMsg[256];
                snprintf(waypointMsg, sizeof(waypointMsg), "  WP %zu: X=%.2f, Y=%.2f, Z=%.2f - FAILED", 
                    i + 1, wpX, wpY, wpZ);
                handler->PSendSysMessage(waypointMsg);
            }
        }
        
        if (failedWaypoints > 0)
        {
            char warningMsg[128];
            snprintf(warningMsg, sizeof(warningMsg), "WARNING: %d waypoint markers failed to spawn!", failedWaypoints);
            handler->PSendSysMessage(warningMsg);
        }

        // Visualize leader position (using same green spotlight as spawn - entry 15631)
        float leaderZ = city.leaderZ;
        groundZ = map->GetHeight(city.leaderX, city.leaderY, leaderZ + 10.0f, true, 50.0f);
        if (groundZ <= INVALID_HEIGHT)
        {
            groundZ = map->GetHeight(city.leaderX, city.leaderY, leaderZ - 10.0f, true, 50.0f);
        }
        if (groundZ > INVALID_HEIGHT)
            leaderZ = groundZ;

        if (Creature* marker = map->SummonCreature(15631, Position(city.leaderX, city.leaderY, leaderZ, 0)))
        {
            marker->SetObjectScale(3.0f); // Same size as spawn marker
            marker->SetReactState(REACT_PASSIVE);
            marker->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            marker->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            visualizations.push_back(marker->GetGUID());
            
            char leaderMsg[256];
            snprintf(leaderMsg, sizeof(leaderMsg), "Leader Position: X=%.2f, Y=%.2f, Z=%.2f - OK", 
                city.leaderX, city.leaderY, city.leaderZ);
            handler->PSendSysMessage(leaderMsg);
            
            if (g_DebugMode)
            {
                LOG_INFO("module", "[City Siege] Spawned leader position marker at {}, {}, {}", city.leaderX, city.leaderY, leaderZ);
            }
        }
        else
        {
            char leaderMsg[256];
            snprintf(leaderMsg, sizeof(leaderMsg), "Leader Position: X=%.2f, Y=%.2f, Z=%.2f - FAILED", 
                city.leaderX, city.leaderY, city.leaderZ);
            handler->PSendSysMessage(leaderMsg);
        }

        g_WaypointVisualizations[cityId] = visualizations;
        
        char summaryMsg[256];
        snprintf(summaryMsg, sizeof(summaryMsg), "Total markers: %zu (1 Spawn + %zu Waypoints + 1 Leader)", 
            visualizations.size(), city.waypoints.size());
        handler->PSendSysMessage(summaryMsg);
        
        handler->PSendSysMessage("Green/Large = Spawn & Leader | White/Medium = Waypoints");
        
        if (g_DebugMode)
        {
            LOG_INFO("module", "[City Siege] Total visualization markers spawned: {}", visualizations.size());
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
