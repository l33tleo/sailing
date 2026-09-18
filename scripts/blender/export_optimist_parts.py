"""Eksporterer Optimisten som tre FBX-er med hver sin pivot: skrog, rigg (bom+sprit+seil) og ror.

Kjør headless mot masterscenen (endrer den ikke):
  /Applications/Blender.app/Contents/MacOS/Blender -b Optimist3735.blend \
      -P scripts/blender/export_optimist_parts.py
Skriver scripts/cache/boat/SM_Boat_{Hull,Rig,Rudder}.fbx + boat_parts_manifest.json.
Importer deretter med scripts/import_boat_parts.py (headless UE).

Seilet i masteren (objekt «Sail») har innbakt bukt, Solidify og ingen UV, og kan ikke brukes til
shader-bukt. Her bygges derfor et nytt FLATT, enkeltsidig seil med UV0 fra de samme hjørnene
(målt fra masteren: forlig på mastaksen x=0.75, fot på bommen z=0.165, roach-knekk ved 60 % høyde).
Spilene (Batten_1..3) er stive bokser over hele korden og ville stukket gjennom et deformert
seil — de utelates, og spilelommene tegnes i seilmaterialet i stedet.

Pivoter (Blender m, X fram / Z opp):
  Rigg: mastaksen i bomhøyde (0.75, 0, 0.165) — hele riggen svinger om denne.
  Ror:  rorakselen midt i rorhodet (-1.19, 0, 0).
Disse tallene speiles i C++ (USailRigComponent::MastPivotLocal, ASailboatPawn::RudderPivotLocal).
"""
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sailing_blender_utils import create_optimist_sail, export_boat_parts  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT_DIR = os.path.join(REPO, "scripts", "cache", "boat")

MAST_PIVOT = (0.75, 0.0, 0.165)
RUDDER_PIVOT = (-1.19, 0.0, 0.0)

# Spriten ligger midt i seilplanet (y=0, Ø42 mm) i masteren; flyttes 3 cm til siden så det flate
# seilet ikke skjærer den. På «dårlig halse» bules seilet inn i spriten — som på en ekte Optimist.
SPRIT_OFFSET = (0.0, 0.03, 0.0)


def build_flat_sail():
    return create_optimist_sail(
        name="Sail_Flat",
        tack=(0.75, 0.0, 0.165),
        clew=(-1.23, 0.0, 0.165),
        throat=(0.75, 0.0, 2.345),
        peak=(-0.60, 0.0, 2.61),
        leech_corner=(-0.972, 0.0, 1.632),
        leech_v=0.6,
        nu=20, nv=20,          # nv=20 → knekken lander eksakt på rad 12
        draft=0.0,
        material="M_Sail",
        uv=True,
    )


def main():
    sail = build_flat_sail()
    parts = {
        "SM_Boat_Hull": {
            "objects": ["Hull.002", "Mast", "Daggerboard", "MastThwart"],
            "pivot": (0.0, 0.0, 0.0),
        },
        "SM_Boat_Rig": {
            "objects": ["Boom", "Sprit", sail.name],
            "pivot": MAST_PIVOT,
            "offsets": {"Sprit": SPRIT_OFFSET},
        },
        "SM_Boat_Rudder": {
            "objects": ["Rudder", "RudderHead", "Tiller"],
            "pivot": RUDDER_PIVOT,
        },
    }
    manifest = export_boat_parts(parts, OUT_DIR)
    for name, info in manifest.items():
        print(f"[BOATEXPORT] {name}: {info['tris']} tris, slots={info['material_slots']}, pivot_cm={info['pivot_cm']}")

    # Rydd: det flate seilet er et eksport-artefakt, ikke en del av masteren.
    me = sail.data
    bpy.data.objects.remove(sail, do_unlink=True)
    bpy.data.meshes.remove(me)


if __name__ == "__main__":
    main()
