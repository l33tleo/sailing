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
    "Cliff": ["mossy_rock", "rock_05"],                    # grått berg med lav; bratte flater, triplanar
    "Shore": ["rock_boulder_dry", "gray_rocks"],           # lyse, glatte svaberg i strandsonen
    "Grass": ["forrest_ground_01", "leafy_grass"],         # gress/mose på åpne flater
    "Forest": ["forest_leaves_02", "brown_mud_leaves_01"], # mørk, mosegrodd skogbunn
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
    src_p = OUT / "sources.json"
    previous = json.loads(src_p.read_text(encoding="utf-8")) if src_p.exists() else {}

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
                # Hopp over bare hvis cachen kommer fra SAMME kilde (lagene kan få ny tekstur).
                if dst.exists() and previous.get(layer) == slug:
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
