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

#include "CitySiegeUnitAI.h"

#include "Creature.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSpline.h"

#include <cmath>

namespace CitySiege
{
    namespace
    {
        constexpr uint32 WATCHDOG_INTERVAL = 2000;   // ms between idle re-checks
        constexpr uint32 SKIP_GUARD        = 4;      // waypoints skipped in one pass

        // Escalation thresholds, in consecutive watchdog ticks without moving.
        constexpr uint32 STALLS_BEFORE_NODE = 2;   // give up on the unit's own spot
        constexpr uint32 STALLS_BEFORE_SKIP = 4;   // give up on the waypoint
        constexpr float  STALL_MOVE_EPSILON = 1.5f;  // yards that count as progress
    }

    SiegeUnitAI::SiegeUnitAI(Creature* creature, CityId cityId, bool attacker, uint8 rank,
                             FormationSlot const& slot, uint32 routeIndex)
        : ScriptedAI(creature), _cityId(cityId), _attacker(attacker), _rank(rank),
          _slot(slot), _routeIndex(routeIndex)
    {
    }

    // -------------------------------------------------------------------------

    void SiegeUnitAI::BeginMarch()
    {
        _marching = true;
        _watchdog = 0;
        _stalls = 0;
        _ignoreSlot = false;
        _haveLastPosition = false;
        IssueOrder();
    }

    void SiegeUnitAI::Halt()
    {
        _marching = false;
        me->GetMotionMaster()->Clear(false);
        me->GetMotionMaster()->MoveIdle();
    }

    // -------------------------------------------------------------------------

    void SiegeUnitAI::MovementInform(uint32 type, uint32 id)
    {
        if (type != POINT_MOTION_TYPE || !_marching)
            return;

        if (id < SIEGE_POINT_ID_BASE)
            return;

        // Ignore the tail of an order we have already moved past.
        if (id - SIEGE_POINT_ID_BASE != _routeIndex)
            return;

        std::vector<Position> const& route = GetCityRoute(g_Cities[_cityId]);

        if (_attacker && _routeIndex < route.size())
            ++_routeIndex;

        // Arrived under its own power, so whatever it was caught on is behind it.
        _stalls = 0;
        _ignoreSlot = false;

        IssueOrder();
    }

    void SiegeUnitAI::EnterEvadeMode(EvadeReason why)
    {
        if (!_EnterEvadeMode(why))
            return;

        // Never walk back to the spawn point: the siege army's "home" is
        // wherever it has fought its way to.
        me->SetHomePosition(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation());

        if (_marching)
            IssueOrder();
        else
            me->GetMotionMaster()->MoveIdle();
    }

    void SiegeUnitAI::UpdateAI(uint32 diff)
    {
        if (UpdateVictim())
        {
            DoMeleeAttackIfReady();
            return;
        }

        if (!_marching)
            return;

        // Safety net for orders that never reported completion - an interrupted
        // spline, a knockback, a failed path. Cheap, and it only fires when the
        // unit is genuinely standing still.
        _watchdog += diff;
        if (_watchdog < WATCHDOG_INTERVAL)
            return;

        _watchdog = 0;

        // Has it actually got anywhere since the last tick?
        Position const current(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f);
        bool moved = !_haveLastPosition || current.GetExactDist(&_lastPosition) > STALL_MOVE_EPSILON;

        _lastPosition = current;
        _haveLastPosition = true;

        if (moved)
        {
            // Making progress: forget any escalation and let it use its slot again.
            _stalls = 0;
            _ignoreSlot = false;

            if (me->movespline->Finalized())
                IssueOrder();

            return;
        }

        OnStalled();
    }

    // Standing still. Re-issuing the same order would only send the unit back
    // into whatever it is wedged against, so widen the target instead: first
    // abandon the formation slot and head for the bare route node, then give up
    // on the node altogether and aim at the next one. Between them these clear
    // both failure modes seen in practice - a slot that turned out to be inside
    // geometry, and a waypoint tucked behind a corner the unit cannot round.
    void SiegeUnitAI::OnStalled()
    {
        ++_stalls;

        if (_stalls == STALLS_BEFORE_NODE)
        {
            _ignoreSlot = true;
        }
        else if (_stalls >= STALLS_BEFORE_SKIP)
        {
            std::vector<Position> const& route = GetCityRoute(g_Cities[_cityId]);

            if (_attacker && _routeIndex < route.size())
                ++_routeIndex;

            // Next waypoint, and a fresh chance at a slot beside it.
            _ignoreSlot = false;
            _stalls = 0;
        }

        IssueOrder();
    }

    // -------------------------------------------------------------------------

    void SiegeUnitAI::IssueOrder()
    {
        if (!_marching || !me->IsAlive() || me->IsInCombat())
            return;

        CityData const& city = g_Cities[_cityId];
        std::vector<Position> const& route = GetCityRoute(city);
        Map* map = me->GetMap();

        Position target;
        bool chargeThrone = false;

        // Walk forward past any waypoints we are already standing on, so a dense
        // stretch of route does not cost one tick per node.
        for (uint32 guard = 0; guard < SKIP_GUARD; ++guard)
        {
            Position node;
            Position previous;
            chargeThrone = false;

            if (_attacker)
            {
                if (_routeIndex < route.size())
                {
                    node = route[_routeIndex];
                    previous = _routeIndex ? route[_routeIndex - 1] : city.muster;
                }
                else
                {
                    node = city.leader;
                    previous = route.empty() ? city.muster : route.back();
                    chargeThrone = true;
                }
            }
            else if (route.empty())
            {
                node = city.leader;
                previous = city.muster;
            }
            else
            {
                // The garrison forms its line facing back down the route.
                uint32 hold = DefenderHoldIndex(route);
                node = route[hold];
                previous = (hold + 1 < route.size()) ? route[hold + 1] : city.leader;
            }

            float fx = 0.0f, fy = 0.0f;
            HeadingBetween(previous, node, fx, fy);

            // Tighten the scatter for the final push so nobody is shoved into a
            // wall in the throne room.
            FormationSlot slot = _slot;
            if (chargeThrone)
            {
                slot.side *= 0.5f;
                slot.depth *= 0.5f;
            }

            // A unit that stalled on its way to a slot heads for the bare node
            // instead. The node came out of the navmesh, so it is reachable
            // even when nothing around it is.
            if (_ignoreSlot)
            {
                slot.side = 0.0f;
                slot.depth = 0.0f;
            }

            target = PlaceFormationSlot(map, node, fx, fy, slot, _ignoreSlot ? 0.0f : g_Config.formationJitter);

            bool arrived = me->GetDistance(target) <= SIEGE_ARRIVE_DIST;
            if (arrived && _attacker && _routeIndex < route.size())
            {
                ++_routeIndex;
                continue;
            }

            if (arrived)
                target.Relocate(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());

            break;
        }

        // At the throne: engage the city leader rather than shuffling around it.
        if (chargeThrone)
        {
            if (SiegeEvent* event = FindSiegeForCity(_cityId))
            {
                if (Creature* leader = event->leaderGuid && map ? map->GetCreature(event->leaderGuid) : nullptr)
                {
                    if (leader->IsAlive() && me->GetDistance(leader) <= SIEGE_ENGAGE_DIST)
                    {
                        AttackStart(leader);
                        return;
                    }
                }
            }
        }

        if (me->GetDistance(target) <= SIEGE_ARRIVE_DIST)
            return;   // holding position

        me->SetHomePosition(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation());

        me->GetMotionMaster()->MovePoint(SIEGE_POINT_ID_BASE + _routeIndex,
                                         target.GetPositionX(), target.GetPositionY(), target.GetPositionZ(),
                                         FORCED_MOVEMENT_NONE, 0.0f, 0.0f,
                                         /*generatePath*/ true, /*forceDestination*/ false);
    }

    // -------------------------------------------------------------------------

    SiegeUnitAI* GetSiegeUnitAI(Creature* creature)
    {
        if (!creature)
            return nullptr;

        return dynamic_cast<SiegeUnitAI*>(creature->AI());
    }
}
