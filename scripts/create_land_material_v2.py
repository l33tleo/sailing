"""Bygger /Game/Fjord/M_LandV2: lagdelt landmateriale for bakt Nanite-terreng.

Kjør headless (editoren LUKKET), etter scripts/fetch_surface_textures.py:
  UnrealEditor-Cmd Sailing.uproject -run=pythonscript \
      -script=scripts/create_land_material_v2.py -unattended -nosplash -nullrhi
Resultatlinjer er merket [LANDMAT].

Lag og blanding
  Cliff   bratte flater (helning fra VertexNormalWS), TRIPLANAR → ingen vertikale fargestriper
  Shore   svaberg/rullestein fra sjøbunn til ~ShoreTopM over vann, med mørkt/blankt våtbånd
  Grass / Forest  velges av flyfotoets lyshet (mørkt = skog)
  Ortho   flyfotoet (verdens-XY, som M_Land) farger detaljlagene på nært hold og tar over på
          avstand (FarStart..FarEnd), bortsett fra på klipper der fotoet bare er striper.
Detalj-UV bruker posisjon RELATIVT til objektets origo (øyas pivot): verdenskoordinatene er
millioner av uu, og float-presisjonen blir for dårlig til å flislegge direkte på dem.

Bevisst unngått (krasjer editoren ved skripting): SetMaterialAttributes-noden og
get_material_expression_input_names() på Make-noden. Alt kobles pinne for pinne.
"""
import json
import os

import unreal

mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC_DIR = os.path.join(PROJECT, "scripts/cache/surfaces")
TEX_DEST = "/Game/Fjord/Surfaces"
MAT_PATH = "/Game/Fjord/M_LandV2"
LAYERS = ["Cliff", "Shore", "Grass", "Forest"]
WATER_Z = 100.0


def log(msg):
    unreal.log(f"[LANDMAT] {msg}")


# ---------------------------------------------------------------- teksturer
def import_textures():
    tasks = []
    for layer in LAYERS:
        for suffix in ("D", "N", "ARM"):
            name = f"T_Land_{layer}_{suffix}"
            src = os.path.join(SRC_DIR, name + ".jpg")
            if not os.path.exists(src):
                raise RuntimeError(f"mangler {src} (kjør scripts/fetch_surface_textures.py)")
            t = unreal.AssetImportTask()
            t.filename, t.destination_path, t.destination_name = src, TEX_DEST, name
            t.automated, t.save, t.replace_existing = True, False, True
            tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    tex = {}
    for layer in LAYERS:
        for suffix in ("D", "N", "ARM"):
            name = f"T_Land_{layer}_{suffix}"
            a = unreal.load_asset(f"{TEX_DEST}/{name}")
            if suffix == "N":
                a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                a.set_editor_property("srgb", False)
            elif suffix == "ARM":
                a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                a.set_editor_property("srgb", False)
            EAL.save_loaded_asset(a)
            tex[(layer, suffix)] = a
    log(f"importerte {len(tex)} teksturer til {TEX_DEST}")
    return tex


# ---------------------------------------------------------------- grafhjelpere
class Graph:
    def __init__(self, material):
        self.m = material
        self.row = 0

    def node(self, cls, **props):
        self.row += 1
        n = mel.create_material_expression(self.m, cls, -2000 + (self.row % 12) * 160, (self.row // 12) * 140)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(self, src, dst, pin, out=""):
        if not mel.connect_material_expressions(src, out, dst, pin):
            raise RuntimeError(f"kobling feilet: {src.get_name()} → {dst.get_name()}.{pin}")
        return dst

    def const(self, v):
        return self.node(unreal.MaterialExpressionConstant, r=float(v))

    def param(self, name, default):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name,
                         default_value=float(default))

    def binop(self, cls, a, b):
        n = self.node(cls)
        for pin, v in (("A", a), ("B", b)):
            if isinstance(v, (int, float)):
                n.set_editor_property("const_a" if pin == "A" else "const_b", float(v))
            else:
                self.link(v, n, pin)
        return n

    def add(self, a, b): return self.binop(unreal.MaterialExpressionAdd, a, b)
    def sub(self, a, b): return self.binop(unreal.MaterialExpressionSubtract, a, b)
    def mul(self, a, b): return self.binop(unreal.MaterialExpressionMultiply, a, b)
    def div(self, a, b): return self.binop(unreal.MaterialExpressionDivide, a, b)

    def unary(self, cls, a):
        return self.link(a, self.node(cls), "")

    def sat(self, a): return self.unary(unreal.MaterialExpressionSaturate, a)
    def one_minus(self, a): return self.unary(unreal.MaterialExpressionOneMinus, a)
    def abs(self, a): return self.unary(unreal.MaterialExpressionAbs, a)

    def lerp(self, a, b, alpha):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, n, "A"); self.link(b, n, "B"); self.link(alpha, n, "Alpha")
        return n

    def mask(self, a, r=False, g=False, b=False):
        n = self.node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False)
        return self.link(a, n, "")

    def append(self, a, b):
        n = self.node(unreal.MaterialExpressionAppendVector)
        self.link(a, n, "A"); self.link(b, n, "B")
        return n

    def ramp(self, x, lo, hi):
        """saturate((x - lo) / (hi - lo)); lo/hi kan være noder eller tall."""
        both_numbers = isinstance(lo, (int, float)) and isinstance(hi, (int, float))
        span = float(hi - lo) if both_numbers else self.sub(hi, lo)
        return self.sat(self.div(self.sub(x, lo), span))

    def sample(self, texture, uv, kind):
        sampler = {"D": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                   "N": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                   "ARM": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind]
        n = self.node(unreal.MaterialExpressionTextureSample, texture=texture, sampler_type=sampler,
                      sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        self.link(uv, n, "UVs")
        return n


# ---------------------------------------------------------------- materialet
def build(tex):
    if EAL.does_asset_exist(MAT_PATH):
        EAL.delete_asset(MAT_PATH)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_LandV2", "/Game/Fjord", unreal.Material, unreal.MaterialFactoryNew())
    g = Graph(material)

    # --- Flyfoto på verdens-XY (samme avbildning som M_Land) ---
    meta = json.load(open(os.path.join(PROJECT, "scripts/terrain/ortho.json"), encoding="utf-8"))["bbox_unreal"]
    size_x, size_y = meta["max_x"] - meta["min_x"], meta["max_y"] - meta["min_y"]
    wp = g.node(unreal.MaterialExpressionWorldPosition)
    u = g.mul(g.sub(g.mask(wp, r=True), meta["min_x"]), 1.0 / size_x)
    v = g.mul(g.sub(meta["max_y"], g.mask(wp, g=True)), 1.0 / size_y)
    ortho = g.node(unreal.MaterialExpressionTextureSample, texture=unreal.load_asset("/Game/Fjord/T_OslofjordOrtho"))
    g.link(g.append(u, v), ortho, "UVs")
    ortho_rgb = g.mul(ortho, 1.0)  # RGB-uttak som vanlig node
    ortho_lum = g.node(unreal.MaterialExpressionDotProduct)
    g.link(ortho, ortho_lum, "A")
    g.link(g.node(unreal.MaterialExpressionConstant3Vector,
                  constant=unreal.LinearColor(0.3, 0.5, 0.2, 1.0)), ortho_lum, "B")

    # --- Masker ---
    normal_ws = g.node(unreal.MaterialExpressionVertexNormalWS)
    slope = g.one_minus(g.mask(normal_ws, b=True))                       # 0 flatt … 1 loddrett
    cliff = g.ramp(slope, g.param("CliffSlopeStart", 0.10), g.param("CliffSlopeEnd", 0.28))
    height_m = g.mul(g.sub(g.mask(wp, b=True), WATER_Z), 0.01)          # meter over middelvann
    shore = g.one_minus(g.ramp(height_m, g.param("ShoreSolidM", 0.4), g.param("ShoreTopM", 2.2)))
    wet = g.one_minus(g.ramp(height_m, -0.2, g.param("WetBandTopM", 0.8)))
    forest = g.one_minus(g.ramp(ortho_lum, g.param("ForestLumLow", 0.10), g.param("ForestLumHigh", 0.22)))
    far = g.ramp(g.node(unreal.MaterialExpressionPixelDepth),
                 g.param("FarStart", 120000.0), g.param("FarEnd", 450000.0))
    # Flyfotoet er projisert rett ovenfra og smøres ut til striper i skråninger lenge før de er
    # bratte nok til å være «klippe»; stol mindre på det jo brattere det er.
    ortho_ok = g.one_minus(g.ramp(slope, g.param("OrthoSlopeStart", 0.04), g.param("OrthoSlopeEnd", 0.14)))

    # --- Detalj-UV relativt til objektets origo ---
    rel = g.sub(wp, g.node(unreal.MaterialExpressionObjectPositionWS))
    rel_xy = g.mask(rel, r=True, g=True)
    rel_xz = g.append(g.mask(rel, r=True), g.mask(rel, b=True))
    rel_yz = g.mask(rel, g=True, b=True)

    def flat_layer(layer, tile_param, tile_default, with_arm=True):
        uv = g.div(rel_xy, g.param(tile_param, tile_default))
        d = g.sample(tex[(layer, "D")], uv, "D")
        n = g.sample(tex[(layer, "N")], uv, "N")
        arm = g.sample(tex[(layer, "ARM")], uv, "ARM") if with_arm else None
        return d, n, arm

    grass_d, grass_n, grass_arm = flat_layer("Grass", "GrassTileUU", 350.0)
    # Svak grønntoning: kildeteksturen er høstlig, og «gyllen time»-lyset trekker alt mot gult.
    grass_d = g.mul(grass_d, g.node(unreal.MaterialExpressionVectorParameter, parameter_name="GrassTint",
                                    default_value=unreal.LinearColor(0.80, 0.95, 0.65, 1.0)))
    forest_d, forest_n, _ = flat_layer("Forest", "ForestTileUU", 500.0, with_arm=False)
    # Kildeteksturen er høstløv i rødbrunt; under barskog er bunnen mørkere og grønnere (mose, lyng).
    forest_d = g.mul(forest_d, g.node(unreal.MaterialExpressionVectorParameter, parameter_name="ForestTint",
                                      default_value=unreal.LinearColor(0.50, 0.62, 0.38, 1.0)))
    shore_d, shore_n, shore_arm = flat_layer("Shore", "ShoreTileUU", 600.0)

    # Klippe: triplanar over XZ/YZ, vektet av |N.x| mot |N.y| (toppflaten er aldri klippe).
    tile_c = g.param("CliffTileUU", 900.0)
    uv_xz, uv_yz = g.div(rel_xz, tile_c), g.div(rel_yz, tile_c)
    ax, ay = g.abs(g.mask(normal_ws, r=True)), g.abs(g.mask(normal_ws, g=True))
    w_yz = g.div(ax, g.add(g.add(ax, ay), 0.0001))                      # flate som vender mot ±X → YZ
    cliff_d = g.lerp(g.sample(tex[("Cliff", "D")], uv_xz, "D"), g.sample(tex[("Cliff", "D")], uv_yz, "D"), w_yz)
    cliff_n = g.lerp(g.sample(tex[("Cliff", "N")], uv_xz, "N"), g.sample(tex[("Cliff", "N")], uv_yz, "N"), w_yz)
    cliff_arm = g.sample(tex[("Cliff", "ARM")], uv_xz, "ARM")
    # Oslofeltets kalkstein/skifer er grå: demp fargen litt og la lavet i teksturen stå igjen.
    desat = g.node(unreal.MaterialExpressionDesaturation)
    g.link(cliff_d, desat, "")
    g.link(g.param("CliffDesaturate", 0.25), desat, "Fraction")
    cliff_d = g.mul(desat, g.node(unreal.MaterialExpressionVectorParameter, parameter_name="CliffTint",
                                  default_value=unreal.LinearColor(0.85, 0.85, 0.86, 1.0)))

    def stack(veg_a, veg_b, shore_v, cliff_v):
        return g.lerp(g.lerp(g.lerp(veg_a, veg_b, forest), shore_v, shore), cliff_v, cliff)

    forest_arm = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 0.9, 0.0, 1.0))
    detail = stack(grass_d, forest_d, shore_d, cliff_d)
    normal = stack(grass_n, forest_n, shore_n, cliff_n)
    arm = stack(grass_arm, forest_arm, shore_arm, cliff_arm)

    # --- Farge: fototonet detalj nært, foto på avstand (klipper beholder steinteksturen) ---
    tinted = g.mul(g.mul(detail, ortho_rgb), g.param("OrthoTintGain", 1.8))
    tint_w = g.mul(ortho_ok, g.param("OrthoTintAmount", 0.25))
    near = g.lerp(detail, tinted, tint_w)
    color = g.lerp(near, ortho_rgb, g.mul(far, ortho_ok))
    # Våtbånd: mørkere og blankere fra sjøbunn til litt over vannlinjen.
    color = g.mul(color, g.lerp(g.const(1.0), g.param("WetDarken", 0.55), wet))
    # Sjøbunn: under vann går fargen mot mørk tang/mudder med dybden, uten fototoning (flyfotoet
    # av havflaten er lyst/gult). Ellers ser bunnen ut som en gul flekk der båtens skygge fjerner
    # speilingen i vannflaten.
    submerged = g.one_minus(g.ramp(height_m, g.param("SeabedDeepM", -5.0), g.param("SeabedShallowM", -0.3)))
    seabed_col = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="SeabedColor",
                        default_value=unreal.LinearColor(0.045, 0.07, 0.05, 1.0))
    color = g.lerp(color, seabed_col, submerged)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = g.lerp(g.mask(arm, g=True), g.param("WetRoughness", 0.25), wet)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.mask(arm, r=True), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    # Normaldetalj fades ut med avstand (shimmer + bortkastet arbeid langt unna).
    flat_n = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    mel.connect_material_property(g.lerp(normal, flat_n, far), "", unreal.MaterialProperty.MP_NORMAL)

    # Oppdaget-markering: svak varm glød, som i M_Land.
    glow = g.mul(g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.25, 0.22, 0.05, 1.0)),
                 g.param("Discovered", 0.0))
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_NANITE)
    mel.recompile_material(material)
    EAL.save_loaded_asset(material)
    log(f"bygget {MAT_PATH}: {mel.get_num_material_expressions(material)} noder")


tex = import_textures()
build(tex)
