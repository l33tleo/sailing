"""Baker plassering av trær/busker per øy → scripts/cache/bake/veg_<slug>.json.

Kjør (etter bake_land.py, make_trees.py og fetch_oslofjord_features_osm.py):
  uv run --with numpy --with scipy --with pillow --with trimesh --with fast-simplification \
      python scripts/bake/bake_vegetation.py [--only Hovedøya]

Regler
- Tett skog i OSM-skogflater (natural=wood/scrub, landuse=forest), spredte trær ellers på land.
- Aldri: i/ved bygninger, på strand/bart fjell (OSM), brattere enn ~35°, lavere enn 1,5 m o.h.
- Arter: gran i lune søkk inne i skog, bjørk i lavt og flatt terreng, ellers furu (koller, berg,
  kystnært). Einer/kratt i kystbeltet og som undervegetasjon i skogkant.
- Deterministisk: seed fra øynavnet.
Format: {"sets": {"<mesh-asset>": [x_cm, y_cm, z_cm, yaw_deg, scale, …]}} i øyas lokale rom
(pivot = Position, z=0 = middelvannstand) — samme rom som den bakte terrengmeshen.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from scipy import ndimage

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(SCRIPTS / "bake"))
from fjord_slug import slugify  # noqa: E402
from bake_land import DTM_DIR, OUT_DIR, rasterize  # noqa: E402

FOREST_SPACING_M = 6.5
OPEN_SPACING_M = 9.0
OPEN_TREE_PROB = 0.10          # spredte trær utenfor skogflatene
BUSH_SPACING_M = 7.0
MIN_HEIGHT_M = 1.5
MAX_SLOPE_TAN = 0.7


def jittered_points(shape, res, spacing, rng):
    """Jitret rutenett (blå-støy-aktig, billig): ett punkt per celle på spacing × spacing."""
    step = spacing / res
    rows = np.arange(step / 2, shape[0] - 1, step)
    cols = np.arange(step / 2, shape[1] - 1, step)
    r, c = np.meshgrid(rows, cols, indexing="ij")
    r = r + rng.uniform(-0.45, 0.45, r.shape) * step
    c = c + rng.uniform(-0.45, 0.45, c.shape) * step
    r = np.clip(r.ravel(), 0, shape[0] - 1)
    c = np.clip(c.ravel(), 0, shape[1] - 1)
    return r, c


def mask_of(features, meta, shape):
    out = np.zeros(shape, dtype=bool)
    for f in features:
        ring = f.get("ring")
        if not ring:
            continue
        xs, ys = [p[0] for p in ring], [p[1] for p in ring]
        if max(xs) < meta["min_x_m"] or min(xs) > meta["max_x_m"] \
                or max(ys) < meta["min_y_m"] or min(ys) > meta["max_y_m"]:
            continue
        out |= rasterize(ring, meta, shape)
    return out


def bake(isl, features, trees, exaggeration):
    name, slug = isl["Name"], slugify(isl["Name"])
    fields_p = OUT_DIR / f"{slug}_fields.npz"
    if not fields_p.exists():
        print(f"{name}: mangler {fields_p.name} (kjør bake_land.py)", file=sys.stderr)
        return None
    meta = json.loads((DTM_DIR / f"{slug}.json").read_text(encoding="utf-8"))
    res = meta["res_m"]
    fields = np.load(fields_p)
    height, land = fields["height"], fields["land"]
    shape = height.shape
    rng = np.random.default_rng(int(hashlib.sha1(("veg" + slug).encode()).hexdigest()[:8], 16))

    forest = mask_of(features["forest"], meta, shape) & land
    blocked = ndimage.binary_dilation(mask_of(features["building"], meta, shape), iterations=int(4 / res))
    blocked |= mask_of(features["beach"], meta, shape) | mask_of(features["bare_rock"], meta, shape)

    gy, gx = np.gradient(height, res)
    slope = np.hypot(gx, gy)
    d_coast = ndimage.distance_transform_edt(land) * res
    hollow = ndimage.gaussian_filter(height, 25 / res) - height      # > 0 = lunt søkk
    ok = land & ~blocked & (height >= MIN_HEIGHT_M) & (slope <= MAX_SLOPE_TAN)

    pivot = (isl["Position"]["X"], isl["Position"]["Y"])
    sets: dict[str, list[float]] = {}

    def emit(asset, r, c, scale):
        ri, ci = int(r), int(c)
        x = meta["min_x_m"] + (c + 0.5) * res - pivot[0]
        y = meta["max_y_m"] - (r + 0.5) * res - pivot[1]
        z = float(height[ri, ci]) * exaggeration
        sets.setdefault(asset, []).extend([round(x * 100, 1), round(y * 100, 1), round(z * 100, 1),
                                           round(float(rng.uniform(0, 360)), 1), round(float(scale), 3)])

    def pick(species):
        v = trees[species]
        return v[int(rng.integers(len(v)))]["asset"]

    # --- Trær ---
    for spacing, in_forest in ((FOREST_SPACING_M, True), (OPEN_SPACING_M, False)):
        r, c = jittered_points(shape, res, spacing, rng)
        ri, ci = r.astype(int), c.astype(int)
        keep = ok[ri, ci] & (forest[ri, ci] == in_forest)
        if not in_forest:
            keep &= rng.random(len(r)) < OPEN_TREE_PROB
        for k in np.flatnonzero(keep):
            h, hol, dc, sl = height[ri[k], ci[k]], hollow[ri[k], ci[k]], d_coast[ri[k], ci[k]], slope[ri[k], ci[k]]
            if in_forest and hol > 0.8 and dc > 60 and rng.random() < 0.7:
                species = "spruce"
            elif sl < 0.2 and h < 15 and dc > 25 and rng.random() < (0.45 if in_forest else 0.6):
                species = "birch"
            else:
                species = "pine"
            # Vindutsatte trær nær sjøen er lavere.
            exposure = np.clip(dc / 50.0, 0.55, 1.0)
            emit(pick(species), r[k], c[k], rng.uniform(0.75, 1.25) * exposure)

    # --- Einer/kratt: kystbelte + skogkant ---
    r, c = jittered_points(shape, res, BUSH_SPACING_M, rng)
    ri, ci = r.astype(int), c.astype(int)
    edge = ndimage.binary_dilation(forest, iterations=int(8 / res)) & ~forest
    prob = np.where(edge[ri, ci], 0.5, np.where(d_coast[ri, ci] < 45, 0.30, 0.06))
    keep = ok[ri, ci] & ~forest[ri, ci] & (rng.random(len(r)) < prob)
    for k in np.flatnonzero(keep):
        emit(pick("juniper"), r[k], c[k], rng.uniform(0.7, 1.6))

    counts = {a: len(v) // 5 for a, v in sets.items()}
    (OUT_DIR / f"veg_{slug}.json").write_text(json.dumps({"name": name, "slug": slug, "sets": sets},
                                                        separators=(",", ":")), encoding="utf-8")
    print(f"{name}: skog {forest.sum() * res * res / 1e4:.1f} ha → {sum(counts.values())} instanser")
    return sum(counts.values())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", action="append")
    ap.add_argument("--exaggeration", type=float, default=1.0)
    args = ap.parse_args()

    data = json.loads((SCRIPTS / "fjord_data.json").read_text(encoding="utf-8"))
    features = json.loads((SCRIPTS / "fjord_features.json").read_text(encoding="utf-8"))
    trees = json.loads((OUT_DIR / "trees.json").read_text(encoding="utf-8"))
    total = 0
    for isl in data["Islands"]:
        if args.only and isl["Name"] not in args.only:
            continue
        total += bake(isl, features, trees, args.exaggeration) or 0
    print(f"Totalt {total} instanser")


if __name__ == "__main__":
    main()
