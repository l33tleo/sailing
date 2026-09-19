"""Baker et globalt avstand-til-kyst-felt for øyene → scripts/cache/bake/T_ShoreDistance.png + .json.

Kjør (etter bake_land.py):
  uv run --with numpy --with scipy --with pillow python scripts/bake/bake_shore_field.py

Teksturen dekker alle øyenes utsnitt med RES_M meter per piksel, rad 0 = nord, og lagrer
saturate(avstand_til_land / RANGE_M) i 8 bit (0 = land, 255 = ≥ RANGE_M fra land). Vannmaterialet
sampler den på verdens-XY (som flyfotoet i M_Land) og former strandskummet av den — WaterInfo-
teksturen til Water-pluginet er for grov (24×40 km på 2048 px) til et pent skumbånd.
Fastlandet er ikke med (det er fortsatt prosedural og bakes senere).
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "bake"))
from bake_land import DTM_DIR, OUT_DIR  # noqa: E402

RES_M = 4.0
RANGE_M = 40.0
MARGIN_M = 100.0


def main() -> None:
    metas = [json.loads(p.read_text(encoding="utf-8")) for p in sorted(DTM_DIR.glob("*.json"))]
    metas = [m for m in metas if (OUT_DIR / f"{m['slug']}_fields.npz").exists()]
    if not metas:
        sys.exit("ingen *_fields.npz — kjør bake_land.py først")

    min_x = min(m["min_x_m"] for m in metas) - MARGIN_M
    max_x = max(m["max_x_m"] for m in metas) + MARGIN_M
    min_y = min(m["min_y_m"] for m in metas) - MARGIN_M
    max_y = max(m["max_y_m"] for m in metas) + MARGIN_M
    w, h = int(np.ceil((max_x - min_x) / RES_M)), int(np.ceil((max_y - min_y) / RES_M))
    land = np.zeros((h, w), dtype=bool)

    for m in metas:
        f = np.load(OUT_DIR / f"{m['slug']}_fields.npz")["land"]
        step = int(round(RES_M / m["res_m"]))
        # Maks-pooling: en 4 m-celle er land hvis noen 1 m-celle i den er land.
        hh, ww = f.shape[0] // step * step, f.shape[1] // step * step
        pooled = f[:hh, :ww].reshape(hh // step, step, ww // step, step).any(axis=(1, 3))
        c0 = int(round((m["min_x_m"] - min_x) / RES_M))
        r0 = int(round((max_y - m["max_y_m"]) / RES_M))
        land[r0:r0 + pooled.shape[0], c0:c0 + pooled.shape[1]] |= pooled

    dist = ndimage.distance_transform_edt(~land) * RES_M
    img = np.clip(dist / RANGE_M, 0.0, 1.0)
    Image.fromarray((img * 255).astype(np.uint8), mode="L").save(OUT_DIR / "T_ShoreDistance.png")
    (OUT_DIR / "T_ShoreDistance.json").write_text(json.dumps({
        "min_x_m": min_x, "max_x_m": max_x, "min_y_m": min_y, "max_y_m": max_y,
        "res_m": RES_M, "range_m": RANGE_M, "width": w, "height": h, "row0_is_north": True,
    }, indent=2), encoding="utf-8")
    print(f"T_ShoreDistance.png {w}x{h} px, {RES_M} m/px, land {land.mean() * 100:.1f} %")


if __name__ == "__main__":
    main()
