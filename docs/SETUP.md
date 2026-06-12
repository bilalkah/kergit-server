# Setup Guide

## Prerequisites

Choose the path you want to use before installing tools.

### Full-stack Docker workflow

Required:

- Docker
- Docker Compose v2
- a populated local `.env` copied from `.env.example`

Recommended:

- `mkcert` for the default HTTPS development flow

`./docker/run-server.sh` calls `scripts/certs/generate-dev-cert.sh` in normal dev mode, so HTTPS localhost development expects either `mkcert` or already-generated dev cert files in `certs/dev/`.

### Native / direct build workflow

Required:

- Bazel or Bazelisk
- a working C++ toolchain compatible with the repo’s Bazel targets
- `pnpm` for the Nuxt clients

## 1. Configure Environment Variables

Initialize the client submodules after cloning:

```bash
git submodule update --init --recursive
```

Copy the example file and replace placeholder values:

```bash
cp .env.example .env
```

Then fill in your own:

- PostgreSQL / Supabase DB values
- your database provider's CA certificate path when `DB_SSL=1`
- Supabase auth/storage values
- LiveKit API key/secret
- the canonical `WEB_DOMAIN` app origin

See `docs/ENVIRONMENT.md` for the variable reference.

## 2. Full Local Stack

From the repo root:

```bash
./docker/run-server.sh
```

This path starts:

- the C++ websocket/control server
- Redis
- every LiveKit node configured in `LIVEKIT_NODES`
- Caddy
- the admin client
- the web client

Useful variants from the existing script:

```bash
./docker/run-server.sh --prod
```

Stop everything with:

```bash
./docker/stop-server.sh
```

## 3. Backend-Only Setup

To start only the backend container path:

```bash
./docker/services/backend/run-container.sh
./docker/services/backend/exec-server.sh
```

Use `--prod` with both scripts for the optimized backend configuration.

## 4. Run the Web Client Without the Full Stack

From `clients/web/`:

```bash
pnpm install
./run_nuxt_dev.sh
```

If the generated protobuf client files are missing, run:

```bash
./docker/generate-proto.sh
```

before running `pnpm test` or direct client development outside Docker.

## 5. Run the Admin Client Without the Full Stack

From `clients/admin/`:

```bash
pnpm install
./run_nuxt_dev.sh
```

## 6. Build and Test Commands

Backend:

```bash
bazel build //...
bazel test //...
```

Web client tests:

```bash
cd clients/web
pnpm install
./docker/generate-proto.sh
pnpm test
```

Formatting:

```bash
bazel run //:list_format_files
bazel run //:format
```

## Troubleshooting

### `mkcert` is missing

`docker/run-server.sh` uses `scripts/certs/generate-dev-cert.sh` for dev TLS. If `mkcert` is unavailable and no local fallback certs exist, the full-stack HTTPS path will fail.

### Production cert files are missing

`./docker/run-server.sh --prod` expects:

- `certs/prod/origin.pem`
- `certs/prod/origin-key.pem`

These are intentionally gitignored and must be provided locally.

Production mode also requires `WEB_DOMAIN` in the root `.env` to be set to the
deployment's non-local HTTPS origin.

### Web tests fail because generated protobuf files are missing

Run:

```bash
./clients/web/docker/generate-proto.sh
```

and then rerun `pnpm test`.

### Database SSL test fails

`infra/persistence/test/connection_ssl_test.cc` now skips unless `DB_HOST`, `DB_NAME`, `DB_USER`, `DB_PASSWORD`, and `DB_SSL=1` are configured. This test is for a real SSL-backed database target, not a fake local default.

When `DB_SSL=1`, place your provider's CA certificate at
`certs/database/root.crt` or set `DB_SSL_ROOT_CERT` to another local path. CA
certificate files are intentionally gitignored.

### Admin endpoints return 403

The Caddy configs intentionally restrict `/admin*`, `/admin-api*`, and
`/admin-livekit-metrics*` to the explicit `ADMIN_ALLOWED_CIDR` allowlist.
Production rejects `private_ranges`; use a specific trusted CIDR or
`127.0.0.1/32` to disable public admin access.
