"""Baker øy-terreng fra 1 m DTM til .glb (render + kollisjon) for import som Nanite-StaticMesh.

Kjør (etter scripts/fetch_island_dtm.py):
  uv run --with numpy --with scipy --with pillow --with trimesh --with fast-simplification \
      python scripts/bake/bake_land.py [--only Hovedøya] [--exaggeration 1.0]

Prinsipp
- Meshen er et høydefelt over DTM-rutenettet, ikke en triangulert OSM-kontur. Kystlinjen følger
  dermed 1 m-DTM-en (viker, skjær, svaberg) i stedet for den 8 m-forenklede OSM-ringen.
- OSM-ringene brukes kun til EIERSKAP: en landcelle tilhører øya hvis den ligger nærmere øyas
  egen ring enn noen annen ring (og innen OWN_BUFFER_M). Nabo-øyer/fastland i utsnittet
  behandles som sjø, så ingen land tegnes to ganger (jf. z-fighting-historikken).
- Under vann fortsetter terrenget som syntetisk sjøbunn (dybde = avstand til eget land × helning,
  klampet) ut til SEABED_BELT_M. Ingen skjørt, ingen Z-hack: z=0 i meshen er middelvannstand
  (= WaterZ i spillet), og Single Layer Water får en bunn å absorbere mot.
- Pivot = øyas Position i fjord_data.json (der AIslandActor spawnes). Lokale koordinater i meter,
  skrevet til glTF som (X, Z, Y) med invertert vinding (verifisert i fase 0-spiken).

Utdata: scripts/cache/bake/SM_Land_<slug>.glb, SM_LandCol_<slug>.glb og manifest.json
(filnavn = asset-navn; Interchange navngir asset etter fil).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import fast_simplification
import numpy as np
import trimesh
from PIL import Image, ImageDraw
from scipy import ndimage

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
from fjord_slug import slugify  # noqa: E402

FJORD_JSON = SCRIPTS / "fjord_data.json"
DTM_DIR = SCRIPTS / "cache" / "dtm"
OUT_DIR = SCRIPTS / "cache" / "bake"

LAND_MIN_M = 0.25        # DTM over dette regnes som land (hav ligger på ~0 ± 0,2 m)
OWN_BUFFER_M = 40.0      # land inntil så langt utenfor egen OSM-ring kan tilhøre øya
SEABED_BELT_M = 150.0    # hvor langt ut sjøbunnen følger med
SEABED_SLOPE = 0.10      # meter dybde per meter fra land
SEABED_MAX_M = 15.0
RENDER_TRIS_PER_M2 = 0.35   # mål etter desimering (land + sjøbunn)
STEEP_START, STEEP_END = 0.6, 1.4   # tan(helning): ~31° … ~54°
ROCK_DETAIL_M = 0.9                 # amplitude på fraktal bergdetalj i bratt terreng
COLLISION_STEP_M = 4.0
COLLISION_KEEP = 0.25


def rasterize(ring_m, meta, shape) -> np.ndarray:  # også brukt av bake_vegetation.py
    """Fyll en ring (spillmeter) inn i DTM-rutenettet. Rad 0 = nord."""
    res = meta["res_m"]
    pts = [((x - meta["min_x_m"]) / res, (meta["max_y_m"] - y) / res) for x, y in ring_m]
    img = Image.new("1", (shape[1], shape[0]), 0)
    ImageDraw.Draw(img).polygon(pts, fill=1)
    return np.asarray(img, dtype=bool)


def rock_noise(shape, res: float, seed: int) -> np.ndarray:
    """Fraktal «ridged» støy (≈ −1..1) med bølgelengder 16/8/4/2 m — sprekker og hyller i berg."""
    rng = np.random.default_rng(seed)
    out = np.zeros(shape, dtype=np.float32)
    for wavelength, amp in ((16.0, 1.0), (8.0, 0.55), (4.0, 0.3), (2.0, 0.15)):
        n = ndimage.gaussian_filter(rng.standard_normal(shape).astype(np.float32), wavelength / (2.5 * res))
        n /= n.std() + 1e-9
        out += amp * (1.0 - 2.0 * np.abs(np.tanh(n)))
    return out / 2.0


def add_cliff_detail(height: np.ndarray, land: np.ndarray, res: float, seed: int) -> np.ndarray:
    """Lidar treffer sjelden bakken i stup, så DTM-en interpolerer bratte flater som store plane
    trekanter (5–20 m) som ser lavpoly ut på nært hold. Glatt ut kantene mellom dem og legg på
    fraktal bergdetalj, vektet av helningen slik at slakt terreng er urørt."""
    gy, gx = np.gradient(height, res)
    steep = np.clip((np.hypot(gx, gy) - STEEP_START) / (STEEP_END - STEEP_START), 0.0, 1.0) * land
    steep = ndimage.gaussian_filter(steep.astype(np.float32), 2.0 / res)
    smooth = ndimage.gaussian_filter(height, 2.5 / res)
    out = height * (1.0 - steep) + smooth * steep
    return out + rock_noise(height.shape, res, seed) * steep * ROCK_DETAIL_M


def grid_mesh(height: np.ndarray, mask: np.ndarray, step: int):
    """Trekanter over celler der alle fire hjørner er innenfor masken. Returnerer (rc, faces)."""
    h = height[::step, ::step]
    m = mask[::step, ::step]
    rows, cols = h.shape
    idx = np.arange(rows * cols).reshape(rows, cols)
    quad = m[:-1, :-1] & m[:-1, 1:] & m[1:, :-1] & m[1:, 1:]
    a, b, c, d = idx[:-1, :-1][quad], idx[:-1, 1:][quad], idx[1:, :-1][quad], idx[1:, 1:][quad]
    # Rad øker mot SØR. Med (X øst, Y nord, Z opp) gir a→c→d og a→d→b normal opp.
    faces = np.concatenate([np.column_stack([a, c, d]), np.column_stack([a, d, b])])
    used, inv = np.unique(faces, return_inverse=True)
    r, cidx = np.divmod(used, cols)
    return r * step, cidx * step, h.ravel()[used], inv.reshape(-1, 3)


def to_local(rows, cols, z, meta, pivot, exaggeration) -> np.ndarray:
    res = meta["res_m"]
    x = meta["min_x_m"] + cols * res - pivot[0]
    y = meta["max_y_m"] - rows * res - pivot[1]
    return np.column_stack([x, y, z * exaggeration]).astype(np.float64)


def export_glb(path: Path, verts_local: np.ndarray, faces: np.ndarray, uv=None):
    gltf_v = np.column_stack([verts_local[:, 0], verts_local[:, 2], verts_local[:, 1]])
    mesh = trimesh.Trimesh(vertices=gltf_v, faces=faces, process=False)
    if uv is not None:
        mesh.visual = trimesh.visual.TextureVisuals(uv=uv)
    mesh.invert()
    # Eksplisitte, arealvektede verteksnormaler → myk shading (uten dem lar vi importøren gjette).
    _ = mesh.vertex_normals
    path.parent.mkdir(parents=True, exist_ok=True)
    mesh.export(path, include_normals=True)


def bake_island(isl: dict, all_rings: list, exaggeration: float) -> dict | None:
    name = isl["Name"]
    slug = slugify(name)
    npy, meta_p = DTM_DIR / f"{slug}.npy", DTM_DIR / f"{slug}.json"
    if not npy.exists():
        print(f"{name}: mangler DTM-cache, hopper over", file=sys.stderr)
        return None
    meta = json.loads(meta_p.read_text(encoding="utf-8"))
    res = meta["res_m"]
    dtm = np.nan_to_num(np.load(npy), nan=0.0)
    shape = dtm.shape

    own_ring = [(p["X"], p["Y"]) for p in isl["Outline"]]
    own = rasterize(own_ring, meta, shape)

    # Andre ringer som berører utsnittet (bbox-test først; det er ~300 ringer totalt).
    other = np.zeros(shape, dtype=bool)
    for ring in all_rings:
        if ring is own_ring or ring == own_ring:
            continue
        xs, ys = [p[0] for p in ring], [p[1] for p in ring]
        if max(xs) < meta["min_x_m"] or min(xs) > meta["max_x_m"] \
                or max(ys) < meta["min_y_m"] or min(ys) > meta["max_y_m"]:
            continue
        other |= rasterize(ring, meta, shape)
    other &= ~own

    d_own = ndimage.distance_transform_edt(~own) * res
    d_other = ndimage.distance_transform_edt(~other) * res if other.any() else np.full(shape, np.inf)

    land = (dtm > LAND_MIN_M) & (d_own <= OWN_BUFFER_M) & (d_own <= d_other)
    # Fjern løse småflekker (støy i DTM-ens havflate), behold alt som er ≥ 25 m².
    lab, n = ndimage.label(land)
    if n:
        sizes = ndimage.sum(land, lab, index=np.arange(1, n + 1))
        land = np.isin(lab, 1 + np.flatnonzero(sizes * res * res >= 25.0))

    d_land = ndimage.distance_transform_edt(~land) * res
    seabed = -np.clip(0.15 + d_land * SEABED_SLOPE, 0.0, SEABED_MAX_M)
    height = np.where(land, dtm, seabed).astype(np.float32)
    # Myk opp overgangen land/sjøbunn litt (kun under ~1 m o.h.) så strandsonen ikke blir en trapp.
    smooth = ndimage.gaussian_filter(height, sigma=1.5 / res)
    shore = np.clip(1.0 - np.abs(height) / 1.0, 0.0, 1.0)
    height = height * (1 - shore) + smooth * shore

    # Seed fra navnet → samme detalj ved hver bake (determinisme).
    height = add_cliff_detail(height, land, res, int(hashlib.sha1(slug.encode()).hexdigest()[:8], 16))

    # Feltene gjenbrukes av bake_vegetation.py (samme høyder som meshen → trærne står på bakken).
    np.savez_compressed(OUT_DIR / f"{slug}_fields.npz", height=height.astype(np.float32), land=land)

    region = d_land <= SEABED_BELT_M
    pivot = (isl["Position"]["X"], isl["Position"]["Y"])

    # --- Rendermesh ---
    area_m2 = float(region.sum()) * res * res
    step = 1 if area_m2 < 1.5e6 else 2
    rows, cols, z, faces = grid_mesh(height, region, step)
    verts = to_local(rows, cols, z, meta, pivot, exaggeration)
    target_tris = int(area_m2 * RENDER_TRIS_PER_M2)
    if len(faces) > target_tris:
        verts, faces = fast_simplification.simplify(
            verts.astype(np.float32), faces.astype(np.int32),
            target_count=target_tris, agg=5)
        verts = verts.astype(np.float64)
    # UV0 = posisjon i DTM-utsnittet (samme utsnitt som ortofotoet per øy vil bruke).
    uv = np.column_stack([
        (verts[:, 0] + pivot[0] - meta["min_x_m"]) / (meta["max_x_m"] - meta["min_x_m"]),
        (verts[:, 1] + pivot[1] - meta["min_y_m"]) / (meta["max_y_m"] - meta["min_y_m"]),
    ])
    export_glb(OUT_DIR / f"SM_Land_{slug}.glb", verts, faces, uv=uv)

    # --- Kollisjonsmesh: kun land + grunt vann (båten stikker ~0,5 m) ---
    col_region = d_land <= 25.0
    cstep = max(1, int(round(COLLISION_STEP_M / res)))
    crow, ccol, cz, cfaces = grid_mesh(height, col_region, cstep)
    cverts = to_local(crow, ccol, cz, meta, pivot, exaggeration)
    ctarget = max(500, int(len(cfaces) * COLLISION_KEEP))
    cverts, cfaces = fast_simplification.simplify(
        cverts.astype(np.float32), cfaces.astype(np.int32), target_count=ctarget, agg=5)
    export_glb(OUT_DIR / f"SM_LandCol_{slug}.glb", cverts.astype(np.float64), cfaces)

    info = {
        "name": name, "slug": slug,
        "asset": f"SM_Land_{slug}", "collision_asset": f"SM_LandCol_{slug}",
        "pivot_m": list(pivot), "exaggeration": exaggeration,
        "render_tris": int(len(faces)), "collision_tris": int(len(cfaces)),
        "land_area_m2": float(land.sum()) * res * res, "max_height_m": float(dtm[land].max()) if land.any() else 0.0,
        "glb_sha1": hashlib.sha1((OUT_DIR / f"SM_Land_{slug}.glb").read_bytes()).hexdigest(),
    }
    print(f"{name}: land {info['land_area_m2'] / 1e4:.1f} ha, topp {info['max_height_m']:.0f} m, "
          f"step {step} m → {info['render_tris']} tris (kollisjon {info['collision_tris']})")
    return info


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--only", action="append")
    ap.add_argument("--exaggeration", type=float, default=1.0)
    args = ap.parse_args()

    data = json.loads(FJORD_JSON.read_text(encoding="utf-8"))
    rings = [[(p["X"], p["Y"]) for p in i["Outline"]] for i in data["Islands"]]
    for lm in data["Landmasses"]:
        pts = lm["Points"] if isinstance(lm, dict) else lm
        if len(pts) >= 3:
            rings.append([(p["X"], p["Y"]) for p in pts])

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest_p = OUT_DIR / "manifest.json"
    manifest = json.loads(manifest_p.read_text(encoding="utf-8")) if manifest_p.exists() else {}
    for isl in data["Islands"]:
        if args.only and isl["Name"] not in args.only:
            continue
        info = bake_island(isl, rings, args.exaggeration)
        if info:
            manifest[info["slug"]] = info
    manifest_p.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")


if __name__ == "__main__":
    main()
