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

#include "Chat.h"
#include "CommandScript.h"
#include "Configuration/Config.h"
#include "Creature.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringFormat.h"

#include <algorithm>

using namespace Acore::ChatCommands;
using namespace CitySiege;

namespace
{
    // Waypoint beacons spawned by `.citysiege showroute`, keyed by city id.
    std::unordered_map<uint32, std::vector<ObjectGuid>> g_RouteBeacons;

    constexpr uint32 BEACON_ENTRY = 15631;   // invisible spotlight, a tall visual beam

    std::string CityNameList()
    {
        std::string list;
        for (CityData const& city : g_Cities)
        {
            if (!list.empty())
                list += ", ";
            list += city.name;
        }
        return list;
    }

    CityData* ResolveCity(ChatHandler* handler, Optional<std::string> const& arg)
    {
        if (!arg)
        {
            handler->PSendSysMessage("Specify a city. Valid names: {}", CityNameList());
            return nullptr;
        }

        CityData* city = FindCityByName(*arg);
        if (!city)
            handler->PSendSysMessage("Unknown city '{}'. Valid names: {}", *arg, CityNameList());

        return city;
    }

    char const* RouteSourceName(RouteSource source)
    {
        switch (source)
        {
            case ROUTE_SRC_AUTO:   return "auto (navmesh)";
            case ROUTE_SRC_MANUAL: return "manual (config waypoints)";
            case ROUTE_SRC_DIRECT: return "direct (no waypoints)";
            default:               return "not generated yet";
        }
    }

    bool RebuildRoute(ChatHandler* handler, CityData& city)
    {
        std::string report;
        bool success = EnsureCityRoute(city, sMapMgr->FindMap(city.mapId, 0), /*force*/ true, &report);

        handler->PSendSysMessage("{}: {}", city.name, report);
        return success;
    }

    void ClearBeacons(CityData const& city)
    {
        auto itr = g_RouteBeacons.find(uint32(city.id));
        if (itr == g_RouteBeacons.end())
            return;

        if (Map* map = sMapMgr->FindMap(city.mapId, 0))
            for (ObjectGuid const& guid : itr->second)
                if (Creature* beacon = map->GetCreature(guid))
                    beacon->DespawnOrUnsummon();

        g_RouteBeacons.erase(itr);
    }
}

class citysiege_commandscript : public CommandScript
{
public:
    citysiege_commandscript() : CommandScript("citysiege_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable citySiegeCommandTable =
        {
            { "start",     HandleStart,     SEC_GAMEMASTER,    Console::No  },
            { "stop",      HandleStop,      SEC_GAMEMASTER,    Console::No  },
            { "cleanup",   HandleCleanup,   SEC_GAMEMASTER,    Console::No  },
            { "status",    HandleStatus,    SEC_PLAYER,        Console::Yes },
            { "list",      HandleList,      SEC_PLAYER,        Console::Yes },
            { "route",     HandleRoute,     SEC_GAMEMASTER,    Console::Yes },
            { "repath",    HandleRepath,    SEC_GAMEMASTER,    Console::No  },
            { "showroute", HandleShowRoute, SEC_GAMEMASTER,    Console::No  },
            { "info",      HandleInfo,      SEC_GAMEMASTER,    Console::No  },
            { "here",      HandleHere,      SEC_GAMEMASTER,    Console::No  },
            { "mapdata",   HandleMapData,   SEC_PLAYER,        Console::No  },
            { "reload",    HandleReload,    SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable commandTable =
        {
            { "citysiege", citySiegeCommandTable }
        };

        return commandTable;
    }

    // -------------------------------------------------------------------------

    static bool HandleStart(ChatHandler* handler, Optional<std::string> cityArg)
    {
        if (!cityArg)
        {
            StartSiege(-1, handler);
            return true;
        }

        CityData* city = ResolveCity(handler, cityArg);
        if (!city)
            return true;

        StartSiege(int32(city->id), handler);
        return true;
    }

    static bool HandleStop(ChatHandler* handler, Optional<std::string> cityArg, Optional<std::string> factionArg)
    {
        CityData* city = ResolveCity(handler, cityArg);
        if (!city)
            return true;

        SiegeEvent* event = FindSiegeForCity(city->id);
        if (!event)
        {
            handler->PSendSysMessage("There is no active siege in {}.", city->name);
            return true;
        }

        // Without a faction the natural outcome (is the leader still alive?)
        // decides the winner, which is what "just end it" should mean.
        int32 winner = -1;
        if (factionArg)
        {
            std::string faction = *factionArg;
            std::transform(faction.begin(), faction.end(), faction.begin(), ::tolower);

            if (faction == "alliance")
                winner = int32(TEAM_ALLIANCE);
            else if (faction == "horde")
                winner = int32(TEAM_HORDE);
            else
            {
                handler->SendSysMessage("Faction must be 'alliance' or 'horde', or omitted to use the natural result.");
                return true;
            }
        }

        EndSiege(*event, winner);
        handler->PSendSysMessage("Siege of {} ended.", city->name);
        return true;
    }

    static bool HandleCleanup(ChatHandler* handler, Optional<std::string> cityArg)
    {
        uint32 cleaned = 0;

        for (SiegeEvent& event : g_ActiveSieges)
        {
            if (!event.active)
                continue;

            if (cityArg)
            {
                CityData* city = FindCityByName(*cityArg);
                if (!city || city->id != event.cityId)
                    continue;
            }

            event.active = false;
            DespawnSiegeUnits(event);
            handler->PSendSysMessage("Removed siege forces from {}.", g_Cities[event.cityId].name);
            ++cleaned;
        }

        if (!cleaned)
            handler->SendSysMessage("There is nothing to clean up.");

        return true;
    }

    static bool HandleStatus(ChatHandler* handler, Optional<std::string>)
    {
        handler->PSendSysMessage("=== City Siege ===");
        handler->PSendSysMessage("Module: {} | Route mode: {} | Level scaling: {}",
                                 g_Config.enabled ? "enabled" : "disabled",
                                 g_Config.routeMode == ROUTE_AUTO ? "auto"
                                     : (g_Config.routeMode == ROUTE_MANUAL ? "manual" : "direct"),
                                 g_Config.levelScalingEnabled ? "on" : "off");

        uint32 now = uint32(GameTime::GetGameTime().count());
        uint32 running = 0;

        for (SiegeEvent const& event : g_ActiveSieges)
        {
            if (!event.active)
                continue;

            ++running;
            CityData const& city = g_Cities[event.cityId];
            uint32 remaining = event.endTime > now ? event.endTime - now : 0;

            handler->PSendSysMessage("  {} - stage {}, army level {}, {} minute(s) remaining",
                                     city.displayName, StageName(event.stage), uint32(event.siegeLevel), remaining / 60);

            if (Map* map = sMapMgr->FindMap(city.mapId, 0))
            {
                if (Creature* leader = event.leaderGuid ? map->GetCreature(event.leaderGuid) : nullptr)
                    handler->PSendSysMessage("    Leader: {} - {}, {:.1f}% health",
                                             leader->GetName(), leader->IsAlive() ? "alive" : "DEAD",
                                             leader->GetHealthPct());
                else
                    handler->SendSysMessage("    Leader: not found (siege will run to the timer)");
            }

            handler->PSendSysMessage("    Units tracked: {} | Kills: {} attackers / {} defenders | Participants: {}",
                                     event.units.size(), event.attackerKills, event.defenderKills,
                                     event.contributions.size());
        }

        if (!running)
            handler->SendSysMessage("  No siege is currently running.");

        if (g_Config.enabled && g_NextSiegeTime > now)
            handler->PSendSysMessage("Next automatic siege in {} minute(s).", (g_NextSiegeTime - now) / 60);

        return true;
    }

    static bool HandleList(ChatHandler* handler, Optional<std::string>)
    {
        handler->SendSysMessage("=== City Siege - cities ===");

        for (CityData const& city : g_Cities)
        {
            auto enabled = g_Config.cityEnabled.find(city.name);
            bool isEnabled = (enabled != g_Config.cityEnabled.end() && enabled->second);
            bool underSiege = FindSiegeForCity(city.id) != nullptr;

            handler->PSendSysMessage("  {} ({}) - {}{} | route: {} ({} node(s))",
                                     city.name, city.owner == TEAM_ALLIANCE ? "Alliance" : "Horde",
                                     isEnabled ? "enabled" : "disabled",
                                     underSiege ? ", UNDER SIEGE" : "",
                                     RouteSourceName(city.routeSource), GetCityRoute(city).size());
        }

        return true;
    }

    static bool HandleRoute(ChatHandler* handler, Optional<std::string> cityArg)
    {
        CityData* city = ResolveCity(handler, cityArg);
        if (!city)
            return true;

        std::vector<Position> const& route = GetCityRoute(*city);

        handler->PSendSysMessage("=== {} route ===", city->name);
        handler->PSendSysMessage("Source: {} | Nodes: {}", RouteSourceName(city->routeSource), route.size());
        handler->PSendSysMessage("Muster: ({:.2f}, {:.2f}, {:.2f})",
                                 city->muster.GetPositionX(), city->muster.GetPositionY(), city->muster.GetPositionZ());
        handler->PSendSysMessage("Throne: ({:.2f}, {:.2f}, {:.2f}) - leader entry {} ({})",
                                 city->leader.GetPositionX(), city->leader.GetPositionY(), city->leader.GetPositionZ(),
                                 city->leaderEntry,
                                 city->leaderPositionResolved ? "read from creature table" : "from config");

        if (!city->routeDiagnostic.empty())
            handler->PSendSysMessage("Last result: {}", city->routeDiagnostic);

        for (size_t i = 0; i < route.size(); ++i)
            handler->PSendSysMessage("  {}: ({:.2f}, {:.2f}, {:.2f})", i + 1,
                                     route[i].GetPositionX(), route[i].GetPositionY(), route[i].GetPositionZ());

        if (route.empty())
            handler->SendSysMessage("No route yet - run '.citysiege repath <city>' to build one.");

        return true;
    }

    static bool HandleRepath(ChatHandler* handler, Optional<std::string> cityArg)
    {
        if (cityArg && (*cityArg == "all" || *cityArg == "ALL"))
        {
            uint32 rebuilt = 0;
            for (CityData& city : g_Cities)
                if (RebuildRoute(handler, city))
                    ++rebuilt;

            handler->PSendSysMessage("Rebuilt {} of {} city route(s).", rebuilt, g_Cities.size());
            return true;
        }

        CityData* city = ResolveCity(handler, cityArg);
        if (!city)
        {
            handler->SendSysMessage("Use '.citysiege repath all' to rebuild every city.");
            return true;
        }

        ClearBeacons(*city);
        RebuildRoute(handler, *city);
        return true;
    }

    static bool HandleShowRoute(ChatHandler* handler, Optional<std::string> cityArg)
    {
        CityData* city = ResolveCity(handler, cityArg);
        if (!city)
            return true;

        if (g_RouteBeacons.count(uint32(city->id)))
        {
            ClearBeacons(*city);
            handler->PSendSysMessage("Route beacons for {} removed.", city->name);
            return true;
        }

        Map* map = sMapMgr->FindMap(city->mapId, 0);
        if (!map)
        {
            handler->PSendSysMessage("Map {} for {} is not loaded.", city->mapId, city->name);
            return true;
        }

        std::vector<Position> const& route = GetCityRoute(*city);
        if (route.empty())
        {
            handler->SendSysMessage("This city has no route. Run '.citysiege repath <city>' first.");
            return true;
        }

        std::vector<ObjectGuid> beacons;

        auto place = [&](Position const& position, float scale)
        {
            map->LoadGrid(position.GetPositionX(), position.GetPositionY());
            if (Creature* beacon = map->SummonCreature(BEACON_ENTRY, position))
            {
                beacon->SetObjectScale(scale);
                beacon->SetReactState(REACT_PASSIVE);
                beacon->SetImmuneToAll(true);
                beacon->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                beacon->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                beacons.push_back(beacon->GetGUID());
            }
        };

        place(city->muster, 3.0f);
        for (Position const& node : route)
            place(node, 2.0f);
        place(city->leader, 3.0f);

        g_RouteBeacons[uint32(city->id)] = beacons;

        handler->PSendSysMessage("Placed {} beacon(s) along the {} route. Run the command again to remove them.",
                                 beacons.size(), city->name);
        return true;
    }

    static bool HandleInfo(ChatHandler* handler, Optional<std::string>)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        Unit* selected = player->GetSelectedUnit();
        if (!selected)
        {
            handler->SendSysMessage("Select a siege unit first.");
            return true;
        }

        bool attacker = false;
        SiegeEvent* event = FindSiegeForUnit(selected->GetGUID(), &attacker);
        if (!event)
        {
            handler->SendSysMessage("That unit is not part of an active siege.");
            return true;
        }

        CityData const& city = g_Cities[event->cityId];
        SiegeUnit const& unit = event->units[selected->GetGUID()];
        std::vector<Position> const& route = GetCityRoute(city);

        Position target = (attacker && unit.routeIndex < route.size()) ? route[unit.routeIndex] : city.leader;

        handler->PSendSysMessage("|cff00ff00[City Siege]|r {} - {} in the siege of {}",
                                 selected->GetName(), attacker ? "attacker" : "defender", city.displayName);
        handler->PSendSysMessage("Rank: {} | Level: {} | Route node: {}/{}",
                                 unit.rank == RANK_WARLORD ? "warlord"
                                     : (unit.rank == RANK_MINIBOSS ? "mini-boss"
                                     : (unit.rank == RANK_ELITE ? "elite" : "minion")),
                                 uint32(selected->GetLevel()), unit.routeIndex, route.size());
        handler->PSendSysMessage("Target: ({:.1f}, {:.1f}, {:.1f}) at {:.1f} yards",
                                 target.GetPositionX(), target.GetPositionY(), target.GetPositionZ(),
                                 selected->GetDistance(target));

        return true;
    }

    static bool HandleHere(ChatHandler* handler, Optional<std::string>)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        handler->PSendSysMessage("Map {} | X={:.3f} Y={:.3f} Z={:.3f} O={:.3f} | Zone {} Area {}",
                                 player->GetMapId(), player->GetPositionX(), player->GetPositionY(),
                                 player->GetPositionZ(), player->GetOrientation(),
                                 player->GetZoneId(), player->GetAreaId());
        return true;
    }

    static bool HandleMapData(ChatHandler* handler, Optional<uint32> cityIdArg)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (!cityIdArg || *cityIdArg >= CITY_MAX)
        {
            handler->SendSysMessage("Usage: .citysiege mapdata <cityId>  (0=Stormwind ... 7=Silvermoon)");
            return true;
        }

        SendMapDataToPlayer(player, CityId(*cityIdArg));
        return true;
    }

    static bool HandleReload(ChatHandler* handler, Optional<std::string>)
    {
        sConfigMgr->Reload();
        LoadConfiguration();
        ResolveLeaderPositions();
        ClearCityRoutes();

        handler->SendSysMessage("|cff00ff00[City Siege]|r Configuration reloaded and cached routes cleared.");
        handler->SendSysMessage("Routes rebuild themselves on the next siege, or run '.citysiege repath all' now.");
        handler->PSendSysMessage("Timer {}-{} min | Duration {} min | Route mode: {}",
                                 g_Config.timerMin / 60, g_Config.timerMax / 60, g_Config.eventDuration / 60,
                                 g_Config.routeMode == ROUTE_AUTO ? "auto"
                                     : (g_Config.routeMode == ROUTE_MANUAL ? "manual" : "direct"));
        return true;
    }
};

namespace CitySiege
{
    void AddCitySiegeCommandScripts()
    {
        new citysiege_commandscript();
    }
}
