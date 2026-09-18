"""Importerer de splittede båtdelene (scripts/cache/boat/SM_Boat_*.fbx fra
scripts/blender/export_optimist_parts.py) til /Game/ModelsV2 og setter prosjektmaterialene.

Kjør headless med editoren LUKKET:
  "/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" Sailing.uproject \
      -run=pythonscript -script=/Users/leovonschwind/sailing/scripts/import_boat_parts.py \
      -unattended -nosplash -nullrhi
Resultatlinjer er merket [BOATIMPORT]. Trekanttall verifiseres mot manifestet (importen har
tidligere «ikke tatt» på første forsøk — da importeres det én gang til).

ASailboatPawn::BeginPlay bruker SM_Boat_Hull/Rig/Rudder hvis alle tre finnes, ellers det gamle
kombinerte Optimist3735-meshet.
"""
import json
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
BOAT_DIR = os.path.join(PROJECT, "scripts/cache/boat")
DEST = "/Game/ModelsV2"

# FBX-materialnavn (fra Blender) → prosjektmateriale. M_Sail byttes til M_SailV2 når den finnes
# (scripts/create_sail_material_v2.py).
MATERIALS = {
    "M_Hull": f"{DEST}/M_Hull",
    "M_Spar": f"{DEST}/M_Spar",
    "M_Wood": f"{DEST}/M_Tiller",
    "M_Foil": f"{DEST}/M_Foil",
    "M_Sail": f"{DEST}/M_SailV2",
}
MATERIAL_FALLBACK = {"M_Sail": f"{DEST}/M_Sail"}

EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"[BOATIMPORT] {msg}")


def import_fbx(name, fbx):
    target = f"{DEST}/{name}"
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)

    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.replace_existing_settings = True

    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_materials = False
    opts.import_textures = False
    opts.import_animations = False
    opts.import_as_skeletal = False
    opts.override_full_name = True
    smd = opts.static_mesh_import_data
    smd.combine_meshes = True
    smd.generate_lightmap_u_vs = True
    smd.auto_generate_collision = False      # rent visuelle deler (kapselen er kollisjonen)
    smd.remove_degenerates = True
    smd.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task.options = opts

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(target)


def assign_materials(mesh):
    names = []
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(sm.get_editor_property("material_slot_name"))
        names.append(slot)
        path = MATERIALS.get(slot)
        mat = unreal.load_asset(path) if path else None
        if not mat and slot in MATERIAL_FALLBACK:
            mat = unreal.load_asset(MATERIAL_FALLBACK[slot])
        if mat:
            mesh.set_material(i, mat)
        else:
            log(f"  ADVARSEL: ingen prosjektmateriale for slot '{slot}'")
    return names


def main():
    manifest = json.load(open(os.path.join(BOAT_DIR, "boat_parts_manifest.json"), encoding="utf-8"))

    # Y-speilingssjekk: det gamle seilet hadde bukt mot +Y i Blender (0..0.177 m). Fortegnet på
    # UE-boundsens Y-utslag forteller om Blender +Y havner på UE +Y eller -Y.
    old = unreal.load_asset(f"{DEST}/Optimist3735")
    if old:
        b = old.get_bounding_box()
        log(f"Optimist3735 (gammel) bounds: min=({b.min.x:.1f},{b.min.y:.1f},{b.min.z:.1f}) "
            f"max=({b.max.x:.1f},{b.max.y:.1f},{b.max.z:.1f})  [Blender-bukt +Y=17.7 cm]")

    ok = True
    for name, info in manifest.items():
        mesh = None
        for attempt in (1, 2):
            mesh = import_fbx(name, info["fbx"])
            tris = mesh.get_num_triangles(0) if mesh and mesh.get_num_lods() > 0 else -1
            if tris == info["tris"]:
                break
            log(f"{name}: forsøk {attempt} ga {tris} trekanter, forventet {info['tris']} — importerer på nytt")
        if not mesh or mesh.get_num_lods() == 0:
            log(f"FEIL: {name} ble ikke importert")
            ok = False
            continue
        slots = assign_materials(mesh)
        EAL.save_loaded_asset(mesh, only_if_is_dirty=False)
        b = mesh.get_bounding_box()
        log(f"{name}: tris={mesh.get_num_triangles(0)} (forventet {info['tris']}) slots={slots} "
            f"bounds min=({b.min.x:.1f},{b.min.y:.1f},{b.min.z:.1f}) max=({b.max.x:.1f},{b.max.y:.1f},{b.max.z:.1f}) "
            f"pivot_cm={info['pivot_cm']}")
    log("FERDIG" if ok else "FERDIG MED FEIL")


if __name__ == "__main__":
    main()
