/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 */

#ifndef MOD_CITY_SIEGE_API_H
#define MOD_CITY_SIEGE_API_H

#include "Define.h"
#include <string>
#include <vector>

namespace CitySiegeAPI
{
    struct ActiveSiegeSnapshot
    {
        uint32 cityId = 0;
        std::string cityName;
        uint32 startTime = 0;
        uint32 endTime = 0;
        bool isActive = false;
        bool cinematicPhase = false;
        uint32 remainingSeconds = 0;
        uint32 spawnedAttackers = 0;
        uint32 spawnedDefenders = 0;
        uint32 attackerBotCount = 0;
        uint32 defenderBotCount = 0;
    };

    std::vector<ActiveSiegeSnapshot> GetActiveSieges();
}

#endif