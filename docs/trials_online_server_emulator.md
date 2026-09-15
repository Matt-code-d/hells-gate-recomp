# Trials of Saint Lucia — Online Server Emulator Research

Research into the online component of the Trials of Saint Lucia DLC and what
is required to build a server emulator. This is a **research/planning
document** — no implementation has been done yet.

## DLC online features (per EA press release)

The Trials of Saint Lucia DLC added four online features on top of the
offline arena/trial content:

1. **Online Co-Op** — 2-player cooperative play over Xbox Live.
2. **Trial Sharing (UGC)** — Upload/download user-created trials. Players
   create trials in the in-game editor and share them worldwide.
3. **Leaderboards** — Worldwide rankings as both **Player** (trial completion
   scores) and **Creator** (based on ratings of your trials).
4. **Trial Rating** — 1-5 star rating system for downloaded trials, plus
   favorites. Ratings feed the Creator leaderboard.

## How the game talks to Xbox Live

The game does **not** connect to EA servers directly. There are no HTTP URLs,
`XTitleServer`, or `XHttp` imports in the game binary. All online traffic
flows through the Xbox Live stack:

- **XGI app (0xFB)** — Session lifecycle, stats, achievements, contexts/properties
- **XLIVEBASE app (0xFC)** — Logon, NAT type, service info, presence, friends
- **XAM exports** — Content (STFS), user profile, enumerators, voice, networking
- **NetDll_* (Winsock + XNet)** — Raw UDP/TCP sockets for gameplay data

The actual co-op gameplay data flows **peer-to-peer** over Winsock sockets
(which already work in ReXGlue). Xbox Live is only used for **matchmaking**
(finding a partner), **stats** (leaderboards), and **content** (sharing
trials). This means the server emulator only needs to handle the
signaling/metadata layer — not relay gameplay traffic.

## Current SDK implementation status

### Already working (no server needed)
| API | Status | Notes |
|-----|--------|-------|
| `NetDll_*` (Winsock) | **Real sockets** | socket/bind/connect/send/recv all work |
| `NetDll_XNet*` | Partial | XnAddr translation, QoS, DNS — partially implemented |
| `XamContentCreateEx` | **Works locally** | Creates/opens STFS packages on HDD |
| `XamContentDelete` | **Works locally** | Deletes STFS packages |
| `XamContentClose` | **Works** | Closes opened content |
| `XamContentCreateEnumerator` | **Works locally** | Enumerates local STFS content |
| `XamEnumerate` | **Works** | Reads items from an enumerator handle |
| `XGIUserWriteAchievements` | **Works** | Unlocks achievements |
| `XamUserCheckPrivilege` | Returns granted (1) | All privileges granted |
| `XamUserIsOnlineEnabled` | Returns 1 | Reports online enabled |
| `XamUserGetMembershipTier` | Returns 6 (Gold) | Reports Gold membership |

### Stubbed — returns success but does nothing (the server emulator targets)
| XGI message | Function | What it should do |
|-------------|----------|------------------|
| `0x000B0006` | `XGIUserSetContextEx` | Set matchmaking context (game mode, map, etc.) |
| `0x000B0007` | `XGIUserSetPropertyEx` | Set matchmaking properties (trial ID, skill, etc.) |
| `0x000B0010` | `XGISessionCreateImpl` | Create a multiplayer session — register with server |
| `0x000B0011` | `XGISessionDelete` | Delete a session |
| `0x000B0012` | `XGISessionJoinLocal/Remote` | Join a session (local or remote player) |
| `0x000B0014` | `XSessionStart` | Start a session (begin ranked play) |
| `0x000B0015` | `XSessionEnd` | End a session (submit final results) |
| `0x000B0016` | `XSessionSearch` | Search for sessions (matchmaking) |
| `0x000B0018` | `XSessionModify` | Modify session slots/flags |
| `0x000B001A` | `XSessionArbitrationRegister` | Register for arbitration (anti-cheat) |
| `0x000B001B` | `XSessionSearchByID` | Search sessions by ID |
| `0x000B001C` | `XSessionSearchEx` | Extended session search |
| `0x000B001D` | `XSessionGetDetails` | Get session details (host info, slots) |
| `0x000B001E` | `XSessionMigrateHost` | Migrate host to another player |
| `0x000B001F` | `XSessionModifySkill` | Update TrueSkill ratings |
| `0x000B0020` | `XUserResetStatsView` | Reset a stats view |
| `0x000B0021` | `XUserReadStats` | **Read leaderboard data** |
| `0x000B0025` | `XSessionWriteStats` | **Write leaderboard scores** |
| `0x000B0026` | `XSessionFlushStats` | Flush pending stats |
| `0x000B0060` | `XSessionSearchByIds` | Search sessions by multiple IDs |
| `0x000B0065` | `XSessionSearchWeighted` | Weighted session search |

### Stubbed via REX_EXPORT_STUB (returns 0)
| Export | Purpose |
|--------|---------|
| `XamUserCreateStatsEnumerator` | Create a leaderboard/stats enumerator |
| `XamUserCreatePlayerEnumerator` | Create a player enumerator |
| `XamUserCreateTitlesPlayedEnumerator` | Create a titles-played enumerator |

### Returns INVALID_PARAMETER (needs implementation)
| Export | Purpose |
|--------|---------|
| `XamCreateEnumeratorHandle` | Generic enumerator creation — leaderboards use this |

### Dummy handles (session)
| Export | Current behavior |
|--------|-----------------|
| `XamSessionCreateHandle` | Returns handle `0xCAFEDEAD` |
| `XamSessionRefObjByHandle` | Returns obj `0xDEADF00D` |

### The online gate
| XGI/XLIVEBASE message | Current | Required |
|----------------------|---------|----------|
| `0x00058007` `CXLiveLogon::GetServiceInfo` | Returns `0x80151802` (ERROR_CONNECTION_INVALID) | Must return success with a valid `XONLINE_SERVICE_INFO` so the game believes Xbox Live is reachable |

This is the **primary gate**: as long as `GetServiceInfo` returns
`ERROR_CONNECTION_INVALID`, the game will not show online menus or attempt
any session/stats/content operations. Fixing this is the first step.

## What the server emulator must provide

### 1. Authentication & presence service
Emulates `CXLiveLogon::GetServiceInfo` and the Xbox Live logon flow:
- Return a valid `XONLINE_SERVICE_INFO` from `GetServiceInfo` (message
  `0x00058007`) so the game enters online mode.
- Assign a stable XUID per player (the game already gets a fake offline
  XUID `0xB13EBABEBABEBABE` — the server should issue real per-account XUIDs).
- Track which players are online (presence).

### 2. Matchmaking / session service
Emulates the `XSession*` family (XGI app messages `0x000B0010`–`0x000B0065`):
- **Session create** — register a new game session with the server. The
  game provides flags, public/private slot counts, and a session nonce.
  The server assigns a session ID and returns it.
- **Session search** — query the server for open sessions matching
  context/property filters (game mode, trial ID, skill bracket). This is
  how co-op partners find each other.
- **Session join** — register a player as joining a session. The server
  notifies the host (via the XNet/QoS layer) so they can accept the peer
  connection.
- **Session start/end** — mark a session as in-progress / completed.
- **Session details** — return host XNAddr (IP + port + online ID) so the
  joining client can connect peer-to-peer via Winsock.
- **Host migration** — reassign the host when the current host leaves.

The server does **not** relay gameplay data. It only facilitates the
handshake; actual co-op traffic flows P2P over the already-working
Winsock layer.

### 3. Stats / leaderboard service
Emulates `XSessionWriteStats`, `XSessionFlushStats`, `XUserReadStats`, and
the stats enumerator:
- **Write stats** — receive trial completion results (time, score, medals)
  from `XSessionWriteStats` (message `0x000B0025`). Store per-trial,
  per-player, per-difficulty.
- **Read stats** — serve leaderboard views from `XUserReadStats`
  (message `0x000B0021`) and `XamUserCreateStatsEnumerator`. The game
  requests a leaderboard view (e.g. "top 100 players for trial X") and the
  server returns ranked entries.
- **Creator leaderboard** — aggregate ratings of a player's uploaded trials
  into a creator ranking.
- **Enumerator plumbing** — implement `XamCreateEnumeratorHandle` and
  `XamUserCreateStatsEnumerator` to return real enumerator handles that
  `XamEnumerate` can read from.

### 4. UGC content sharing service
Emulates online content enumeration/upload/download:
- **Upload** — when a player creates and uploads a trial, the game writes
  it as an STFS package via `XamContentCreateEx` (which works locally).
  The server emulator needs to detect upload intent (likely a specific
  content type / flags) and transmit the STFS package to the server.
- **Download** — when a player browses community trials, the game calls
  `XamContentCreateEnumerator` with an online content type. The server
  returns metadata (trial name, creator, rating, download count) and the
  game downloads the STFS package, which `XamContentCreateEx` then
  installs locally.
- **Rating** — trial ratings (1-5 stars) are submitted as stats via
  `XSessionWriteStats` or as properties via `XGIUserSetPropertyEx`.

This is the most complex component because it bridges the local STFS
content system (which works) with a remote catalog.

### 5. Voice chat (optional)
`XamVoice*` (`XamVoiceCreate`, `XamVoiceSubmitPacket`, `XamVoiceClose`)
is currently stubbed. Voice chat is not required for co-op to function
but would need a voice relay/codec implementation for full parity.

## Recommended architecture

```
┌─────────────────────────────────────────────────────┐
│  Game (dantes_inferno.exe)                           │
│                                                       │
│  ┌─────────────┐   ┌──────────────┐   ┌────────────┐  │
│  │ XGI app     │   │ XLIVEBASE app│   │ XAM content│  │
│  │ (sessions,  │   │ (logon gate) │   │ (UGC)      │  │
│  │  stats)     │   │              │   │            │  │
│  └──────┬──────┘   └──────┬───────┘   └─────┬──────┘  │
│         │                 │                 │          │
│         └────────────┬────┘─────────────────┘          │
│                      │  SDK patch layer               │
│                      │  (routes stubs to server)       │
│                      ▼                                │
│  ┌──────────────────────────────────────────────────┐ │
│  │  NetDll Winsock (ALREADY WORKS — P2P gameplay)  │ │
│  └──────────────────────────────────────────────────┘ │
└─────────────────────────┬───────────────────────────┬──┘
                          │  TCP (signaling)          │  UDP (P2P gameplay)
                          ▼                           ▼
┌─────────────────────────────────────┐   ┌──────────────────┐
│  Server Emulator (new)               │   │  Peer-to-peer    │
│                                       │   │  co-op traffic   │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ │   │  (no server      │
│  │Matchmak.│ │Leaderbd │ │  UGC    │ │   │   relay needed)  │
│  │ service │ │ service │ │ service │ │   └──────────────────┘
│  └─────────┘ └─────────┘ └─────────┘ │
│         ┌──────────────┐              │
│         │  Database    │              │
│         │ (sessions,   │              │
│         │  stats, UGC) │              │
│         └──────────────┘              │
└───────────────────────────────────────┘
```

### Server components
1. **TCP signaling server** (custom protocol or HTTP/REST) — handles
   session, stats, and content metadata. Listens on a fixed port.
2. **Database** (SQLite or PostgreSQL) — stores:
   - Player accounts (XUID, gamertag, online state)
   - Game sessions (session ID, host XNAddr, slots, state, contexts/properties)
   - Leaderboard entries (trial ID, player XUID, score, time, medals, timestamp)
   - UGC trial catalog (trial ID, creator XUID, STFS package, rating, downloads)
3. **Content storage** (filesystem or blob store) — stores uploaded trial
   STFS packages.

### Client-side (SDK patches)
All patches go into the SDK source under `patches/sdk/` (same pattern as
existing patches):

1. **`xlivebase_app.cpp`** — Change `GetServiceInfo` (0x00058007) to return
   success with a valid `XONLINE_SERVICE_INFO` struct.
2. **`xgi_app.cpp`** — Replace the no-op `return X_E_SUCCESS` in each
   `XSession*` / `XUserReadStats` / `XSessionWriteStats` handler with a call
   to the server emulator client.
3. **`xam_content.cpp`** — Intercept `XamContentCreateEnumerator` for
   online content types to query the server's UGC catalog.
4. **`xam_enum.cpp` / `xam_user.cpp`** — Implement
   `XamCreateEnumeratorHandle` and `XamUserCreateStatsEnumerator` to create
   real enumerator objects backed by server data.
5. **`xam_user.cpp`** — Replace the dummy `XamSessionCreateHandle` /
   `XamSessionRefObjByHandle` with real session objects.

### Configuration
A cvar (e.g. `online_server_host` / `online_server_port`) in `OnPreSetup`
to point the game at the emulator server. Default to `localhost` for
development.

## Implementation phases

### Phase 1 — Open the online gate (smallest, highest impact)
- Patch `GetServiceInfo` to return success.
- Verify the game shows online menus and attempts session/stats calls.
- Log all unimplemented XAM/XGI messages the game sends when navigating
  the Trials online menus (this fills in any gaps in the table above).

### Phase 2 — Leaderboards (stats service)
- Implement `XSessionWriteStats` / `XSessionFlushStats` to POST results.
- Implement `XUserReadStats` + `XamUserCreateStatsEnumerator` +
  `XamCreateEnumeratorHandle` to serve leaderboard views.
- Server: simple stats DB + HTTP endpoint.
- This enables the Player and Creator leaderboards (offline trial
  completion can upload scores).

### Phase 3 — Matchmaking (session service)
- Implement `XGISessionCreateImpl`, `XSessionSearch`, `XSessionJoin*`,
  `XSessionStart`, `XSessionEnd`, `XSessionGetDetails`.
- Server: session registry + matchmaking.
- This enables online co-op (P2P over Winsock after handshake).

### Phase 4 — Trial sharing (UGC service)
- Intercept content enumeration for online trial content type.
- Implement upload (trial STFS → server) and download (server → local STFS).
- Implement trial rating (via stats or properties).
- Server: UGC catalog + blob storage.
- This enables the full create/upload/download/rate trial loop.

### Phase 5 — Voice chat (optional)
- Implement `XamVoice*` with a real codec/relay if co-op voice is desired.

## Key files to modify

| File | Changes |
|------|---------|
| `thirdparty/rexglue-sdk/src/kernel/xam/apps/xlivebase_app.cpp` | `GetServiceInfo` gate |
| `thirdparty/rexglue-sdk/src/kernel/xam/apps/xgi_app.cpp` | All `XSession*` and stats handlers |
| `thirdparty/rexglue-sdk/src/kernel/xam/xam_content.cpp` | Online content enumeration |
| `thirdparty/rexglue-sdk/src/kernel/xam/xam_enum.cpp` | `XamCreateEnumeratorHandle` |
| `thirdparty/rexglue-sdk/src/kernel/xam/xam_user.cpp` | `XamSessionCreateHandle`, `XamUserCreateStatsEnumerator` |
| `thirdparty/rexglue-sdk/src/kernel/xam/xam_net.cpp` | XNAddr/QoS for session host discovery |
| `patches/sdk/rexglue-sdk-v0.10.0.patch` | All SDK changes tracked here |
| `src/dantes_inferno_app.h` | `online_server_host` / `online_server_port` cvars in `OnPreSetup` |

## Open questions (require runtime RE)

1. **Exact `XONLINE_SERVICE_INFO` layout** the game expects from
   `GetServiceInfo` — need to dump the buffer the game passes and
   reverse the struct fields.
2. **Stats view spec format** — `XUserReadStats` takes a "specs" array
   describing which stats/columns to read. Need to RE the game's spec
   definitions to know the leaderboard column IDs.
3. **UGC content type ID** — which `XContentType` value the game uses for
   user-created trials (to distinguish from save data and DLC).
4. **Session context/property IDs** — which context and property IDs the
   game sets for matchmaking (game mode, trial ID, difficulty, etc.).
5. **Whether the game uses arbitration** (`XSessionArbitrationRegister`)
   for ranked co-op — if so, the server must implement the arbitration
   protocol (anti-cheat result submission).

These can be answered by Phase 1 (open the gate + log everything the game
sends when navigating online menus).
