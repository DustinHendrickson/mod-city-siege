# City Siege Addon - Info & Stats Tabs Enhancement

## Changes Made

### Info Tab
Enhanced to show comprehensive siege information with better formatting:

**When No Sieges Active:**
- List of all 8 available cities (Alliance and Horde)
- How City Sieges work (explanation)
- Siege phases breakdown (1-4)
- Participation information
- Visual ASCII art borders

**When Sieges Are Active:**
- Box-style formatting with borders
- City name with faction color
- Status, phase, duration
- Attacker/Defender counts with icons (⚔️ 🛡)
- Kill statistics
- Colored formatting for better readability

### Stats Tab
Complete overhaul with detailed statistics:

**Always Shows:**
- Siege participation count (won/lost/ongoing)
- Win rate with color coding:
  - Green (≥60%)
  - Yellow (40-59%)
  - Red (<40%)
- Combat statistics (kills/deaths)
- K/D ratio with color coding:
  - Green (≥2.0)
  - Yellow (1.0-1.9)
  - Red (<1.0)

**When Player Has Experience:**
- Average kills/deaths per siege
- Role performance (attacker vs defender)
- City-specific experience
- Performance rank system:
  - Recruit (0-4 sieges)
  - Initiate (5-9)
  - Regular (10-19)
  - Experienced (20-49)
  - Veteran (50+)

**When No Experience:**
- Helpful getting started message
- Tips on how to participate
- What statistics will be tracked

## Visual Features

### Color Coding
- **Yellow** (`|cFFFFFF00`): Headers and titles
- **Green** (`|cFF00FF00`): Positive stats, victories
- **Red** (`|cFFFF0000`): Negative stats, defeats
- **Blue** (`|cFF0088FF`): Defenders
- **Orange** (`|cFFFF6600`): Warnings, special notes
- **Grey** (`|cFF808080`): Help text, N/A values
- **White** (`|cFFFFFFFF`): Normal values

### Formatting Elements
- Box drawing characters: `═ ║ ╔ ╗ ╚ ╝`
- Tree characters: `├ └ ─`
- Unicode symbols: `⚔️` (attacks), `🛡` (defense)
- Progress indicators with bars

## Text Layout

### Info Tab Layout
```
═══ City Siege Information ═══

[Active Sieges Count or "No active sieges"]

╔═══ City Name ═══╗
║ Status: Active
║ Phase: 2/4
║ Faction: Alliance
║ Duration: 15:30
║ Attacking: Horde
║
║ ⚔ Attackers: 12 players
║ 🛡 Defenders: 8 players
║
║ Attacker Kills: 45
║ Defender Kills: 32
╚═══════════════╝

═══════════════════════════════
Use Commands tab to start/manage sieges
Use Map tab to view siege battlefield
```

### Stats Tab Layout
```
═══ Personal Statistics ═══

━━━ Siege Participation ━━━
Total Sieges: 25
├─ Victories: 15
├─ Defeats: 8
└─ Ongoing: 2

Win Rate: 60.0%

━━━ Combat Statistics ━━━
Total Kills: 234
Total Deaths: 89
K/D Ratio: 2.63

━━━ Averages Per Siege ━━━
Avg Kills: 9.4
Avg Deaths: 3.6

━━━ Role Performance ━━━
As Attacker: 12 sieges
As Defender: 13 sieges

━━━━━━━━━━━━━━━━━━━━
Rank: Experienced

Stats update in real-time during sieges
```

## How Data Updates

### Info Tab
- Updates when:
  - Siege starts/stops
  - Phase changes
  - Player counts change
  - Tab is switched to
  - Main frame is shown

### Stats Tab
- Updates when:
  - Player participates in siege
  - Kills/deaths occur
  - Siege completes (win/loss recorded)
  - Tab is switched to
  - Main frame is shown

## Integration

Both tabs automatically call their update functions:
- On frame show: `MainFrame:Show()` calls `UpdateSiegeDisplay()`
- On tab switch: `ShowTab()` calls respective update functions
- During siege: Updates triggered by event handlers

## Testing

To see the enhanced content:

1. **Info Tab - No Sieges:**
   ```
   /cs show
   [Click Info tab]
   ```
   Should show city list and siege information

2. **Info Tab - With Sieges:**
   ```
   /cs testmap
   /cs show
   [Click Info tab]
   ```
   Should show formatted siege data

3. **Stats Tab - No Experience:**
   ```
   /cs show
   [Click Stats tab]
   ```
   Should show getting started message

4. **Stats Tab - With Data:**
   Statistics will populate as you participate in sieges

## Future Enhancements

Possible additions:
- Graphs/charts for statistics
- City-by-city breakdown
- Time-based statistics (today, this week, all time)
- Leaderboard integration
- Achievement system
- Detailed combat log
- Siege history timeline
- Export statistics to file
