import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary

def create_color_material(name, path, r, g, b, roughness_val=0.5):
    full_path = path + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)

    mat = asset_tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())

    # Create VectorParameter for BaseColor (so we can change it via MID later)
    color_param = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -300, 0)
    color_param.set_editor_property("parameter_name", "BaseColor")
    color_param.set_editor_property("default_value", unreal.LinearColor(r, g, b, 1.0))

    # Create ScalarParameter for Roughness
    rough_param = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -300, 200)
    rough_param.set_editor_property("parameter_name", "Roughness")
    rough_param.set_editor_property("default_value", roughness_val)

    # Connect to material outputs
    mel.connect_material_property(color_param, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough_param, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(full_path)
    unreal.log("Created material: " + full_path)
    return mat

# Brown boat material
create_color_material("M_Boat", "/Game/Materials", 0.45, 0.25, 0.1, 0.8)

# Green island material
create_color_material("M_Island", "/Game/Materials", 0.15, 0.4, 0.1, 0.9)

# Bright green discovered island
create_color_material("M_IslandDiscovered", "/Game/Materials", 0.2, 0.8, 0.2, 0.7)

unreal.log("SUCCESS: All colored materials created!")
