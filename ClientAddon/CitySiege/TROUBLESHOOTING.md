# City Siege addon - troubleshooting

## The addon does not load

Check the AddOns list at character select. If **City Siege** is missing, the `CitySiege` folder is in
the wrong place; it must sit directly inside `Interface/AddOns/`, so that
`Interface/AddOns/CitySiege/CitySiege.toc` exists.

If it is listed but greyed out, tick **Load out of date AddOns**.

## No minimap button

Type `/cs` to open the window; if that works, the addon is loaded and the button is simply hidden.
Right-click the minimap button (or open Settings from the window) and clear **Hide minimap button**.

If the button was never there on an older install, that was a bug: the `.toc` referenced
LibDataBroker-1.1 and LibDBIcon-1.0 without shipping them. Update to the current version.

## The window opens but everything says "No siege in progress"

That is correct when nothing is under siege. Confirm with `.citysiege status` in chat.

If a siege *is* running and the addon still shows nothing, the server is not sending data:

1. `CitySiege.Addon.Enabled` must be `1` in `mod_city_siege.conf`.
2. Reload the module config with `.citysiege reload`, or restart the world server.
3. If your client build does not deliver addon messages, set
   `CitySiege.Addon.UseSystemChannel = 1` and reload. The addon handles both transports.

## The battle map is blank or says "Map not loaded"

The map images are BLP files under `Interface/AddOns/CitySiege/Media/Maps/`. WoW only reads textures
that were present when the client started, so add the files and restart the game fully - `/reload` is
not enough.

## The map shows no route

Routes are generated server-side from the navmesh. Ask a GM to run:

```
.citysiege route <city>
```

If it reports `direct` or `manual` as the source, automatic routing failed on the server and the
diagnostic line says why - usually missing mmaps. That is a server issue, not an addon one.

## Command buttons do nothing

The server enforces permissions. `Start`, `Stop`, `Clear Forces`, and the route buttons need Game
Master; `Reload Config` needs Administrator. Without the rank you get an error in chat.

Commands are sent as chat beginning with a dot, which the server consumes before it reaches any
channel, so nothing leaks to other players either way.

## Raw text like `CitySiege UPDATE:0:2:...` in chat

The server is using the legacy system-chat transport. Set `CitySiege.Addon.UseSystemChannel = 0` in
`mod_city_siege.conf` and reload. The addon filters the text out regardless, but other players
without the addon will see it.

## Lua errors

Enable error display with `/console scriptErrors 1`, reproduce the problem, and open an issue with
the full error text and what you were doing.
