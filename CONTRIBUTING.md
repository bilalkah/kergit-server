# Contributing

Thanks for taking a look at Kergit.

## Scope

This repository is an open-source advanced prototype. Contributions are welcome, but the first priority is correctness, safety, and clear documentation—not feature sprawl.

## Build

Backend:

```bash
bazel build //...
```

Full local stack:

```bash
./docker/run-server.sh
```

## Test

Backend:

```bash
bazel test //...
```

Web client:

```bash
cd clients/web
pnpm install
./docker/generate-proto.sh
pnpm test
```

## Formatting and Style

Use the existing repo tooling:

```bash
bazel run //:list_format_files
bazel run //:format
```

Expectations:

- keep changes focused and minimal
- do not introduce unrelated refactors
- keep protobuf, websocket, and voice flows consistent with existing patterns
- update docs when behavior or setup changes

## Reporting Issues

When filing an issue, include:

- the area affected (`backend`, `web client`, `admin`, `docker`, `docs`, `voice`)
- reproduction steps
- expected vs actual behavior
- relevant logs or error messages

For security-sensitive issues, follow `docs/SECURITY.md` instead of opening a public exploit report.

## Experimental Areas

The following areas are still more experimental than the core text-chat path:

- browser voice/video UX
- screen share flows
- multi-node LiveKit routing
- admin/operator tooling
- reconnect edge cases around session ownership

## Non-Goals for Early Contributions

Please avoid opening broad PRs for:

- Kafka
- microservices
- Kubernetes
- multi-region deployment
- large architectural rewrites

The near-term goal is a safe and understandable public baseline.
