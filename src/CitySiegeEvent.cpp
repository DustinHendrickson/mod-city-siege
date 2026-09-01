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

#include "CitySiege.h"
#include "CitySiegeUnitAI.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "MiscPackets.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "StringFormat.h"
// Map::SummonCreature returns TempSummon*, which Map.h only forward-declares.
#include "TemporarySummon.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

#ifdef MOD_PLAYERBOTS
#include "AiObjectContext.h"
#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
#include "RandomPlayerbotMgr.h"
#include "TravelMgr.h"
#endif

namespace CitySiege
{
    namespace
    {
        // Movement bookkeeping is polled rather than event driven, so it runs on
        // its own cadence instead of on every world tick.
        constexpr uint32 MOVEMENT_TICK_SECONDS  = 1;
        constexpr uint32 RESPAWN_TICK_SECONDS   = 2;
        constexpr uint32 STATUS_ANNOUNCE_PERIOD = 300;
        // Waypoint arrival tolerance for playerbots, which are steered by the
        // playerbots travel system rather than by SiegeUnitAI.
        constexpr float  WAYPOINT_ARRIVE_DIST   = 10.0f;
        constexpr float  LEADER_SEARCH_RADIUS   = 120.0f;

        uint32 Now() { return uint32(GameTime::GetGameTime().count()); }

        Map* GetCityMap(CityData const& city) { return sMapMgr->FindMap(city.mapId, 0); }

        bool IsAllianceCity(CityData const& city) { return city.owner == TEAM_ALLIANCE; }

        TeamId AttackingTeam(CityData const& city)
        {
            return city.owner == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
        }

        char const* TeamName(TeamId team) { return team == TEAM_ALLIANCE ? "Alliance" : "Horde"; }

        uint32 HostileFactionFor(TeamId team)
        {
            return team == TEAM_ALLIANCE ? uint32(FACTION_ALLIANCE_GENERIC) : uint32(FACTION_HORDE_GENERIC);
        }

        std::string Substitute(std::string text, std::string const& city, std::string const& leader)
        {
            auto replaceAll = [&text](char const* token, std::string const& value)
            {
                size_t len = std::strlen(token);
                size_t pos = 0;
                while ((pos = text.find(token, pos)) != std::string::npos)
                {
                    text.replace(pos, len, value);
                    pos += value.length();
                }
            };

            replaceAll("{CITYNAME}", city);
            replaceAll("{CITY}", city);
            replaceAll("{LEADER}", leader.empty() ? "the city leader" : leader);
            return text;
        }

        // ---------------------------------------------------------------------
        // Creature lookup without needing a WorldObject anchor
        // ---------------------------------------------------------------------

        class EntryCheck
        {
        public:
            explicit EntryCheck(uint32 entry) : _entry(entry) { }
            bool operator()(Creature* creature) const { return creature && creature->GetEntry() == _entry; }

        private:
            uint32 _entry;
        };

        template<typename Check>
        struct EntrySearcher : Acore::ContainerInserter<Creature*>
        {
            Check& _check;

            template<typename Container>
            EntrySearcher(Container& container, Check& check)
                : Acore::ContainerInserter<Creature*>(container), _check(check) { }

            void Visit(CreatureMapType& m)
            {
                for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                    if (_check(itr->GetSource()))
                        this->Insert(itr->GetSource());
            }

            template<class NOT_INTERESTED> void Visit(GridRefMgr<NOT_INTERESTED>&) { }
        };

        Creature* ResolveCityLeader(CityData const& city, Map* map)
        {
            if (!city.leaderEntry || !map)
                return nullptr;

            // Pull the throne's grids in first: an empty capital lets them
            // unload, and a leader we cannot see must never be mistaken for a
            // leader we have killed.
            map->LoadGridsInRange(city.leader, LEADER_SEARCH_RADIUS);

            std::list<Creature*> found;
            EntryCheck check(city.leaderEntry);
            EntrySearcher<EntryCheck> searcher(found, check);
            Cell::VisitObjects(city.leader.GetPositionX(), city.leader.GetPositionY(), map, searcher, LEADER_SEARCH_RADIUS);

            Creature* best = nullptr;
            float bestDistance = LEADER_SEARCH_RADIUS + 1.0f;

            for (Creature* creature : found)
            {
                float distance = creature->GetDistance(city.leader);
                // Prefer a living leader; a corpse still counts so the siege can
                // detect that the throne has already fallen.
                if (!best || (creature->IsAlive() && !best->IsAlive()) ||
                    (creature->IsAlive() == best->IsAlive() && distance < bestDistance))
                {
                    best = creature;
                    bestDistance = distance;
                }
            }

            return best;
        }

        // ---------------------------------------------------------------------
        // Broadcasting
        // ---------------------------------------------------------------------

        void SendToCity(CityData const& city, std::string const& message, bool worldWide)
        {
            if (worldWide && g_Config.worldAnnounce)
            {
                sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message);
                return;
            }

            Map* map = GetCityMap(city);
            if (!map)
                return;

            float radius = g_Config.announceRadius ? float(g_Config.announceRadius) : 100000.0f;

            Map::PlayerList const& players = map->GetPlayers();
            for (auto itr = players.begin(); itr != players.end(); ++itr)
            {
                Player* player = itr->GetSource();
                if (!player || !player->GetSession())
                    continue;

                if (player->GetDistance(city.center) <= radius)
                    ChatHandler(player->GetSession()).SendSysMessage(message);
            }
        }

        void PlayMusicForCity(CityData const& city, uint32 soundId)
        {
            if (!g_Config.musicEnabled || !soundId)
                return;

            Map* map = GetCityMap(city);
            if (!map)
                return;

            float radius = g_Config.announceRadius ? float(g_Config.announceRadius) : 100000.0f;

            Map::PlayerList const& players = map->GetPlayers();
            for (auto itr = players.begin(); itr != players.end(); ++itr)
            {
                Player* player = itr->GetSource();
                if (!player || !player->GetSession())
                    continue;

                if (player->GetDistance(city.center) <= radius)
                    player->SendDirectMessage(WorldPackets::Misc::PlayMusic(soundId).Write());
            }
        }

        // ---------------------------------------------------------------------
        // Weather
        // ---------------------------------------------------------------------

        void ApplySiegeWeather(CityData const& city, SiegeEvent& event)
        {
            if (!g_Config.weatherEnabled)
                return;

            Map* map = GetCityMap(city);
            if (!map)
                return;

            event.weatherZoneId = map->GetZoneId(PHASEMASK_NORMAL, city.center.GetPositionX(),
                                                 city.center.GetPositionY(), city.center.GetPositionZ());
            if (!event.weatherZoneId)
                return;

            // Weather::GetWeatherState()/GetGrade() are not reachable from a
            // module, so the pre-siege weather cannot be captured. Restoring to
            // "fine" hands control back to the regular weather system.
            event.originalWeatherType = WEATHER_STATE_FINE;
            event.originalWeatherGrade = 0.0f;
            event.weatherOverridden = true;

            map->SetZoneWeather(event.weatherZoneId, g_Config.weatherType, g_Config.weatherGrade);
        }

        void RestoreSiegeWeather(CityData const& city, SiegeEvent& event)
        {
            if (!event.weatherOverridden)
                return;

            if (Map* map = GetCityMap(city))
                map->SetZoneWeather(event.weatherZoneId, event.originalWeatherType, event.originalWeatherGrade);

            event.weatherOverridden = false;
        }

        // ---------------------------------------------------------------------
        // Unit configuration
        // ---------------------------------------------------------------------

        uint32 RespawnDelayFor(uint8 rank, bool attacker)
        {
            if (!attacker)
                return g_Config.respawnDefender;

            switch (rank)
            {
                case RANK_WARLORD:  return g_Config.respawnWarlord;
                case RANK_MINIBOSS: return g_Config.respawnMiniBoss;
                case RANK_ELITE:    return g_Config.respawnElite;
                default:            return g_Config.respawnMinion;
            }
        }

        void RankModifiers(uint8 rank, bool attacker, int32& levelOffset, float& healthMult, float& damageMult, float& scale)
        {
            if (!attacker)
            {
                levelOffset = g_Config.levelOffsetDefender;
                healthMult = g_Config.healthMultDefender;
                damageMult = g_Config.damageMultDefender;
                scale = 1.0f;
                return;
            }

            switch (rank)
            {
                case RANK_WARLORD:
                    levelOffset = g_Config.levelOffsetWarlord;
                    healthMult = g_Config.healthMultWarlord;
                    damageMult = g_Config.damageMultWarlord;
                    scale = g_Config.scaleWarlord;
                    break;
                case RANK_MINIBOSS:
                    levelOffset = g_Config.levelOffsetMiniBoss;
                    healthMult = g_Config.healthMultMiniBoss;
                    damageMult = g_Config.damageMultMiniBoss;
                    scale = g_Config.scaleMiniBoss;
                    break;
                case RANK_ELITE:
                    levelOffset = g_Config.levelOffsetElite;
                    healthMult = g_Config.healthMultElite;
                    damageMult = g_Config.damageMultElite;
                    scale = 1.0f;
                    break;
                default:
                    levelOffset = g_Config.levelOffsetMinion;
                    healthMult = g_Config.healthMultMinion;
                    damageMult = g_Config.damageMultMinion;
                    scale = 1.0f;
                    break;
            }
        }

        void GroundUnit(Creature* creature)
        {
            creature->SetDisableGravity(false);
            creature->SetCanFly(false);
            creature->SetHover(false);
            creature->RemoveUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY |
                                             MOVEMENTFLAG_FLYING | MOVEMENTFLAG_HOVER);
            creature->SetWalk(false);
        }

        // Puts a freshly summoned unit into the passive "mustering" state used
        // during the roleplay build-up.
        void PrepareForMuster(Creature* creature)
        {
            GroundUnit(creature);
            creature->SetFaction(FACTION_FRIENDLY);
            creature->SetReactState(REACT_PASSIVE);
            creature->SetImmuneToAll(true);
            creature->GetMotionMaster()->Clear(false);
            creature->GetMotionMaster()->MoveIdle();
            creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(),
                                      creature->GetPositionZ(), creature->GetOrientation());
        }

        // Flips a unit from the roleplay state into its combat faction.
        void ReleaseForBattle(Creature* creature, CityData const& city, bool attacker)
        {
            GroundUnit(creature);
            creature->SetImmuneToAll(false);

            TeamId side = attacker ? AttackingTeam(city) : city.owner;
            creature->SetFaction(HostileFactionFor(side));

            bool aggressive = attacker ? (g_Config.aggroPlayers || g_Config.aggroNPCs) : true;
            creature->SetReactState(aggressive ? REACT_AGGRESSIVE : REACT_DEFENSIVE);

            creature->SetHomePosition(creature->GetPositionX(), creature->GetPositionY(),
                                      creature->GetPositionZ(), creature->GetOrientation());
            creature->GetMotionMaster()->Clear(false);
            creature->GetMotionMaster()->MoveIdle();

            // Hand control to the unit's own AI.
            if (SiegeUnitAI* ai = GetSiegeUnitAI(creature))
                ai->BeginMarch();
        }

        float RankDepth(uint8 rank)
        {
            switch (rank)
            {
                case RANK_WARLORD:  return g_Config.formationDepthWarlord;
                case RANK_MINIBOSS: return g_Config.formationDepthMiniBoss;
                case RANK_ELITE:    return g_Config.formationDepthElite;
                default:            return g_Config.formationDepthMinion;
            }
        }

        // Lays a rank out in rows, centred on the line of march. Every unit keeps
        // this slot for the whole siege, which is what keeps the army looking
        // like ranked troops instead of a single heap of overlapping models.
        FormationSlot MakeSlot(uint8 rank, uint32 index, uint32 total, bool attacker)
        {
            FormationSlot slot;

            if (!attacker)
            {
                // The garrison forms a single wide line across the street.
                float centre = (float(total) - 1.0f) * 0.5f;
                slot.side = (float(index) - centre) * g_Config.formationSpacing;
                slot.depth = (index % 2) ? g_Config.formationRowDepth * 0.5f : 0.0f;
                return slot;
            }

            uint32 width = std::min<uint32>(std::max<uint32>(1, g_Config.formationWidth),
                                            std::max<uint32>(1, total));
            uint32 row = index / width;
            uint32 column = index % width;

            // A partly filled last row is centred on its own width rather than
            // on the full row width, so it does not sit lopsided.
            uint32 consumed = row * width;
            uint32 unitsInRow = (total > consumed) ? std::min<uint32>(width, total - consumed) : 1u;
            if (!unitsInRow)
                unitsInRow = 1;

            float centre = (float(unitsInRow) - 1.0f) * 0.5f;
            slot.side = (float(column) - centre) * g_Config.formationSpacing;
            slot.depth = RankDepth(rank) + float(row) * g_Config.formationRowDepth;

            return slot;
        }

        // Unit vector pointing from `from` toward `to`, defaulting to due east
        // when the two coincide.
        void ComputeHeading(Position const& from, Position const& to, float& fx, float& fy)
        {
            float dx = to.GetPositionX() - from.GetPositionX();
            float dy = to.GetPositionY() - from.GetPositionY();
            float length = std::sqrt(dx * dx + dy * dy);

            if (length < 0.1f)
            {
                fx = 1.0f;
                fy = 0.0f;
                return;
            }

            fx = dx / length;
            fy = dy / length;
        }

        // How far the ground under a formation slot may differ from the route
        // node before the slot is considered unusable.
        constexpr float FORMATION_GROUND_TOLERANCE = 4.0f;

        // Projects a formation slot onto walkable ground beside `anchor`.
        //
        // This matters more than it looks: PointMovementGenerator only follows a
        // generated path when it produced more than two points, and otherwise
        // straight-lines to the destination. A slot hanging over a canal or off a
        // ledge is not on the navmesh, so the unit flies at it. The offset is
        // therefore shrunk until the ground beneath it lines up with the node,
        // and collapses onto the node itself if nothing beside it is walkable -
        // the node came out of the navmesh corridor, so it is always valid.
        Position PlaceSlot(Map* map, Position const& anchor, float fx, float fy,
                           FormationSlot const& slot, float jitter)
        {
            // Right-hand normal of the heading.
            float rx = fy;
            float ry = -fx;
            float orientation = std::atan2(fy, fx);
            float anchorZ = anchor.GetPositionZ();

            for (float factor : { 1.0f, 0.6f, 0.3f })
            {
                float x = anchor.GetPositionX() + (rx * slot.side - fx * slot.depth) * factor
                        + frand(-jitter, jitter);
                float y = anchor.GetPositionY() + (ry * slot.side - fy * slot.depth) * factor
                        + frand(-jitter, jitter);

                if (!map)
                    return Position(x, y, anchorZ, orientation);

                float ground = map->GetHeight(x, y, anchorZ + 4.0f, true, 25.0f);
                if (ground > INVALID_HEIGHT && std::fabs(ground - anchorZ) <= FORMATION_GROUND_TOLERANCE)
                    return Position(x, y, ground + 0.5f, orientation);
            }

            return Position(anchor.GetPositionX(), anchor.GetPositionY(), anchorZ, orientation);
        }

        struct SideCounts
        {
            uint32 attackersAlive = 0;
            uint32 defendersAlive = 0;
        };

        SideCounts CountLivingUnits(SiegeEvent const& event, Map* map)
        {
            SideCounts counts;
            if (!map)
                return counts;

            for (auto const& pair : event.units)
            {
                Creature* creature = map->GetCreature(pair.first);
                if (!creature || !creature->IsAlive())
                    continue;

                if (pair.second.attacker)
                    ++counts.attackersAlive;
                else
                    ++counts.defendersAlive;
            }

            return counts;
        }

        // Where the garrison digs in, as an index into the route.
        uint32 HoldIndexFor(std::vector<Position> const& route)
        {
            if (route.empty())
                return 0;

            return std::min<uint32>(uint32(float(route.size()) * g_Config.defenderHoldFraction),
                                    uint32(route.size() - 1));
        }
    }

    // -------------------------------------------------------------------------
    // Movement contract shared with SiegeUnitAI
    // -------------------------------------------------------------------------

    FormationSlot MakeFormationSlot(uint8 rank, uint32 index, uint32 total, bool attacker)
    {
        return MakeSlot(rank, index, total, attacker);
    }

    Position PlaceFormationSlot(Map* map, Position const& anchor, float fx, float fy,
                                FormationSlot const& slot, float jitter)
    {
        return PlaceSlot(map, anchor, fx, fy, slot, jitter);
    }

    void HeadingBetween(Position const& from, Position const& to, float& fx, float& fy)
    {
        ComputeHeading(from, to, fx, fy);
    }

    uint32 DefenderHoldIndex(std::vector<Position> const& route)
    {
        return HoldIndexFor(route);
    }

    // -------------------------------------------------------------------------
    // Public helpers
    // -------------------------------------------------------------------------

    char const* StageName(SiegeStage stage)
    {
        switch (stage)
        {
            case STAGE_MUSTER:  return "Muster";
            case STAGE_ASSAULT: return "Assault";
            case STAGE_BREACH:  return "Breach";
            case STAGE_THRONE:  return "Throne";
            default:            return "Finished";
        }
    }

    CityData* FindCityByName(std::string const& name)
    {
        std::string needle = name;
        std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
        needle.erase(std::remove(needle.begin(), needle.end(), ' '), needle.end());

        for (CityData& city : g_Cities)
        {
            std::string candidate = city.name;
            std::transform(candidate.begin(), candidate.end(), candidate.begin(), ::tolower);
            if (candidate == needle)
                return &city;
        }

        return nullptr;
    }

    SiegeEvent* FindSiegeForCity(CityId cityId)
    {
        for (SiegeEvent& event : g_ActiveSieges)
            if (event.active && event.cityId == cityId)
                return &event;

        return nullptr;
    }

    SiegeEvent* FindSiegeForUnit(ObjectGuid guid, bool* isAttacker)
    {
        for (SiegeEvent& event : g_ActiveSieges)
        {
            auto itr = event.units.find(guid);
            if (itr != event.units.end())
            {
                if (isAttacker)
                    *isAttacker = itr->second.attacker;
                return &event;
            }
        }

        return nullptr;
    }

    uint8 ComputeSiegeLevel(CityData const& city)
    {
        uint32 fallback = std::clamp<uint32>(g_Config.levelFallback, g_Config.levelMin, g_Config.levelMax);

        if (!g_Config.levelScalingEnabled)
            return uint8(std::clamp<uint32>(g_Config.levelFallback, 1, 83));

        Map* map = GetCityMap(city);
        if (!map)
            return uint8(fallback);

        std::vector<uint32> levels;
        float radius = float(g_Config.levelSampleRadius);

        Map::PlayerList const& players = map->GetPlayers();
        for (auto itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player || !player->IsInWorld() || player->IsGameMaster())
                continue;

            if (player->GetLevel() < g_Config.minimumLevel)
                continue;

            if (player->GetDistance(city.center) <= radius)
                levels.push_back(player->GetLevel());
        }

        if (levels.empty())
            return uint8(fallback);

        // Weight toward the stronger half of the crowd so a couple of low level
        // alts parked at the bank do not trivialise the whole event.
        std::sort(levels.begin(), levels.end());
        size_t upperHalf = levels.size() / 2;
        uint64 sum = 0;
        size_t count = 0;
        for (size_t i = upperHalf; i < levels.size(); ++i)
        {
            sum += levels[i];
            ++count;
        }

        uint32 average = count ? uint32(sum / count) : fallback;
        return uint8(std::clamp<uint32>(average, g_Config.levelMin, g_Config.levelMax));
    }

    void ApplyCreatureScaling(Creature* creature, uint8 level, float healthMult, float damageMult)
    {
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        if (!info)
            return;

        level = uint8(std::clamp<uint32>(level, 1, 83));
        creature->SetLevel(level);

        // Creature::SetLevel only changes the displayed level; the base stats
        // have to be re-derived by hand or the unit keeps its template numbers.
        CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, info->unit_class);
        if (!stats)
            return;

        uint32 health = std::max<uint32>(1, uint32(stats->GenerateHealth(info) * std::max(0.1f, healthMult)));
        creature->SetCreateHealth(health);
        creature->SetMaxHealth(health);
        creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(health));

        uint32 mana = stats->GenerateMana(info);
        creature->SetCreateMana(mana);
        creature->SetMaxPower(POWER_MANA, mana);
        creature->SetPower(POWER_MANA, mana);
        creature->SetStatFlatModifier(UNIT_MOD_MANA, BASE_VALUE, float(mana));

        float baseDamage = stats->GenerateBaseDamage(info) * std::max(0.1f, damageMult);
        for (uint8 attack = BASE_ATTACK; attack < MAX_ATTACK; ++attack)
        {
            creature->SetBaseWeaponDamage(WeaponAttackType(attack), MINDAMAGE, baseDamage);
            creature->SetBaseWeaponDamage(WeaponAttackType(attack), MAXDAMAGE, baseDamage * 1.5f);
        }

        creature->SetStatFlatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, float(stats->AttackPower) * damageMult);
        creature->SetStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, float(stats->RangedAttackPower) * damageMult);

        creature->UpdateAllStats();
        creature->SetFullHealth();
    }

    // -------------------------------------------------------------------------
    // Addon bridge
    // -------------------------------------------------------------------------

    namespace
    {
        void SendAddonPayload(Player* player, std::string const& payload)
        {
            if (!player || !player->GetSession())
                return;

            std::string full = "CitySiege\t" + payload;
            WorldPacket data;

            if (g_Config.addonUseSystemChannel)
            {
                // Legacy behaviour: visible in the chat log for anyone without
                // the addon. Only used when explicitly opted into.
                ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, full, LANG_UNIVERSAL);
            }
            else
            {
                // LANG_ADDON traffic is swallowed by the client and surfaced to
                // addons through CHAT_MSG_ADDON, so nobody sees raw protocol.
                ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, full, LANG_ADDON, CHAT_TAG_NONE,
                                             player->GetGUID(), player->GetName(),
                                             player->GetGUID(), player->GetName());
            }

            player->GetSession()->SendPacket(&data);
        }

        void BroadcastAddonPayload(CityData const& city, std::string const& payload)
        {
            Map* map = GetCityMap(city);
            if (!map)
                return;

            Map::PlayerList const& players = map->GetPlayers();
            for (auto itr = players.begin(); itr != players.end(); ++itr)
                SendAddonPayload(itr->GetSource(), payload);
        }

        std::string SerialiseRoute(CityData const& city)
        {
            std::vector<Position> const& route = GetCityRoute(city);

            std::ostringstream stream;
            stream << ":WP:" << route.size();
            stream << std::fixed << std::setprecision(2);
            for (Position const& node : route)
                stream << ':' << node.GetPositionX() << ':' << node.GetPositionY() << ':' << node.GetPositionZ();

            return stream.str();
        }
    }

    void BroadcastSiegeData(SiegeEvent const& event, std::string const& messageType)
    {
        if (!g_Config.addonEnabled || event.cityId >= CITY_MAX)
            return;

        CityData const& city = g_Cities[event.cityId];
        Map* map = GetCityMap(city);
        if (!map)
            return;

        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2);

        if (messageType == "START")
        {
            stream << "START:" << uint32(event.cityId) << ':' << TeamName(AttackingTeam(city))
                   << ':' << city.muster.GetPositionX() << ':' << city.muster.GetPositionY() << ':' << city.muster.GetPositionZ()
                   << ':' << city.leader.GetPositionX() << ':' << city.leader.GetPositionY() << ':' << city.leader.GetPositionZ()
                   << ':' << city.center.GetPositionX() << ':' << city.center.GetPositionY() << ':' << city.center.GetPositionZ();
        }
        else if (messageType == "UPDATE")
        {
            float leaderHealth = 0.0f;
            if (event.leaderGuid)
                if (Creature* leader = map->GetCreature(event.leaderGuid))
                    if (leader->IsAlive())
                        leaderHealth = leader->GetHealthPct();

            SideCounts counts = CountLivingUnits(event, map);

            uint32 now = Now();
            uint32 elapsed = now > event.startTime ? now - event.startTime : 0;
            uint32 remaining = event.endTime > now ? event.endTime - now : 0;

            stream << "UPDATE:" << uint32(event.cityId) << ':' << uint32(event.stage) + 1
                   << ':' << counts.attackersAlive << ':' << counts.defendersAlive
                   << ':' << elapsed << ':' << remaining
                   << ':' << std::setprecision(1) << leaderHealth << std::setprecision(2)
                   << ':' << (event.leaderName.empty() ? "Unknown" : event.leaderName)
                   << SerialiseRoute(city);
        }
        else if (messageType == "END")
        {
            stream << "END:" << uint32(event.cityId) << ':' << event.winnerName;
        }
        else
        {
            return;
        }

        BroadcastAddonPayload(city, stream.str());
    }

    void SendMapDataToPlayer(Player* player, CityId cityId)
    {
        if (!player || cityId >= CITY_MAX || !g_Config.addonEnabled)
            return;

        CityData const& city = g_Cities[cityId];

        std::ostringstream stream;
        stream << "MAP_DATA:" << uint32(cityId) << SerialiseRoute(city)
               << std::fixed << std::setprecision(2)
               << ":LEADER:" << city.leader.GetPositionX() << ':' << city.leader.GetPositionY()
               << ':' << city.leader.GetPositionZ();

        SendAddonPayload(player, stream.str());
    }

    // -------------------------------------------------------------------------
    // Spawning
    // -------------------------------------------------------------------------

    namespace
    {
        Creature* SummonSiegeUnit(SiegeEvent& event, CityData const& city, Map* map, uint32 entry,
                                  Position const& position, uint8 rank, bool attacker, uint32 routeIndex,
                                  FormationSlot const& slot)
        {
            if (!entry || !map)
                return nullptr;

            Creature* creature = map->SummonCreature(entry, position);
            if (!creature)
            {
                LOG_ERROR("module.citysiege", "[City Siege] {}: failed to summon creature entry {} at ({:.1f}, {:.1f}, {:.1f}).",
                          city.name, entry, position.GetPositionX(), position.GetPositionY(), position.GetPositionZ());
                return nullptr;
            }

            int32 levelOffset = 0;
            float healthMult = 1.0f, damageMult = 1.0f, scale = 1.0f;
            RankModifiers(rank, attacker, levelOffset, healthMult, damageMult, scale);

            int32 level = int32(event.siegeLevel) + levelOffset;

            // Minions come in a small level band so there is always something a
            // lower level defender can realistically bring down.
            if (attacker && rank == RANK_MINION && g_Config.minionLevelSpread)
                level -= int32(urand(0, g_Config.minionLevelSpread));

            ApplyCreatureScaling(creature, uint8(std::clamp<int32>(level, 1, 83)), healthMult, damageMult);

            if (scale != 1.0f)
                creature->SetObjectScale(scale);

            // Corpses are cleared explicitly on respawn/cleanup; a short natural
            // delay keeps the battlefield tidy if that ever slips through.
            creature->SetCorpseDelay(60);
            PrepareForMuster(creature);

            // Give the unit its own marching AI. Movement is then driven by
            // MovementInform rather than by an external poll, which is what stops
            // units from stalling in the street when the motion master ends up in
            // a state the poll did not recognise.
            creature->AIM_Initialize(new SiegeUnitAI(creature, city.id, attacker, rank, slot, routeIndex));

            SiegeUnit unit;
            unit.guid = creature->GetGUID();
            unit.entry = entry;
            unit.rank = rank;
            unit.attacker = attacker;
            unit.slot = slot;
            unit.routeIndex = routeIndex;
            event.units[unit.guid] = unit;

            return creature;
        }

        void SpawnAttackingArmy(SiegeEvent& event, CityData const& city, Map* map)
        {
            TeamId attackers = AttackingTeam(city);
            bool allianceAttacks = (attackers == TEAM_ALLIANCE);

            uint32 minionEntry   = allianceAttacks ? g_Config.creatureAllianceMinion   : g_Config.creatureHordeMinion;
            uint32 eliteEntry    = allianceAttacks ? g_Config.creatureAllianceElite    : g_Config.creatureHordeElite;
            uint32 miniBossEntry = allianceAttacks ? g_Config.creatureAllianceMiniBoss : g_Config.creatureHordeMiniBoss;

            std::vector<uint32> const& warlordPool = allianceAttacks ? g_Config.allianceWarlords : g_Config.hordeWarlords;
            uint32 warlordEntry = warlordPool.empty() ? miniBossEntry : warlordPool[urand(0, warlordPool.size() - 1)];

            // The whole host forms up facing the city, so it starts spread across
            // ranks rather than stacked on the muster point.
            std::vector<Position> const& route = GetCityRoute(city);
            float fx = 0.0f, fy = 0.0f;
            ComputeHeading(city.muster, route.empty() ? city.leader : route.front(), fx, fy);

            struct Wave { uint32 entry; uint8 rank; uint32 count; };
            Wave const waves[] =
            {
                { minionEntry,   RANK_MINION,   g_Config.countMinions    },
                { eliteEntry,    RANK_ELITE,    g_Config.countElites     },
                { miniBossEntry, RANK_MINIBOSS, g_Config.countMiniBosses },
                { warlordEntry,  RANK_WARLORD,  g_Config.countWarlords   }
            };

            Creature* firstWarlord = nullptr;

            for (Wave const& wave : waves)
            {
                for (uint32 i = 0; i < wave.count; ++i)
                {
                    FormationSlot slot = MakeSlot(wave.rank, i, wave.count, true);
                    Position spot = PlaceSlot(map, city.muster, fx, fy, slot, g_Config.formationJitter);

                    Creature* creature = SummonSiegeUnit(event, city, map, wave.entry, spot,
                                                         wave.rank, true, 0, slot);
                    if (creature && wave.rank == RANK_WARLORD && !firstWarlord)
                        firstWarlord = creature;
                }
            }

            // The warlord announces the assault as soon as the army forms up.
            std::vector<std::string> spawnYells = SplitString(
                Substitute(g_Config.yellWarlordSpawn, city.displayName, event.leaderName), ';');

            if (firstWarlord && !spawnYells.empty())
                firstWarlord->Yell(spawnYells[urand(0, spawnYells.size() - 1)], LANG_UNIVERSAL);

            LOG_INFO("module.citysiege", "[City Siege] {}: mustered {} attacker(s) scaled to level {}.",
                     city.name, event.units.size(), uint32(event.siegeLevel));
        }

        void SpawnGarrison(SiegeEvent& event, CityData const& city, Map* map)
        {
            if (!g_Config.defendersEnabled || !g_Config.defendersCount)
                return;

            uint32 entry = IsAllianceCity(city) ? g_Config.creatureAllianceDefender : g_Config.creatureHordeDefender;
            std::vector<Position> const& route = GetCityRoute(city);
            uint32 startIndex = uint32(route.size());

            // The garrison faces back down the route, toward the incoming host.
            float fx = 0.0f, fy = 0.0f;
            ComputeHeading(city.leader, route.empty() ? city.muster : route.back(), fx, fy);

            uint32 spawned = 0;
            for (uint32 i = 0; i < g_Config.defendersCount; ++i)
            {
                FormationSlot slot = MakeSlot(RANK_MINION, i, g_Config.defendersCount, false);
                Position spot = PlaceSlot(map, city.leader, fx, fy, slot, g_Config.formationJitter);

                if (SummonSiegeUnit(event, city, map, entry, spot, RANK_MINION, false, startIndex, slot))
                    ++spawned;
            }

            LOG_INFO("module.citysiege", "[City Siege] {}: {} city defender(s) took up position.", city.name, spawned);
        }

    }

    // -------------------------------------------------------------------------
    // Despawn / cleanup
    // -------------------------------------------------------------------------

    void DespawnSiegeUnits(SiegeEvent& event)
    {
        if (event.cityId >= CITY_MAX)
            return;

        CityData const& city = g_Cities[event.cityId];
        if (Map* map = GetCityMap(city))
            for (auto const& pair : event.units)
                if (Creature* creature = map->GetCreature(pair.first))
                    creature->DespawnOrUnsummon();

        event.units.clear();
        event.pendingRespawns.clear();
    }

    // -------------------------------------------------------------------------
    // Playerbots
    // -------------------------------------------------------------------------

    namespace
    {
#ifdef MOD_PLAYERBOTS
        bool BotIsEligible(Player* bot, TeamId team)
        {
            if (!bot || !bot->IsInWorld() || !bot->IsAlive())
                return false;
            if (bot->GetTeamId() != team)
                return false;
            if (bot->GetLevel() < g_Config.playerbotsMinLevel)
                return false;
            if (bot->IsInCombat() || bot->GetGroup())
                return false;
            if (!bot->GetMap() || bot->GetMap()->IsDungeon() || bot->GetMap()->IsBattleground())
                return false;

            return true;
        }

        void SuspendBotRoleplay(Player* bot, BotReturnPosition& record)
        {
            PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(bot);
            if (!ai)
                return;

            if (ai->HasStrategy("new rpg", BOT_STATE_NON_COMBAT))
            {
                record.rpgStrategy = "new rpg";
                ai->ChangeStrategy("-new rpg", BOT_STATE_NON_COMBAT);
            }
            else if (ai->HasStrategy("rpg", BOT_STATE_NON_COMBAT))
            {
                record.rpgStrategy = "rpg";
                ai->ChangeStrategy("-rpg", BOT_STATE_NON_COMBAT);
            }
        }

        // Sentinel node key for the throne, which is not part of the route.
        constexpr uint32 BOT_NODE_THRONE = 0xFFFFFFFFu;

        struct BotWaypoint
        {
            TravelDestination* destination = nullptr;
            WorldPosition*     position = nullptr;
        };

        // TravelTarget::setTarget takes no ownership and neither it nor the
        // destructor ever frees what it was handed, so anything allocated per
        // order leaks for the lifetime of the server. Worse, freeing it here
        // would leave bots dereferencing a dead destination, because
        // TravelTarget::addVisitors() does not null-check.
        //
        // So one destination is allocated per city node, shared by every bot,
        // reused across sieges, and simply relocated when a route is rebuilt.
        // The cost is bounded (a few dozen kilobytes for all eight cities) and
        // nothing is ever freed while a bot might still point at it.
        std::unordered_map<uint64, BotWaypoint> g_BotWaypoints;

        BotWaypoint const* AcquireBotWaypoint(uint32 cityId, uint32 nodeKey, uint32 mapId, Position const& target)
        {
            uint64 key = (uint64(cityId) << 32) | uint64(nodeKey);

            auto itr = g_BotWaypoints.find(key);
            if (itr != g_BotWaypoints.end())
            {
                // Routes can be regenerated; move the shared point rather than
                // allocating a replacement and orphaning the old one.
                itr->second.position->Relocate(target.GetPositionX(), target.GetPositionY(), target.GetPositionZ());
                return &itr->second;
            }

            BotWaypoint waypoint;
            waypoint.position = new WorldPosition(mapId, target.GetPositionX(), target.GetPositionY(),
                                                  target.GetPositionZ(), 0.0f);
            waypoint.destination = new TravelDestination(0.0f, 12.0f);
            waypoint.destination->addPoint(waypoint.position);

            return &g_BotWaypoints.emplace(key, waypoint).first->second;
        }

        // Issues a travel order, rate limited so a bot is not retargeted every
        // world tick while it is already walking somewhere sensible.
        void OrderBotTo(SiegeEvent& event, Player* bot, uint32 cityId, uint32 mapId,
                        uint32 nodeKey, Position const& target, uint32 now)
        {
            auto itr = event.botLastOrder.find(bot->GetGUID());
            if (itr != event.botLastOrder.end() && now - itr->second < g_Config.playerbotsOrderCooldown)
                return;

            PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(bot);
            if (!ai || !ai->GetAiObjectContext())
                return;

            TravelTarget* travelTarget = ai->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
            if (!travelTarget || travelTarget->isTraveling())
                return;

            BotWaypoint const* waypoint = AcquireBotWaypoint(cityId, nodeKey, mapId, target);

            // setTarget() clears the forced flag, so it has to be set after.
            travelTarget->setTarget(waypoint->destination, waypoint->position);
            travelTarget->setForced(true);

            if (!ai->HasStrategy("travel", BOT_STATE_NON_COMBAT))
                ai->ChangeStrategy("+travel", BOT_STATE_NON_COMBAT);

            event.botLastOrder[bot->GetGUID()] = now;
        }
#endif

        void RecruitPlayerbots(SiegeEvent& event, CityData const& city)
        {
#ifdef MOD_PLAYERBOTS
            if (!g_Config.playerbotsEnabled)
                return;

            auto recruit = [&event, &city](TeamId team, uint32 maximum, Position const& destination,
                                           std::vector<ObjectGuid>& out)
            {
                std::vector<Player*> eligible;
                auto allBots = sRandomPlayerbotMgr.GetAllBots();
                for (auto& pair : allBots)
                    if (BotIsEligible(pair.second, team))
                        eligible.push_back(pair.second);

                if (eligible.size() > maximum)
                {
                    Acore::Containers::RandomShuffle(eligible);
                    eligible.resize(maximum);
                }

                for (Player* bot : eligible)
                {
                    BotReturnPosition record;
                    record.guid = bot->GetGUID();
                    record.mapId = bot->GetMapId();
                    record.x = bot->GetPositionX();
                    record.y = bot->GetPositionY();
                    record.z = bot->GetPositionZ();
                    record.o = bot->GetOrientation();
                    record.wasPvPFlagged = bot->IsPvP();
                    SuspendBotRoleplay(bot, record);
                    event.botReturnPositions.push_back(record);

                    float angle = frand(0.0f, 2.0f * float(M_PI));
                    float distance = frand(0.0f, 12.0f);
                    bot->TeleportTo(city.mapId,
                                    destination.GetPositionX() + distance * std::cos(angle),
                                    destination.GetPositionY() + distance * std::sin(angle),
                                    destination.GetPositionZ(), 0.0f);

                    out.push_back(bot->GetGUID());
                }
            };

            recruit(city.owner, g_Config.playerbotsMaxDefenders, city.leader, event.defenderBots);
            recruit(AttackingTeam(city), g_Config.playerbotsMaxAttackers, city.muster, event.attackerBots);

            LOG_INFO("module.citysiege", "[City Siege] {}: recruited {} defending and {} attacking bots.",
                     city.name, event.defenderBots.size(), event.attackerBots.size());
#else
            (void)event;
            (void)city;
#endif
        }

        void ActivatePlayerbots(SiegeEvent& event, CityData const& city)
        {
#ifdef MOD_PLAYERBOTS
            if (!g_Config.playerbotsEnabled)
                return;

            std::vector<Position> const& route = GetCityRoute(city);
            uint32 now = Now();

            auto activate = [&](std::vector<ObjectGuid> const& guids, bool attacker)
            {
                for (ObjectGuid const& guid : guids)
                {
                    Player* bot = ObjectAccessor::FindPlayer(guid);
                    if (!bot || !bot->IsInWorld())
                        continue;

                    bot->SetPvP(true);
                    bot->RemovePlayerFlag(PLAYER_FLAGS_AFK);

                    if (PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(bot))
                        if (!ai->HasStrategy("pvp", BOT_STATE_NON_COMBAT))
                            ai->ChangeStrategy("+pvp", BOT_STATE_NON_COMBAT);

                    event.botRouteIndex[guid] = attacker ? 0u : uint32(route.size());

                    uint32 nodeKey = BOT_NODE_THRONE;
                    Position target = city.leader;

                    if (attacker)
                    {
                        nodeKey = route.empty() ? BOT_NODE_THRONE : 0u;
                        target = route.empty() ? city.leader : route.front();
                    }
                    else
                    {
                        // Defenders form up on the same line the NPC garrison holds.
                        nodeKey = route.empty() ? BOT_NODE_THRONE : HoldIndexFor(route);
                        target = route.empty() ? city.leader : route[HoldIndexFor(route)];
                    }

                    OrderBotTo(event, bot, uint32(city.id), city.mapId, nodeKey, target, now);
                }
            };

            activate(event.attackerBots, true);
            activate(event.defenderBots, false);
#else
            (void)event;
            (void)city;
#endif
        }

        void UpdatePlayerbots(SiegeEvent& event, CityData const& city)
        {
#ifdef MOD_PLAYERBOTS
            if (!g_Config.playerbotsEnabled)
                return;

            uint32 now = Now();
            std::vector<Position> const& route = GetCityRoute(city);

            // Deaths -> respawn queue
            auto queueDeaths = [&](std::vector<ObjectGuid> const& guids, bool defender)
            {
                for (ObjectGuid const& guid : guids)
                {
                    Player* bot = ObjectAccessor::FindPlayer(guid);
                    if (!bot || !bot->IsInWorld() || bot->IsAlive())
                        continue;

                    auto known = std::find_if(event.deadBots.begin(), event.deadBots.end(),
                                              [&guid](BotRespawnEntry const& e) { return e.guid == guid; });
                    if (known != event.deadBots.end())
                        continue;

                    BotRespawnEntry entry;
                    entry.guid = guid;
                    entry.deathTime = now;
                    entry.defender = defender;
                    event.deadBots.push_back(entry);
                }
            };

            queueDeaths(event.defenderBots, true);
            queueDeaths(event.attackerBots, false);

            // Respawns
            for (auto itr = event.deadBots.begin(); itr != event.deadBots.end();)
            {
                if (now - itr->deathTime < g_Config.playerbotsRespawnDelay)
                {
                    ++itr;
                    continue;
                }

                Player* bot = ObjectAccessor::FindPlayer(itr->guid);
                if (!bot || !bot->IsInWorld())
                {
                    ++itr;
                    continue;
                }

                Position const& anchor = itr->defender ? city.leader : city.muster;

                if (!bot->IsAlive())
                {
                    bot->ResurrectPlayer(1.0f);
                    bot->SpawnCorpseBones();
                }

                bot->RemovePlayerFlag(PLAYER_FLAGS_AFK);
                bot->SetPvP(true);

                float angle = frand(0.0f, 2.0f * float(M_PI));
                float distance = frand(0.0f, 10.0f);
                bot->TeleportTo(city.mapId,
                                anchor.GetPositionX() + distance * std::cos(angle),
                                anchor.GetPositionY() + distance * std::sin(angle),
                                anchor.GetPositionZ(), 0.0f);

                event.botRouteIndex[itr->guid] = itr->defender ? uint32(route.size()) : 0u;
                itr = event.deadBots.erase(itr);
            }

            // Marching orders
            auto march = [&](std::vector<ObjectGuid> const& guids, bool attacker)
            {
                for (ObjectGuid const& guid : guids)
                {
                    Player* bot = ObjectAccessor::FindPlayer(guid);
                    if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat())
                        continue;

                    uint32& index = event.botRouteIndex[guid];
                    uint32 nodeKey = BOT_NODE_THRONE;
                    Position target = city.leader;

                    if (attacker)
                    {
                        if (index < route.size())
                        {
                            target = route[index];
                            if (bot->GetDistance(target) <= WAYPOINT_ARRIVE_DIST * 2.0f)
                                ++index;

                            nodeKey = index < route.size() ? index : BOT_NODE_THRONE;
                            target = index < route.size() ? route[index] : city.leader;
                        }
                        else
                        {
                            nodeKey = BOT_NODE_THRONE;
                            target = city.leader;
                        }
                    }
                    else
                    {
                        if (route.empty())
                        {
                            nodeKey = BOT_NODE_THRONE;
                            target = city.leader;
                        }
                        else
                        {
                            uint32 holdIndex = HoldIndexFor(route);
                            nodeKey = holdIndex;
                            target = route[holdIndex];
                        }

                        // Already on the line - hold it.
                        if (bot->GetDistance(target) <= WAYPOINT_ARRIVE_DIST * 2.0f)
                            continue;
                    }

                    OrderBotTo(event, bot, uint32(city.id), city.mapId, nodeKey, target, now);
                }
            };

            march(event.attackerBots, true);
            march(event.defenderBots, false);
#else
            (void)event;
            (void)city;
#endif
        }

        void ReleasePlayerbots(SiegeEvent& event)
        {
#ifdef MOD_PLAYERBOTS
            if (!g_Config.playerbotsEnabled)
                return;

            for (BotReturnPosition const& record : event.botReturnPositions)
            {
                Player* bot = ObjectAccessor::FindPlayer(record.guid);
                if (!bot || !bot->IsInWorld() || bot->IsBeingTeleported())
                    continue;

                if (bot->GetMap() && (bot->GetMap()->IsDungeon() || bot->GetMap()->IsRaid() ||
                                      bot->GetMap()->IsBattleground() || bot->GetMap()->IsBattleArena()))
                    continue;

                bot->CombatStop(true);
                bot->SetPvP(record.wasPvPFlagged);

                if (!record.rpgStrategy.empty())
                    if (PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(bot))
                        ai->ChangeStrategy("+" + record.rpgStrategy, BOT_STATE_NON_COMBAT);

                if (!bot->IsAlive())
                {
                    bot->ResurrectPlayer(1.0f);
                    bot->SpawnCorpseBones();
                }

                bot->TeleportTo(record.mapId, record.x, record.y, record.z, record.o);
            }
#endif
            event.defenderBots.clear();
            event.attackerBots.clear();
            event.botReturnPositions.clear();
            event.deadBots.clear();
            event.botRouteIndex.clear();
            event.botLastOrder.clear();
        }
    }

    // -------------------------------------------------------------------------
    // Contribution tracking
    // -------------------------------------------------------------------------

    namespace
    {
        // Resolves the player who should get credit for a unit's actions.
        Player* CreditedPlayer(Unit* unit)
        {
            if (!unit)
                return nullptr;

            if (Player* player = unit->ToPlayer())
                return player;

            if (Unit* owner = unit->GetCharmerOrOwner())
                return owner->ToPlayer();

            return nullptr;
        }

        Contribution* TouchContribution(SiegeEvent& event, Player* player)
        {
            if (!player || player->IsGameMaster())
                return nullptr;

            Contribution& record = event.contributions[player->GetGUID()];
            record.name = player->GetName();
            record.level = player->GetLevel();
            record.team = player->GetTeamId();
            record.lastActive = Now();
            return &record;
        }

        // Multiplier applied so an under-levelled defender is neither deleted in
        // one hit nor completely unable to dent the army.
        float BracketMultiplier(Unit const* siegeUnit, Unit const* player, bool playerIsAttacking)
        {
            if (!g_Config.bracketScalingEnabled || !siegeUnit || !player)
                return 1.0f;

            int32 creatureLevel = int32(siegeUnit->GetLevel());
            int32 playerLevel = int32(player->GetLevel());
            int32 gap = creatureLevel - playerLevel - int32(g_Config.bracketFreeLevels);

            if (gap <= 0 || creatureLevel <= 0 || playerLevel <= 0)
                return 1.0f;

            float ratio = float(playerLevel) / float(creatureLevel);

            if (playerIsAttacking)
            {
                // Boost so the low level player's contribution stays meaningful.
                return std::clamp(1.0f / std::max(0.05f, ratio), 1.0f, g_Config.bracketMaxDamageDealt);
            }

            // Soften the hits the army lands on them.
            return std::clamp(ratio * ratio, g_Config.bracketMinDamageTaken, 1.0f);
        }
    }

    void RecordDamage(Unit* attacker, Unit* victim, uint32 damage)
    {
        if (g_ActiveSieges.empty() || !attacker || !victim || !damage)
            return;

        for (SiegeEvent& event : g_ActiveSieges)
        {
            if (!event.active)
                continue;

            if (!event.units.count(victim->GetGUID()) && victim->GetGUID() != event.leaderGuid)
                continue;

            if (Contribution* record = TouchContribution(event, CreditedPlayer(attacker)))
                record->damage += damage;

            return;
        }
    }

    void RecordHealing(Unit* healer, Unit* target, uint32 amount)
    {
        if (g_ActiveSieges.empty() || !healer || !target || !amount)
            return;

        Player* healerPlayer = CreditedPlayer(healer);
        if (!healerPlayer)
            return;

        for (SiegeEvent& event : g_ActiveSieges)
        {
            if (!event.active || event.cityId >= CITY_MAX)
                continue;

            CityData const& city = g_Cities[event.cityId];
            if (healerPlayer->GetMapId() != city.mapId)
                continue;

            // Only count healing done inside the siege zone, so field medics get
            // credit without rewarding people healing across the continent.
            if (healerPlayer->GetDistance(city.center) > float(g_Config.levelSampleRadius))
                continue;

            if (Contribution* record = TouchContribution(event, healerPlayer))
                record->healing += amount;

            return;
        }
    }

    void RecordKill(Unit* killer, Unit* victim)
    {
        if (g_ActiveSieges.empty() || !killer || !victim)
            return;

        for (SiegeEvent& event : g_ActiveSieges)
        {
            if (!event.active)
                continue;

            auto itr = event.units.find(victim->GetGUID());
            bool victimIsSiegeUnit = (itr != event.units.end());
            bool victimIsLeader = (victim->GetGUID() == event.leaderGuid);

            if (victimIsSiegeUnit || victimIsLeader)
            {
                if (victimIsSiegeUnit && itr->second.attacker)
                    ++event.attackerKills;

                if (Contribution* record = TouchContribution(event, CreditedPlayer(killer)))
                    ++record->kills;

                return;
            }

            // A siege unit killed a player.
            if (event.units.count(killer->GetGUID()) && victim->IsPlayer())
            {
                ++event.defenderKills;
                return;
            }
        }
    }

    void ScaleBracketDamage(Unit* attacker, Unit* victim, float& damage)
    {
        if (!g_Config.bracketScalingEnabled || g_ActiveSieges.empty() || !attacker || !victim)
            return;

        for (SiegeEvent& event : g_ActiveSieges)
        {
            if (!event.active)
                continue;

            bool attackerIsSiege = event.units.count(attacker->GetGUID()) != 0;
            bool victimIsSiege = event.units.count(victim->GetGUID()) != 0;

            if (attackerIsSiege && !victimIsSiege)
            {
                if (Player* player = victim->ToPlayer())
                    damage *= BracketMultiplier(attacker, player, false);
                return;
            }

            if (victimIsSiege && !attackerIsSiege)
            {
                if (Player* player = CreditedPlayer(attacker))
                    damage *= BracketMultiplier(victim, player, true);
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Rewards
    // -------------------------------------------------------------------------

    namespace
    {
        void GrantRewards(SiegeEvent& event, CityData const& city, TeamId winningTeam)
        {
            if (!g_Config.rewardsEnabled)
                return;

            uint32 now = Now();

            std::vector<std::pair<ObjectGuid, Contribution>> ranked;
            ranked.reserve(event.contributions.size());
            for (auto const& pair : event.contributions)
                if (pair.second.Score() >= g_Config.participationThreshold &&
                    now - pair.second.lastActive <= g_Config.participationWindow)
                    ranked.push_back(pair);

            std::sort(ranked.begin(), ranked.end(),
                      [](auto const& a, auto const& b) { return a.second.Score() > b.second.Score(); });

            uint32 rewarded = 0;

            for (auto const& pair : ranked)
            {
                Player* player = ObjectAccessor::FindPlayer(pair.first);
                if (!player || !player->GetSession() || !player->IsInWorld())
                    continue;

                if (player->GetLevel() < g_Config.minimumLevel)
                    continue;

                bool won = (player->GetTeamId() == winningTeam);
                if (!won && !g_Config.rewardLosingSide)
                    continue;

                float factor = won ? 1.0f : g_Config.rewardLoserFactor;
                if (factor <= 0.0f)
                    continue;

                uint32 level = player->GetLevel();
                uint32 honor = uint32(float(g_Config.rewardHonor) * factor);
                uint32 gold = uint32((float(g_Config.rewardGoldBase) + float(g_Config.rewardGoldPerLevel) * level) * factor);
                uint32 experience = g_Config.rewardXpEnabled
                    ? uint32(float(g_Config.rewardXpPerLevel) * level * factor) : 0;

                if (honor)
                    player->RewardHonor(nullptr, 1, int32(honor));

                if (gold)
                    player->ModifyMoney(int32(gold));

                if (experience && level < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
                    player->GiveXP(experience, nullptr);

                if (won && g_Config.rewardItemId)
                    player->AddItem(g_Config.rewardItemId, g_Config.rewardItemCount);

                std::string message = Substitute(g_Config.messageReward, city.displayName, event.leaderName);
                ChatHandler(player->GetSession()).SendSysMessage(message);

                std::string experienceText = experience
                    ? Acore::StringFormat(", |cffFFD700{} XP|r", experience) : std::string();
                std::string consolation = won
                    ? std::string() : std::string(" |cff888888(consolation - your side did not hold the field)|r");

                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cff00ff00[City Siege]|r Rewards: |cffFFD700{} honor|r, |cffFFD700{}g {}s {}c|r{}{}",
                    honor, gold / 10000, (gold % 10000) / 100, gold % 100, experienceText, consolation);

                ++rewarded;
            }

            if (g_Config.announceTopContributors && !ranked.empty())
            {
                std::string honourRoll = "|cffFFD700[City Siege]|r Most valuable combatants at " + city.displayName + ": ";
                for (size_t i = 0; i < ranked.size() && i < 3; ++i)
                {
                    if (i)
                        honourRoll += ", ";
                    honourRoll += Acore::StringFormat("{} ({} kills)", ranked[i].second.name, ranked[i].second.kills);
                }

                SendToCity(city, honourRoll, true);
            }

            LOG_INFO("module.citysiege", "[City Siege] {}: rewarded {} participant(s) of {} tracked.",
                     city.name, rewarded, event.contributions.size());
        }
    }

    // -------------------------------------------------------------------------
    // Siege lifecycle
    // -------------------------------------------------------------------------

    namespace
    {
        CityData* SelectRandomCity()
        {
            std::vector<CityData*> candidates;

            for (CityData& city : g_Cities)
            {
                auto enabled = g_Config.cityEnabled.find(city.name);
                if (enabled == g_Config.cityEnabled.end() || !enabled->second)
                    continue;

                // A city never hosts two sieges at once, whatever
                // AllowMultipleCities says - that flag is about parallel sieges
                // in *different* cities.
                if (FindSiegeForCity(city.id))
                    continue;

                candidates.push_back(&city);
            }

            if (candidates.empty())
                return nullptr;

            return candidates[urand(0, candidates.size() - 1)];
        }

        void BuildRoleplayScript(SiegeEvent& event, CityData const& city)
        {
            std::string const& source = (AttackingTeam(city) == TEAM_ALLIANCE)
                ? g_Config.rpScriptsAlliance : g_Config.rpScriptsHorde;

            std::vector<std::string> scripts = SplitString(source, '|');
            if (scripts.empty())
                return;

            std::string const& chosen = scripts[urand(0, scripts.size() - 1)];
            for (std::string const& line : SplitString(chosen, ';'))
                event.rpScript.push_back(Substitute(line, city.displayName, event.leaderName));
        }
    }

    void StartSiege(int32 cityId, ChatHandler* reporter)
    {
        auto report = [reporter](std::string const& text)
        {
            if (reporter)
                reporter->SendSysMessage(text);
        };

        if (!g_Config.enabled)
        {
            report("City Siege is disabled in the configuration.");
            return;
        }

        if (!g_Config.allowMultipleCities)
        {
            for (SiegeEvent const& event : g_ActiveSieges)
            {
                if (event.active)
                {
                    report("Another siege is already running and CitySiege.AllowMultipleCities is off.");
                    return;
                }
            }
        }

        CityData* city = nullptr;
        if (cityId >= 0 && cityId < int32(CITY_MAX))
        {
            city = &g_Cities[cityId];

            auto enabled = g_Config.cityEnabled.find(city->name);
            if (enabled == g_Config.cityEnabled.end() || !enabled->second)
            {
                report(Acore::StringFormat("{} is disabled in mod_city_siege.conf.", city->name));
                return;
            }

            if (FindSiegeForCity(city->id))
            {
                report(Acore::StringFormat("{} is already under siege.", city->name));
                return;
            }
        }
        else
        {
            city = SelectRandomCity();
        }

        if (!city)
        {
            report("No city is available for a siege right now.");
            LOG_INFO("module.citysiege", "[City Siege] No available cities for a siege event.");
            return;
        }

        Map* map = GetCityMap(*city);
        if (!map)
        {
            report(Acore::StringFormat("Map {} for {} is not loaded.", city->mapId, city->name));
            LOG_ERROR("module.citysiege", "[City Siege] Map {} not loaded for {}.", city->mapId, city->name);
            return;
        }

        uint32 now = Now();

        SiegeEvent event;
        event.cityId = city->id;
        event.active = true;
        event.startTime = now;
        event.endTime = now + g_Config.eventDuration;
        event.stage = STAGE_MUSTER;
        event.lastRpTime = now;
        event.lastYellTime = now;
        event.lastStatusAnnounce = now;
        event.lastMovementTick = now;
        event.lastRespawnTick = now;
        event.lastMusicPulse = now;
        event.siegeLevel = ComputeSiegeLevel(*city);

        // Locate the city leader up front: it decides the win condition, gives
        // the roleplay lines a name to shout at, and anchors the route.
        if (Creature* leader = ResolveCityLeader(*city, map))
        {
            event.leaderGuid = leader->GetGUID();
            event.leaderName = leader->GetName();

            if (!city->leaderPositionPinned)
                city->leader.Relocate(leader->GetPositionX(), leader->GetPositionY(), leader->GetPositionZ());
        }
        else
        {
            LOG_WARN("module.citysiege",
                     "[City Siege] {}: city leader (entry {}) could not be found near the throne. "
                     "The siege will run to the timer instead of ending on their death.",
                     city->name, city->leaderEntry);
        }

        BuildRoleplayScript(event, *city);

        // Build the route before anything spawns: the army forms up facing the
        // first leg of the march, so it needs to know where that leg goes.
        EnsureCityRoute(*city, map);

        g_ActiveSieges.push_back(std::move(event));
        SiegeEvent& live = g_ActiveSieges.back();

        SpawnAttackingArmy(live, *city, map);
        SpawnGarrison(live, *city, map);
        RecruitPlayerbots(live, *city);
        ApplySiegeWeather(*city, live);

        std::string announcement = Substitute(g_Config.messageSiegeStart, city->displayName, live.leaderName);
        SendToCity(*city, announcement, true);
        SendToCity(*city, Acore::StringFormat(
            "|cffff0000[City Siege]|r A {} war host is massing outside {}. The assault begins in {} seconds - "
            "all levels are welcome, the defenders' strength is matched to whoever answers the call.",
            TeamName(AttackingTeam(*city)), city->displayName, g_Config.musterDuration), true);

        PlayMusicForCity(*city, g_Config.musicRp);
        BroadcastSiegeData(live, "START");

        report(Acore::StringFormat("Siege of {} started (army level {}, {} route node(s)).",
                                   city->name, uint32(live.siegeLevel), GetCityRoute(*city).size()));

        LOG_INFO("module.citysiege", "[City Siege] Siege of {} began. Army level {}, leader '{}', {} route node(s).",
                 city->name, uint32(live.siegeLevel), live.leaderName.empty() ? "unknown" : live.leaderName,
                 GetCityRoute(*city).size());
    }

    void EndSiege(SiegeEvent& event, int32 forcedWinnerTeam)
    {
        if (!event.active || event.cityId >= CITY_MAX)
            return;

        event.active = false;
        event.stage = STAGE_FINISHED;

        CityData const& city = g_Cities[event.cityId];
        Map* map = GetCityMap(city);

        bool defendersHeld = true;
        Creature* leader = (map && event.leaderGuid) ? map->GetCreature(event.leaderGuid) : nullptr;

        if (map && event.leaderGuid && !leader)
            leader = ResolveCityLeader(city, map);

        // With no leader to check at all, the defenders keep their city: a
        // bookkeeping failure must not hand the attackers a victory.
        if (leader)
            defendersHeld = leader->IsAlive();

        TeamId winner = defendersHeld ? city.owner : AttackingTeam(city);
        if (forcedWinnerTeam >= 0 && forcedWinnerTeam <= 1)
        {
            winner = TeamId(forcedWinnerTeam);
            defendersHeld = (winner == city.owner);
        }

        event.winnerName = TeamName(winner);

        DespawnSiegeUnits(event);
        RestoreSiegeWeather(city, event);
        ReleasePlayerbots(event);

        std::string outcome = defendersHeld
            ? Acore::StringFormat("|cff00ff00[City Siege]|r The {} held {}! The war host has been driven back.",
                                  TeamName(city.owner), city.displayName)
            : Acore::StringFormat("|cffff0000[City Siege]|r The {} has taken {}! {} has fallen.",
                                  TeamName(AttackingTeam(city)), city.displayName,
                                  event.leaderName.empty() ? "The throne" : event.leaderName);

        SendToCity(city, Substitute(g_Config.messageSiegeEnd, city.displayName, event.leaderName), true);
        SendToCity(city, outcome, true);
        SendToCity(city, Acore::StringFormat(
            "|cffFFFF00[City Siege]|r Final tally: {} siege troops destroyed, {} defenders slain.",
            event.attackerKills, event.defenderKills), false);

        PlayMusicForCity(city, defendersHeld ? g_Config.musicVictory : g_Config.musicDefeat);

        GrantRewards(event, city, winner);

        // Bring the city leader back if the assault brought them down.
        if (!defendersHeld && leader && !leader->IsAlive())
            leader->Respawn();

        BroadcastSiegeData(event, "END");

        LOG_INFO("module.citysiege", "[City Siege] Siege of {} ended - {} won ({} attacker deaths, {} defender deaths).",
                 city.name, TeamName(winner), event.attackerKills, event.defenderKills);
    }

    void ShutdownAllSieges()
    {
        for (SiegeEvent& event : g_ActiveSieges)
        {
            if (!event.active)
                continue;

            event.active = false;

            if (event.cityId < CITY_MAX)
                RestoreSiegeWeather(g_Cities[event.cityId], event);

            DespawnSiegeUnits(event);
            ReleasePlayerbots(event);
        }

        g_ActiveSieges.clear();
    }

    // -------------------------------------------------------------------------
    // Per-tick work
    // -------------------------------------------------------------------------

    namespace
    {
        std::vector<Creature*> CollectSpeakers(SiegeEvent const& event, Map* map)
        {
            std::vector<Creature*> speakers;

            for (auto const& pair : event.units)
            {
                if (!pair.second.attacker || pair.second.rank < RANK_MINIBOSS)
                    continue;

                if (Creature* creature = map->GetCreature(pair.first))
                    if (creature->IsAlive())
                        speakers.push_back(creature);
            }

            return speakers;
        }

        void TickMuster(SiegeEvent& event, CityData const& city, Map* map, uint32 now)
        {
            uint32 elapsed = now - event.startTime;
            uint32 remaining = g_Config.musterDuration > elapsed ? g_Config.musterDuration - elapsed : 0;
            float percent = g_Config.musterDuration
                ? float(remaining) / float(g_Config.musterDuration) * 100.0f : 0.0f;

            auto countdown = [&](bool& flag, char const* colour, char const* suffix)
            {
                if (flag)
                    return;
                flag = true;
                SendToCity(city, Acore::StringFormat("|cffff0000[City Siege]|r |c{}{} seconds|r until the assault on {}. {}",
                                                     colour, remaining, city.displayName, suffix), true);
            };

            if (percent <= 25.0f)
                countdown(event.countdown25Announced, "ffFF0000", "FINAL WARNING!");
            else if (percent <= 50.0f)
                countdown(event.countdown50Announced, "ffFF8800", "Defenders, to your posts!");
            else if (percent <= 75.0f)
                countdown(event.countdown75Announced, "ffFFFF00", "Defenders, prepare!");

            // Roleplay lines, delivered one at a time by the officers.
            if (now - event.lastRpTime >= g_Config.yellFrequency && event.rpIndex < event.rpScript.size())
            {
                event.lastRpTime = now;

                std::vector<Creature*> speakers = CollectSpeakers(event, map);
                if (!speakers.empty())
                {
                    speakers[urand(0, speakers.size() - 1)]->Yell(event.rpScript[event.rpIndex], LANG_UNIVERSAL);
                    ++event.rpIndex;
                }
            }

            if (elapsed < g_Config.musterDuration)
                return;

            // --- release the army -------------------------------------------
            event.stage = STAGE_ASSAULT;
            event.combatStartTime = now;

            for (auto& pair : event.units)
                if (Creature* creature = map->GetCreature(pair.first))
                    ReleaseForBattle(creature, city, pair.second.attacker);

            ActivatePlayerbots(event, city);

            SendToCity(city, Acore::StringFormat(
                "|cffff0000[City Siege]|r |cffFF0000THE ASSAULT ON {} HAS BEGUN!|r Every defender counts - "
                "join the fight at any level.", city.displayName), true);

            PlayMusicForCity(city, g_Config.musicCombat);
            event.lastMusicPulse = now;

            LOG_INFO("module.citysiege", "[City Siege] {}: muster over, the army is marching.", city.name);
        }


        void AdvanceStage(SiegeEvent& event, CityData const& city, std::vector<Position> const& route)
        {
            if (event.stage != STAGE_ASSAULT && event.stage != STAGE_BREACH)
                return;

            if (route.empty())
                return;

            Map* map = sMapMgr->FindMap(city.mapId, 0);
            if (!map)
                return;

            // The vanguard is the attacker furthest along the route. Progress
            // lives in each unit's AI now, so read it from there.
            uint32 vanguard = 0;
            for (auto const& pair : event.units)
            {
                if (!pair.second.attacker)
                    continue;

                if (SiegeUnitAI* ai = GetSiegeUnitAI(map->GetCreature(pair.first)))
                    vanguard = std::max(vanguard, ai->GetRouteIndex());
            }

            float progress = float(vanguard) / float(route.size());

            if (event.stage == STAGE_ASSAULT && progress >= 0.34f)
            {
                event.stage = STAGE_BREACH;
                SendToCity(city, Acore::StringFormat(
                    "|cffff0000[City Siege]|r The gates of {} are breached - the enemy is in the streets!",
                    city.displayName), false);
            }
            else if (event.stage == STAGE_BREACH && progress >= 0.85f)
            {
                event.stage = STAGE_THRONE;
                SendToCity(city, Acore::StringFormat(
                    "|cffff0000[City Siege]|r They have reached {}! Defend the throne room!",
                    event.leaderName.empty() ? "the throne room" : event.leaderName), true);
            }
        }

        void TickCombat(SiegeEvent& event, CityData& city, Map* map, uint32 now)
        {
            std::vector<Position> const& route = GetCityRoute(city);
            Creature* leader = event.leaderGuid ? map->GetCreature(event.leaderGuid) : nullptr;

            // --- stage progress ------------------------------------------------
            // Movement itself is handled by each unit's SiegeUnitAI; all this
            // tick has to do is watch how far the vanguard has pushed.
            if (now - event.lastMovementTick >= MOVEMENT_TICK_SECONDS)
            {
                event.lastMovementTick = now;
                AdvanceStage(event, city, route);
            }

            // --- deaths and respawns -----------------------------------------
            if (now - event.lastRespawnTick >= RESPAWN_TICK_SECONDS)
            {
                event.lastRespawnTick = now;

                if (g_Config.respawnEnabled)
                {
                    // A unit counts as fallen once its creature is dead, and also
                    // once the object is gone entirely - a corpse can decay well
                    // before a warlord's five minute respawn timer elapses, so the
                    // entry is taken from our own record rather than the creature.
                    for (auto& pair : event.units)
                    {
                        Creature* creature = map->GetCreature(pair.first);
                        if (creature && creature->IsAlive())
                            continue;

                        bool queued = std::any_of(event.pendingRespawns.begin(), event.pendingRespawns.end(),
                                                  [&pair](RespawnEntry const& e) { return e.guid == pair.first; });
                        if (queued)
                            continue;

                        RespawnEntry entry;
                        entry.guid = pair.first;
                        entry.entry = pair.second.entry;
                        entry.deathTime = now;
                        entry.rank = pair.second.rank;
                        entry.attacker = pair.second.attacker;
                        entry.slot = pair.second.slot;
                        event.pendingRespawns.push_back(entry);
                    }

                    for (auto itr = event.pendingRespawns.begin(); itr != event.pendingRespawns.end();)
                    {
                        if (now - itr->deathTime < RespawnDelayFor(itr->rank, itr->attacker))
                        {
                            ++itr;
                            continue;
                        }

                        // Reinforcements muster in the same file they died in, so
                        // the formation stays intact across the whole battle.
                        Position const& anchor = itr->attacker ? city.muster : city.leader;
                        Position const& facing = itr->attacker
                            ? (route.empty() ? city.leader : route.front())
                            : (route.empty() ? city.muster : route.back());

                        float fx = 0.0f, fy = 0.0f;
                        ComputeHeading(anchor, facing, fx, fy);
                        Position spot = PlaceSlot(map, anchor, fx, fy, itr->slot, g_Config.formationJitter);

                        // Clear the corpse before the replacement walks in.
                        if (Creature* corpse = map->GetCreature(itr->guid))
                            corpse->DespawnOrUnsummon();

                        uint32 routeIndex = itr->attacker ? 0u : uint32(route.size());

                        event.units.erase(itr->guid);

                        if (Creature* replacement = SummonSiegeUnit(event, city, map, itr->entry, spot,
                                                                    itr->rank, itr->attacker, routeIndex,
                                                                    itr->slot))
                            ReleaseForBattle(replacement, city, itr->attacker);

                        itr = event.pendingRespawns.erase(itr);
                    }
                }
                else
                {
                    // No respawns: tidy corpses so the city is not littered.
                    for (auto itr = event.units.begin(); itr != event.units.end();)
                    {
                        Creature* creature = map->GetCreature(itr->first);
                        if (creature && !creature->IsAlive())
                        {
                            creature->DespawnOrUnsummon(30s);
                            itr = event.units.erase(itr);
                        }
                        else if (!creature)
                        {
                            itr = event.units.erase(itr);
                        }
                        else
                        {
                            ++itr;
                        }
                    }
                }
            }

            // --- taunts -------------------------------------------------------
            if (now - event.lastYellTime >= g_Config.yellFrequency)
            {
                event.lastYellTime = now;

                std::vector<Creature*> speakers = CollectSpeakers(event, map);
                std::vector<std::string> yells = SplitString(
                    Substitute(g_Config.yellsCombat, city.displayName, event.leaderName), ';');

                if (!speakers.empty() && !yells.empty())
                    speakers[urand(0, speakers.size() - 1)]->Yell(yells[urand(0, yells.size() - 1)], LANG_UNIVERSAL);
            }

            // --- leader health callouts --------------------------------------
            if (leader && leader->IsAlive())
            {
                uint8 healthPct = uint8(leader->GetHealthPct());
                for (uint8 threshold : { uint8(50), uint8(25), uint8(10) })
                {
                    if (healthPct <= threshold && event.leaderHealthAnnounced > threshold)
                    {
                        event.leaderHealthAnnounced = threshold;
                        SendToCity(city, Acore::StringFormat(
                            "|cffff0000[City Siege]|r {} is at |cffFF0000{}%|r health! Rally to the throne!",
                            event.leaderName.empty() ? "The city leader" : event.leaderName, uint32(threshold)), true);
                        break;
                    }
                }
            }

            // --- music loop ---------------------------------------------------
            if (g_Config.musicLoopSeconds && now - event.lastMusicPulse >= g_Config.musicLoopSeconds)
            {
                event.lastMusicPulse = now;
                PlayMusicForCity(city, g_Config.musicCombat);
            }

            // --- periodic status ----------------------------------------------
            if (now - event.lastStatusAnnounce >= STATUS_ANNOUNCE_PERIOD)
            {
                event.lastStatusAnnounce = now;

                uint32 remaining = event.endTime > now ? event.endTime - now : 0;
                SideCounts counts = CountLivingUnits(event, map);

                std::string status = Acore::StringFormat(
                    "|cffff0000[City Siege]|r {}: {} minute(s) left, {} attackers still standing, {} defenders holding.",
                    city.displayName, remaining / 60, counts.attackersAlive, counts.defendersAlive);

                if (leader && leader->IsAlive())
                    status += Acore::StringFormat(" {} at {:.0f}% health.",
                                                  event.leaderName.empty() ? "The leader" : event.leaderName,
                                                  leader->GetHealthPct());

                SendToCity(city, status, false);
            }

            UpdatePlayerbots(event, city);
        }
    }

    void UpdateSieges(uint32 /*diff*/)
    {
        uint32 now = Now();

        for (SiegeEvent& event : g_ActiveSieges)
        {
            if (!event.active || event.cityId >= CITY_MAX)
                continue;

            CityData& city = g_Cities[event.cityId];
            Map* map = GetCityMap(city);
            if (!map)
                continue;

            if (g_Config.addonEnabled && now - event.lastAddonBroadcast >= g_Config.addonBroadcastInterval)
            {
                event.lastAddonBroadcast = now;
                BroadcastSiegeData(event, "UPDATE");
            }

            if (event.stage == STAGE_MUSTER)
            {
                TickMuster(event, city, map, now);
                continue;
            }

            TickCombat(event, city, map, now);

            // Win condition: the leader falls. A missing creature object is not
            // proof of death - the throne's grid may simply have unloaded - so
            // re-locate before declaring the city taken.
            if (event.leaderGuid)
            {
                Creature* leader = map->GetCreature(event.leaderGuid);
                if (!leader)
                {
                    leader = ResolveCityLeader(city, map);
                    if (leader)
                        event.leaderGuid = leader->GetGUID();
                }

                if (leader && !leader->IsAlive())
                {
                    EndSiege(event, int32(AttackingTeam(city)));
                    continue;
                }
            }

            // Win condition: the defenders run out the clock.
            if (now >= event.endTime)
                EndSiege(event, int32(city.owner));
        }

        // Retire finished events after a grace period so late lookups still work.
        g_ActiveSieges.erase(
            std::remove_if(g_ActiveSieges.begin(), g_ActiveSieges.end(),
                           [now](SiegeEvent const& event)
                           {
                               return !event.active && now > event.endTime + 60;
                           }),
            g_ActiveSieges.end());

        if (g_Config.enabled && g_NextSiegeTime && now >= g_NextSiegeTime)
        {
            StartSiege();
            uint32 delay = urand(g_Config.timerMin, g_Config.timerMax);
            g_NextSiegeTime = now + delay;

            LOG_INFO("module.citysiege", "[City Siege] Next automatic siege in {} minute(s).", delay / 60);
        }
    }
}
