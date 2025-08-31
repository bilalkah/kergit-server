#!/usr/bin/env python3
import sys
from pathlib import Path

def main():
    if len(sys.argv) != 2:
        print("usage: gen_config.py PATH_TO_ENV", file=sys.stderr)
        return 2

    env_path = Path(sys.argv[1])
    out = []

    if env_path.exists():
        for raw in env_path.read_text().splitlines():
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            k = k.strip()
            v = v.strip().replace('\\', '\\\\').replace('"', '\\"')
            out.append(f'  {k}: "{v}",')

    print("// auto-generated from .env")
    print("export const CONFIG = {")
    for row in out:
        print(row)
    print("};")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
