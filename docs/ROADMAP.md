# Roadmap

## Current Priorities

- fix the C++ standard/build-setting drift affecting some full backend targets
- remove previously committed credentials and personal data from published history, then rotate or revoke affected credentials
- improve integration coverage for auth, state sync, and voice ownership paths
- verify the public docs against a fresh-clone setup flow

## Near-Term Work

- add a small browser E2E path for login -> hub -> channel -> message send
- split large client transport/state files into narrower modules
- make link preview metadata server-authoritative
- tighten admin/control-plane auth and exposure assumptions

## Good Engineering Improvements

- isolate more of `VoiceService` into smaller testable units
- centralize attachment policy shared by client and server
- reduce repeated attachment signed-URL refresh work in the message list
- add more structured logging around reconnect and revoke flows
- reduce duplicated policy/config validation between client and server
- improve contributor guidance for protobuf regeneration and local dev

## Explicit Non-Goals

- no Kafka yet
- no microservices yet
- no Kubernetes yet
- no multi-region yet

The near-term goal is a safe, understandable, technically credible codebase—not infrastructure breadth.
