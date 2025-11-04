# City Siege Addon - Changelog

All notable changes to the City Siege client addon will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned Features
- Heat maps showing combat intensity zones
- Siege replay system with timeline scrubbing
- Achievement system integration
- Guild leaderboards
- Custom alert sound selection
- Advanced statistics with graphs
- Cross-realm coordination support
- 3D arrow indicators for objectives
- Minimap icon customization
- Voice chat integration markers

### Under Consideration
- Mobile companion app
- Web dashboard integration
- Video recording integration
- Tournament mode
- Spectator mode
- Custom map overlay images
- Weather effects on map
- Time-of-day visualization

---

## [1.0.0] - 2025-11-04

### Added - Initial Release
- Complete addon framework based on Ace3
- Interactive city map display with real-time updates
- Siege tracking system for all 8 major cities
- Command panel for GM/Admin siege management
- Minimap button with LibDBIcon integration
- Comprehensive settings panel with live preview
- Main window with 4-tab interface (Commands, Map, Info, Stats)
- Personal statistics tracking
- Event notification system with sound support
- Localization support (enUS complete, others stubbed)
- Full documentation (README, INSTALLATION, DEVELOPMENT_SUMMARY)
- Quick reference card
- XML templates for consistent UI styling

#### Features
- **Map Display**
  - Player position tracking (red = attackers, blue = defenders)
  - NPC position markers with color coding
  - Waypoint path visualization
  - City switching capability
  - Configurable update intervals (0.5s - 5.0s)
  - Icon scaling (0.5x - 2.0x)

- **Siege Tracking**
  - Active siege monitoring
  - Phase tracking
  - Participant management (attackers/defenders)
  - NPC tracking with spawn times
  - Statistics collection (kills, deaths, objectives)
  - Auto-save siege states

- **Command Interface**
  - City selection dropdown
  - One-click command execution
  - Commands: start, stop, cleanup, status, info, reload
  - Visual feedback
  - Permission awareness

- **Minimap Integration**
  - Show/hide toggle
  - Lock/unlock position
  - Customizable angle
  - Tooltip with active siege summary
  - Multiple click actions (left/right/middle)

- **Settings System**
  - Minimap button configuration
  - UI scale and transparency
  - Combat visibility toggle
  - Auto-hide when leaving siege zones
  - Window locking
  - Notification preferences
  - Map display options
  - Profile saving

- **Statistics**
  - Sieges participated
  - Win/loss record
  - Win rate percentage
  - Total kills and deaths
  - K/D ratio calculation

#### Supported Cities
- Alliance: Stormwind, Ironforge, Darnassus, The Exodar
- Horde: Orgrimmar, Undercity, Thunder Bluff, Silvermoon City

#### Commands
- `/cs` or `/citysiege` - Main command
- `/cs show` - Toggle main window
- `/cs hide` - Hide main window
- `/cs config` - Open settings
- `/cs minimap` - Toggle minimap button
- `/cs status` - Request siege status
- `/cs help` - Show help
- `/cs reset` - Reset settings
- `/cs debug` - Toggle debug mode

#### Documentation
- Complete README with 5,200+ words
- Step-by-step installation guide
- Library installation instructions
- Development summary
- Quick reference card
- Troubleshooting guide
- FAQ section

#### Technical
- Modular architecture (7 core modules)
- Event-driven updates
- Throttled map rendering
- Memory-efficient icon reuse
- SavedVariables persistence
- Ace3 framework integration
- LibDataBroker support
- Multi-language ready

### Known Issues
- Libraries must be manually installed by users
- Map coordinate conversion uses estimation (needs calibration)
- Line rotation not fully implemented (WoW 3.3.5 API limitation)
- Real-time updates depend on server-side communication
- Some Lua linter warnings for WoW API functions (expected)

### Dependencies
- WoW Client 3.3.5a (12340)
- AzerothCore with mod-city-siege
- Ace3 libraries (must download separately)
- LibDataBroker-1.1
- LibDBIcon-1.0

---

## Version History Template

Use this template for future releases:

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- New features

### Changed
- Changes in existing functionality

### Deprecated
- Soon-to-be removed features

### Removed
- Removed features

### Fixed
- Bug fixes

### Security
- Security improvements
```

---

## Versioning Scheme

- **Major** (X.0.0): Breaking changes, major rewrites
- **Minor** (1.X.0): New features, backward compatible
- **Patch** (1.0.X): Bug fixes, minor improvements

### Examples
- `1.0.0` → `1.1.0`: Added heat map feature
- `1.1.0` → `1.1.1`: Fixed minimap button positioning bug
- `1.1.1` → `2.0.0`: Complete UI redesign

---

## Contribution Guidelines

When updating this changelog:

1. **Keep entries clear and concise**
2. **Group similar changes together**
3. **Use present tense** ("Add feature" not "Added feature")
4. **Link to issues/PRs when applicable**
5. **Note any breaking changes prominently**
6. **Include upgrade instructions for breaking changes**
7. **Credit contributors**

---

## Links

- **Source Code**: https://github.com/azerothcore/mod-city-siege
- **Issue Tracker**: https://github.com/azerothcore/mod-city-siege/issues
- **Discussion Forum**: (Server Discord/Forum)
- **Documentation**: See README.md

---

## Migration Guides

### Upgrading from Future Versions

#### From 1.0.x to 1.1.x
(To be documented when 1.1.x is released)

#### From 1.x.x to 2.0.0
(To be documented when 2.0.0 is released)

---

**Note**: This is version 1.0.0 - the initial release. Future versions will be documented above as they are released.

---

*Last Updated: 2025-11-04*
