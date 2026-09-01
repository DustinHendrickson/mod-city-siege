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

#ifndef CITY_SIEGE_H
#define CITY_SIEGE_H

#include "Common.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "SharedDefines.h"
#include "Weather.h"

#include <string>
#include <unordered_map>
#include <vector>

class ChatHandler;
class Creature;
class Map;
class Player;
class Unit;

namespace CitySiege
{
    // -------------------------------------------------------------------------
    // Identifiers
    // -------------------------------------------------------------------------

    enum CityId : uint32
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

    // Rank of a siege unit. Drives level offset, scale, respawn timer and which
    // units are allowed to deliver roleplay lines.
    enum SiegeRank : uint8
    {
        RANK_MINION = 0,
        RANK_ELITE,
        RANK_MINIBOSS,
        RANK_WARLORD,
        RANK_MAX
    };

    // Narrative stage of a siege. Advances on time (muster) and on how far the
    // vanguard has pushed along the route (assault -> breach -> throne).
    enum SiegeStage : uint8
    {
        STAGE_MUSTER = 0,   // roleplay build-up, army is passive
        STAGE_ASSAULT,      // marching from the muster point toward the walls
        STAGE_BREACH,       // fighting through the city proper
        STAGE_THRONE,       // vanguard has reached the city leader
        STAGE_FINISHED
    };

    // How the marching route from the muster point to the throne is produced.
    enum RouteMode : uint8
    {
        ROUTE_AUTO = 0,     // generate with the server navmesh, fall back to manual
        ROUTE_MANUAL,       // only use waypoints from the config file
        ROUTE_DIRECT        // no route at all, walk straight at the leader
    };

    enum RouteSource : uint8
    {
        ROUTE_SRC_NONE = 0,
        ROUTE_SRC_AUTO,
        ROUTE_SRC_MANUAL,
        ROUTE_SRC_DIRECT
    };

    // -------------------------------------------------------------------------
    // Static city description
    // -------------------------------------------------------------------------

    struct CityData
    {
        CityId      id = CITY_MAX;
        std::string name;                       // config key / command name
        std::string displayName;                // pretty name used in messages
        uint32      mapId = 0;
        TeamId      owner = TEAM_ALLIANCE;      // faction that defends this city

        Position    center;                     // announcement anchor
        Position    muster;                     // where the attacking army forms up
        Position    leader;                     // resolved throne position

        uint32      leaderEntry = 0;
        bool        leaderPositionPinned = false;   // admin supplied explicit coords
        bool        leaderPositionResolved = false; // read back from creature table

        std::vector<Position> manualRoute;      // legacy waypoints from the config
        std::vector<Position> autoRoute;        // navmesh generated route
        RouteSource           routeSource = ROUTE_SRC_NONE;
        bool                  routeAttempted = false;
        std::string           routeDiagnostic;
    };

    // -------------------------------------------------------------------------
    // Per-unit and per-player siege state
    // -------------------------------------------------------------------------

    // A unit's fixed place in the marching formation, in yards relative to the
    // route line: side is lateral (signed, right of travel is positive), depth
    // is how far back from the vanguard it marches.
    struct FormationSlot
    {
        float side = 0.0f;
        float depth = 0.0f;
    };

    // Bookkeeping the siege event keeps per unit. Live marching state (which
    // waypoint it is walking to, whether it is stuck) belongs to the unit's
    // SiegeUnitAI, not here.
    struct SiegeUnit
    {
        ObjectGuid    guid;
        uint32        entry = 0;    // kept so a unit can respawn after its corpse decays
        uint8         rank = RANK_MINION;
        bool          attacker = true;
        FormationSlot slot;
        uint32        routeIndex = 0;   // waypoint the unit starts its march from
    };

    struct Contribution
    {
        uint64      damage = 0;
        uint64      healing = 0;
        uint32      kills = 0;
        uint32      lastActive = 0;
        uint32      level = 0;
        TeamId      team = TEAM_NEUTRAL;
        std::string name;

        // Weighted score used for ranking and reward tiers.
        uint64 Score() const { return damage + healing / 2 + uint64(kills) * 500; }
    };

    struct RespawnEntry
    {
        ObjectGuid    guid;     // guid of the corpse being replaced
        uint32        entry = 0;
        uint32        deathTime = 0;
        uint8         rank = RANK_MINION;
        bool          attacker = true;
        FormationSlot slot;     // reinforcements march back into the same file
    };

    struct BotReturnPosition
    {
        ObjectGuid  guid;
        uint32      mapId = 0;
        float       x = 0.0f, y = 0.0f, z = 0.0f, o = 0.0f;
        bool        wasPvPFlagged = false;
        std::string rpgStrategy;
    };

    struct BotRespawnEntry
    {
        ObjectGuid guid;
        uint32     deathTime = 0;
        bool       defender = false;
    };

    // -------------------------------------------------------------------------
    // A live siege
    // -------------------------------------------------------------------------

    struct SiegeEvent
    {
        CityId  cityId = CITY_MAX;
        bool    active = false;

        uint32  startTime = 0;          // when the muster phase began
        uint32  combatStartTime = 0;    // when the army was released
        uint32  endTime = 0;            // hard deadline

        SiegeStage stage = STAGE_MUSTER;
        uint8      siegeLevel = 80;     // level the army was scaled to

        // Attacking army and city garrison, keyed by creature guid. The
        // SiegeUnit::attacker flag says which side a unit belongs to.
        std::unordered_map<ObjectGuid, SiegeUnit> units;
        std::vector<RespawnEntry> pendingRespawns;

        // City leader being defended
        ObjectGuid  leaderGuid;
        std::string leaderName;
        uint8       leaderHealthAnnounced = 100;

        // Set when the siege resolves, reported to the client addon
        std::string winnerName = "none";

        // Roleplay
        std::vector<std::string> rpScript;
        uint32 rpIndex = 0;
        uint32 lastRpTime = 0;
        uint32 lastYellTime = 0;

        // Pacing / bookkeeping
        uint32 lastStatusAnnounce = 0;
        uint32 lastAddonBroadcast = 0;
        uint32 lastMusicPulse = 0;
        uint32 lastMovementTick = 0;
        uint32 lastRespawnTick = 0;
        bool   countdown75Announced = false;
        bool   countdown50Announced = false;
        bool   countdown25Announced = false;

        // Scoreboard
        uint32 attackerKills = 0;   // siege units killed by the defence
        uint32 defenderKills = 0;   // players/garrison killed by the army
        std::unordered_map<ObjectGuid, Contribution> contributions;

        // Weather restore
        WeatherState originalWeatherType = WEATHER_STATE_FINE;
        float        originalWeatherGrade = 0.0f;
        bool         weatherOverridden = false;
        uint32       weatherZoneId = 0;

        // Playerbots
        std::vector<ObjectGuid>        defenderBots;
        std::vector<ObjectGuid>        attackerBots;
        std::vector<BotReturnPosition> botReturnPositions;
        std::vector<BotRespawnEntry>   deadBots;
        std::unordered_map<ObjectGuid, uint32> botRouteIndex;
        std::unordered_map<ObjectGuid, uint32> botLastOrder;
    };

    // -------------------------------------------------------------------------
    // Shared state
    // -------------------------------------------------------------------------

    extern std::vector<CityData>  g_Cities;
    extern std::vector<SiegeEvent> g_ActiveSieges;
    extern uint32                 g_NextSiegeTime;

    // -------------------------------------------------------------------------
    // Configuration (defined in CitySiegeConfig.cpp)
    // -------------------------------------------------------------------------

    struct Config
    {
        bool   enabled = true;
        bool   debug = false;

        uint32 timerMin = 120 * 60;
        uint32 timerMax = 240 * 60;
        uint32 eventDuration = 30 * 60;
        uint32 musterDuration = 150;
        uint32 yellFrequency = 30;

        bool   allowMultipleCities = false;
        uint32 announceRadius = 1500;
        bool   worldAnnounce = true;
        uint32 minimumLevel = 1;

        std::unordered_map<std::string, bool> cityEnabled;

        // Route generation
        RouteMode routeMode = ROUTE_AUTO;
        float     routeNodeSpacing = 28.0f;
        uint32    routeMaxNodes = 64;
        uint32    routeMaxLegs = 16;
        float     routeMaxSlope = 45.0f;   // degrees an army will march up
        bool      autoDetectLeader = true;

        // Army composition
        uint32 countMinions = 15;
        uint32 countElites = 5;
        uint32 countMiniBosses = 2;
        uint32 countWarlords = 1;
        bool   defendersEnabled = true;
        uint32 defendersCount = 10;

        uint32 creatureAllianceMinion = 17919;
        uint32 creatureAllianceElite = 17920;
        uint32 creatureAllianceMiniBoss = 17921;
        uint32 creatureAllianceDefender = 17919;
        uint32 creatureHordeMinion = 17932;
        uint32 creatureHordeElite = 17933;
        uint32 creatureHordeMiniBoss = 17934;
        uint32 creatureHordeDefender = 17932;

        std::vector<uint32> allianceWarlords;
        std::vector<uint32> hordeWarlords;

        bool aggroPlayers = true;
        bool aggroNPCs = true;

        // Level scaling
        bool   levelScalingEnabled = true;
        uint32 levelFallback = 80;
        uint32 levelMin = 20;
        uint32 levelMax = 83;
        uint32 levelSampleRadius = 600;
        int32  levelOffsetWarlord = 3;
        int32  levelOffsetMiniBoss = 2;
        int32  levelOffsetElite = 0;
        int32  levelOffsetMinion = -2;
        int32  levelOffsetDefender = -1;
        uint32 minionLevelSpread = 4;

        float healthMultWarlord = 12.0f;
        float healthMultMiniBoss = 5.0f;
        float healthMultElite = 2.5f;
        float healthMultMinion = 1.0f;
        float healthMultDefender = 1.5f;
        float damageMultWarlord = 1.6f;
        float damageMultMiniBoss = 1.3f;
        float damageMultElite = 1.1f;
        float damageMultMinion = 1.0f;
        float damageMultDefender = 1.0f;

        float scaleWarlord = 1.6f;
        float scaleMiniBoss = 1.3f;

        // Bracket handicap so every level can join in
        bool  bracketScalingEnabled = true;
        float bracketMinDamageTaken = 0.15f;   // floor on damage the army deals down-level
        float bracketMaxDamageDealt = 8.0f;    // cap on the boost a low level player gets
        uint32 bracketFreeLevels = 3;          // levels of difference ignored before scaling

        // Where the garrison meets the assault, as a fraction of the route
        float defenderHoldFraction = 0.5f;

        // Marching formation. Units keep a fixed slot relative to the route line
        // instead of all walking at the same point, which is what stops a war
        // host from collapsing into one pile of overlapping models.
        float  formationSpacing = 6.0f;     // yards between neighbours in a rank
        float  formationRowDepth = 6.0f;    // yards between rows of the same rank
        uint32 formationWidth = 5;          // units per row
        float  formationDepthMinion = 0.0f; // yards behind the vanguard, per rank
        float  formationDepthElite = 14.0f;
        float  formationDepthMiniBoss = 26.0f;
        float  formationDepthWarlord = 34.0f;
        float  formationJitter = 1.5f;      // small random wobble so ranks are not robotic

        // Respawn
        bool   respawnEnabled = true;
        uint32 respawnWarlord = 300;
        uint32 respawnMiniBoss = 180;
        uint32 respawnElite = 120;
        uint32 respawnMinion = 60;
        uint32 respawnDefender = 45;

        // Rewards
        bool   rewardsEnabled = true;
        bool   rewardLosingSide = true;
        uint32 rewardHonor = 100;
        uint32 rewardGoldBase = 5000;
        uint32 rewardGoldPerLevel = 5000;
        bool   rewardXpEnabled = true;
        uint32 rewardXpPerLevel = 400;
        uint32 rewardItemId = 0;
        uint32 rewardItemCount = 1;
        float  rewardLoserFactor = 0.35f;
        uint64 participationThreshold = 1;
        uint32 participationWindow = 600;   // seconds since last hit to still count
        bool   announceTopContributors = true;

        // Presentation
        bool         weatherEnabled = true;
        WeatherState weatherType = WEATHER_STATE_MEDIUM_RAIN;
        float        weatherGrade = 0.8f;
        bool         musicEnabled = true;
        uint32       musicRp = 11803;
        uint32       musicCombat = 11804;
        uint32       musicVictory = 16039;
        uint32       musicDefeat = 14127;
        uint32       musicLoopSeconds = 120;

        // Addon bridge
        bool        addonEnabled = true;
        bool        addonUseSystemChannel = false;   // legacy visible-chat fallback
        uint32      addonBroadcastInterval = 15;

        // Text
        std::string messageSiegeStart;
        std::string messageSiegeEnd;
        std::string messageReward;
        std::string yellWarlordSpawn;
        std::string yellsCombat;
        std::string rpScriptsAlliance;
        std::string rpScriptsHorde;

        // Playerbots
        bool   playerbotsEnabled = false;
        uint32 playerbotsMinLevel = 50;
        uint32 playerbotsMaxDefenders = 20;
        uint32 playerbotsMaxAttackers = 20;
        uint32 playerbotsRespawnDelay = 30;
        uint32 playerbotsOrderCooldown = 8;
    };

    extern Config g_Config;

    // -------------------------------------------------------------------------
    // Config / setup (CitySiegeConfig.cpp)
    // -------------------------------------------------------------------------

    void LoadConfiguration();
    void ResetCityDefaults();
    std::vector<std::string> SplitString(std::string const& source, char delimiter);

    // -------------------------------------------------------------------------
    // Route + leader resolution (CitySiegePathing.cpp)
    // -------------------------------------------------------------------------

    // Reads the throne coordinates back from the creature table so every city
    // targets its real leader without hand-placed coordinates.
    void ResolveLeaderPositions();

    // Builds (and caches) the marching route for a city. `pathOwner` must be a
    // creature standing in the city's map; it supplies the navmesh filter.
    bool BuildCityRoute(CityData& city, Creature* pathOwner, std::string* report = nullptr);

    // Makes sure a city has a usable route, generating one via a throwaway probe
    // unit if needed. Safe to call before any siege units exist.
    bool EnsureCityRoute(CityData& city, Map* map, bool force = false, std::string* report = nullptr);

    // Route currently in force for a city, honouring the configured route mode.
    std::vector<Position> const& GetCityRoute(CityData const& city);

    void ClearCityRoutes();

    // -------------------------------------------------------------------------
    // Siege lifecycle (CitySiegeEvent.cpp)
    // -------------------------------------------------------------------------

    void StartSiege(int32 cityId = -1, ChatHandler* reporter = nullptr);
    void EndSiege(SiegeEvent& event, int32 forcedWinnerTeam = -1);
    void UpdateSieges(uint32 diff);
    void ShutdownAllSieges();

    SiegeEvent* FindSiegeForCity(CityId cityId);
    SiegeEvent* FindSiegeForUnit(ObjectGuid guid, bool* isAttacker = nullptr);

    void RecordDamage(Unit* attacker, Unit* victim, uint32 damage);
    void RecordHealing(Unit* healer, Unit* target, uint32 amount);
    void RecordKill(Unit* killer, Unit* victim);
    void ScaleBracketDamage(Unit* attacker, Unit* victim, float& damage);

    void DespawnSiegeUnits(SiegeEvent& event);

    // -------------------------------------------------------------------------
    // Movement contract shared between the siege event and the unit AI
    // -------------------------------------------------------------------------

    // Base id for MovePoint orders. The route index is added to it, so
    // MovementInform can tell which waypoint a unit just reached.
    constexpr uint32 SIEGE_POINT_ID_BASE = 41000;

    constexpr float SIEGE_ARRIVE_DIST   = 10.0f;   // close enough to a waypoint
    constexpr float SIEGE_ENGAGE_DIST   = 45.0f;   // charge the leader from here

    // Lays a rank out in rows centred on the line of march.
    FormationSlot MakeFormationSlot(uint8 rank, uint32 index, uint32 total, bool attacker);

    // Projects a formation slot onto walkable ground beside `anchor`.
    Position PlaceFormationSlot(Map* map, Position const& anchor, float fx, float fy,
                                FormationSlot const& slot, float jitter);

    // Unit vector from `from` toward `to`, defaulting to due east when they coincide.
    void HeadingBetween(Position const& from, Position const& to, float& fx, float& fy);

    // Where the garrison digs in, as an index into the route.
    uint32 DefenderHoldIndex(std::vector<Position> const& route);

    // Helpers shared with the command script
    CityData* FindCityByName(std::string const& name);
    char const* StageName(SiegeStage stage);
    void SendMapDataToPlayer(Player* player, CityId cityId);
    void BroadcastSiegeData(SiegeEvent const& event, std::string const& messageType);
    uint8 ComputeSiegeLevel(CityData const& city);
    void ApplyCreatureScaling(Creature* creature, uint8 level, float healthMult, float damageMult);

    // -------------------------------------------------------------------------
    // Script registration
    // -------------------------------------------------------------------------

    void AddCitySiegeCommandScripts();
}

#endif // CITY_SIEGE_H
