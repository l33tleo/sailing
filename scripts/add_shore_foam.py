"""Legger strandskum inn i /Game/Materials/Water/M_FjordWater (idempotent — kan kjøres om igjen).

Kjør headless (editoren LUKKET), etter scripts/bake/bake_shore_field.py:
  UnrealEditor-Cmd Sailing.uproject -run=pythonscript \
      -script=scripts/add_shore_foam.py -unattended -nosplash -nullrhi
Resultatlinjer er merket [FOAM].

M_FjordWater er Water-pluginets havmateriale i attributt-modus: pluginets logikk → Break → Make,
med skrogmasken på OpacityMask. Her kobles Break.BaseColor og Break.Roughness om via Lerp-noder
mot skumfarge/-ruhet, styrt av:
  bånd    = 1 − saturate(avstand_til_kyst / FoamWidth), fra T_ShoreDistance (verdens-XY)
  bølging = 0,5 + 0,5·sin(Time·FoamSpeed − avstand·FoamPhase)   (skummet «trekker» inn mot land)
  struktur = panorerende, sømløs støytekstur
Alle noder som legges inn får desc «SHOREFOAM», så en ny kjøring fjerner de gamle først.

NB: get_material_expression_input_names() på Make-noden krasjet editoren interaktivt (CLAUDE.md),
men går fint i commandlet — vi bruker den kun her, headless.
"""
import json
import os

import unreal

mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
BAKE_DIR = os.path.join(PROJECT, "scripts/cache/bake")
MAT = "/Game/Materials/Water/M_FjordWater"
TEX_DEST = "/Game/Materials/Water"
TAG = "SHOREFOAM"


def log(msg):
    unreal.log(f"[FOAM] {msg}")


def import_texture(name, srgb, mips=True, wrap=True):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(BAKE_DIR, name + ".png")
    t.destination_path, t.destination_name = TEX_DEST, name
    t.automated, t.save, t.replace_existing = True, False, True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    tex = unreal.load_asset(f"{TEX_DEST}/{name}")
    tex.set_editor_property("srgb", srgb)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    if not mips:
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    addr = unreal.TextureAddress.TA_WRAP if wrap else unreal.TextureAddress.TA_CLAMP
    tex.set_editor_property("address_x", addr)
    tex.set_editor_property("address_y", addr)
    EAL.save_loaded_asset(tex)
    return tex


class G:
    def __init__(self, m):
        self.m, self.i = m, 0

    def node(self, cls, **props):
        self.i += 1
        n = mel.create_material_expression(self.m, cls, -2600 + (self.i % 8) * 180, 900 + (self.i // 8) * 150)
        n.set_editor_property("desc", TAG)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(self, src, dst, pin, out=""):
        assert mel.connect_material_expressions(src, out, dst, pin), f"kobling {pin} feilet"
        return dst

    def param(self, name, v):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=float(v))

    def binop(self, cls, a, b):
        n = self.node(cls)
        for pin, v in (("A", a), ("B", b)):
            if isinstance(v, (int, float)):
                n.set_editor_property("const_a" if pin == "A" else "const_b", float(v))
            else:
                self.link(v, n, pin)
        return n

    def mask(self, a, r=False, g=False, b=False):
        return self.link(a, self.node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False), "")

    def lerp(self, a, b, alpha, a_out=""):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, n, "A", a_out); self.link(b, n, "B"); self.link(alpha, n, "Alpha")
        return n


def main():
    m = unreal.load_asset(MAT)
    make = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    assert make and make.get_class().get_name() == "MaterialExpressionMakeMaterialAttributes"
    inputs = list(mel.get_inputs_for_material_expression(m, make))
    names = list(mel.get_material_expression_input_names(make))
    brk = inputs[names.index("Metallic")]   # Metallic er urørt → peker alltid på Break-noden
    assert brk.get_class().get_name() == "MaterialExpressionBreakMaterialAttributes"

    # Fjern forrige kjørings noder og koble Break → Make direkte igjen.
    old = [e for e in mel.get_material_expressions(m) if e.get_editor_property("desc") == TAG]
    for e in old:
        mel.delete_material_expression(m, e)
    for pin in ("BaseColor", "Roughness"):
        mel.connect_material_expressions(brk, pin, make, pin)
    log(f"fjernet {len(old)} gamle noder")

    meta = json.load(open(os.path.join(BAKE_DIR, "T_ShoreDistance.json"), encoding="utf-8"))
    shore_tex = import_texture("T_ShoreDistance", srgb=False, mips=False, wrap=False)
    noise_tex = import_texture("T_FoamNoise", srgb=False)

    g = G(m)
    wp = g.node(unreal.MaterialExpressionWorldPosition)
    x, y = g.mask(wp, r=True), g.mask(wp, g=True)
    sx, sy = (meta["max_x_m"] - meta["min_x_m"]) * 100.0, (meta["max_y_m"] - meta["min_y_m"]) * 100.0
    u = g.binop(unreal.MaterialExpressionMultiply, g.binop(unreal.MaterialExpressionSubtract, x, meta["min_x_m"] * 100.0), 1.0 / sx)
    v = g.binop(unreal.MaterialExpressionMultiply, g.binop(unreal.MaterialExpressionSubtract, meta["max_y_m"] * 100.0, y), 1.0 / sy)
    uv = g.node(unreal.MaterialExpressionAppendVector)
    g.link(u, uv, "A"); g.link(v, uv, "B")
    dist = g.node(unreal.MaterialExpressionTextureSample, texture=shore_tex,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    g.link(uv, dist, "UVs")
    dist_r = g.mask(dist, r=True)                                      # 0 = land … 1 = ≥ range_m

    # Bånd: 1 − saturate(d / FoamWidth)   (FoamWidth i andel av range_m; 0,45·40 m = 18 m)
    band = g.link(g.binop(unreal.MaterialExpressionDivide, dist_r, g.param("FoamWidth", float(os.environ.get("FOAM_WIDTH", 0.5)))),
                  g.node(unreal.MaterialExpressionOneMinus), "")
    band = g.link(band, g.node(unreal.MaterialExpressionSaturate), "")
    # Bratt kant mot land unngås: skummet er svakt helt inne ved land (d≈0) og sterkest litt ute.
    band_sq = g.binop(unreal.MaterialExpressionMultiply, band, band)

    # Bølging: sin(Time·speed − d·phase) → 0..1
    time = g.node(unreal.MaterialExpressionTime)
    phase = g.binop(unreal.MaterialExpressionSubtract,
                    g.binop(unreal.MaterialExpressionMultiply, time, g.param("FoamSpeed", 0.9)),
                    g.binop(unreal.MaterialExpressionMultiply, dist_r, g.param("FoamPhase", 9.0)))
    surge = g.link(phase, g.node(unreal.MaterialExpressionSine, period=6.2831853), "")
    surge01 = g.binop(unreal.MaterialExpressionAdd, g.binop(unreal.MaterialExpressionMultiply, surge, 0.5), 0.5)

    # Struktur: panorerende støy på verdens-XY
    nuv = g.binop(unreal.MaterialExpressionDivide, g.mask(wp, r=True, g=True), g.param("FoamNoiseTileUU", 900.0))
    pan = g.binop(unreal.MaterialExpressionMultiply, time, 0.03)
    nuv2 = g.binop(unreal.MaterialExpressionAdd, nuv, pan)
    noise = g.node(unreal.MaterialExpressionTextureSample, texture=noise_tex,
                   sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    g.link(nuv2, noise, "UVs")
    noise_r = g.binop(unreal.MaterialExpressionMultiply, g.mask(noise, r=True), 1.6)

    foam = g.binop(unreal.MaterialExpressionMultiply, band_sq,
                   g.binop(unreal.MaterialExpressionAdd, 0.45, g.binop(unreal.MaterialExpressionMultiply, surge01, 0.55)))
    foam = g.binop(unreal.MaterialExpressionMultiply, foam, noise_r)
    foam = g.binop(unreal.MaterialExpressionMultiply, foam, g.param("FoamStrength", float(os.environ.get("FOAM_STRENGTH", 2.0))))
    # NB: DepthFade (kontaktlinje i snittet vann/terreng) FORSØKT og forkastet: i Single Layer Water
    # leser den 0 overalt (scenedybden bak vannet er ikke tilgjengelig i dette passet), og hele
    # havflaten ble skum.
    foam = g.link(foam, g.node(unreal.MaterialExpressionSaturate), "")

    foam_color = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="FoamColor",
                        default_value=unreal.LinearColor(0.92, 0.93, 0.93, 1.0))
    base = g.lerp(brk, foam_color, foam, a_out="BaseColor")
    rough = g.lerp(brk, g.param("FoamRoughness", 0.6), foam, a_out="Roughness")
    g.link(base, make, "BaseColor")
    g.link(rough, make, "Roughness")

    mel.recompile_material(m)
    EAL.save_loaded_asset(m)
    log(f"skum lagt inn: {mel.get_num_material_expressions(m)} noder totalt")


main()
