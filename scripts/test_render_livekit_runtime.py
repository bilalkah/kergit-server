import importlib.util
from pathlib import Path
import tempfile
import unittest


MODULE_PATH = Path(__file__).with_name("render_livekit_runtime.py")
SPEC = importlib.util.spec_from_file_location("render_livekit_runtime", MODULE_PATH)
assert SPEC and SPEC.loader
RENDERER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RENDERER)


ONE_NODE = (
    '[{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,'
    '"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789}]'
)


class RenderLivekitRuntimeTest(unittest.TestCase):
    def test_one_node_rendering(self):
        nodes = RENDERER.parse_nodes(ONE_NODE, False)
        with tempfile.TemporaryDirectory() as tmp:
            compose = RENDERER.render_compose(nodes, Path(tmp))
        routes = RENDERER.render_caddy_routes(nodes)
        self.assertTrue(compose.startswith("# kergit-livekit-compose-version: 1\nservices:\n"))
        self.assertIn("node-a:", compose)
        self.assertIn("      - redis-node", compose)
        self.assertIn("/livekit/node-a", routes)
        self.assertIn("/admin-livekit-metrics/node-a", routes)

    def test_three_node_rendering(self):
        raw = (
            '[{"id":"node-a","signal_port":7880,"rtc_tcp_port":7881,'
            '"rtc_udp_start":50000,"rtc_udp_end":50100,"prometheus_port":6789},'
            '{"id":"node-b","signal_port":7890,"rtc_tcp_port":7891,'
            '"rtc_udp_start":50101,"rtc_udp_end":50200,"prometheus_port":6790},'
            '{"id":"node-c","signal_port":7900,"rtc_tcp_port":7901,'
            '"rtc_udp_start":50201,"rtc_udp_end":50300,"prometheus_port":6791}]'
        )
        nodes = RENDERER.parse_nodes(raw, False)
        self.assertEqual([node["id"] for node in nodes], ["node-a", "node-b", "node-c"])
        with tempfile.TemporaryDirectory() as tmp:
            compose = RENDERER.render_compose(nodes, Path(tmp))
        routes = RENDERER.render_caddy_routes(nodes)
        self.assertEqual(compose.count("image: livekit/livekit-server"), 3)
        self.assertEqual(routes.count("handle_path /livekit/"), 3)

    def test_validation(self):
        with self.assertRaises(ValueError):
            RENDERER.parse_nodes("[]", False)
        with self.assertRaises(ValueError):
            RENDERER.parse_nodes(ONE_NODE, True)
        with self.assertRaises(ValueError):
            RENDERER.parse_nodes(ONE_NODE.replace('"id":"node-a"', '"id":"Bad_Node"'), False)
        with self.assertRaises(ValueError):
            RENDERER.parse_nodes(ONE_NODE.replace('"id":"node-a"', '"id":"redis-node"'), False)
        with self.assertRaises(ValueError):
            RENDERER.parse_nodes(ONE_NODE.replace('"id":"node-a"', '"id":"server-node"'), False)
        with self.assertRaises(ValueError):
            RENDERER.parse_nodes(ONE_NODE.replace('"signal_port":7880', '"signal_port":9001'), False)
        with self.assertRaises(ValueError):
            RENDERER.parse_nodes(
                ONE_NODE[:-1]
                + ',{"id":"node-ab","signal_port":7890,"rtc_tcp_port":7891,'
                + '"rtc_udp_start":50101,"rtc_udp_end":50200,"prometheus_port":6790}]',
                False,
            )


if __name__ == "__main__":
    unittest.main()
