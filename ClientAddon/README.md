# City Siege - client addon

Optional companion addon for the [mod-city-siege](../README.md) server module. It shows a live
overview of any siege in progress, a battle map of the city, and a control panel for game masters.

Everything it displays is pushed by the server. The addon is read-only apart from the Commands tab,
and it is not required to take part in a siege.

- **Interface:** 3.3.5a (30300)
- **License:** AGPL-3.0, see [LICENSE](LICENSE)

---

## Installation

1. Copy the `CitySiege` folder into `World of Warcraft/Interface/AddOns/`.
2. Restart the client (or `/reload` if it was already running).
3. Make sure **City Siege** is ticked in the AddOns list at character select.

The addon ships with everything it needs. Earlier versions listed LibDataBroker-1.1 and LibDBIcon-1.0
in the `.toc` without shipping them, which silently disabled the minimap button; the button is now
self-contained and only uses LibDBIcon if another addon has already loaded it.

---

## Using it

| How | What it does |
|---|---|
| Minimap button, left-click | Open or close the siege window |
| Minimap button, right-click | Open the settings panel |
| Minimap button, drag | Move it around the minimap ring |
| `/citysiege` or `/cs` | Open the siege window |
| Escape | Close the focused window |

Hovering the minimap button lists every siege currently running and how long each has left.

### Tabs

**Overview** — the selected city's live state: which stage the assault has reached, the time left,
the city leader's name and health, how many attackers are still standing, how many defenders are
holding, and whether your faction is attacking or defending.

**Battle Map** — the city map with the generated marching route, the muster point and the throne
marked on it.

**Commands** — buttons for the server's `.citysiege` commands, including rebuilding a city's route
and placing route beacons in the world. Destructive actions ask for confirmation first. The buttons
are always visible; the server enforces the rank each command needs, and every tooltip says what
that rank is.

---

## Settings

Right-click the minimap button, or use the **Settings** entry, to configure:

- minimap button visibility and lock
- window scale and opacity
- which notifications appear in chat, and whether they play a sound
- map icon scale

Settings are stored per character profile in `CitySiegeDB`.

---

## Troubleshooting

See [CitySiege/TROUBLESHOOTING.md](CitySiege/TROUBLESHOOTING.md).

The most common issue is the addon showing nothing at all: that means the server is not sending
data. Check that `CitySiege.Addon.Enabled = 1` in `mod_city_siege.conf`, and that a siege is
actually running (`.citysiege status`).

---

## Extending it

The server-to-client message format is documented in [PROTOCOL.md](PROTOCOL.md).
