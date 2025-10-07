# AzerothCore Module: City Siege

## Overview

The **City Siege** module adds dynamic, timed siege events to all major cities in World of Warcraft. On a random, configurable timer, opposing faction forces will assault a major city, spawning enemy units, elite soldiers, mini-bosses, and faction leaders who engage in cinematic roleplay before attacking the city's leader and any players or NPCs in their path.

This module creates exciting world events that encourage player participation in defending their cities and creates a more dynamic, living world experience.

## Features

- **Dynamic Siege Events**: Random timer-based events that bring warfare to major cities
- **All Major Cities Supported**: 
  - Alliance: Stormwind, Ironforge, Darnassus, Exodar
  - Horde: Orgrimmar, Undercity, Thunder Bluff, Silvermoon
- **Configurable Spawn System**: 
  - Regular minions
  - Elite soldiers
  - Mini-bosses
  - Faction leaders
- **Cinematic RP Phase**: Initial delay where siege forces engage in roleplay before combat
- **Periodic Yells**: Leaders and bosses yell during the event for immersion
- **Flexible Event Timing**: Configurable minimum and maximum intervals between events
- **Single or Multiple Sieges**: Choose whether only one city can be under siege at a time, or allow multiple simultaneous events
- **Radius-Based or World-Wide Announcements**: Announce to nearby players or the entire server
- **City-Specific Configuration**: Enable or disable events for individual cities
- **Reward System**: Optional rewards for players who defend their cities (Honor, Gold)
- **Debug Mode**: Detailed logging for server administrators

## Installation

### 1. Clone the Module

Navigate to your AzerothCore modules directory and clone this repository:

```bash
cd /path/to/azerothcore/modules
git clone https://github.com/your-repo/mod-city-siege.git
```

### 2. Recompile AzerothCore

Rebuild the server with the new module:

```bash
cd /path/to/azerothcore
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/server
make -j$(nproc)
make install
```

### 3. Configure the Module

Copy the configuration file to your server's config directory:

```bash
cp /path/to/azerothcore/modules/mod-city-siege/conf/mod_city_siege.conf.dist /path/to/server/etc/modules/mod_city_siege.conf
```

Edit the configuration file to customize the module behavior:

```bash
nano /path/to/server/etc/modules/mod_city_siege.conf
```

### 4. Restart the Server

Restart your worldserver for the changes to take effect:

```bash
./worldserver
```

## Configuration Options

All configuration options are located in `mod_city_siege.conf`. Below is a comprehensive list:

### Core Settings

| Setting | Description | Default | Values |
|---------|-------------|---------|--------|
| `CitySiege.Enabled` | Enable/disable the module | 1 | 0 (disabled) / 1 (enabled) |
| `CitySiege.DebugMode` | Enable debug logging | 0 | 0 (disabled) / 1 (enabled) |
| `CitySiege.TimerMin` | Minimum time between events (minutes) | 120 | Any positive integer |
| `CitySiege.TimerMax` | Maximum time between events (minutes) | 240 | Any positive integer |
| `CitySiege.EventDuration` | Duration of each siege (minutes) | 30 | Any positive integer |
| `CitySiege.AllowMultipleCities` | Allow simultaneous sieges | 0 | 0 (single) / 1 (multiple) |
| `CitySiege.AnnounceRadius` | Announcement radius in yards (0 = world-wide) | 500 | Any positive integer |
| `CitySiege.MinimumLevel` | Minimum player level to participate | 1 | 1-80 |

### City-Specific Settings

Each major city can be individually enabled or disabled:

| Setting | City | Default |
|---------|------|---------|
| `CitySiege.Stormwind.Enabled` | Stormwind | 1 |
| `CitySiege.Ironforge.Enabled` | Ironforge | 1 |
| `CitySiege.Darnassus.Enabled` | Darnassus | 1 |
| `CitySiege.Exodar.Enabled` | Exodar | 1 |
| `CitySiege.Orgrimmar.Enabled` | Orgrimmar | 1 |
| `CitySiege.Undercity.Enabled` | Undercity | 1 |
| `CitySiege.ThunderBluff.Enabled` | Thunder Bluff | 1 |
| `CitySiege.Silvermoon.Enabled` | Silvermoon | 1 |

### Spawn Settings

| Setting | Description | Default |
|---------|-------------|---------|
| `CitySiege.SpawnCount.Minions` | Number of regular enemy units | 15 |
| `CitySiege.SpawnCount.Elites` | Number of elite enemy units | 5 |
| `CitySiege.SpawnCount.MiniBosses` | Number of mini-bosses | 2 |
| `CitySiege.SpawnCount.Leaders` | Number of faction leaders | 1 |
| `CitySiege.AggroPlayers` | Whether enemies aggro players | 1 |
| `CitySiege.AggroNPCs` | Whether enemies aggro city NPCs | 1 |

### Cinematic and RP Settings

| Setting | Description | Default |
|---------|-------------|---------|
| `CitySiege.CinematicDelay` | Initial RP phase duration (seconds) | 45 |
| `CitySiege.YellFrequency` | How often leaders yell (seconds) | 30 |

### Reward Settings

| Setting | Description | Default |
|---------|-------------|---------|
| `CitySiege.RewardOnDefense` | Enable rewards for defenders | 1 |
| `CitySiege.RewardHonor` | Honor points for successful defense | 100 |
| `CitySiege.RewardGold` | Gold reward in copper (50g = 500000) | 500000 |

## How It Works

### Event Flow

1. **Timer**: The module waits for a random interval between `TimerMin` and `TimerMax`
2. **City Selection**: A random enabled city is selected for siege
3. **Announcement**: Players in range (or server-wide) are notified of the attack
4. **Spawn Phase**: Enemy forces spawn at designated locations around the city
5. **Cinematic Phase**: For the configured delay, enemies engage in roleplay
6. **Combat Phase**: After the cinematic, enemies become aggressive and attack the city leader
7. **Event Duration**: The siege lasts for the configured duration
8. **Resolution**: Event ends, creatures despawn, rewards are distributed (if enabled)

### Event Mechanics

- **Target Priority**: Siege forces prioritize the city leader but will attack players and NPCs in their path
- **Periodic Yells**: Leaders and mini-bosses yell threatening messages throughout the event
- **Player Defense**: Players can engage the invaders to protect their city
- **Rewards**: Successful defenders receive honor and gold (if enabled)

## Customization

### Adding Custom Creatures

To use custom creatures for siege events, you'll need to:

1. Create or select creature entries in your database
2. Modify the `SpawnSiegeCreatures()` function in `mod-city-siege.cpp`
3. Define spawn locations for each city
4. Configure creature AI for attacking behavior

### Creating Custom Yells

Siege leaders can be configured to yell custom messages. This requires:

1. Adding creature text entries to your database
2. Implementing yell logic in the update loop
3. Linking yells to specific creature entries

### Adjusting Spawn Locations

Spawn coordinates for each city are defined in the `CityData` structure. You can:

1. Modify the center coordinates in `g_Cities` vector
2. Add multiple spawn points per city
3. Create different spawn patterns for different enemy types

## Development Notes

### Current Implementation Status

This is a **skeleton module** with the following features implemented:

✅ Complete configuration system  
✅ Timer and event management framework  
✅ City selection and tracking  
✅ Announcement system  
✅ Event lifecycle management  
✅ Debug logging

The following features require additional implementation:

⚠️ Actual creature spawning (placeholder functions provided)  
⚠️ Creature AI configuration  
⚠️ Custom yell system  
⚠️ Reward distribution  
⚠️ Specific spawn locations per city  
⚠️ Database integration for persistent siege data

### Extending the Module

To complete this module, developers should:

1. **Define Creature Entries**: Create or select appropriate creature entries for:
   - Minions (regular units)
   - Elites (stronger units)
   - Mini-bosses (named characters)
   - Faction leaders (major lore characters)

2. **Implement Spawning**: In `SpawnSiegeCreatures()`:
   ```cpp
   Map* map = sMapMgr->FindMap(city.mapId, 0);
   if (map)
   {
       for (uint32 i = 0; i < g_SpawnCountMinions; i++)
       {
           // Calculate spawn position
           Position pos = CalculateSpawnPosition(city);
           
           // Spawn creature
           if (Creature* creature = map->SummonCreature(CREATURE_ENTRY, pos))
           {
               event.spawnedCreatures.push_back(creature->GetGUID());
               // Configure AI, faction, etc.
           }
       }
   }
   ```

3. **Add Creature Texts**: Insert into `creature_text` table:
   ```sql
   INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`)
   VALUES (ENTRY, 0, 0, 'Your city will fall!', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader Yell');
   ```

4. **Implement Rewards**: In `EndSiegeEvent()`:
   ```cpp
   // Find players who participated
   // Award honor and gold based on contribution
   ```

## Troubleshooting

### Events Not Starting

- Check if module is enabled: `CitySiege.Enabled = 1`
- Verify at least one city is enabled
- Check server logs for errors
- Enable debug mode for detailed information

### No Announcements

- Verify `CitySiege.AnnounceRadius` is set correctly
- Check if players are in range (if radius is not 0)
- Ensure players are on the correct map

### Configuration Not Loading

- Verify config file is in the correct location
- Check file permissions
- Restart worldserver after configuration changes
- Enable debug mode to see loaded values

## Support

For issues, questions, or contributions:

- **GitHub Issues**: [Report a bug or request a feature](https://github.com/your-repo/mod-city-siege/issues)
- **AzerothCore Discord**: Join the community for support
- **Documentation**: See [AzerothCore Module Documentation](http://www.azerothcore.org/wiki/Create-a-Module)

## Credits

- **Module Author**: [Your Name]
- **Based on**: AzerothCore skeleton-module
- **Inspired by**: Classic WoW world events and city raids
- **Framework**: [AzerothCore](https://www.azerothcore.org/)

## License

This module is released under the [GNU AGPL v3](https://www.gnu.org/licenses/agpl-3.0.en.html) license, consistent with AzerothCore's licensing.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

Please follow AzerothCore's coding standards and include appropriate documentation.

## Changelog

### Version 1.0.0 (Initial Release)
- Complete configuration system
- Event timer and management framework
- Support for all 8 major cities
- Announcement system
- Debug logging
- Skeleton implementation ready for expansion
