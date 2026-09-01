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
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "StringFormat.h"
// Map::SummonCreature returns TempSummon*, which Map.h only forward-declares.
#include "TemporarySummon.h"

#include <algorithm>
#include <cmath>

namespace CitySiege
{
    namespace
    {
        // Route generation runs in two passes.
        //
        // Pass 1 asks the navmesh for the *corridor* between the muster point and
        // the throne, using findStraightPath. Those corner points say which way to
        // go around buildings, but their heights are not usable: findStraightPath
        // does not produce ground heights, so NormalizePath() snaps each corner to
        // whatever surface happens to be nearest. Over Stormwind's canals that
        // picks the water floor for one corner and the bridge deck for the next,
        // which is how a route ended up with a 45 yard vertical step across four
        // horizontal yards.
        //
        // Pass 2 therefore throws the corner heights away and treats the corners
        // purely as XY aim points, walking between them with the *smooth*
        // pathfinder. Smooth paths follow the mesh surface, so every point they
        // produce is genuinely walkable - which is what the movement generators
        // need, because a destination that is off the navmesh makes them abandon
        // pathfinding and straight-line through the terrain instead.

        constexpr float LEG_ARRIVAL_TOLERANCE = 6.0f;    // close enough to an aim point
        constexpr float LEG_MIN_PROGRESS      = 2.5f;    // below this a leg is stalled
        constexpr float SMOOTH_LEG_MAX        = 120.0f;  // straight-line reach per step
        constexpr float SMOOTH_LEG_MIN        = 6.0f;    // give up shortening below this
        constexpr uint32 SHORTEN_ATTEMPTS     = 8;       // halvings before a step is failed
        constexpr float THRONE_TRIM_RADIUS    = 10.0f;   // final approach is a charge
        constexpr float LOOP_REMOVAL_RADIUS   = 18.0f;   // de-loop the coarse corridor
        constexpr float TURN_COSINE           = 0.94f;   // ~20 degrees counts as a turn
        constexpr uint32 WALK_STEP_GUARD      = 96;      // sub-steps toward one aim point
        constexpr float SHORTCUT_GAIN         = 6.0f;    // yards saved before a node is dropped
        constexpr uint32 SMOOTH_PASSES        = 4;       // shortcut sweeps over the route
        constexpr uint32 ROUTE_PUSH_ATTEMPTS  = 250;     // pushes toward the throne before giving up

        std::vector<Position> const g_EmptyRoute;

        float Distance2D(Position const& a, Position const& b)
        {
            float dx = a.GetPositionX() - b.GetPositionX();
            float dy = a.GetPositionY() - b.GetPositionY();
            return std::sqrt(dx * dx + dy * dy);
        }

        float Distance3D(Position const& a, Position const& b)
        {
            float dx = a.GetPositionX() - b.GetPositionX();
            float dy = a.GetPositionY() - b.GetPositionY();
            float dz = a.GetPositionZ() - b.GetPositionZ();
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        void Heading(Position const& from, Position const& to, float& fx, float& fy)
        {
            float dx = to.GetPositionX() - from.GetPositionX();
            float dy = to.GetPositionY() - from.GetPositionY();
            float length = std::sqrt(dx * dx + dy * dy);

            if (length < 0.01f)
            {
                fx = 1.0f;
                fy = 0.0f;
                return;
            }

            fx = dx / length;
            fy = dy / length;
        }

        // ---------------------------------------------------------------------
        // Pass 1: corridor corners (XY guidance only)
        // ---------------------------------------------------------------------

        std::vector<Position> BuildCorridor(Creature* pathOwner, Position const& start,
                                            Position const& destination, std::string& diagnostic)
        {
            std::vector<Position> corners;
            Position cursor = start;

            for (uint32 leg = 0; leg < g_Config.routeMaxLegs; ++leg)
            {
                PathGenerator generator(pathOwner);
                generator.SetUseStraightPath(true);
                generator.SetSlopeCheck(false);   // no-op with straight paths

                if (!generator.CalculatePath(cursor.GetPositionX(), cursor.GetPositionY(), cursor.GetPositionZ(),
                                             destination.GetPositionX(), destination.GetPositionY(),
                                             destination.GetPositionZ(), false))
                {
                    diagnostic = Acore::StringFormat("Corridor leg {} rejected: invalid map coordinates.", leg + 1);
                    break;
                }

                PathType type = generator.GetPathType();

                if (type & PATHFIND_NOT_USING_PATH)
                {
                    diagnostic = "Server has no mmaps for this area - install/enable mmaps to use automatic routing.";
                    corners.clear();
                    break;
                }

                if (type & (PATHFIND_NOPATH | PATHFIND_SHORTCUT))
                {
                    diagnostic = Acore::StringFormat("Corridor leg {} failed: navmesh reported no usable path.", leg + 1);
                    break;
                }

                Movement::PointsArray const& points = generator.GetPath();
                for (size_t i = 1; i < points.size(); ++i)
                    corners.emplace_back(points[i].x, points[i].y, points[i].z, 0.0f);

                G3D::Vector3 const& actualEnd = generator.GetActualEndPosition();
                Position legEnd(actualEnd.x, actualEnd.y, actualEnd.z, 0.0f);

                float progress = Distance2D(cursor, legEnd);
                cursor = legEnd;

                if (Distance2D(cursor, destination) <= LEG_ARRIVAL_TOLERANCE || !(type & PATHFIND_INCOMPLETE))
                    return corners;

                if (progress < LEG_MIN_PROGRESS)
                {
                    diagnostic = Acore::StringFormat(
                        "Corridor stalled {:.0f} yards short of the throne after {} leg(s).",
                        Distance2D(cursor, destination), leg + 1);
                    break;
                }

                if (leg + 1 == g_Config.routeMaxLegs)
                    diagnostic = Acore::StringFormat(
                        "Hit the {}-leg limit {:.0f} yards short of the throne; raise CitySiege.Route.MaxLegs.",
                        g_Config.routeMaxLegs, Distance2D(cursor, destination));
            }

            return corners;
        }

        // Chained legs can double back on themselves where one leg ends and the
        // next re-approaches. Cut any excursion that returns near an earlier point.
        void RemoveLoops(std::vector<Position>& points)
        {
            for (size_t i = 0; i + 2 < points.size(); ++i)
            {
                for (size_t j = points.size() - 1; j > i + 1; --j)
                {
                    if (Distance2D(points[i], points[j]) <= LOOP_REMOVAL_RADIUS)
                    {
                        points.erase(points.begin() + i + 1, points.begin() + j + 1);
                        break;
                    }
                }
            }
        }

        // ---------------------------------------------------------------------
        // Pass 2: walk the corridor with smooth paths
        // ---------------------------------------------------------------------

        // True if one step of a path is a gradient an army could march up.
        //
        // PathGenerator::IsWalkableClimb() is not usable here: it allows a rise
        // of sourceHeight * (1 - angle/100), so for a footman on a 30 degree
        // slope that is 1.4 yards - while smooth path steps are 4 yards apart and
        // a 30 degree ramp rises 2.3 yards over one. Its own documentation says
        // it is meant for short distances. Used on path steps it rejects ordinary
        // road ramps along with the cliffs we actually want to exclude, which is
        // what reduced route building to three corners out of ten.
        //
        // A plain gradient test separates the two cleanly and is tunable.
        bool StepIsClimbable(G3D::Vector3 const& from, G3D::Vector3 const& to)
        {
            float dx = to.x - from.x;
            float dy = to.y - from.y;
            float run = std::sqrt(dx * dx + dy * dy);
            float rise = std::fabs(to.z - from.z);

            // Stairs and ledges: almost no horizontal travel, small step up.
            if (run < 0.5f)
                return rise <= 2.0f;

            float degrees = std::atan2(rise, run) * 180.0f / float(M_PI);
            return degrees <= g_Config.routeMaxSlope;
        }

        /// Index of the first point that cannot be reached from its predecessor,
        /// or points.size() when the whole path is marchable.
        size_t FirstUnclimbableStep(Movement::PointsArray const& points)
        {
            for (size_t i = 1; i < points.size(); ++i)
                if (!StepIsClimbable(points[i - 1], points[i]))
                    return i;

            return points.size();
        }

        bool IsClimbable(Movement::PointsArray const& points)
        {
            return FirstUnclimbableStep(points) == points.size();
        }

        // One ground-hugging leg. Atomic: nothing is appended unless the whole
        // leg succeeded, so a failed attempt cannot pollute the route.
        bool SmoothLeg(Creature* pathOwner, Position const& from, Position const& to,
                       std::vector<Position>& out, Position& reached)
        {
            PathGenerator generator(pathOwner);
            generator.SetUseStraightPath(false);
            generator.SetSlopeCheck(false);   // enforced per-leg below instead

            if (!generator.CalculatePath(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(),
                                         to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false))
                return false;

            PathType type = generator.GetPathType();
            if (type & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH))
                return false;

            Movement::PointsArray const& points = generator.GetPath();
            if (points.size() < 2)
                return false;

            // Truncate at the first step the army could not march up rather than
            // discarding the leg. Everything before the cliff is perfectly good
            // route, and keeping it means the search resumes from there instead
            // of losing the ground it already covered.
            size_t usable = FirstUnclimbableStep(points);
            if (usable < 2)
                return false;

            for (size_t i = 1; i < usable; ++i)
                out.emplace_back(points[i].x, points[i].y, points[i].z, 0.0f);

            if (usable == points.size())
            {
                G3D::Vector3 const& end = generator.GetActualEndPosition();
                reached = Position(end.x, end.y, end.z, 0.0f);
            }
            else
            {
                G3D::Vector3 const& last = points[usable - 1];
                reached = Position(last.x, last.y, last.z, 0.0f);
            }

            return true;
        }

        // Re-seats a point on the surface nearest the height we are actually
        // walking at.
        //
        // Corridor corners carry whatever height NormalizePath() snapped them to,
        // which beside a road is frequently the hillside above it. Feeding that
        // height back to the pathfinder makes closestPointOnPoly pick the poly up
        // the hill, and the smooth path then faithfully walks up the mountain.
        // The corner's XY is the useful part; its Z has to be thrown away, which
        // is what this does.
        Position ReseatOnSurface(Map* map, Position const& point, float walkingZ)
        {
            float x = point.GetPositionX();
            float y = point.GetPositionY();

            if (!map)
                return Position(x, y, walkingZ, 0.0f);

            // Search downward from just above the height we are walking at, so a
            // ledge overhead cannot win over the street underfoot.
            float ground = map->GetHeight(x, y, walkingZ + 6.0f, true, 80.0f);
            if (ground > INVALID_HEIGHT)
                return Position(x, y, ground, 0.0f);

            return Position(x, y, walkingZ, 0.0f);
        }

        Position Lerp(Position const& from, Position const& to, float t)
        {
            return Position(
                from.GetPositionX() + (to.GetPositionX() - from.GetPositionX()) * t,
                from.GetPositionY() + (to.GetPositionY() - from.GetPositionY()) * t,
                from.GetPositionZ() + (to.GetPositionZ() - from.GetPositionZ()) * t,
                0.0f);
        }

        // Walks from `cursor` to `aim` in smooth-path steps.
        //
        // The step length is a *straight-line* distance, but the walk it implies
        // can be far longer - around a wall, through a gate. A smooth path that
        // overruns the pathfinder's budget is discarded wholesale and comes back
        // as a shortcut, so an over-long step fails outright rather than
        // returning a partial result. Each step therefore halves its reach until
        // one fits.
        bool WalkToward(Creature* pathOwner, Position& cursor, Position const& rawAim,
                        std::vector<Position>& dense)
        {
            Map* map = pathOwner->GetMap();

            for (uint32 guard = 0; guard < WALK_STEP_GUARD; ++guard)
            {
                // Re-seat the aim every step: as the cursor climbs a ramp or
                // drops into a district, the surface under the aim changes too.
                Position aim = ReseatOnSurface(map, rawAim, cursor.GetPositionZ());

                float remaining = Distance2D(cursor, aim);
                if (remaining <= LEG_ARRIVAL_TOLERANCE)
                    return true;

                float reach = std::min(remaining, SMOOTH_LEG_MAX);
                bool advanced = false;

                for (uint32 attempt = 0; attempt < SHORTEN_ATTEMPTS; ++attempt)
                {
                    Position step = (reach >= remaining)
                        ? aim
                        : ReseatOnSurface(map, Lerp(cursor, aim, reach / remaining), cursor.GetPositionZ());

                    std::vector<Position> leg;
                    Position reached;

                    if (SmoothLeg(pathOwner, cursor, step, leg, reached) &&
                        Distance2D(cursor, reached) >= LEG_MIN_PROGRESS)
                    {
                        dense.insert(dense.end(), leg.begin(), leg.end());
                        cursor = reached;
                        advanced = true;
                        break;
                    }

                    reach *= 0.5f;
                    if (reach < SMOOTH_LEG_MIN)
                        break;
                }

                if (!advanced)
                    return false;
            }

            return false;
        }

        // ---------------------------------------------------------------------
        // Simplification
        // ---------------------------------------------------------------------

        // Thins the dense smooth path down to marching waypoints, keeping the
        // corners where the route actually turns.
        void Simplify(std::vector<Position>& points, Position const& start, float spacing)
        {
            if (points.empty())
                return;

            std::vector<Position> result;
            result.reserve(points.size() / 4 + 4);

            Position lastKept = start;
            float lastFx = 0.0f, lastFy = 0.0f;
            bool haveHeading = false;

            for (size_t i = 0; i < points.size(); ++i)
            {
                bool isLast = (i + 1 == points.size());
                float travelled = Distance2D(lastKept, points[i]);

                float fx, fy;
                Heading(lastKept, points[i], fx, fy);
                bool turned = haveHeading && (fx * lastFx + fy * lastFy) < TURN_COSINE;

                if (isLast || travelled >= spacing || (turned && travelled >= 8.0f))
                {
                    result.push_back(points[i]);
                    lastKept = points[i];
                    lastFx = fx;
                    lastFy = fy;
                    haveHeading = true;
                }
            }

            points.swap(result);
        }

        // True if a unit can actually walk from `from` to `to` - both reachable
        // on the mesh and climbable the whole way.
        bool IsWalkable(Creature* pathOwner, Position const& from, Position const& to)
        {
            PathGenerator generator(pathOwner);
            generator.SetUseStraightPath(false);
            generator.SetSlopeCheck(false);   // enforced explicitly below

            if (!generator.CalculatePath(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(),
                                         to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false))
                return false;

            if (generator.GetPathType() & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH))
                return false;

            return IsClimbable(generator.GetPath());
        }

        // Drops waypoints that can simply be skipped.
        //
        // A raw navmesh path zigzags around the mesh triangulation, and the
        // simplify pass above deliberately keeps direction changes - so it
        // preserves those zigzags instead of removing them. Here, whenever going
        // straight from the previous node to the next one is both walkable and
        // meaningfully shorter, the node in between was only a detour and goes.
        // This is what turns a jagged mesh path into a marching route.
        void ShortcutSmooth(Creature* pathOwner, std::vector<Position>& nodes, Position const& start)
        {
            if (nodes.size() < 3)
                return;

            for (uint32 pass = 0; pass < SMOOTH_PASSES; ++pass)
            {
                bool changed = false;

                for (size_t i = 0; i + 1 < nodes.size() && nodes.size() > 2; )
                {
                    Position const& previous = (i == 0) ? start : nodes[i - 1];
                    Position const& next = nodes[i + 1];

                    float viaNode = Distance3D(previous, nodes[i]) + Distance3D(nodes[i], next);
                    float direct = Distance3D(previous, next);

                    if (viaNode - direct > SHORTCUT_GAIN && IsWalkable(pathOwner, previous, next))
                    {
                        nodes.erase(nodes.begin() + i);
                        changed = true;
                        continue;
                    }

                    ++i;
                }

                if (!changed)
                    break;
            }
        }

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

        // ---------------------------------------------------------------------
        // Verification
        // ---------------------------------------------------------------------

        // Confirms every hop is genuinely walkable. A hop that fails here is one
        // the movement generator would turn into a straight line through the air.
        uint32 CountBrokenHops(Creature* pathOwner, Position const& start,
                               std::vector<Position> const& route)
        {
            uint32 broken = 0;
            Position previous = start;

            for (Position const& node : route)
            {
                // Same test the army will face: reachable and climbable.
                if (!IsWalkable(pathOwner, previous, node))
                    ++broken;

                previous = node;
            }

            return broken;
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

        // Read spawns out of ObjectMgr rather than querying the creature table
        // directly. The column layout of that table differs between AzerothCore
        // revisions (id vs id1), but ObjectMgr has already normalised it.
        CreatureDataContainer const& spawns = sObjectMgr->GetAllCreatureData();

        for (CityData& city : g_Cities)
        {
            if (city.leaderPositionPinned || !city.leaderEntry)
                continue;

            CreatureData const* found = nullptr;

            for (auto const& pair : spawns)
            {
                CreatureData const& data = pair.second;

                if (data.mapid != city.mapId)
                    continue;

                if (data.id != city.leaderEntry && data.id2 != city.leaderEntry && data.id3 != city.leaderEntry)
                    continue;

                found = &data;
                break;
            }

            if (!found)
            {
                LOG_WARN("module.citysiege",
                         "[City Siege] {}: no spawn found for leader entry {} on map {}. "
                         "Falling back to the coordinates in mod_city_siege.conf.",
                         city.name, city.leaderEntry, city.mapId);
                continue;
            }

            city.leader.Relocate(found->posX, found->posY, found->posZ);
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

        std::string diagnostic;

        // --- pass 1: corridor shape ------------------------------------------
        std::vector<Position> corridor = BuildCorridor(pathOwner, city.muster, city.leader, diagnostic);
        RemoveLoops(corridor);

        // --- pass 2: walk it on the ground -----------------------------------
        std::vector<Position> dense;
        Position cursor = city.muster;
        uint32 aimsReached = 0;

        // Keep pushing toward the throne from wherever the walk has got to.
        //
        // A single sweep over the corridor gives up the moment one corner is
        // unreachable, even though every sub-step already taken counts as
        // progress. This retries from the new position, skipping corners that do
        // not help, until it either arrives or genuinely stops moving.
        size_t corner = 0;
        for (uint32 attempt = 0; attempt < ROUTE_PUSH_ATTEMPTS; ++attempt)
        {
            if (Distance2D(cursor, city.leader) <= THRONE_TRIM_RADIUS)
                break;

            Position before = cursor;

            if (corner < corridor.size())
            {
                if (WalkToward(pathOwner, cursor, corridor[corner], dense))
                {
                    ++aimsReached;
                    ++corner;
                }
            }
            else
            {
                WalkToward(pathOwner, cursor, city.leader, dense);
            }

            // Made no headway toward this corner - drop it and aim at the next.
            if (Distance2D(before, cursor) < LEG_MIN_PROGRESS)
            {
                if (corner < corridor.size())
                {
                    ++corner;
                    continue;
                }

                break;
            }
        }

        bool reachedThrone = Distance2D(cursor, city.leader) <= THRONE_TRIM_RADIUS * 3.0f;

        if (!reachedThrone || dense.empty())
        {
            city.routeSource = city.manualRoute.empty() ? ROUTE_SRC_DIRECT : ROUTE_SRC_MANUAL;

            std::string message = diagnostic.empty()
                ? Acore::StringFormat(
                      "Could not walk a ground path to the throne; stopped {:.0f} yards short "
                      "(corridor gave {} corner(s), {} reached, {} path point(s) collected).",
                      Distance2D(cursor, city.leader), corridor.size(), aimsReached, dense.size())
                : diagnostic;

            message += city.manualRoute.empty()
                ? " Falling back to a direct march."
                : Acore::StringFormat(" Falling back to {} configured waypoint(s).", city.manualRoute.size());

            LOG_WARN("module.citysiege", "[City Siege] {}: {}", city.name, message);
            return finish(false, std::move(message));
        }

        // Trim the tail so the army stops fighting the pathfinder once it is on
        // top of the throne, then thin the dense path into marching waypoints.
        while (!dense.empty() && Distance3D(dense.back(), city.leader) < THRONE_TRIM_RADIUS)
            dense.pop_back();
        dense.push_back(city.leader);

        Simplify(dense, city.muster, g_Config.routeNodeSpacing);
        ShortcutSmooth(pathOwner, dense, city.muster);
        RemoveLoops(dense);
        EnforceNodeBudget(dense, g_Config.routeMaxNodes);

        city.autoRoute = std::move(dense);
        city.routeSource = ROUTE_SRC_AUTO;

        // --- verify -----------------------------------------------------------
        uint32 broken = CountBrokenHops(pathOwner, city.muster, city.autoRoute);

        float length = 0.0f;
        float worstClimb = 0.0f;
        size_t worstHop = 0;
        Position previous = city.muster;

        for (size_t i = 0; i < city.autoRoute.size(); ++i)
        {
            Position const& node = city.autoRoute[i];
            length += Distance3D(previous, node);

            float climb = std::fabs(node.GetPositionZ() - previous.GetPositionZ());
            if (climb > worstClimb)
            {
                worstClimb = climb;
                worstHop = i + 1;
            }

            previous = node;
        }

        std::string message = Acore::StringFormat(
            "Generated {} waypoint(s) covering {:.0f} yards, {} unwalkable hop(s); "
            "steepest climb {:.1f}y into WP{}.",
            city.autoRoute.size(), length, broken, worstClimb, worstHop);

        if (broken)
            LOG_WARN("module.citysiege", "[City Siege] {}: {}", city.name, message);
        else
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
