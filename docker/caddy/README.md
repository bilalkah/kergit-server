# Caddy Config Selection

This project uses a single `caddy` service and selects config by directory.

## Config Files
- Dev: `docker/caddy/conf.dev/Caddyfile`
- Prod: `docker/caddy/conf.prod/Caddyfile`

## Selection Logic
- Default: `conf.dev`
- `./docker/run-server.sh --prod`: sets `CADDY_CONF_DIR=conf.prod`

Compose mount (from `docker/docker-compose.yml`):
- `./caddy/${CADDY_CONF_DIR:-conf.dev}:/etc/caddy:ro`

Caddy always reads:
- `/etc/caddy/Caddyfile`

## Environment Differences
- Dev config is localhost-oriented.
- Prod config is domain-oriented (`kergit.com`) and does not include localhost routes/origins.

## Manual Run Examples
- Dev:
  - `CADDY_CONF_DIR=conf.dev docker compose -p sercom -f docker/docker-compose.yml up -d caddy`
- Prod:
  - `CADDY_CONF_DIR=conf.prod docker compose -p sercom -f docker/docker-compose.yml up -d caddy`
