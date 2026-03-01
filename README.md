# Sercom Server

Private development repository for the Sercom realtime chat and voice stack.

## What Is Here

- `server/`, `app/`, `net/`, `control/`: C++ socket server, protocol handling, control-plane pieces
- `clients/web/`: main Nuxt web client
- `clients/admin/`: Nuxt admin dashboard
- `docker/`: local Docker stack for the server, Caddy, Redis, and LiveKit

This README is intentionally short and does not include credentials, private endpoints, or deployment secrets. Supply runtime configuration through the repo-root `.env` file or environment variables.

## Run The Full Stack

From the repo root:

```bash
./docker/run-server.sh
```

Useful variants:

```bash
./docker/run-server.sh --prod
./docker/run-server.sh --multi
./docker/run-server.sh --server-only
```

Default local endpoints:

- Web app: `http://localhost:3000`
- Admin app: `http://localhost:3001`
- Edge proxy: `https://localhost`
- Socket server: `ws://localhost:9001` behind local development routing

To stop everything:

```bash
./docker/stop-server.sh
```

## Notes

- Docker scripts load `.env` from the repo root when present.
- The full stack starts Redis, LiveKit, Caddy, the C++ server, and both Nuxt clients unless `--server-only` is used.
- For client-specific details, see [clients/web/README.md](/Users/bilal/repos/sercom-server/clients/web/README.md) and [clients/admin/README.md](/Users/bilal/repos/sercom-server/clients/admin/README.md).
