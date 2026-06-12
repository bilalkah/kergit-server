#!/usr/bin/env python3

import argparse
import ipaddress
import json
import os
from pathlib import Path
import re
import sys


NODE_FIELDS = {
    "id",
    "signal_port",
    "rtc_tcp_port",
    "rtc_udp_start",
    "rtc_udp_end",
    "prometheus_port",
    "node_ip",
}
NODE_ID_RE = re.compile(r"^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$")
RESERVED_NODE_IDS = {
    "admin-node",
    "server-node",
    "caddy-node",
    "redis-node",
    "web-node",
    "web-node-dev",
    "web-node-prod",
}
RESERVED_HOST_TCP_PORTS = {80, 443, 3000, 3001, 6379, 8080, 8081, 9001}
LIVEKIT_COMPOSE_SCHEMA_VERSION = 1


def fail(message: str) -> None:
    raise ValueError(f"LIVEKIT_NODES {message}")


def parse_port(node: dict, field: str) -> int:
    value = node.get(field)
    if type(value) is not int or not 1 <= value <= 65535:
        fail(f"field '{field}' must be an integer between 1 and 65535")
    return value


def parse_nodes(raw: str, production: bool) -> list[dict]:
    try:
        nodes = json.loads(raw)
    except json.JSONDecodeError as exc:
        fail(f"must be valid JSON: {exc.msg}")

    if not isinstance(nodes, list) or not nodes:
        fail("must be a non-empty JSON array")

    parsed: list[dict] = []
    ids: set[str] = set()
    tcp_ports: set[int] = set()
    udp_ranges: list[tuple[int, int]] = []

    for node in nodes:
        if not isinstance(node, dict):
            fail("entries must be objects")
        unknown = set(node) - NODE_FIELDS
        if unknown:
            fail(f"contains unknown field '{sorted(unknown)[0]}'")

        node_id = node.get("id")
        if not isinstance(node_id, str) or not NODE_ID_RE.fullmatch(node_id):
            fail("node ids must use lowercase letters, digits, and internal hyphens")
        if node_id in RESERVED_NODE_IDS:
            fail(f"node id '{node_id}' collides with a shared service")
        if node_id in ids:
            fail(f"contains duplicate node id '{node_id}'")
        if any(node_id.startswith(existing) or existing.startswith(node_id) for existing in ids):
            fail("node ids must not be path prefixes")
        ids.add(node_id)

        signal_port = parse_port(node, "signal_port")
        rtc_tcp_port = parse_port(node, "rtc_tcp_port")
        rtc_udp_start = parse_port(node, "rtc_udp_start")
        rtc_udp_end = parse_port(node, "rtc_udp_end")
        prometheus_port = parse_port(node, "prometheus_port")
        if rtc_udp_start > rtc_udp_end:
            fail("rtc_udp_start must not exceed rtc_udp_end")

        for port in (signal_port, rtc_tcp_port, prometheus_port):
            if port in RESERVED_HOST_TCP_PORTS:
                fail(f"TCP port {port} collides with a shared service")
            if port in tcp_ports:
                fail(f"contains duplicate TCP port {port}")
            tcp_ports.add(port)

        for existing_start, existing_end in udp_ranges:
            if rtc_udp_start <= existing_end and existing_start <= rtc_udp_end:
                fail("contains overlapping RTC UDP ranges")
        udp_ranges.append((rtc_udp_start, rtc_udp_end))

        node_ip = node.get("node_ip", "")
        if not isinstance(node_ip, str):
            fail("field 'node_ip' must be a string")
        if node_ip:
            try:
                ipaddress.ip_address(node_ip)
            except ValueError:
                fail("field 'node_ip' must be a valid IP address")
        if production and not node_ip:
            fail("production entries require node_ip")

        parsed.append(
            {
                "id": node_id,
                "signal_port": signal_port,
                "rtc_tcp_port": rtc_tcp_port,
                "rtc_udp_start": rtc_udp_start,
                "rtc_udp_end": rtc_udp_end,
                "prometheus_port": prometheus_port,
                "node_ip": node_ip,
            }
        )

    return parsed


def render_node_config(template: str, node: dict, api_key: str, api_secret: str,
                       webhook_url: str, production: bool) -> str:
    node_ip_settings = ""
    if production:
        node_ip_settings = (
            "  enable_loopback_candidate: false\n"
            "  use_external_ip: true\n"
            f"  node_ip: {node['node_ip']}\n"
        )

    replacements = {
        "__SIGNAL_PORT__": str(node["signal_port"]),
        "__RTC_TCP_PORT__": str(node["rtc_tcp_port"]),
        "__RTC_UDP_START__": str(node["rtc_udp_start"]),
        "__RTC_UDP_END__": str(node["rtc_udp_end"]),
        "__NODE_IP_SETTINGS__": node_ip_settings,
        "__NODE_ID__": node["id"],
        "__LIVEKIT_API_KEY__": api_key,
        "__LIVEKIT_API_SECRET__": api_secret,
        "__LIVEKIT_WEBHOOK_URL__": webhook_url,
        "__PROMETHEUS_PORT__": str(node["prometheus_port"]),
    }
    for placeholder, value in replacements.items():
        template = template.replace(placeholder, value)
    return template


def render_compose(nodes: list[dict], config_dir: Path) -> str:
    lines = [f"# kergit-livekit-compose-version: {LIVEKIT_COMPOSE_SCHEMA_VERSION}", "services:"]
    for node in nodes:
        node_id = node["id"]
        config_path = str((config_dir / f"{node_id}.yaml").resolve())
        lines.extend(
            [
                f"  {node_id}:",
                "    image: livekit/livekit-server:v1.9",
                f"    container_name: {node_id}",
                '    command: ["--config", "/config/livekit.yaml"]',
                "    ports:",
                f'      - "{node["signal_port"]}:{node["signal_port"]}"',
                f'      - "{node["rtc_tcp_port"]}:{node["rtc_tcp_port"]}"',
                f'      - "{node["prometheus_port"]}:{node["prometheus_port"]}"',
                f'      - "{node["rtc_udp_start"]}-{node["rtc_udp_end"]}:{node["rtc_udp_start"]}-{node["rtc_udp_end"]}/udp"',
                "    volumes:",
                f"      - {json.dumps(config_path + ':/config/livekit.yaml:ro')}",
                "    restart: unless-stopped",
                "    depends_on:",
                "      - redis-node",
            ]
        )
    return "\n".join(lines) + "\n"


def render_caddy_routes(nodes: list[dict]) -> str:
    blocks: list[str] = []
    for node in nodes:
        node_id = node["id"]
        blocks.append(
            f"""handle_path /admin-livekit-metrics/{node_id}* {{
  rewrite * /metrics
  reverse_proxy {node_id}:{node["prometheus_port"]}
}}

handle_path /livekit/{node_id}* {{
  import livekit_gateway {node_id}:{node["signal_port"]}
}}"""
        )
    return "\n\n".join(blocks) + "\n"


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("dev", "prod"), required=True)
    parser.add_argument("--template", type=Path, required=True)
    parser.add_argument("--config-dir", type=Path, required=True)
    parser.add_argument("--compose-output", type=Path, required=True)
    parser.add_argument("--caddy-output", type=Path, required=True)
    args = parser.parse_args()

    try:
        nodes = parse_nodes(os.environ.get("LIVEKIT_NODES", ""), args.mode == "prod")
        template = args.template.read_text()
        api_key = os.environ["LIVEKIT_API_KEY"]
        api_secret = os.environ["LIVEKIT_API_SECRET"]
        webhook_url = os.environ["LIVEKIT_WEBHOOK_URL"]

        args.config_dir.mkdir(parents=True, exist_ok=True)
        for stale in args.config_dir.glob("*.yaml"):
            stale.unlink()
        for node in nodes:
            write_text(
                args.config_dir / f"{node['id']}.yaml",
                render_node_config(template, node, api_key, api_secret, webhook_url,
                                   args.mode == "prod"),
            )
        write_text(args.compose_output, render_compose(nodes, args.config_dir))
        write_text(args.caddy_output, render_caddy_routes(nodes))
    except (KeyError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(" ".join(node["id"] for node in nodes))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
