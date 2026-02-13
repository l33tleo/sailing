import unreal

# Create ocean material asset
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mat_factory = unreal.MaterialFactoryNew()

# Create material
material = asset_tools.create_asset("M_Ocean", "/Game/Materials", unreal.Material, mat_factory)

if material:
    # Set blend mode to translucent
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

    # Use Material Editing Library to create nodes
    mel = unreal.MaterialEditingLibrary

    # Create Color vector parameter
    color_param = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    color_param.set_editor_property("parameter_name", "Color")
    color_param.set_editor_property("default_value", unreal.LinearColor(0.02, 0.15, 0.45, 1.0))

    # Create Opacity scalar parameter
    opacity_param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 200)
    opacity_param.set_editor_property("parameter_name", "Opacity")
    opacity_param.set_editor_property("default_value", 0.85)

    # Create Roughness scalar parameter
    roughness_param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 400)
    roughness_param.set_editor_property("parameter_name", "Roughness")
    roughness_param.set_editor_property("default_value", 0.1)

    # Connect to material outputs
    mel.connect_material_property(color_param, "RGBA", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(opacity_param, "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(roughness_param, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Recompile
    mel.recompile_material(material)

    # Save
    unreal.EditorAssetLibrary.save_asset("/Game/Materials/M_Ocean")

    unreal.log("M_Ocean material created successfully!")
else:
    unreal.log_error("Failed to create material")
