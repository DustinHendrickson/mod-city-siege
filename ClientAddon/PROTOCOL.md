# City Siege - server to client protocol

Reference for anyone extending the module or writing a different client.

## Transport

The server sends a hidden addon message. In 3.3.5 that is an `SMSG_MESSAGECHAT` whispered to the
receiving player with `language = LANG_ADDON (0xFFFFFFFF)`; the client swallows it and fires
`CHAT_MSG_ADDON` with the prefix split off at the first tab. Players without the addon see nothing.

```
CitySiege<TAB><payload>
```

Handled in `Core.lua`:

```lua
function Core:CHAT_MSG_ADDON(event, prefix, message)
    if prefix ~= "CitySiege" then return end
    CitySiege_EventHandler:ParseAddonMessage(message)
end
```

### Legacy transport

Setting `CitySiege.Addon.UseSystemChannel = 1` sends the same payload as a plain `CHAT_MSG_SYSTEM`
line instead. This is only for clients that do not deliver addon messages: the raw payload is then
visible in everyone's chat log, so the addon installs a filter to hide it. Leave it off.

---

## Messages

Fields are colon-separated. Floats are sent with two decimal places.

### START

Sent once when a siege begins.

```
START:cityId:attackingFaction:musterX:musterY:musterZ:leaderX:leaderY:leaderZ:centerX:centerY:centerZ
```

| Field | Notes |
|---|---|
| `cityId` | 0 Stormwind, 1 Ironforge, 2 Darnassus, 3 Exodar, 4 Orgrimmar, 5 Undercity, 6 Thunder Bluff, 7 Silvermoon |
| `attackingFaction` | `Alliance` or `Horde` |
| `muster*` | where the war host forms up |
| `leader*` | the throne being defended |
| `center*` | announcement anchor for the city |

### UPDATE

Sent every `CitySiege.Addon.BroadcastInterval` seconds while a siege runs.

```
UPDATE:cityId:stage:attackers:defenders:elapsed:remaining:leaderHealth:leaderName:WP:count:x:y:z...
```

| Field | Notes |
|---|---|
| `stage` | 1 Muster, 2 Assault, 3 Breach, 4 Throne |
| `attackers` / `defenders` | units still alive on each side |
| `elapsed` / `remaining` | seconds |
| `leaderHealth` | percentage, one decimal place; `0.0` if the leader is dead or not found |
| `leaderName` | `Unknown` rather than empty, so the field count stays fixed |
| `WP` | section marker, followed by a node count and that many `x:y:z` triples |

Section markers begin at field 10. The route is the only section currently sent; `ATK` and `DEF`
position sections are parsed by the addon but are not emitted by the server, because streaming every
unit's position was too much traffic for what it added.

### END

Sent once when a siege resolves.

```
END:cityId:winner
```

`winner` is `Alliance` or `Horde`.

### MAP_DATA

Sent to one player in response to `.citysiege mapdata <cityId>`, which the addon issues when a city
is selected so the map has a route to draw before the next UPDATE arrives.

```
MAP_DATA:cityId:WP:count:x:y:z...:LEADER:x:y:z
```

---

## Adding a field

Append rather than insert. The addon reads the fixed fields by index and then scans for section
markers, so anything added after `leaderName` and before `WP` will shift the section scan and break
route parsing.
