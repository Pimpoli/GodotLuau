# GodotLuau — Backend (local dev)

Local **Nakama + PostgreSQL** stack for GodotLuau's networking layer (DataStore, auth,
world server). This folder is the "central backend" that games talk to. For now it runs
entirely on your machine; the same `docker-compose.yml` deploys to a VPS later unchanged.

## Requirements

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) (includes `docker compose`).

## Run

```bash
cd backend
docker compose up -d              # start Nakama + Postgres in the background
docker compose logs -f nakama     # watch the server log (Ctrl+C to stop watching)
docker compose down               # stop (data is kept in the volume)
docker compose down -v            # stop AND wipe the database
```

| What | URL / value |
|---|---|
| HTTP API (what `HttpService` calls) | `http://127.0.0.1:7350` |
| Admin console | `http://127.0.0.1:7351` — user `admin`, password `password` |
| Server key (for the client later) | `defaultkey` |

## Phase 1 test — is `HttpService` alive?

Once the extension is rebuilt, drop this into a **ServerScript** and press Play:

```lua
local Http = game:GetService("HttpService")

-- 1) Any HTTPS endpoint proves real HTTP + TLS work (was returning "" before).
print("GET zen:", Http:GetAsync("https://api.github.com/zen"))

-- 2) Local Nakama proves the backend is reachable (start docker compose first).
local r = Http:RequestAsync({ Url = "http://127.0.0.1:7350/", Method = "GET" })
print("Nakama status:", r.StatusCode, r.StatusMessage)
```

Before Phase 1 both calls returned an empty string with a `[HttpService] … sin conexión`
warning. Now `GetAsync` returns the real body and `RequestAsync` returns the real HTTP
status code.

## Roadmap

- **Phase 3** — `NakamaService` (C++ singleton, WebSocket + non-blocking HTTP) exposed to Luau.
- **Phase 4** — route the existing `DataStoreService` to Nakama Storage (server-authoritative,
  with optimistic-concurrency `UpdateAsync`); local-disk fallback stays for offline/single-player.
- **Phase 5** — authoritative world server (Nakama match handlers) + networked `RemoteEvent`s.

Server-side modules (Lua/Go/TS) will live under `backend/modules/` and are mounted into the
container automatically (see the volume in `docker-compose.yml`).
