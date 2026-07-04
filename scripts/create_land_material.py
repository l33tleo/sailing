#!/usr/bin/env python3
"""
Create /Game/Fjord/M_Land: samples the aerial ortho (T_OslofjordOrtho) by absolute
world position so all land polygons share one seamless map, with a Discovered scalar
parameter for a subtle highlight (keeps the photo, unlike a flat discovered material).

Reads the world-unit bbox from scripts/terrain/ortho.json.
Run from Unreal Editor Python Console, or via MCP execute_python.
"""
import json
import os
import unreal


def main() -> bool:
    project_dir = unreal.SystemLibrary.get_project_directory()
    meta_path = os.path.join(project_dir, "scripts", "terrain", "ortho.json")
    if not os.path.isfile(meta_path):
        unreal.log_error(f"Missing {meta_path} (run fetch_oslofjord_terrain.py first)")
        return False
    with open(meta_path, encoding="utf-8") as f:
        meta = json.load(f)
    b = meta["bbox_unreal"]
    min_x, min_y, max_x, max_y = b["min_x"], b["min_y"], b["max_x"], b["max_y"]
    size_x = max(max_x - min_x, 1.0)
    size_y = max(max_y - min_y, 1.0)

    ortho = unreal.load_asset("/Game/Fjord/T_OslofjordOrtho")
    if not ortho:
        unreal.log_error("T_OslofjordOrtho not found (run import_ortho_texture.py first)")
        return False

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    existing = unreal.load_asset("/Game/Fjord/M_Land")
    if existing:
        unreal.EditorAssetLibrary.delete_asset("/Game/Fjord/M_Land")
    material = asset_tools.create_asset("M_Land", "/Game/Fjord", unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        unreal.log_error("Failed to create M_Land")
        return False

    mel = unreal.MaterialEditingLibrary
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

    # World position -> XY components.
    world_pos = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, 0)
    mask_x = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -700, -80)
    mask_x.set_editor_property("r", True); mask_x.set_editor_property("g", False)
    mask_x.set_editor_property("b", False); mask_x.set_editor_property("a", False)
    mask_y = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -700, 120)
    mask_y.set_editor_property("r", False); mask_y.set_editor_property("g", True)
    mask_y.set_editor_property("b", False); mask_y.set_editor_property("a", False)
    mel.connect_material_expressions(world_pos, "", mask_x, "")
    mel.connect_material_expressions(world_pos, "", mask_y, "")

    # U = (X - min_x) / size_x
    sub_x = mel.create_material_expression(material, unreal.MaterialExpressionSubtract, -500, -80)
    sub_x.set_editor_property("const_b", float(min_x))
    mel.connect_material_expressions(mask_x, "", sub_x, "A")
    mul_x = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -320, -80)
    mul_x.set_editor_property("const_b", float(1.0 / size_x))
    mel.connect_material_expressions(sub_x, "", mul_x, "A")

    # V = (max_y - Y) / size_y   (image row 0 = north = max Y)
    sub_y = mel.create_material_expression(material, unreal.MaterialExpressionSubtract, -500, 120)
    sub_y.set_editor_property("const_a", float(max_y))
    mel.connect_material_expressions(mask_y, "", sub_y, "B")
    mul_y = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -320, 120)
    mul_y.set_editor_property("const_b", float(1.0 / size_y))
    mel.connect_material_expressions(sub_y, "", mul_y, "A")

    # UV = append(U, V)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionAppendVector, -150, 20)
    mel.connect_material_expressions(mul_x, "", uv, "A")
    mel.connect_material_expressions(mul_y, "", uv, "B")

    # Sample the ortho.
    tex = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, 60, 0)
    tex.set_editor_property("texture", ortho)
    mel.connect_material_expressions(uv, "", tex, "UVs")
    mel.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    # Roughness (land is matte).
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, 60, 260)
    rough.set_editor_property("r", 0.85)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Discovered highlight: Emissive = Discovered * warm tint (subtle, keeps photo).
    discovered = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -320, 420)
    discovered.set_editor_property("parameter_name", "Discovered")
    discovered.set_editor_property("default_value", 0.0)
    tint = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -320, 560)
    tint.set_editor_property("constant", unreal.LinearColor(0.25, 0.22, 0.05, 1.0))
    emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -100, 480)
    mel.connect_material_expressions(discovered, "", emissive, "A")
    mel.connect_material_expressions(tint, "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset("/Game/Fjord/M_Land")
    unreal.log(f"Created /Game/Fjord/M_Land (bbox {min_x:.0f},{min_y:.0f}..{max_x:.0f},{max_y:.0f})")
    return True


if __name__ == "__main__":
    main()
