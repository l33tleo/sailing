# Blender-konvensjoner — Sailing Game

Les dette før du rører Blender-scenen eller endrer FBX-eksporten.

## Enheter og akser
- Scene units: meters, scale 1.0
- **Bygg modellen med baug (forover) på +X og opp på +Z.** Dette matcher UE5 pawn forward.
- UE5-eksport (brukes av `setup_ue_export()`):
  - `apply_scale_options='FBX_SCALE_ALL'`
  - `axis_forward='X'`, `axis_up='Z'` med `bake_space_transform=True`
  - `global_scale=1.0`, `apply_unit_scale=True`
  - `object_types={'MESH'}`, `add_leaf_bones=False`, `embed_textures=False`
- **Vannlinje = z=0 i Blender.** Skrog-kjøl ved negativ z (f.eks. z=-0.10 for Optimist-dybde), dekk ved positiv z. Da havner båten naturlig på vannet i UE uten å tune `BoatVisualZOffset`.

Bekreftet i praksis (Optimist): hull z=[-0.10, 0.30] i Blender → bounds 234×113×40cm for skroget (hele meshet 234×113×322cm inkl. mast).

## Navngivning
- Static mesh: `SM_<Category>_<Name>_<LOD>` (f.eks. `SM_Boat_Optimist_LOD0`)
- Empties:
  - `PIVOT_<Name>` — rotasjonspunkter (f.eks. `PIVOT_Rudder`, `PIVOT_Boom`)
  - `SOCKET_<Name>` — festepunkter for UE (f.eks. `SOCKET_MastStep`, `SOCKET_RudderMount`)
- Materialer matcher UE-asset-navn: `M_Hull`, `M_Sail`, `M_Spar`, `M_Foil`, `M_Tiller` (som finnes i `Content/ModelsV2/`)

## Collections
- `01_Hull` — skrog, dekk, cockpit
- `02_Rigging` — mast, bom, sprit, battens
- `03_Sails` — seil, flagg
- `04_Fittings` — ror, rorkult, daggerboard, thwart, beslag
- `99_Reference` — referansebilder, gamle versjoner, backup (eksporteres ikke)

Gamle versjoner skal flyttes til `99_Reference`, ikke slettes. Eksport-scriptet ignorerer alt under `99_*` og `_Old*`-prefiks.

## Topologi
- Kvader kun på deformerbart mesh (seil, flagg) — trekanter OK på statisk skrog
- Skrog: stasjoner hver 20 cm langs X, subsurf level 2 kun ved behov (slå av før eksport hvis LOD0 skal være lav-poly)
- Mirror modifier på symmetriske deler (skrog), apply først rett før eksport
- Shade smooth på alle skrog-faces, flat shading på beslag/spars

## Arbeidsmåte — før hver endring
1. **Mål først, geometri etterpå.** Plasser empties som stasjoner, mast-step, spantene, dekk-kant før mesh. Gir målbare, korrigerbare referanser.
2. **Bryt ned til primitiver + modifikatorer.** `cube → loop cuts → bevel → subsurf → mirror` er mer robust enn rå vertex-manipulering. Unngå edit mode på individuelle verts med mindre nødvendig.
3. **Kall helpere, ikke skriv fra scratch.** Bruk `sailing_blender_utils.py` (se `scripts/blender/sailing_blender_utils.py`). Hver oppgave = funksjonskall, ikke 200 linjer ny bpy-kode.
4. **Planlegg før utførelse** for ikke-triviell modellering: bounding box, topologi-strategi, hvilke modifikatorer, pivot-posisjoner for UE, LOD-plan. Skriv planen først, brukeren reviewer, deretter kjør.
5. **Render ortho før og etter** — front/side/top til `renders/<object>_<step>.png`. Sammenlign mot `reference/<object>.png` før du går videre.

## Versjonering
- `.blend`-filer: `<Navn>_v001.blend`, `v002.blend`, ... (aldri overskriv forrige)
- Scripts: git
- Ved topologi-ødeleggelse: rull tilbake til siste .blend-versjon, ikke forsøk å reparere vertex-vertex

## Hva Claude kan og ikke kan
- **Kan:** blockout, hard-surface (rigg, beslag, dekksmoduler), konstruksjonsdrevne former, helper-funksjonsbygging
- **Kan ikke godt:** organisk formspråk (skrogkurver som *faktisk* ser riktige ut), seildrap/folds, sculpting. For organisk: bruker sculpting sist, eller start fra en scan/kit-bash og la Claude modifisere

## FBX-import-gotcha (Unreal-siden)
Husk disse `static_mesh_import_data`-flaggene — import feiler stille uten:
- `generate_lightmap_u_vs = True`
- `auto_generate_collision = True`
- `remove_degenerates = True`
- `normal_import_method = FBXNIM_IMPORT_NORMALS_AND_TANGENTS`

Etter import, reassign materialer med `asset.set_material(i, mat)` og lagre med `save_loaded_asset(asset, only_if_is_dirty=False)`.
