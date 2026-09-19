"""Fase 0-spike: importerer scripts/cache/spike/SM_NaniteSpike.glb headless, slår på Nanite og
skriver ut bounds + høyeste verteks slik at akse-/enhetskonverteringen kan verifiseres.

Kjør (editoren LUKKET):
  UnrealEditor-Cmd Sailing.uproject -run=pythonscript \
      -script=scripts/bake/spike_import_test_mesh.py -unattended -nosplash -nullrhi
Resultatlinjene er merket [SPIKE] i loggen.
"""
import os

import unreal

SRC = os.path.join(unreal.Paths.project_dir(), "scripts/cache/spike/SM_NaniteSpike.glb")
DEST = "/Game/Fjord/Spike"
NAME = "SM_NaniteSpike"


def log(msg):
    unreal.log(f"[SPIKE] {msg}")


def main():
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(SRC)
    task.destination_path = DEST
    task.destination_name = NAME
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    paths = [str(p) for p in task.get_editor_property("imported_object_paths")]
    log(f"importerte: {paths}")

    mesh = None
    for p in paths:
        obj = unreal.load_asset(p.split(".")[0])
        if isinstance(obj, unreal.StaticMesh):
            mesh = obj
            break
    if mesh is None:
        log("FEIL: ingen StaticMesh importert")
        return

    # Editor-subsystemer finnes ikke i en commandlet; sett egenskapen direkte
    # (set_editor_property utløser PostEditChange → Nanite-bygg).
    settings = mesh.get_editor_property("nanite_settings")
    settings.enabled = True
    mesh.set_editor_property("nanite_settings", settings)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    b = mesh.get_bounds()
    o, e = b.origin, b.box_extent
    log(f"asset={mesh.get_path_name()} nanite={mesh.get_editor_property('nanite_settings').enabled}")
    log(f"bounds min=({o.x - e.x:.0f},{o.y - e.y:.0f},{o.z - e.z:.0f}) "
        f"max=({o.x + e.x:.0f},{o.y + e.y:.0f},{o.z + e.z:.0f})")
    log(f"lod0 (fallback) tris={mesh.get_num_triangles(0)} verts={mesh.get_num_vertices(0)}")

    try:
        verts = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, 0)[0]
        top = max(verts, key=lambda v: v.z)
        log(f"høyeste verteks=({top.x:.0f},{top.y:.0f},{top.z:.0f})  forventet ≈(15000,0,6000)")
    except Exception as ex:  # noqa: BLE001 - kun diagnostikk
        log(f"kunne ikke lese vertekser: {ex}")


main()
