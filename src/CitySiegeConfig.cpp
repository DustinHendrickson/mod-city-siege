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

#include "Configuration/Config.h"
#include "Log.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace CitySiege
{
    Config g_Config;

    std::vector<CityData>  g_Cities;
    std::vector<SiegeEvent> g_ActiveSieges;
    uint32                 g_NextSiegeTime = 0;

    namespace
    {
        // Default text blocks. Kept here so the config file and the code cannot
        // drift apart: sConfigMgr falls back to exactly these strings.
        char const* DEFAULT_MSG_START =
            "|cffff0000[City Siege]|r The city of {CITYNAME} is under attack! Defenders are needed!";
        char const* DEFAULT_MSG_END =
            "|cff00ff00[City Siege]|r The siege of {CITYNAME} has ended!";
        char const* DEFAULT_MSG_REWARD =
            "|cff00ff00[City Siege]|r You fought for {CITYNAME} and have been rewarded!";
        char const* DEFAULT_YELL_SPAWN =
            "This city will fall before our might!;Prepare to witness your doom!;"
            "We have come to raze your walls!;Today, {CITY} falls!;Your defenses will not hold!";
        char const* DEFAULT_YELLS_COMBAT =
            "Your defenses crumble!;This city will burn!;Face your doom!;"
            "None can stand against us!;{LEADER} will fall with the rest of you!;"
            "Push on! The throne is within reach!";

        char const* DEFAULT_RP_ALLIANCE =
            "Citizens of {CITY}, your time has come! We march under the banner of the Alliance!;"
            "{LEADER}, your people cry out for mercy, but you have shown none to ours!;"
            "We have crossed mountains and seas to bring justice to {CITY}. Surrender now, or face annihilation!;"
            "The Light guides our blades, and the might of Stormwind stands behind us. Your defenses will crumble!;"
            "This ends today! {LEADER}, come forth and face the Alliance, or watch {CITY} burn!"
            "|The Alliance has gathered its greatest heroes for this assault on {CITY}. You cannot stand against us!;"
            "{LEADER}, your leadership has made the Horde enemies it cannot defeat! We will tear down these walls!;"
            "Too long have you raided our villages and slaughtered our people. Today, we bring the war to {CITY}!;"
            "Your shamans' magic cannot protect you. Our priests and paladins have blessed this army!;"
            "Prepare to face the wrath of the Alliance! {LEADER}, your reign over {CITY} ends here and now!"
            "|By order of the King, {CITY} is to be taken! Resistance is futile!;"
            "{LEADER}! Come forth and face us, or hide like a coward while your people suffer!;"
            "The Horde's reign of terror ends here at {CITY}. We will show no mercy to those who threaten peace!;"
            "Our siege engines are ready. The walls of {CITY} mean nothing to the might of the Alliance!;"
            "For every innocent killed by Horde aggression, {LEADER}, you will pay with your life!";

        char const* DEFAULT_RP_HORDE =
            "The Horde has come to claim {CITY}! Your precious Alliance ends today!;"
            "{LEADER}, you have oppressed our people for the last time! Come out and face your fate!;"
            "We are not savages - we are warriors! And today, we show {CITY} what true strength means!;"
            "Your guards are weak. Your walls are weak. {LEADER} hides in the throne room while we stand at the gates!;"
            "Blood and honor! Today we prove that the Horde is the superior force in Azeroth!"
            "|Citizens of {CITY}, flee while you can! We have come for your leaders, not for you!;"
            "{LEADER}! Your reign of tyranny over {CITY} ends today! The throne will belong to the Horde!;"
            "You call us monsters, but it is YOU who started this war! We finish it today at {CITY}!;"
            "The spirits of our ancestors guide us. No amount of Light magic will save {CITY} from our wrath!;"
            "Lok'tar Ogar! {LEADER}, today you fall, and the Horde claims {CITY}!"
            "|The Warchief has sent his finest warriors to end Alliance tyranny at {CITY} once and for all!;"
            "Your pitiful city guard cannot stop the Horde war machine! {LEADER}, your time has come!;"
            "We march for honor! We march for glory! We march to prove that the Horde will take {CITY}!;"
            "Every siege tower, every warrior, every drop of blood spilled today at {CITY} - it all leads to YOUR defeat!;"
            "{LEADER}, the Alliance has grown soft under your leadership. Today at {CITY}, the Horde reminds you why you should fear us!";

        // Entry ids for the named champions that lead an assault. These are the
        // opposing faction's own leaders, which is what the module has always
        // used; they are configurable so servers can swap in custom warlords.
        char const* DEFAULT_ALLIANCE_WARLORDS = "29611,2784,7999,17468";
        char const* DEFAULT_HORDE_WARLORDS    = "4949,3057,10181,16802";

        std::vector<uint32> ParseEntryList(std::string const& value, char const* fallback)
        {
            std::vector<uint32> result;
            for (std::string const& token : SplitString(value.empty() ? fallback : value, ','))
            {
                try
                {
                    uint32 entry = uint32(std::stoul(token));
                    if (entry)
                        result.push_back(entry);
                }
                catch (std::exception const&)
                {
                    LOG_ERROR("module.citysiege", "[City Siege] Ignoring malformed creature entry '{}'.", token);
                }
            }

            if (result.empty())
                for (std::string const& token : SplitString(fallback, ','))
                    result.push_back(uint32(std::stoul(token)));

            return result;
        }
    }

    std::vector<std::string> SplitString(std::string const& source, char delimiter)
    {
        std::vector<std::string> result;
        std::string current;

        for (char c : source)
        {
            if (c == delimiter)
            {
                if (!current.empty())
                    result.push_back(current);
                current.clear();
            }
            else if (c != '\r' && c != '\n')
            {
                current.push_back(c);
            }
        }

        if (!current.empty())
            result.push_back(current);

        // Trim surrounding whitespace on every token.
        for (std::string& token : result)
        {
            size_t begin = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            token = (begin == std::string::npos) ? std::string() : token.substr(begin, end - begin + 1);
        }

        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](std::string const& s) { return s.empty(); }),
                     result.end());

        return result;
    }

    void ResetCityDefaults()
    {
        g_Cities.clear();
        g_Cities.resize(CITY_MAX);

        auto define = [](CityId id, char const* name, char const* display, uint32 mapId, TeamId owner,
                         Position center, Position muster, Position leaderHint, uint32 leaderEntry)
        {
            CityData& city = g_Cities[id];
            city.id = id;
            city.name = name;
            city.displayName = display;
            city.mapId = mapId;
            city.owner = owner;
            city.center = center;
            city.muster = muster;
            city.leader = leaderHint;
            city.leaderEntry = leaderEntry;
        };

        // Muster points sit outside each city's main gate; leader hints are the
        // throne rooms and are refined from the creature table on startup.
        define(CITY_STORMWIND, "Stormwind", "Stormwind", 0, TEAM_ALLIANCE,
               Position(-8913.23f, 554.633f, 93.7944f, 0.0f),
               Position(-9161.16f, 353.365f, 88.117f, 0.0f),
               Position(-8442.578f, 334.6064f, 122.476685f, 0.0f), 29611);

        define(CITY_IRONFORGE, "Ironforge", "Ironforge", 0, TEAM_ALLIANCE,
               Position(-4981.25f, -881.542f, 501.660f, 0.0f),
               Position(-5174.09f, -594.361f, 397.853f, 0.0f),
               Position(-4981.25f, -881.542f, 501.660f, 0.0f), 2784);

        define(CITY_DARNASSUS, "Darnassus", "Darnassus", 1, TEAM_ALLIANCE,
               Position(9947.52f, 2482.73f, 1316.21f, 0.0f),
               Position(9887.36f, 1856.49f, 1317.14f, 0.0f),
               Position(9947.52f, 2482.73f, 1316.21f, 0.0f), 7999);

        define(CITY_EXODAR, "Exodar", "the Exodar", 530, TEAM_ALLIANCE,
               Position(-3864.92f, -11643.7f, -137.644f, 0.0f),
               Position(-4080.80f, -12193.2f, 1.712f, 0.0f),
               Position(-3864.92f, -11643.7f, -137.644f, 0.0f), 17468);

        define(CITY_ORGRIMMAR, "Orgrimmar", "Orgrimmar", 1, TEAM_HORDE,
               Position(1633.75f, -4439.39f, 15.4396f, 0.0f),
               Position(1250.33f, -4412.59f, 25.48f, 0.0f),
               Position(1921.57f, -4148.41f, 41.64f, 0.0f), 4949);

        define(CITY_UNDERCITY, "Undercity", "the Undercity", 0, TEAM_HORDE,
               Position(1633.75f, 240.167f, -43.1034f, 0.0f),
               Position(1982.26f, 226.674f, 35.951f, 0.0f),
               Position(1633.75f, 240.167f, -43.1034f, 0.0f), 10181);

        define(CITY_THUNDERBLUFF, "ThunderBluff", "Thunder Bluff", 1, TEAM_HORDE,
               Position(-1043.11f, 285.809f, 135.165f, 0.0f),
               Position(-1558.61f, -5.071f, 5.384f, 0.0f),
               Position(-1043.11f, 285.809f, 135.165f, 0.0f), 3057);

        define(CITY_SILVERMOON, "Silvermoon", "Silvermoon City", 530, TEAM_HORDE,
               Position(9338.74f, -7277.27f, 13.7014f, 0.0f),
               Position(9230.47f, -6962.67f, 5.004f, 0.0f),
               Position(9338.74f, -7277.27f, 13.7014f, 0.0f), 16802);
    }

    void LoadConfiguration()
    {
        Config& c = g_Config;

        c.enabled = sConfigMgr->GetOption<bool>("CitySiege.Enabled", true);
        c.debug = sConfigMgr->GetOption<bool>("CitySiege.DebugMode", false);

        c.timerMin = sConfigMgr->GetOption<uint32>("CitySiege.TimerMin", 120) * 60;
        c.timerMax = sConfigMgr->GetOption<uint32>("CitySiege.TimerMax", 240) * 60;
        if (c.timerMax < c.timerMin)
            std::swap(c.timerMin, c.timerMax);

        c.eventDuration = std::max<uint32>(60, sConfigMgr->GetOption<uint32>("CitySiege.EventDuration", 30) * 60);
        c.musterDuration = sConfigMgr->GetOption<uint32>("CitySiege.CinematicDelay", 150);
        c.yellFrequency = std::max<uint32>(5, sConfigMgr->GetOption<uint32>("CitySiege.YellFrequency", 30));

        c.allowMultipleCities = sConfigMgr->GetOption<bool>("CitySiege.AllowMultipleCities", false);
        c.announceRadius = sConfigMgr->GetOption<uint32>("CitySiege.AnnounceRadius", 1500);
        c.worldAnnounce = sConfigMgr->GetOption<bool>("CitySiege.WorldAnnounce", true);
        c.minimumLevel = sConfigMgr->GetOption<uint32>("CitySiege.MinimumLevel", 1);

        // --- route generation -------------------------------------------------
        std::string routeMode = sConfigMgr->GetOption<std::string>("CitySiege.Route.Mode", "auto");
        std::transform(routeMode.begin(), routeMode.end(), routeMode.begin(), ::tolower);
        if (routeMode == "manual")
            c.routeMode = ROUTE_MANUAL;
        else if (routeMode == "direct")
            c.routeMode = ROUTE_DIRECT;
        else
            c.routeMode = ROUTE_AUTO;

        c.routeNodeSpacing = std::max(8.0f, sConfigMgr->GetOption<float>("CitySiege.Route.NodeSpacing", 28.0f));
        c.routeMaxNodes = std::max<uint32>(4, sConfigMgr->GetOption<uint32>("CitySiege.Route.MaxNodes", 64));
        c.routeMaxLegs = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CitySiege.Route.MaxLegs", 16));
        c.autoDetectLeader = sConfigMgr->GetOption<bool>("CitySiege.Route.AutoDetectLeaderPosition", true);

        // --- army composition -------------------------------------------------
        c.countMinions = sConfigMgr->GetOption<uint32>("CitySiege.SpawnCount.Minions", 15);
        c.countElites = sConfigMgr->GetOption<uint32>("CitySiege.SpawnCount.Elites", 5);
        c.countMiniBosses = sConfigMgr->GetOption<uint32>("CitySiege.SpawnCount.MiniBosses", 2);
        c.countWarlords = sConfigMgr->GetOption<uint32>("CitySiege.SpawnCount.Leaders", 1);

        c.creatureAllianceMinion = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Minion", 17919);
        c.creatureAllianceElite = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Elite", 17920);
        c.creatureAllianceMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.MiniBoss", 17921);
        c.creatureAllianceDefender = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Alliance.Defender", 17919);
        c.creatureHordeMinion = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Minion", 17932);
        c.creatureHordeElite = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Elite", 17933);
        c.creatureHordeMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.MiniBoss", 17934);
        c.creatureHordeDefender = sConfigMgr->GetOption<uint32>("CitySiege.Creature.Horde.Defender", 17932);

        c.allianceWarlords = ParseEntryList(
            sConfigMgr->GetOption<std::string>("CitySiege.Creature.Alliance.Warlords", DEFAULT_ALLIANCE_WARLORDS),
            DEFAULT_ALLIANCE_WARLORDS);
        c.hordeWarlords = ParseEntryList(
            sConfigMgr->GetOption<std::string>("CitySiege.Creature.Horde.Warlords", DEFAULT_HORDE_WARLORDS),
            DEFAULT_HORDE_WARLORDS);

        c.aggroPlayers = sConfigMgr->GetOption<bool>("CitySiege.AggroPlayers", true);
        c.aggroNPCs = sConfigMgr->GetOption<bool>("CitySiege.AggroNPCs", true);

        c.defendersEnabled = sConfigMgr->GetOption<bool>("CitySiege.Defenders.Enabled", true);
        c.defendersCount = sConfigMgr->GetOption<uint32>("CitySiege.Defenders.Count", 10);
        c.defenderHoldFraction = std::clamp(sConfigMgr->GetOption<float>("CitySiege.Defenders.HoldLineFraction", 0.5f), 0.0f, 1.0f);

        c.formationSpacing = std::max(2.0f, sConfigMgr->GetOption<float>("CitySiege.Formation.Spacing", 6.0f));
        c.formationRowDepth = std::max(2.0f, sConfigMgr->GetOption<float>("CitySiege.Formation.RowDepth", 6.0f));
        c.formationWidth = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CitySiege.Formation.Width", 5));
        c.formationDepthMinion = sConfigMgr->GetOption<float>("CitySiege.Formation.MinionDepth", 0.0f);
        c.formationDepthElite = sConfigMgr->GetOption<float>("CitySiege.Formation.EliteDepth", 14.0f);
        c.formationDepthMiniBoss = sConfigMgr->GetOption<float>("CitySiege.Formation.MiniBossDepth", 26.0f);
        c.formationDepthWarlord = sConfigMgr->GetOption<float>("CitySiege.Formation.WarlordDepth", 34.0f);
        c.formationJitter = std::max(0.0f, sConfigMgr->GetOption<float>("CitySiege.Formation.Jitter", 1.5f));

        // --- level scaling ----------------------------------------------------
        c.levelScalingEnabled = sConfigMgr->GetOption<bool>("CitySiege.Scaling.Enabled", true);
        c.levelFallback = sConfigMgr->GetOption<uint32>("CitySiege.Scaling.FallbackLevel", 80);
        c.levelMin = sConfigMgr->GetOption<uint32>("CitySiege.Scaling.MinLevel", 20);
        c.levelMax = sConfigMgr->GetOption<uint32>("CitySiege.Scaling.MaxLevel", 83);
        c.levelMin = std::clamp<uint32>(c.levelMin, 1, 83);
        c.levelMax = std::clamp<uint32>(c.levelMax, c.levelMin, 83);
        c.levelFallback = std::clamp<uint32>(c.levelFallback, c.levelMin, c.levelMax);
        c.levelSampleRadius = sConfigMgr->GetOption<uint32>("CitySiege.Scaling.SampleRadius", 600);

        c.levelOffsetWarlord = sConfigMgr->GetOption<int32>("CitySiege.Level.WarlordOffset", 3);
        c.levelOffsetMiniBoss = sConfigMgr->GetOption<int32>("CitySiege.Level.MiniBossOffset", 2);
        c.levelOffsetElite = sConfigMgr->GetOption<int32>("CitySiege.Level.EliteOffset", 0);
        c.levelOffsetMinion = sConfigMgr->GetOption<int32>("CitySiege.Level.MinionOffset", -2);
        c.levelOffsetDefender = sConfigMgr->GetOption<int32>("CitySiege.Level.DefenderOffset", -1);
        c.minionLevelSpread = sConfigMgr->GetOption<uint32>("CitySiege.Level.MinionSpread", 4);

        c.healthMultWarlord = sConfigMgr->GetOption<float>("CitySiege.Health.WarlordMultiplier", 12.0f);
        c.healthMultMiniBoss = sConfigMgr->GetOption<float>("CitySiege.Health.MiniBossMultiplier", 5.0f);
        c.healthMultElite = sConfigMgr->GetOption<float>("CitySiege.Health.EliteMultiplier", 2.5f);
        c.healthMultMinion = sConfigMgr->GetOption<float>("CitySiege.Health.MinionMultiplier", 1.0f);
        c.healthMultDefender = sConfigMgr->GetOption<float>("CitySiege.Health.DefenderMultiplier", 1.5f);
        c.damageMultWarlord = sConfigMgr->GetOption<float>("CitySiege.Damage.WarlordMultiplier", 1.6f);
        c.damageMultMiniBoss = sConfigMgr->GetOption<float>("CitySiege.Damage.MiniBossMultiplier", 1.3f);
        c.damageMultElite = sConfigMgr->GetOption<float>("CitySiege.Damage.EliteMultiplier", 1.1f);
        c.damageMultMinion = sConfigMgr->GetOption<float>("CitySiege.Damage.MinionMultiplier", 1.0f);
        c.damageMultDefender = sConfigMgr->GetOption<float>("CitySiege.Damage.DefenderMultiplier", 1.0f);

        c.scaleWarlord = sConfigMgr->GetOption<float>("CitySiege.Scale.Leader", 1.6f);
        c.scaleMiniBoss = sConfigMgr->GetOption<float>("CitySiege.Scale.MiniBoss", 1.3f);

        c.bracketScalingEnabled = sConfigMgr->GetOption<bool>("CitySiege.Bracket.Enabled", true);
        c.bracketMinDamageTaken = std::clamp(sConfigMgr->GetOption<float>("CitySiege.Bracket.MinDamageTaken", 0.15f), 0.01f, 1.0f);
        c.bracketMaxDamageDealt = std::max(1.0f, sConfigMgr->GetOption<float>("CitySiege.Bracket.MaxDamageBoost", 8.0f));
        c.bracketFreeLevels = sConfigMgr->GetOption<uint32>("CitySiege.Bracket.FreeLevels", 3);

        // --- respawn ----------------------------------------------------------
        c.respawnEnabled = sConfigMgr->GetOption<bool>("CitySiege.Respawn.Enabled", true);
        c.respawnWarlord = sConfigMgr->GetOption<uint32>("CitySiege.Respawn.LeaderTime", 300);
        c.respawnMiniBoss = sConfigMgr->GetOption<uint32>("CitySiege.Respawn.MiniBossTime", 180);
        c.respawnElite = sConfigMgr->GetOption<uint32>("CitySiege.Respawn.EliteTime", 120);
        c.respawnMinion = sConfigMgr->GetOption<uint32>("CitySiege.Respawn.MinionTime", 60);
        c.respawnDefender = sConfigMgr->GetOption<uint32>("CitySiege.Defenders.RespawnTime", 45);

        // --- rewards ----------------------------------------------------------
        c.rewardsEnabled = sConfigMgr->GetOption<bool>("CitySiege.RewardOnDefense", true);
        c.rewardLosingSide = sConfigMgr->GetOption<bool>("CitySiege.Reward.LosingSide", true);
        c.rewardHonor = sConfigMgr->GetOption<uint32>("CitySiege.RewardHonor", 100);
        c.rewardGoldBase = sConfigMgr->GetOption<uint32>("CitySiege.RewardGoldBase", 5000);
        c.rewardGoldPerLevel = sConfigMgr->GetOption<uint32>("CitySiege.RewardGoldPerLevel", 5000);
        c.rewardXpEnabled = sConfigMgr->GetOption<bool>("CitySiege.Reward.ExperienceEnabled", true);
        c.rewardXpPerLevel = sConfigMgr->GetOption<uint32>("CitySiege.Reward.ExperiencePerLevel", 400);
        c.rewardItemId = sConfigMgr->GetOption<uint32>("CitySiege.Reward.ItemId", 0);
        c.rewardItemCount = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CitySiege.Reward.ItemCount", 1));
        c.rewardLoserFactor = std::clamp(sConfigMgr->GetOption<float>("CitySiege.Reward.LoserFactor", 0.35f), 0.0f, 1.0f);
        c.participationThreshold = sConfigMgr->GetOption<uint32>("CitySiege.Reward.ParticipationThreshold", 1);
        c.participationWindow = sConfigMgr->GetOption<uint32>("CitySiege.Reward.ParticipationWindow", 600);
        c.announceTopContributors = sConfigMgr->GetOption<bool>("CitySiege.Reward.AnnounceTopContributors", true);

        // --- presentation -----------------------------------------------------
        c.weatherEnabled = sConfigMgr->GetOption<bool>("CitySiege.Weather.Enabled", true);
        c.weatherType = static_cast<WeatherState>(sConfigMgr->GetOption<uint32>("CitySiege.Weather.Type",
                                                                               uint32(WEATHER_STATE_MEDIUM_RAIN)));
        c.weatherGrade = std::clamp(sConfigMgr->GetOption<float>("CitySiege.Weather.Grade", 0.8f), 0.0f, 1.0f);

        c.musicEnabled = sConfigMgr->GetOption<bool>("CitySiege.Music.Enabled", true);
        c.musicRp = sConfigMgr->GetOption<uint32>("CitySiege.Music.RPMusicId", 11803);
        c.musicCombat = sConfigMgr->GetOption<uint32>("CitySiege.Music.CombatMusicId", 11804);
        c.musicVictory = sConfigMgr->GetOption<uint32>("CitySiege.Music.VictoryMusicId", 16039);
        c.musicDefeat = sConfigMgr->GetOption<uint32>("CitySiege.Music.DefeatMusicId", 14127);
        c.musicLoopSeconds = sConfigMgr->GetOption<uint32>("CitySiege.Music.LoopSeconds", 120);

        c.addonEnabled = sConfigMgr->GetOption<bool>("CitySiege.Addon.Enabled", true);
        c.addonUseSystemChannel = sConfigMgr->GetOption<bool>("CitySiege.Addon.UseSystemChannel", false);
        c.addonBroadcastInterval = std::max<uint32>(5, sConfigMgr->GetOption<uint32>("CitySiege.Addon.BroadcastInterval", 15));

        // --- text -------------------------------------------------------------
        c.messageSiegeStart = sConfigMgr->GetOption<std::string>("CitySiege.Message.SiegeStart", DEFAULT_MSG_START);
        c.messageSiegeEnd = sConfigMgr->GetOption<std::string>("CitySiege.Message.SiegeEnd", DEFAULT_MSG_END);
        c.messageReward = sConfigMgr->GetOption<std::string>("CitySiege.Message.Reward", DEFAULT_MSG_REWARD);
        c.yellWarlordSpawn = sConfigMgr->GetOption<std::string>("CitySiege.Yell.LeaderSpawn", DEFAULT_YELL_SPAWN);
        c.yellsCombat = sConfigMgr->GetOption<std::string>("CitySiege.Yell.Combat", DEFAULT_YELLS_COMBAT);
        c.rpScriptsAlliance = sConfigMgr->GetOption<std::string>("CitySiege.RP.Alliance", DEFAULT_RP_ALLIANCE);
        c.rpScriptsHorde = sConfigMgr->GetOption<std::string>("CitySiege.RP.Horde", DEFAULT_RP_HORDE);

        // --- playerbots -------------------------------------------------------
        c.playerbotsEnabled = sConfigMgr->GetOption<bool>("CitySiege.Playerbots.Enabled", false);
        c.playerbotsMinLevel = sConfigMgr->GetOption<uint32>("CitySiege.Playerbots.MinLevel", 50);
        c.playerbotsMaxDefenders = sConfigMgr->GetOption<uint32>("CitySiege.Playerbots.MaxDefenders", 20);
        c.playerbotsMaxAttackers = sConfigMgr->GetOption<uint32>("CitySiege.Playerbots.MaxAttackers", 20);
        c.playerbotsRespawnDelay = sConfigMgr->GetOption<uint32>("CitySiege.Playerbots.RespawnDelay", 30);
        c.playerbotsOrderCooldown = std::max<uint32>(2, sConfigMgr->GetOption<uint32>("CitySiege.Playerbots.OrderCooldown", 8));

        // --- per city ---------------------------------------------------------
        if (g_Cities.size() != CITY_MAX)
            ResetCityDefaults();

        for (CityData& city : g_Cities)
        {
            std::string const prefix = "CitySiege." + city.name + ".";

            c.cityEnabled[city.name] = sConfigMgr->GetOption<bool>(prefix + "Enabled", true);

            city.muster.Relocate(sConfigMgr->GetOption<float>(prefix + "SpawnX", city.muster.GetPositionX()),
                                 sConfigMgr->GetOption<float>(prefix + "SpawnY", city.muster.GetPositionY()),
                                 sConfigMgr->GetOption<float>(prefix + "SpawnZ", city.muster.GetPositionZ()));

            // The leader coordinates in the config are only a hint. With
            // auto-detection on (the default) the throne position is read back
            // from the creature table instead; with it off these win.
            city.leader.Relocate(sConfigMgr->GetOption<float>(prefix + "LeaderX", city.leader.GetPositionX()),
                                 sConfigMgr->GetOption<float>(prefix + "LeaderY", city.leader.GetPositionY()),
                                 sConfigMgr->GetOption<float>(prefix + "LeaderZ", city.leader.GetPositionZ()));
            city.leaderPositionPinned = !c.autoDetectLeader;
            city.leaderPositionResolved = false;

            city.leaderEntry = sConfigMgr->GetOption<uint32>(prefix + "LeaderEntry", city.leaderEntry);

            // Legacy hand-placed waypoints. Still honoured as a fallback and as
            // the sole source when Route.Mode = manual.
            city.manualRoute.clear();
            uint32 waypointCount = sConfigMgr->GetOption<uint32>(prefix + "WaypointCount", 0);
            for (uint32 i = 1; i <= waypointCount; ++i)
            {
                std::string const key = prefix + "Waypoint" + std::to_string(i) + ".";
                float x = sConfigMgr->GetOption<float>(key + "X", 0.0f);
                float y = sConfigMgr->GetOption<float>(key + "Y", 0.0f);
                float z = sConfigMgr->GetOption<float>(key + "Z", 0.0f);

                if (x != 0.0f || y != 0.0f || z != 0.0f)
                    city.manualRoute.emplace_back(x, y, z, 0.0f);
            }
        }

        LOG_INFO("module.citysiege",
                 "[City Siege] Config loaded - enabled: {}, route mode: {}, timer: {}-{} min, duration: {} min, level scaling: {}",
                 c.enabled ? "yes" : "no",
                 c.routeMode == ROUTE_AUTO ? "auto" : (c.routeMode == ROUTE_MANUAL ? "manual" : "direct"),
                 c.timerMin / 60, c.timerMax / 60, c.eventDuration / 60,
                 c.levelScalingEnabled ? "on" : "off");
    }
}
