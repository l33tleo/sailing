"""Importerer tremodeller og bakt vegetasjonsplassering til Unreal (headless, editoren LUKKET).

  UnrealEditor-Cmd Sailing.uproject -run=pythonscript \
      -script=scripts/bake/import_vegetation.py -unattended -nosplash -nullrhi
Resultatlinjer er merket [VEGIMPORT].

Lager:  /Game/Fjord/Vegetation/SM_Tree_* / SM_Bush_*   Nanite, materiale M_TreeVC
        /Game/Fjord/Vegetation/M_TreeVC                palett-tekstur via UV0 → BaseColor (Nanite-flagg)
        /Game/Fjord/Land/DA_Bake_<slug>                UFjordIslandBakeData per øy
og kobler FFjordIslandDef.BakeData i /Game/Fjord/OslofjordMapData.
"""
import json
import os

import unreal

mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
BAKE_DIR = os.path.join(PROJECT, "scripts/cache/bake")
VEG_DEST = "/Game/Fjord/Vegetation"
STAGING = VEG_DEST + "/_import"
DATA_DEST = "/Game/Fjord/Land"
MAP_DATA = "/Game/Fjord/OslofjordMapData"

TREE_CULL_UU = 600000.0   # 6 km
BUSH_CULL_UU = 80000.0    # 800 m


def log(msg):
    unreal.log(f"[VEGIMPORT] {msg}")


def build_tree_material():
    path = f"{VEG_DEST}/M_TreeVC"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = tools.create_asset("M_TreeVC", VEG_DEST, unreal.Material, unreal.MaterialFactoryNew())
    # Farge fra palett-teksturen (UV0: u = lyshet, v = rad per treslag/bark), se make_trees.py.
    pal_task = unreal.AssetImportTask()
    pal_task.filename = os.path.join(BAKE_DIR, "T_TreePalette.png")
    pal_task.destination_path, pal_task.destination_name = VEG_DEST, "T_TreePalette"
    pal_task.automated, pal_task.save, pal_task.replace_existing = True, True, True
    tools.import_asset_tasks([pal_task])
    palette = unreal.load_asset(f"{VEG_DEST}/T_TreePalette")
    palette.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    palette.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    palette.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    EAL.save_loaded_asset(palette)
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
    tex.set_editor_property("texture", palette)
    mel.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
    spec.set_editor_property("r", 0.15)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_NANITE)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def import_mesh(asset_name, material):
    src = os.path.join(BAKE_DIR, asset_name + ".glb")
    target = f"{VEG_DEST}/{asset_name}"
    if EAL.does_asset_exist(target):
        EAL.delete_asset(target)
    task = unreal.AssetImportTask()
    task.filename, task.destination_path = src, STAGING
    task.automated, task.save, task.replace_existing = True, False, True
    tools.import_asset_tasks([task])
    mesh_path = None
    for p in task.get_editor_property("imported_object_paths"):
        if isinstance(unreal.load_asset(str(p).split(".")[0]), unreal.StaticMesh):
            mesh_path = str(p).split(".")[0]
    if not mesh_path or not EAL.rename_asset(mesh_path, target):
        log(f"FEIL: import av {asset_name}")
        return None
    mesh = unreal.load_asset(target)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = True
    mesh.set_editor_property("nanite_settings", nanite)
    mesh.set_material(0, material)
    EAL.save_loaded_asset(mesh)
    return mesh


def main():
    trees = json.load(open(os.path.join(BAKE_DIR, "trees.json"), encoding="utf-8"))
    material = build_tree_material()
    meshes = {}
    for species, variants in trees.items():
        for v in variants:
            m = import_mesh(v["asset"], material)
            if m:
                meshes[v["asset"]] = (m, species)
    if EAL.does_directory_exist(STAGING):
        EAL.delete_directory(STAGING)
    log(f"importerte {len(meshes)} tre-/buskmodeller")

    data = unreal.load_asset(MAP_DATA)
    islands = list(data.get_editor_property("islands"))
    by_name = {str(i.get_editor_property("name")): i for i in islands}
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.FjordIslandBakeData)
    total = 0

    for fname in sorted(os.listdir(BAKE_DIR)):
        if not (fname.startswith("veg_") and fname.endswith(".json")):
            continue
        veg = json.load(open(os.path.join(BAKE_DIR, fname), encoding="utf-8"))
        asset_name = f"DA_Bake_{veg['slug']}"
        if EAL.does_asset_exist(f"{DATA_DEST}/{asset_name}"):
            EAL.delete_asset(f"{DATA_DEST}/{asset_name}")
        da = tools.create_asset(asset_name, DATA_DEST, unreal.FjordIslandBakeData, factory)

        sets = []
        for mesh_name, packed in veg["sets"].items():
            if mesh_name not in meshes:
                continue
            mesh, species = meshes[mesh_name]
            s = unreal.FjordInstanceSet()
            s.set_editor_property("mesh", mesh)
            s.set_editor_property("packed", [float(x) for x in packed])
            is_bush = species == "juniper"
            s.set_editor_property("cull_distance", BUSH_CULL_UU if is_bush else TREE_CULL_UU)
            s.set_editor_property("cast_shadow", not is_bush)
            sets.append(s)
            total += len(packed) // 5
        da.set_editor_property("instance_sets", sets)
        EAL.save_loaded_asset(da)

        isl = by_name.get(veg["name"])
        if isl:
            isl.set_editor_property("bake_data", da)

    data.set_editor_property("islands", islands)
    EAL.save_loaded_asset(data)
    log(f"ferdig: {total} instanser fordelt på {len(by_name)} øyer")


main()
