# City Siege Module - Quick Start Guide

## Installation (3 Steps)

1. **Copy module to modules directory**
2. **Recompile AzerothCore**
3. **Configure and restart server**

## Essential Configuration

Edit `mod_city_siege.conf`:

```ini
CitySiege.Enabled = 1              # Enable the module
CitySiege.TimerMin = 120           # Min time between events (minutes)
CitySiege.TimerMax = 240           # Max time between events (minutes)
CitySiege.EventDuration = 30       # How long each siege lasts (minutes)
```

## City Control

Enable/disable specific cities:

```ini
CitySiege.Stormwind.Enabled = 1    # Alliance
CitySiege.Ironforge.Enabled = 1    # Alliance
CitySiege.Darnassus.Enabled = 1    # Alliance
CitySiege.Exodar.Enabled = 1       # Alliance
CitySiege.Orgrimmar.Enabled = 1    # Horde
CitySiege.Undercity.Enabled = 1    # Horde
CitySiege.ThunderBluff.Enabled = 1 # Horde
CitySiege.Silvermoon.Enabled = 1   # Horde
```

## Testing

Enable debug mode to see detailed logging:

```ini
CitySiege.DebugMode = 1
```

Check server logs for:
- `[City Siege] Module enabled`
- `[City Siege] First siege in X minutes`
- `[City Siege] Configuration loaded`

## Common Settings

### For Frequent Events (Testing)
```ini
CitySiege.TimerMin = 5      # 5 minutes
CitySiege.TimerMax = 10     # 10 minutes
```

### For Rare Events (Production)
```ini
CitySiege.TimerMin = 180    # 3 hours
CitySiege.TimerMax = 360    # 6 hours
```

### Multiple Simultaneous Sieges
```ini
CitySiege.AllowMultipleCities = 1
```

### World-Wide Announcements
```ini
CitySiege.AnnounceRadius = 0
```

## Spawn Configuration

Adjust enemy counts:

```ini
CitySiege.SpawnCount.Minions = 15      # Regular enemies
CitySiege.SpawnCount.Elites = 5        # Elite enemies
CitySiege.SpawnCount.MiniBosses = 2    # Mini-bosses
CitySiege.SpawnCount.Leaders = 1       # Faction leaders
```

## Rewards

Configure defender rewards:

```ini
CitySiege.RewardOnDefense = 1      # Enable rewards
CitySiege.RewardHonor = 100        # Honor points
CitySiege.RewardGold = 500000      # Gold (in copper, 50g = 500000)
```

## Event Flow

1. **Wait** → Random time between TimerMin and TimerMax
2. **Select** → Random enabled city chosen
3. **Announce** → Players notified
4. **Spawn** → Enemies appear
5. **Cinematic** → Initial RP phase (45 seconds default)
6. **Combat** → Enemies attack city leader and defenders
7. **End** → Event concludes after EventDuration
8. **Rewards** → Defenders receive honor and gold

## Troubleshooting Quick Tips

| Problem | Solution |
|---------|----------|
| Events not starting | Check `CitySiege.Enabled = 1` |
| No announcements | Verify `AnnounceRadius` setting |
| Too frequent | Increase `TimerMin` and `TimerMax` |
| Too rare | Decrease `TimerMin` and `TimerMax` |
| Config not loading | Restart worldserver |
| Want more info | Enable `DebugMode = 1` |

## File Locations

- **Config**: `/path/to/server/etc/modules/mod_city_siege.conf`
- **Logs**: Check worldserver logs for `[City Siege]` entries

## Development Status

✅ Framework complete  
⚠️ Needs creature implementation  
⚠️ Needs AI configuration  
⚠️ Needs text system setup  

See `SUMMARY.md` for detailed implementation status.

## Support

- GitHub Issues: Report bugs
- AzerothCore Discord: Get help
- README.md: Full documentation
