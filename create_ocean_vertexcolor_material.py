import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Delete existing if present
if unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/M_OceanVC"):
    unreal.EditorAssetLibrary.delete_asset("/Game/Materials/M_OceanVC")

# Create material
mat_ocean = asset_tools.create_asset("M_OceanVC", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
mat_ocean.set_editor_property("two_sided", True)

# Create VertexColor node
vc_node = unreal.MaterialEditingLibrary.create_material_expression(mat_ocean, unreal.MaterialExpressionVertexColor, -300, 0)

# Create roughness constant
roughness_const = unreal.MaterialEditingLibrary.create_material_expression(mat_ocean, unreal.MaterialExpressionConstant, -300, 200)
roughness_const.set_editor_property("r", 0.15)

# Connect using the property-based method
unreal.MaterialEditingLibrary.connect_material_property(vc_node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
unreal.MaterialEditingLibrary.connect_material_property(roughness_const, "", unreal.MaterialProperty.MP_ROUGHNESS)

# Compile and save
unreal.MaterialEditingLibrary.recompile_material(mat_ocean)
unreal.EditorAssetLibrary.save_asset("/Game/Materials/M_OceanVC")

unreal.log("SUCCESS: M_OceanVC created - uses VertexColor for ProceduralMesh!")
