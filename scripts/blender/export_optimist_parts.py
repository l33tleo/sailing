"""Eksporterer Optimisten som tre FBX-er med hver sin pivot: skrog, rigg (bom+sprit+seil) og ror.

Kjør headless mot masterscenen (endrer den ikke):
  /Applications/Blender.app/Contents/MacOS/Blender -b Optimist3735.blend \
      -P scripts/blender/export_optimist_parts.py
Skriver scripts/cache/boat/SM_Boat_{Hull,Rig,Rudder}.fbx + boat_parts_manifest.json.
Importer deretter med scripts/import_boat_parts.py (headless UE).

Seilet i masteren (objekt «Sail») har innbakt bukt, Solidify og ingen UV, og kan ikke brukes til
shader-bukt. Her bygges derfor et nytt FLATT, enkeltsidig seil med UV0.
Spilene (Batten_1..3) er stive bokser over hele korden og ville stukket gjennom et deformert
seil — de utelates, og spilelommene tegnes i seilmaterialet i stedet.

Riggens geometri følger IODA-klassereglene (scripts/blender/reference/), ikke masteren: masteren
hadde bommen i ripehøyde (z=0.165) og spriten festet på pinnen 1680 mm under mastetoppen — men den
pinnen er for BOMMEN (CR 3.5.2.13 / 3.5.3.3). Med bommen der (z = 2.345 − 1.68 = 0.665) gir
forlik 1.68 m, fot 1.98 m (bomband, CR 3.5.3.4), sprit ≤ 2.286 m (CR 3.5.4.2) og peaken målt fra
klassetegningen (optimist_main.jpg: 0.49 × foten akter for masten, 1.44 × forliket over bommen)
et seil på 3.2 m² (klassens 3.3 m²) med topplinjen stigende ~37° — den karakteristiske Optimist-
silhuetten. Akterliket er rett (CR 6.3.3.4 tillater bare +5/−10 mm avvik). Bommen løftes derfor
0.5 m her, og spriten bygges på nytt mellom masten og peaken; masterscenen endres ikke.

Pivoter (Blender m, X fram / Z opp):
  Rigg: mastaksen i bomhøyde (0.75, 0, 0.665) — hele riggen svinger om denne.
  Ror:  rorakselen midt i rorhodet (-1.19, 0, 0).
Disse tallene speiles i C++ (USailRigComponent::MastPivotLocal, ASailboatPawn::RudderPivotLocal).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sailing_blender_utils import create_optimist_sail, export_boat_parts  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT_DIR = os.path.join(REPO, "scripts", "cache", "boat")

MAST_X = 0.75
MAST_TOP_Z = 2.345                     # masterens mast: z −0.005..2.345 (2.35 m, CR 3.5.2.4)
BOOM_Z = MAST_TOP_Z - 1.68             # bompinnen 1680 mm under toppen (CR 3.5.2.13)
BOOM_RAISE = BOOM_Z - 0.165            # masterens bom ligger på z=0.165
FOOT = 1.98                            # clew ved bombandet (CR 3.5.3.4)
PEAK_AFT = 0.975                       # 0.49 × foten (klassetegningen)
PEAK_ABOVE_BOOM = 2.42                 # 1.44 × forliket (klassetegningen)
SPRIT_LEN = 2.25                       # ≤ 2.286 m inkl. endebeslag (CR 3.5.4.2)
SPRIT_RADIUS = 0.01375                 # Ø27.5 mm (CR 3.5.4.1)

MAST_PIVOT = (MAST_X, 0.0, BOOM_Z)
RUDDER_PIVOT = (-1.19, 0.0, 0.0)

TACK = (MAST_X, 0.0, BOOM_Z)
CLEW = (MAST_X - FOOT, 0.0, BOOM_Z)
THROAT = (MAST_X, 0.0, MAST_TOP_Z)
PEAK = (MAST_X - PEAK_AFT, 0.0, BOOM_Z + PEAK_ABOVE_BOOM)
# Spriten går fra masten (nedre ende i snotteren) til peaken; høyden på masten gis av lengden.
SPRIT_HEEL = (MAST_X, 0.0, PEAK[2] - math.sqrt(SPRIT_LEN ** 2 - PEAK_AFT ** 2))
# 3 cm til siden så det flate seilet ikke skjærer spriten. På «dårlig halse» bules seilet inn i
# spriten — som på en ekte Optimist.
SPRIT_SIDE = 0.03


def build_flat_sail():
    return create_optimist_sail(
        name="Sail_Flat",
        tack=TACK,
        clew=CLEW,
        throat=THROAT,
        peak=PEAK,
        leech_corner=None,     # rett akterlik (CR 6.3.3.4)
        nu=20, nv=20,
        draft=0.0,
        material="M_Sail",
        uv=True,
    )


def build_sprit():
    """Sylinder (8 sider) fra SPRIT_HEEL til PEAK, 3 cm ut av seilplanet, med masterens M_Spar."""
    heel = Vector(SPRIT_HEEL) + Vector((0.0, SPRIT_SIDE, 0.0))
    top = Vector(PEAK) + Vector((0.0, SPRIT_SIDE, 0.0))
    axis = top - heel
    me = bpy.data.meshes.new("Sprit_Rule")
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=8,
                          radius1=SPRIT_RADIUS, radius2=SPRIT_RADIUS, depth=axis.length)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new("Sprit_Rule", me)
    bpy.context.scene.collection.objects.link(obj)
    # create_cone lager sylinderen langs +Z sentrert i origo.
    obj.matrix_world = Matrix.Translation((heel + top) / 2) @ axis.to_track_quat("Z", "Y").to_matrix().to_4x4()
    master = bpy.data.objects.get("Sprit")
    if master and master.material_slots and master.material_slots[0].material:
        me.materials.append(master.material_slots[0].material)
    return obj


def main():
    sail = build_flat_sail()
    sprit = build_sprit()
    print(f"[BOATEXPORT] rigg: bom z={BOOM_Z:.3f}, peak={PEAK}, sprit fra z={SPRIT_HEEL[2]:.3f} "
          f"({SPRIT_LEN} m), seilareal={0.5 * (FOOT * PEAK_ABOVE_BOOM + (MAST_TOP_Z - BOOM_Z) * PEAK_AFT):.2f} m²")
    parts = {
        "SM_Boat_Hull": {
            "objects": ["Hull.002", "Mast", "Daggerboard", "MastThwart"],
            "pivot": (0.0, 0.0, 0.0),
        },
        "SM_Boat_Rig": {
            "objects": ["Boom", sprit.name, sail.name],
            "pivot": MAST_PIVOT,
            "offsets": {"Boom": (0.0, 0.0, BOOM_RAISE)},
        },
        "SM_Boat_Rudder": {
            "objects": ["Rudder", "RudderHead", "Tiller"],
            "pivot": RUDDER_PIVOT,
        },
    }
    manifest = export_boat_parts(parts, OUT_DIR)
    for name, info in manifest.items():
        print(f"[BOATEXPORT] {name}: {info['tris']} tris, slots={info['material_slots']}, pivot_cm={info['pivot_cm']}")

    # Rydd: det flate seilet og den nye spriten er eksport-artefakter, ikke en del av masteren.
    for obj in (sail, sprit):
        me = obj.data
        bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.meshes.remove(me)


if __name__ == "__main__":
    main()
