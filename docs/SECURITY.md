# Security Notes

## Secret Handling

The tracked tree must not contain:

- real `.env` files
- private TLS keys or generated certificates
- production-only LiveKit secrets
- Supabase service-role keys
- internal production hostnames or IP addresses

Use `.env.example` as a template and keep your real values in a local `.env`, which is gitignored.

Before publishing a repository or fork, scan its full Git history and remove
previously committed credentials or personal data. Rotate or revoke any affected
credential even after removing it from history.

## Local Secret Configuration

1. Copy `.env.example` to `.env`.
2. Replace every placeholder value with your own credentials.
3. Generate or place local TLS certificates under:
   - `certs/dev/`
   - `certs/prod/`
4. Never commit those files.

## Admin and Control-Plane Exposure Warning

The admin UI and metrics endpoints are **not** intended for open public exposure.

Relevant paths:

- `/admin`
- `/admin-api`
- `/admin-livekit-metrics`

The Caddy configs restrict these routes to the explicit `ADMIN_ALLOWED_CIDR`
allowlist and fail closed to loopback-only by default. Production startup rejects
the broad `private_ranges` matcher because Docker and reverse proxies can make
public requests appear to originate from a private address. Keep an explicit
allowlist or add stronger authentication in any real deployment.

The production Caddy config trusts `CF-Connecting-IP` only when the immediate
proxy belongs to Cloudflare's published IP ranges. This allows
`ADMIN_ALLOWED_CIDR` to match the original visitor while preventing direct
callers from spoofing the header. Keep the configured Cloudflare ranges current
and restrict direct origin access at the firewall when practical.

The backend control-plane endpoints in `control/http/HttpServer.cpp` currently serve permissive CORS headers. Treat them as operator surfaces, not public APIs.

The local Docker defaults also bind the backend control-plane ports (`8080`, `8081`) and the standalone admin dev port (`3001`) to loopback-only addresses where practical. Do not loosen those bindings unless you are deliberately operating inside a trusted network.

## LiveKit Webhook and Auth Notes

- `LIVEKIT_API_KEY` and `LIVEKIT_API_SECRET` must match the key/secret pair configured in the LiveKit YAML files.
- Voice join uses server-issued LiveKit tokens; the browser should never mint its own.
- LiveKit webhook delivery should only target a server endpoint you control.
- Keep webhook URLs private to your deployment/network whenever possible.

## Link Preview / SSRF Notes

The repo contains a server-side link preview endpoint at `clients/web/server/api/chat/link-preview.post.ts`.

Current behavior includes DNS, private-address, and redirect restrictions, which is good, but this path still deserves care:

- do not broaden outbound network access casually,
- do not disable the private-address checks,
- do not assume link previewing is “safe by default” in arbitrary deployment environments.

Treat this endpoint as a hardened convenience feature, not a proof that SSRF is solved forever.

## Responsible Disclosure

- Do **not** include exploit details, credentials, or personal data in a public issue.
- Use GitHub private vulnerability reporting when it is enabled for the repository.
- If no private channel is available, open a minimal public issue requesting a
  private contact method without including sensitive details.

A dedicated security contact and response policy are not currently published.
