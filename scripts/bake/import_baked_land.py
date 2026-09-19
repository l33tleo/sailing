"""Importerer bakte øy-mesher (scripts/cache/bake) til /Game/Fjord/Land og kobler dem til
/Game/Fjord/OslofjordMapData (FFjordIslandDef.BakedMesh).

Kjør headless med editoren LUKKET (C++ må være bygget først, ellers finnes ikke BakedMesh-feltet):
  UnrealEditor-Cmd Sailing.uproject -run=pythonscript \
      -script=scripts/bake/import_baked_land.py -unattended -nosplash -nullrhi
Resultatlinjer er merket [BAKEIMPORT] i loggen.

Per øy:  SM_Land_<slug>    Nanite, M_Land i slot 0, kompleks kollisjon = SM_LandCol_<slug>
         SM_LandCol_<slug> lavoppløst, ikke Nanite, brukes kun som kollisjonsgeometri
"""
import json
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
BAKE_DIR = os.path.join(PROJECT, "scripts/cache/bake")
DEST = "/Game/Fjord/Land"
STAGING = "/Game/Fjord/Land/_import"
MAP_DATA = "/Game/Fjord/OslofjordMapData"
LAND_MATERIAL = "/Game/Fjord/M_Land"

EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log(f"[BAKEIMPORT] {msg}")


def import_glb(asset_name):
    """Importer <asset_name>.glb og flytt StaticMesh-en til DEST/<asset_name>. Returnerer meshen."""
    src = os.path.join(BAKE_DIR, asset_name + ".glb")
    if not os.path.exists(src):
        log(f"mangler {src}")
        return None

    target = f"{DEST}/{asset_name}"
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)

    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = STAGING
    task.automated = True
    task.save = False
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    mesh_path = None
    for p in task.get_editor_property("imported_object_paths"):
        obj = unreal.load_asset(str(p).split(".")[0])
        if isinstance(obj, unreal.StaticMesh):
            mesh_path = str(p).split(".")[0]
    if not mesh_path:
        log(f"FEIL: {asset_name} ga ingen StaticMesh")
        return None

    if not EAL.rename_asset(mesh_path, target):
        log(f"FEIL: kunne ikke flytte {mesh_path} → {target}")
        return None
    return unreal.load_asset(target)


def main():
    manifest = json.load(open(os.path.join(BAKE_DIR, "manifest.json"), encoding="utf-8"))
    # Lagdelt M_LandV2 hvis den er bygget (scripts/create_land_material_v2.py), ellers flyfoto-M_Land.
    land_mat = unreal.load_asset(LAND_MATERIAL + "V2") or unreal.load_asset(LAND_MATERIAL)
    if land_mat:
        # Uten dette flagget faller Nanite-mesher tilbake til standardmaterialet i spillet
        # («missing usage flag Nanite»).
        unreal.MaterialEditingLibrary.set_material_usage(land_mat, unreal.MaterialUsage.MATUSAGE_NANITE)
        EAL.save_loaded_asset(land_mat)
    baked = {}

    for slug, info in manifest.items():
        col = import_glb(info["collision_asset"])
        mesh = import_glb(info["asset"])
        if not mesh:
            continue

        nanite = mesh.get_editor_property("nanite_settings")
        nanite.enabled = True
        mesh.set_editor_property("nanite_settings", nanite)

        if land_mat:
            mesh.set_material(0, land_mat)

        if col:
            col_nanite = col.get_editor_property("nanite_settings")
            col_nanite.enabled = False
            col.set_editor_property("nanite_settings", col_nanite)
            mesh.set_editor_property("complex_collision_mesh", col)
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag",
                                 unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)

        for asset in (col, mesh):
            if asset:
                EAL.save_loaded_asset(asset)

        b = mesh.get_bounds()
        log(f"{info['name']}: {mesh.get_path_name()} nanite=1 "
            f"kilde_tris={info['render_tris']} fallback_tris={mesh.get_num_triangles(0)} "
            f"kollisjon_tris={info['collision_tris']} "
            f"bounds_z=({b.origin.z - b.box_extent.z:.0f}..{b.origin.z + b.box_extent.z:.0f})")
        baked[info["name"]] = mesh

    # Rydd staging (materialer/teksturer Interchange lager ved siden av meshen).
    if EAL.does_directory_exist(STAGING):
        EAL.delete_directory(STAGING)

    # Koble til kartdata. Struct-arrayer returneres BY VALUE i Unreal Python: bygg hele arrayet
    # på nytt og tildel det i ett.
    data = unreal.load_asset(MAP_DATA)
    islands = list(data.get_editor_property("islands"))
    linked = 0
    for isl in islands:
        mesh = baked.get(str(isl.get_editor_property("name")))
        if mesh:
            isl.set_editor_property("baked_mesh", mesh)
            linked += 1
    data.set_editor_property("islands", islands)
    EAL.save_loaded_asset(data)
    log(f"koblet {linked} av {len(islands)} øyer til bakt mesh")


main()
