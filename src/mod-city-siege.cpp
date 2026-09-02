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

#include "mod-city-siege.h"
#include "CitySiege.h"

#include "Creature.h"
#include "GameTime.h"
#include "Log.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "Unit.h"

#include <algorithm>

using namespace CitySiege;

// -----------------------------------------------------------------------------
// World lifecycle
// -----------------------------------------------------------------------------

class CitySiegeWorldScript : public WorldScript
{
public:
    CitySiegeWorldScript() : WorldScript("CitySiegeWorldScript",
        { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_SHUTDOWN }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        if (!reload || g_Cities.empty())
            ResetCityDefaults();

        LoadConfiguration();

        if (reload)
        {
            // Coordinates and route settings may have changed; drop the caches
            // so the next siege regenerates them.
            ResolveLeaderPositions();
            ClearCityRoutes();
        }
    }

    void OnStartup() override
    {
        LOG_INFO("module.citysiege", "[City Siege] Loading City Siege...");

        if (g_Cities.empty())
        {
            ResetCityDefaults();
            LoadConfiguration();
        }

        // The world database is available by now, so the throne of every city
        // can be read straight from its leader's spawn.
        ResolveLeaderPositions();

        if (!g_Config.enabled)
        {
            LOG_INFO("module.citysiege", "[City Siege] Module disabled by configuration.");
            return;
        }

        uint32 delay = urand(g_Config.timerMin, g_Config.timerMax);
        g_NextSiegeTime = uint32(GameTime::GetGameTime().count()) + delay;

        LOG_INFO("module.citysiege", "[City Siege] Enabled. First siege in {} minute(s). "
                 "Marching routes are generated from the server navmesh on first use.", delay / 60);
    }

    void OnUpdate(uint32 diff) override
    {
        if (!g_Config.enabled)
            return;

        UpdateSieges(diff);
    }

    void OnShutdown() override
    {
        ShutdownAllSieges();
        LOG_INFO("module.citysiege", "[City Siege] Shutdown complete.");
    }
};

// -----------------------------------------------------------------------------
// Combat bookkeeping: participation credit and cross-bracket damage scaling
// -----------------------------------------------------------------------------

class CitySiegeUnitScript : public UnitScript
{
public:
    CitySiegeUnitScript() : UnitScript("CitySiegeUnitScript", true,
        { UNITHOOK_ON_DAMAGE, UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
          UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_HEAL_RECEIVED, UNITHOOK_ON_UNIT_DEATH }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        RecordDamage(attacker, victim, damage);
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        float scaled = float(damage);
        ScaleBracketDamage(attacker, target, scaled);
        damage = uint32(std::max(1.0f, scaled));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (damage <= 0)
            return;

        float scaled = float(damage);
        ScaleBracketDamage(attacker, target, scaled);
        damage = int32(std::max(1.0f, scaled));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* /*spellInfo*/) override
    {
        float scaled = float(damage);
        ScaleBracketDamage(attacker, target, scaled);
        damage = uint32(std::max(1.0f, scaled));
    }

    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* /*spellInfo*/) override
    {
        RecordHealing(healer, target, heal);
    }

    void OnUnitDeath(Unit* unit, Unit* killer) override
    {
        RecordKill(killer, unit);
    }
};

// -----------------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------------
//
// Pet and minion kills need no separate hook: OnUnitDeath reports the pet as the
// killer and the credit resolver walks up to its owner.

void Addmod_city_siegeScripts()
{
    new CitySiegeWorldScript();
    new CitySiegeUnitScript();

    AddCitySiegeCommandScripts();
}
