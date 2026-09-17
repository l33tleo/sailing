"""Hent CC0 PBR-teksturer fra Poly Haven til scripts/cache/surfaces/ (for landmaterialet M_LandV2).

Kjør:  python3 scripts/fetch_surface_textures.py [--res 2k]

Per lag hentes Diffuse, nor_dx (DirectX-normaler = Unreal-konvensjon) og arm (AO/Roughness/Metallic
pakket i R/G/B) som JPG. Filnavn = asset-navn i Unreal: T_Land_<lag>_<D|N|ARM>.jpg.
Lisens: CC0 (polyhaven.com/license) — ingen attribusjon påkrevd.
"""
import argparse
import json
import sys
import urllib.request
from pathlib import Path

OUT = Path(__file__).resolve().parent / "cache" / "surfaces"
UA = {"User-Agent": "sailing-game/1.0 (texture fetch)"}

# lag → kandidater i prioritert rekkefølge (første som finnes brukes)
LAYERS = {
    "Cliff": ["rock_face_03", "rock_05"],                  # bratte flater, triplanar
    "Shore": ["coast_sand_rocks_02", "gray_rocks"],        # svaberg/rullestein i strandsonen
    "Grass": ["sparse_grass", "leafy_grass", "aerial_grass_rock"],
    "Forest": ["forest_ground_04", "forest_leaves_02"],    # skogbunn
}
MAPS = {"Diffuse": "D", "nor_dx": "N", "arm": "ARM"}


def get_json(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
        return json.load(r)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--res", default="2k")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    chosen = {}

    for layer, candidates in LAYERS.items():
        for slug in candidates:
            try:
                files = get_json(f"https://api.polyhaven.com/files/{slug}")
            except Exception:  # noqa: BLE001 - prøv neste kandidat
                continue
            if not all(m in files and args.res in files[m] for m in MAPS):
                continue
            for m, suffix in MAPS.items():
                dst = OUT / f"T_Land_{layer}_{suffix}.jpg"
                if dst.exists():
                    continue
                url = files[m][args.res]["jpg"]["url"]
                print(f"{layer}: {slug} {m} …", flush=True)
                with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=180) as r:
                    dst.write_bytes(r.read())
            chosen[layer] = slug
            break
        else:
            print(f"FEIL: ingen kandidat funnet for {layer}", file=sys.stderr)
            sys.exit(1)

    (OUT / "sources.json").write_text(json.dumps(chosen, indent=2), encoding="utf-8")
    print("Ferdig:", chosen)


if __name__ == "__main__":
    main()
