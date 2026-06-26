#!/usr/bin/env python3
"""
Fetch Kartverket nautical chart image for Oslofjord and save as PNG.
Uses same WMS as mcp-kartverket. Output: Content/Charts/OslofjordChart.png (or --output path).

Usage:
  python scripts/fetch_oslofjord_chart.py
  python scripts/fetch_oslofjord_chart.py --output Content/Charts/OslofjordChart.png --width 1024 --height 1024
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Add mcp-kartverket so we can import wms
SCRIPT_DIR = Path(__file__).resolve().parent
MCP_DIR = SCRIPT_DIR / "mcp-kartverket"
if not MCP_DIR.exists():
    sys.exit("mcp-kartverket not found; run from repo root")
sys.path.insert(0, str(MCP_DIR))

from wms import get_map  # noqa: E402

# Oslofjord WGS84 bbox (lon, lat): roughly 10.4-11.2 lon, 59.7-60.0 lat
DEFAULT_BBOX = (10.4, 59.7, 11.2, 60.0)
# Layer for Oslofjord area (kart549 is one option; overseiling is overview)
DEFAULT_LAYER = "kart549"
DEFAULT_WIDTH = 1024
DEFAULT_HEIGHT = 1024


def main() -> int:
    ap = argparse.ArgumentParser(description="Fetch Oslofjord chart from Kartverket WMS")
    ap.add_argument("--bbox", nargs=4, type=float, default=DEFAULT_BBOX,
                    metavar=("min_lon", "min_lat", "max_lon", "max_lat"),
                    help="WGS84 bbox (default: Oslofjord)")
    ap.add_argument("--layer", type=str, default=DEFAULT_LAYER, help="WMS layer name")
    ap.add_argument("--width", type=int, default=DEFAULT_WIDTH, help="Image width")
    ap.add_argument("--height", type=int, default=DEFAULT_HEIGHT, help="Image height")
    ap.add_argument("--output", "-o", type=Path,
                    default=Path("Content/Charts/OslofjordChart.png"),
                    help="Output PNG path")
    args = ap.parse_args()

    bbox = tuple(args.bbox)
    if len(bbox) != 4:
        print("bbox must have 4 values: min_lon min_lat max_lon max_lat", file=sys.stderr)
        return 1

    out_path = args.output
    out_path.parent.mkdir(parents=True, exist_ok=True)

    print(f"Fetching chart: bbox={bbox}, layer={args.layer}, size={args.width}x{args.height}...")
    png_bytes = get_map(bbox, args.layer, args.width, args.height)
    out_path.write_bytes(png_bytes)
    print(f"Saved {len(png_bytes)} bytes to {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
