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

#include "Creature.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "Map.h"
#include "PathGenerator.h"
// DatabaseEnv.h only forward-declares ResultSet; the definition lives here.
#include "QueryResult.h"
#include "StringFormat.h"

#include <cmath>

namespace CitySiege
{
    namespace
    {
        // A single findPath() call is capped at MAX_PATH_LENGTH polygons, so a
        // long march has to be stitched together from several legs. Each leg
        // resumes from the furthest point the previous one could actually reach.
        constexpr float LEG_ARRIVAL_TOLERANCE = 8.0f;   // close enough to the throne
        constexpr float LEG_MIN_PROGRESS      = 3.0f;   // below this a leg is stalled
        constexpr float NODE_MERGE_DISTANCE   = 6.0f;   // drop nodes this close together
        constexpr float NODE_MAX_SEGMENT      = 200.0f; // subdivide longer hops
        constexpr float THRONE_TRIM_RADIUS    = 12.0f;  // final approach is a charge

        std::vector<Position> const g_EmptyRoute;

        float Distance3D(Position const& a, Position const& b)
        {
            float dx = a.GetPositionX() - b.GetPositionX();
            float dy = a.GetPositionY() - b.GetPositionY();
            float dz = a.GetPositionZ() - b.GetPositionZ();
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        // Splits hops that are longer than the creature pathfinder can smooth in
        // one go, so units never fall back to a straight line over the terrain.
        void SubdivideLongSegments(std::vector<Position>& nodes, Position const& start)
        {
            std::vector<Position> result;
            result.reserve(nodes.size() * 2);

            Position previous = start;
            for (Position const& node : nodes)
            {
                float distance = Distance3D(previous, node);
                if (distance > NODE_MAX_SEGMENT)
                {
                    uint32 pieces = uint32(std::ceil(distance / NODE_MAX_SEGMENT));
                    for (uint32 i = 1; i < pieces; ++i)
                    {
                        float t = float(i) / float(pieces);
                        result.emplace_back(
                            previous.GetPositionX() + (node.GetPositionX() - previous.GetPositionX()) * t,
                            previous.GetPositionY() + (node.GetPositionY() - previous.GetPositionY()) * t,
                            previous.GetPositionZ() + (node.GetPositionZ() - previous.GetPositionZ()) * t,
                            0.0f);
                    }
                }

                result.push_back(node);
                previous = node;
            }

            nodes.swap(result);
        }

        // Keeps the corridor corners but throws away the near-duplicates that
        // Detour emits around tile seams.
        void MergeCloseNodes(std::vector<Position>& nodes, Position const& start)
        {
            std::vector<Position> result;
            result.reserve(nodes.size());

            Position previous = start;
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                bool isLast = (i + 1 == nodes.size());
                if (!isLast && Distance3D(previous, nodes[i]) < NODE_MERGE_DISTANCE)
                    continue;

                result.push_back(nodes[i]);
                previous = nodes[i];
            }

            nodes.swap(result);
        }

        // Evenly thins the route when a very convoluted city produces more nodes
        // than the configured budget. Start and end are always preserved.
        void EnforceNodeBudget(std::vector<Position>& nodes, uint32 budget)
        {
            if (budget < 2 || nodes.size() <= budget)
                return;

            std::vector<Position> result;
            result.reserve(budget);

            double step = double(nodes.size() - 1) / double(budget - 1);
            for (uint32 i = 0; i < budget; ++i)
                result.push_back(nodes[size_t(std::llround(step * i))]);

            nodes.swap(result);
        }
    }

    // -------------------------------------------------------------------------

    void ResolveLeaderPositions()
    {
        if (!g_Config.autoDetectLeader)
        {
            LOG_INFO("module.citysiege", "[City Siege] Leader auto-detection disabled; using configured coordinates.");
            return;
        }

        for (CityData& city : g_Cities)
        {
            if (city.leaderPositionPinned || !city.leaderEntry)
                continue;

            QueryResult result = WorldDatabase.Query(
                "SELECT position_x, position_y, position_z FROM creature "
                "WHERE map = {} AND (id1 = {} OR id2 = {} OR id3 = {}) LIMIT 1",
                city.mapId, city.leaderEntry, city.leaderEntry, city.leaderEntry);

            if (!result)
            {
                LOG_WARN("module.citysiege",
                         "[City Siege] {}: no spawn found for leader entry {} on map {}. "
                         "Falling back to the coordinates in mod_city_siege.conf.",
                         city.name, city.leaderEntry, city.mapId);
                continue;
            }

            Field* fields = result->Fetch();
            city.leader.Relocate(fields[0].Get<float>(), fields[1].Get<float>(), fields[2].Get<float>());
            city.leaderPositionResolved = true;

            LOG_INFO("module.citysiege", "[City Siege] {}: throne located at ({:.2f}, {:.2f}, {:.2f}) from creature entry {}.",
                     city.name, city.leader.GetPositionX(), city.leader.GetPositionY(),
                     city.leader.GetPositionZ(), city.leaderEntry);
        }
    }

    // -------------------------------------------------------------------------

    bool BuildCityRoute(CityData& city, Creature* pathOwner, std::string* report)
    {
        auto finish = [&city, report](bool success, std::string diagnostic)
        {
            city.routeDiagnostic = std::move(diagnostic);
            if (report)
                *report = city.routeDiagnostic;
            return success;
        };

        city.routeAttempted = true;
        city.autoRoute.clear();

        if (g_Config.routeMode == ROUTE_DIRECT)
        {
            city.routeSource = ROUTE_SRC_DIRECT;
            return finish(true, "Route mode is 'direct'; units charge straight at the throne.");
        }

        if (g_Config.routeMode == ROUTE_MANUAL)
        {
            city.routeSource = city.manualRoute.empty() ? ROUTE_SRC_DIRECT : ROUTE_SRC_MANUAL;
            return finish(!city.manualRoute.empty(),
                          Acore::StringFormat("Route mode is 'manual'; {} configured waypoint(s) in use.",
                                              city.manualRoute.size()));
        }

        if (!pathOwner || !pathOwner->IsInWorld())
        {
            city.routeSource = city.manualRoute.empty() ? ROUTE_SRC_DIRECT : ROUTE_SRC_MANUAL;
            return finish(false, "No pathfinding reference unit available.");
        }

        Position cursor = city.muster;
        Position const destination = city.leader;

        std::vector<Position> raw;
        bool reachedThrone = false;
        std::string diagnostic;

        for (uint32 leg = 0; leg < g_Config.routeMaxLegs; ++leg)
        {
            PathGenerator generator(pathOwner);
            // Corridor corners rather than a 4-yard smooth path: one call then
            // covers a whole district instead of ~300 yards, and consecutive
            // corners are guaranteed to be connected by walkable navmesh.
            generator.SetUseStraightPath(true);
            generator.SetSlopeCheck(false);

            if (!generator.CalculatePath(cursor.GetPositionX(), cursor.GetPositionY(), cursor.GetPositionZ(),
                                         destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(),
                                         false))
            {
                diagnostic = Acore::StringFormat("Leg {} rejected: invalid map coordinates.", leg + 1);
                break;
            }

            PathType type = generator.GetPathType();

            if (type & PATHFIND_NOT_USING_PATH)
            {
                diagnostic = "Server has no mmaps for this area - install/enable mmaps to use automatic routing.";
                break;
            }

            if (type & (PATHFIND_NOPATH | PATHFIND_SHORTCUT))
            {
                diagnostic = Acore::StringFormat("Leg {} failed: navmesh reported no usable path.", leg + 1);
                break;
            }

            Movement::PointsArray const& points = generator.GetPath();
            if (points.size() < 2)
            {
                diagnostic = Acore::StringFormat("Leg {} produced a degenerate path.", leg + 1);
                break;
            }

            // points[0] is always the leg's start position.
            for (size_t i = 1; i < points.size(); ++i)
                raw.emplace_back(points[i].x, points[i].y, points[i].z, 0.0f);

            G3D::Vector3 const& actualEnd = generator.GetActualEndPosition();
            Position legEnd(actualEnd.x, actualEnd.y, actualEnd.z, 0.0f);

            float progress = Distance3D(cursor, legEnd);
            cursor = legEnd;

            if (Distance3D(cursor, destination) <= LEG_ARRIVAL_TOLERANCE || !(type & PATHFIND_INCOMPLETE))
            {
                reachedThrone = true;
                break;
            }

            if (progress < LEG_MIN_PROGRESS)
            {
                diagnostic = Acore::StringFormat(
                    "Route stalled {:.0f} yards short of the throne after {} leg(s).",
                    Distance3D(cursor, destination), leg + 1);
                break;
            }

            if (leg + 1 == g_Config.routeMaxLegs)
            {
                diagnostic = Acore::StringFormat(
                    "Hit the {}-leg limit {:.0f} yards short of the throne; raise CitySiege.Route.MaxLegs.",
                    g_Config.routeMaxLegs, Distance3D(cursor, destination));
            }
        }

        if (!reachedThrone || raw.empty())
        {
            city.routeSource = city.manualRoute.empty() ? ROUTE_SRC_DIRECT : ROUTE_SRC_MANUAL;

            std::string message = diagnostic.empty() ? "Automatic route generation failed." : diagnostic;
            message += city.manualRoute.empty()
                ? " Falling back to a direct march."
                : Acore::StringFormat(" Falling back to {} configured waypoint(s).", city.manualRoute.size());

            LOG_WARN("module.citysiege", "[City Siege] {}: {}", city.name, message);
            return finish(false, std::move(message));
        }

        // Trim the tail so the army stops fighting the pathfinder once it is on
        // top of the throne, then tidy the node spacing.
        while (!raw.empty() && Distance3D(raw.back(), destination) < THRONE_TRIM_RADIUS)
            raw.pop_back();
        raw.push_back(destination);

        MergeCloseNodes(raw, city.muster);
        SubdivideLongSegments(raw, city.muster);
        EnforceNodeBudget(raw, g_Config.routeMaxNodes);

        city.autoRoute = std::move(raw);
        city.routeSource = ROUTE_SRC_AUTO;

        float length = 0.0f;
        Position previous = city.muster;
        for (Position const& node : city.autoRoute)
        {
            length += Distance3D(previous, node);
            previous = node;
        }

        std::string message = Acore::StringFormat(
            "Generated {} waypoint(s) covering {:.0f} yards from the muster point to the throne.",
            city.autoRoute.size(), length);

        LOG_INFO("module.citysiege", "[City Siege] {}: {}", city.name, message);

        if (g_Config.debug)
            for (size_t i = 0; i < city.autoRoute.size(); ++i)
                LOG_INFO("module.citysiege", "[City Siege]   {} WP{}: ({:.2f}, {:.2f}, {:.2f})",
                         city.name, i + 1, city.autoRoute[i].GetPositionX(),
                         city.autoRoute[i].GetPositionY(), city.autoRoute[i].GetPositionZ());

        return finish(true, std::move(message));
    }

    // -------------------------------------------------------------------------

    bool EnsureCityRoute(CityData& city, Map* map, bool force, std::string* report)
    {
        if (force)
        {
            city.autoRoute.clear();
            city.routeAttempted = false;
            city.routeSource = ROUTE_SRC_NONE;
        }
        else if (city.routeAttempted && (g_Config.routeMode != ROUTE_AUTO || !city.autoRoute.empty()))
        {
            if (report)
                *report = city.routeDiagnostic.empty()
                    ? Acore::StringFormat("Using the cached route ({} node(s)).", GetCityRoute(city).size())
                    : city.routeDiagnostic;
            return !GetCityRoute(city).empty() || g_Config.routeMode == ROUTE_DIRECT;
        }

        if (!map)
        {
            if (report)
                *report = "Map is not loaded, so no route can be generated.";
            return false;
        }

        // Route generation needs a unit to seed the navmesh filter with the same
        // movement capabilities the war host will have. Borrow one for a moment.
        uint32 probeEntry = (city.owner == TEAM_ALLIANCE)
            ? g_Config.creatureHordeMinion : g_Config.creatureAllianceMinion;

        map->LoadGrid(city.muster.GetPositionX(), city.muster.GetPositionY());

        Creature* probe = map->SummonCreature(probeEntry, city.muster);
        if (!probe)
        {
            if (report)
                *report = "Could not place a pathfinding probe at the muster point.";
            LOG_ERROR("module.citysiege", "[City Siege] {}: could not summon a pathfinding probe (entry {}).",
                      city.name, probeEntry);
            return false;
        }

        probe->SetImmuneToAll(true);
        probe->SetReactState(REACT_PASSIVE);
        probe->SetVisible(false);

        bool success = BuildCityRoute(city, probe, report);

        probe->DespawnOrUnsummon();
        return success;
    }

    // -------------------------------------------------------------------------

    std::vector<Position> const& GetCityRoute(CityData const& city)
    {
        switch (g_Config.routeMode)
        {
            case ROUTE_DIRECT:
                return g_EmptyRoute;
            case ROUTE_MANUAL:
                return city.manualRoute;
            case ROUTE_AUTO:
            default:
                if (!city.autoRoute.empty())
                    return city.autoRoute;
                return city.manualRoute;
        }
    }

    void ClearCityRoutes()
    {
        for (CityData& city : g_Cities)
        {
            city.autoRoute.clear();
            city.routeSource = ROUTE_SRC_NONE;
            city.routeAttempted = false;
            city.routeDiagnostic.clear();
        }
    }
}
