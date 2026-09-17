"""Fase 0-spike: lager en tett, asymmetrisk testmesh som .glb for å verifisere
glTF→UE-import (akser/enheter) og Nanite på denne Mac-en.

Kjør:  uv run --with trimesh --with numpy python scripts/bake/spike_make_test_mesh.py

Meshen er 500 x 500 m med ~1M trekanter. Asymmetrien gjør aksefeil lette å se på
bounding-boksen i UE: en 60 m høy topp mot ØST (+X i spillet) og en 25 m topp mot
NORD, begge forskjøvet fra sentrum. Spillkoordinater (X, Y, Z-opp, meter) skrives til
glTF (høyrehendt, Y-opp) via game_to_gltf().
"""
from pathlib import Path

import numpy as np
import trimesh

OUT = Path(__file__).resolve().parents[1] / "cache" / "spike" / "SM_NaniteSpike.glb"
N = 708          # N*N vertekser → 2*(N-1)^2 ≈ 1,0 M trekanter
SIZE_M = 500.0


def game_to_gltf(v: np.ndarray) -> np.ndarray:
    """(X, Y, Z-opp) i spillet → glTF (x, y-opp, z). Verifiseres av spiken."""
    return np.column_stack([v[:, 0], v[:, 2], v[:, 1]])


def main() -> None:
    lin = np.linspace(-SIZE_M / 2, SIZE_M / 2, N)
    x, y = np.meshgrid(lin, lin, indexing="xy")

    def bump(cx, cy, h, r):
        return h * np.exp(-(((x - cx) ** 2 + (y - cy) ** 2) / (2 * r * r)))

    rng = np.random.default_rng(1337)
    z = bump(150, 0, 60, 45) + bump(0, 180, 25, 30)
    z += 1.5 * np.sin(x / 9.0) * np.cos(y / 7.0) + rng.normal(0, 0.15, x.shape)

    verts = np.column_stack([x.ravel(), y.ravel(), z.ravel()])
    i = np.arange(N * N).reshape(N, N)
    a, b, c, d = i[:-1, :-1].ravel(), i[:-1, 1:].ravel(), i[1:, :-1].ravel(), i[1:, 1:].ravel()
    faces = np.concatenate([np.column_stack([a, b, d]), np.column_stack([a, d, c])])

    uv = np.column_stack([(x.ravel() / SIZE_M) + 0.5, (y.ravel() / SIZE_M) + 0.5])
    mesh = trimesh.Trimesh(vertices=game_to_gltf(verts), faces=faces, process=False,
                           visual=trimesh.visual.TextureVisuals(uv=uv))
    # Aksebyttet speiler håndetheten; snu vindingen så normalene peker opp.
    mesh.invert()

    OUT.parent.mkdir(parents=True, exist_ok=True)
    mesh.export(OUT)
    print(f"{OUT}  verts={len(verts)}  tris={len(faces)}  "
          f"forventet UE-bounds (cm): X±25000 Y±25000 Z≈0..6000, høy topp ved X=+15000")


if __name__ == "__main__":
    main()
