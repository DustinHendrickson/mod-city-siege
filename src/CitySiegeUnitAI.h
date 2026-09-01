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

#ifndef CITY_SIEGE_UNIT_AI_H
#define CITY_SIEGE_UNIT_AI_H

#include "CitySiege.h"
#include "ScriptedCreature.h"

namespace CitySiege
{
    /**
     * Marching AI for a single siege unit.
     *
     * Movement is event driven: MovementInform() fires when a waypoint is
     * reached and immediately issues the next order. The previous design polled
     * every unit from the world update and only re-issued when it recognised the
     * unit as idle, which meant any state it did not anticipate - combat just
     * ended, the point generator still on the stack, arrived but not finalised -
     * left the unit standing in the street forever.
     *
     * EnterEvadeMode is overridden so a unit that drops combat resumes the march
     * instead of walking back to where it spawned.
     */
    class SiegeUnitAI : public ScriptedAI
    {
    public:
        SiegeUnitAI(Creature* creature, CityId cityId, bool attacker, uint8 rank,
                    FormationSlot const& slot, uint32 routeIndex);

        // CreatureAI
        void MovementInform(uint32 type, uint32 id) override;
        void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override;
        void UpdateAI(uint32 diff) override;

        /// Releases the unit from the muster phase and starts it marching.
        void BeginMarch();

        /// Stops the unit where it stands (siege over, cleanup).
        void Halt();

        [[nodiscard]] uint32 GetRouteIndex() const { return _routeIndex; }
        [[nodiscard]] bool IsAttacker() const { return _attacker; }
        [[nodiscard]] uint8 GetRank() const { return _rank; }
        [[nodiscard]] FormationSlot const& GetSlot() const { return _slot; }
        [[nodiscard]] bool IsMarching() const { return _marching; }

    private:
        /// Works out where this unit should be standing and orders it there.
        void IssueOrder();

        /// Called when a unit has not moved since the last watchdog tick.
        void OnStalled();

        CityId        _cityId;
        bool          _attacker;
        uint8         _rank;
        FormationSlot _slot;
        uint32        _routeIndex;
        bool          _marching = false;
        uint32        _watchdog = 0;

        // Stuck detection. A unit wedged on a wall or a corner reports its
        // spline as finalised and never arrives, so re-issuing the same order
        // achieves nothing; these track how long it has genuinely not moved so
        // the order can be escalated instead of repeated.
        uint32        _stalls = 0;
        bool          _ignoreSlot = false;   // head for the bare route node
        Position      _lastPosition;
        bool          _haveLastPosition = false;
    };

    /// Returns the siege AI attached to a creature, or nullptr.
    SiegeUnitAI* GetSiegeUnitAI(Creature* creature);
}

#endif // CITY_SIEGE_UNIT_AI_H
