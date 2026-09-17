"""Genererer trær og busker som ugjennomsiktig Nanite-geometri (.glb) fargelagt via en palett-tekstur.

Kjør:  uv run --with numpy --with trimesh --with pillow python scripts/bake/make_trees.py

Hvorfor prosedurale, ugjennomsiktige trær: alfa-maskerte løvkort gir tung overdraw og er dyre som
Nanite på Apple-GPU, og fotoskannede CC0-trær (Poly Haven) er 7–17 M polygoner per tre. Lukkede
«klumper» med palettfarger instansieres svært billig av Nanite, og fra en seilbåt (50 m–flere km)
er det silhuett, tetthet og farge som leses. Modellene kan byttes mot f.eks. Megascans senere uten
at plasseringen (bake_vegetation.py) endres — behold pivot i rotpunktet og høyde ≈ HEIGHT_M.

Arter (Oslofjordens øyer): furu på koller og berg, gran og bjørk i lune søkk, einer/kratt i kystsonen.
Alle mål i meter, Z opp, pivot ved bakken (stammen går 0,5 m ned for å tåle skrått terreng).
Utdata: scripts/cache/bake/SM_Tree_<art>_<variant>.glb + trees.json
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import trimesh

OUT_DIR = Path(__file__).resolve().parents[1] / "cache" / "bake"


def srgb(r, g, b):
    return np.array([r, g, b], dtype=np.float64) / 255.0


BARK_PINE_LOW, BARK_PINE_HIGH = srgb(92, 74, 62), srgb(168, 98, 58)     # furu: rødlig øvre stamme
BARK_SPRUCE = srgb(84, 70, 60)
BARK_BIRCH, BARK_BIRCH_MARK = srgb(225, 222, 212), srgb(60, 56, 52)
NEEDLE_PINE_DARK, NEEDLE_PINE_LIGHT = srgb(30, 46, 32), srgb(66, 88, 54)
NEEDLE_SPRUCE_DARK, NEEDLE_SPRUCE_LIGHT = srgb(20, 38, 28), srgb(46, 70, 46)
LEAF_BIRCH_DARK, LEAF_BIRCH_LIGHT = srgb(52, 78, 36), srgb(112, 138, 62)
JUNIPER_DARK, JUNIPER_LIGHT = srgb(36, 52, 38), srgb(74, 94, 66)


# Palett-tekstur: én rad per «stoff», u = lyshet 0..1 (mørk → lys). Trærne fargelegges via UV0 inn i
# denne i stedet for verteksfarger (som ikke overlevde glTF-importen til Unreal).
PALETTE_ROWS = [
    ("pine_needle", NEEDLE_PINE_DARK, NEEDLE_PINE_LIGHT),
    ("spruce_needle", NEEDLE_SPRUCE_DARK, NEEDLE_SPRUCE_LIGHT),
    ("birch_leaf", LEAF_BIRCH_DARK, LEAF_BIRCH_LIGHT),
    ("juniper", JUNIPER_DARK, JUNIPER_LIGHT),
    ("pine_bark", BARK_PINE_LOW, BARK_PINE_HIGH),
    ("spruce_bark", BARK_SPRUCE * 0.8, BARK_SPRUCE * 1.15),
    ("birch_bark", BARK_BIRCH_MARK, BARK_BIRCH),
]
ROW_V = {name: (i + 0.5) / len(PALETTE_ROWS) for i, (name, _, _) in enumerate(PALETTE_ROWS)}


def write_palette(path):
    from PIL import Image
    w, row_h = 256, 8
    img = np.zeros((row_h * len(PALETTE_ROWS), w, 3), dtype=np.uint8)
    t = np.linspace(0, 1, w)[None, :, None]
    for i, (_, dark, light) in enumerate(PALETTE_ROWS):
        img[i * row_h:(i + 1) * row_h] = np.clip((dark[None, None, :] * (1 - t) + light[None, None, :] * t) * 255, 0, 255)
    Image.fromarray(img).save(path)


def noise3(p: np.ndarray, rng: np.random.Generator, octaves=3, scale=1.0) -> np.ndarray:
    """Billig pseudo-støy: sum av tilfeldig orienterte sinusbølger (glatt, deterministisk per rng)."""
    out = np.zeros(len(p))
    for o in range(octaves):
        f = scale * (2.0 ** o)
        for _ in range(4):
            d = rng.normal(size=3)
            d /= np.linalg.norm(d)
            out += np.sin(p @ d * f + rng.uniform(0, 6.283)) / (2.0 ** o)
    return out / 4.0


def blob(center, radii, rng, row, subdiv=2, rough=0.22):
    """Støyforskjøvet ellipsoide; lysere på toppen og ytterst (belyst bar/løv), mørkere under."""
    m = trimesh.creation.icosphere(subdivisions=subdiv)
    v = m.vertices.copy()
    n = noise3(v * 2.2, rng)
    v *= (1.0 + rough * n)[:, None]
    up = np.clip(v[:, 2] * 0.5 + 0.5, 0, 1)
    shade = np.clip(0.2 + 0.7 * up + 0.2 * n, 0.02, 0.98)
    uv = np.column_stack([shade, np.full(len(v), ROW_V[row])])
    v = v * np.asarray(radii)[None, :] + np.asarray(center)[None, :]
    return v, m.faces.copy(), uv


def trunk(height, r_base, r_top, rng, row, sections=8, rings=7, lean=0.25, marks=0.0):
    """Avsmalnende, svakt krum stamme. u går mørk→lys oppover; marks = andel mørke flekker (bjørk)."""
    zs = np.linspace(-0.5, height, rings)
    t = np.clip(zs / height, 0, 1)
    bend = np.column_stack([np.sin(t * 2.1 + rng.uniform(0, 6)) * lean, np.cos(t * 1.7 + rng.uniform(0, 6)) * lean])
    verts, cols = [], []
    for i, z in enumerate(zs):
        r = r_base + (r_top - r_base) * t[i]
        for s in range(sections):
            a = 2 * np.pi * s / sections
            verts.append([bend[i, 0] + r * np.cos(a), bend[i, 1] + r * np.sin(a), z])
            u = 0.05 if rng.random() < marks else np.clip((0.75 if marks else t[i]) + rng.uniform(-0.08, 0.08), 0.02, 0.98)
            cols.append([u, ROW_V[row]])
    faces = []
    for i in range(rings - 1):
        for s in range(sections):
            a, b = i * sections + s, i * sections + (s + 1) % sections
            c, d = a + sections, b + sections
            faces += [[a, b, d], [a, d, c]]
    top = len(verts)
    verts.append([bend[-1, 0], bend[-1, 1], height])
    cols.append([0.9, ROW_V[row]])
    faces += [[(rings - 1) * sections + s, (rings - 1) * sections + (s + 1) % sections, top] for s in range(sections)]
    return np.array(verts), np.array(faces), np.array(cols), bend


def merge(parts):
    verts, faces, cols, off = [], [], [], 0
    for v, f, c in parts:
        verts.append(v); faces.append(f + off); cols.append(c); off += len(v)
    return np.vstack(verts), np.vstack(faces), np.vstack(cols)


def pine(seed: int, height=13.0):
    """Furu: høy, naken rødlig stamme og uregelmessig, flat krone av klumper i øvre tredel."""
    rng = np.random.default_rng(seed)
    tv, tf, tc, bend = trunk(height * 0.9, 0.24, 0.07, rng, "pine_bark")
    parts = [(tv, tf, tc)]
    for _ in range(int(rng.integers(11, 16))):
        z = height * rng.uniform(0.55, 0.96)
        reach = (1.0 - (z / height - 0.55) / 0.41) * 2.4 + 0.3
        a = rng.uniform(0, 2 * np.pi)
        c = [bend[-1, 0] + np.cos(a) * reach * rng.uniform(0.2, 1.0),
             bend[-1, 1] + np.sin(a) * reach * rng.uniform(0.2, 1.0), z]
        r = rng.uniform(1.3, 2.3)
        parts.append(blob(c, [r, r * rng.uniform(0.8, 1.1), r * rng.uniform(0.6, 0.85)], rng, "pine_needle", rough=0.45))
    return merge(parts)


def spruce(seed: int, height=16.0):
    """Gran: smal kjegle av overlappende, hengende lag helt ned mot bakken."""
    rng = np.random.default_rng(seed)
    tv, tf, tc, _ = trunk(height * 0.95, 0.26, 0.04, rng, "spruce_bark", lean=0.08)
    parts = [(tv, tf, tc)]
    layers = 9
    for i in range(layers):
        t = i / (layers - 1)
        z = height * (0.12 + 0.86 * t)
        r = (1.0 - t) * 2.7 + 0.35
        for k in range(3):
            a = 2 * np.pi * (k / 3.0) + rng.uniform(-0.5, 0.5) + i
            c = [np.cos(a) * r * 0.35, np.sin(a) * r * 0.35, z - r * 0.15]
            parts.append(blob(c, [r * 0.85, r * 0.85, r * 0.55], rng, "spruce_needle",
                              subdiv=1 if t > 0.7 else 2, rough=0.3))
    return merge(parts)


def birch(seed: int, height=11.0):
    """Bjørk: hvit stamme med mørke flekker, luftig og høyreist lysegrønn krone."""
    rng = np.random.default_rng(seed)
    tv, tf, tc, bend = trunk(height * 0.85, 0.16, 0.04, rng, "birch_bark", lean=0.35, marks=0.18)
    parts = [(tv, tf, tc)]
    for _ in range(int(rng.integers(9, 13))):
        z = height * rng.uniform(0.38, 0.98)
        t = (z / height - 0.38) / 0.6
        reach = np.sin(np.clip(t, 0, 1) * np.pi) * 2.0 + 0.4
        a = rng.uniform(0, 2 * np.pi)
        c = [bend[-1, 0] * t + np.cos(a) * reach * rng.uniform(0.3, 1.0),
             bend[-1, 1] * t + np.sin(a) * reach * rng.uniform(0.3, 1.0), z]
        r = rng.uniform(0.9, 1.6)
        parts.append(blob(c, [r, r, r * rng.uniform(1.0, 1.4)], rng, "birch_leaf", rough=0.35))
    return merge(parts)


def juniper(seed: int, height=1.8):
    """Einer/kratt: lav, tett klynge."""
    rng = np.random.default_rng(seed)
    parts = []
    for _ in range(int(rng.integers(3, 6))):
        a, d = rng.uniform(0, 2 * np.pi), rng.uniform(0, 1.0)
        r = rng.uniform(0.6, 1.1)
        parts.append(blob([np.cos(a) * d, np.sin(a) * d, r * 0.5 * rng.uniform(0.6, 1.2)],
                          [r, r, r * rng.uniform(0.6, height / 1.6)], rng, "juniper", subdiv=1, rough=0.3))
    return merge(parts)


def export(name: str, verts, faces, cols):
    gltf_v = np.column_stack([verts[:, 0], verts[:, 2], verts[:, 1]])  # spill (X,Y,Z) → glTF (X,Z,Y)
    # cols = UV inn i paletten (u = lyshet, v = rad). glTF har v=0 øverst, som bilderaden.
    mesh = trimesh.Trimesh(vertices=gltf_v, faces=faces, process=False,
                           visual=trimesh.visual.TextureVisuals(uv=np.column_stack([cols[:, 0], 1.0 - cols[:, 1]])))
    mesh.invert()
    _ = mesh.vertex_normals
    mesh.export(OUT_DIR / f"{name}.glb", include_normals=True)
    return {"asset": name, "tris": int(len(faces)), "height_m": float(verts[:, 2].max())}


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    species = {
        "pine": [("SM_Tree_Pine_A", pine(11)), ("SM_Tree_Pine_B", pine(12, 10.5)), ("SM_Tree_Pine_C", pine(13, 15.0))],
        "spruce": [("SM_Tree_Spruce_A", spruce(21)), ("SM_Tree_Spruce_B", spruce(22, 12.0))],
        "birch": [("SM_Tree_Birch_A", birch(31)), ("SM_Tree_Birch_B", birch(32, 8.5))],
        "juniper": [("SM_Bush_Juniper_A", juniper(41)), ("SM_Bush_Juniper_B", juniper(42, 1.2))],
    }
    write_palette(OUT_DIR / "T_TreePalette.png")
    manifest = {sp: [export(name, *mesh) for name, mesh in variants] for sp, variants in species.items()}
    (OUT_DIR / "trees.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    for sp, items in manifest.items():
        print(sp, [(i["asset"], i["tris"], round(i["height_m"], 1)) for i in items])


if __name__ == "__main__":
    main()
