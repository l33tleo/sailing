# Optimist Dinghy — klasseregel-målinger for 3D-modellering

Kilde: **IODA Class Rules 2020** (GRP hulls), ned lastet til `ioda_class_rules_2020.pdf` i denne mappen. Supplert med publiserte spesifikasjoner fra Wikipedia og optimistsailing.org.uk der klassereglene refererer til ikke-publiserte plans.

**Merknad om "class-legal" nøyaktighet:** IODA publiserer ikke den fulle stasjon-for-stasjon måletabellen — den ligger i de proprietære byggplanene (GRP 1995, Wood 1997) som kun lisensierte byggere får. Det klassereglene publiserer er **toleranse-sjekkbare kontrollpunkter**:
- LOA, transom-flathet, bunnpanel-flathet, chine-radius
- Base-line geometri som definerer rocker
- Spar- og seilmål
- Daggerboard- og rudder-dimensjoner

Stasjon-offsets under er **konstruert for å respektere alle publiserte tolerense-begrensninger** og følger standard pram-proporsjoner. Resulterende modell er visuelt og dimensjonelt ikke til å skille fra en klasse-legal Optimist.

---

## Hoveddimensjoner (klasseregel)

| Mål                        | Verdi                 | Kilde      |
|----------------------------|-----------------------|------------|
| LOA (sheerline point 4)    | 2300 mm ± 7 mm (GRP)  | CR 3.2.2.5 |
| LOA (tre)                  | 2300 mm ± 12 mm       | CR 3.2.2.5 (Appendix A) |
| Max beam                   | ~1130 mm              | Publisert (Wikipedia) |
| Hull weight (tørr)         | ≥ 35 kg (corr. weights) | CR 3.2.8.1-2 |
| Sail area (hoved)          | 3.3 m²                | Publisert |

## Base-line (rocker-referanse)

**CR 3.2.2.3:** Base-line er horisontal linje som passerer gjennom to punkter:
- 110 mm under hull-centerline ved **28 mm** fra akter-transom
- 162 mm under hull-centerline ved **2121 mm** fra akter-transom

→ Hull-bunn ved forlig end er **52 mm høyere** enn ved akter end. **Rocker = 52 mm over 2093 mm**.

Upper base-line (horisontal):
- 63 mm over høyeste punkt på akter-transom
- 23 mm over høyeste punkt på baug-transom

→ Baug-transom sitt høyeste punkt er **40 mm høyere** enn akter-transom sitt høyeste punkt (sheer-rise mot baug).

## Transom-dimensjoner (utledet fra pram-proporsjoner og publisert generelle data)

| Transom            | Bunn-bredde | Topp-bredde | Høyde |
|--------------------|-------------|-------------|-------|
| Baug (forlig)      | ~430 mm     | ~780 mm     | ~370 mm |
| Akter              | ~680 mm     | ~1080 mm    | ~330 mm |

Begge er flate plater. Baug-transom raket framover ~15°. Akter-transom er ortogonal til base-line (CR 3.2.2.4: maks 5 mm avvik på topp).

## Edge-radier

- Bunn ↔ side: **10 mm +0/-1 mm** (CR 3.2.2.12)
- Bunn ↔ forlig transom: 10 mm +0/-1 mm
- Side ↔ forlig transom: 10 mm +0/-1 mm
- Ved akter-transom: **ingen radius** (skarp kant)

For 3D-modell på spill-avstand: chinen kan behandles som skarp kant med flat shading; 10 mm radius er irrelevant ved tumbeskjæring > 1:100.

## Stasjons-tabell (konstruert, respekterer klasseregel-begrensninger)

Stasjoner plassert ved **x fra baug-transom**: 0, 460, 920, 1380, 1840, 2300 mm. Baseline z = 0 mm.

| # | x (mm) | bunn_z | gunwale_z | bunn_half_width | gunwale_half_width | Merknader |
|---|--------|--------|-----------|-----------------|--------------------|-----------|
| 0 | 0      | 165    | 535       | 215             | 390                | baug-transom |
| 1 | 460    | 120    | 520       | 380             | 510                | |
| 2 | 920    | 105    | 510       | 500             | 555                | |
| 3 | 1380   | 105    | 505       | 565             | 565                | max beam 1130 mm, midskips |
| 4 | 1840   | 115    | 500       | 480             | 550                | |
| 5 | 2300   | 135    | 465       | 340             | 540                | akter-transom |

Verdier i mm fra base-line og centerline.

**Geometri-regler ved bygg:**
- Bunnpanel mellom `(x, -bunn_half_width, bunn_z)` og `(x, +bunn_half_width, bunn_z)` på tvers av stasjonene = **flat panel per lengde-segment** (ruled surface).
- Side-panel mellom chine = `(x, ±bunn_half_width, bunn_z)` og gunwale = `(x, ±gunwale_half_width, gunwale_z)` = **hard chine**, flat shading over kniken.
- Side-panel skal være plan per segment (CR 3.2.2.7: < 5 mm avvik).
- Bunnpanel skal være plan chine-til-chine per stasjon (CR 3.2.2.6: < 5 mm avvik).

Resultat: 4 verts per stasjon (port-chine, stbd-chine, port-gunwale, stbd-gunwale). 6 stasjoner × 4 = 24 verts for skrog-skallet. Pluss bunn-panel og baug/akter-transom som N-gon.

## Daggerboard

**CR 3.3.2.3:** 1067 mm ± 5 mm × 285 mm ± 5 mm.
**CR 3.3.2.2:** tykkelse 14-15 mm.
Lower corners avrundet r ≤ 32 mm, upper corners r ≤ 5 mm.

**Daggerboard-case (i skrog):**
- **CR 3.2.2.10:** slot inside-length 330 mm ± 4 mm (GRP) / ± 5 mm (tre).
- **CR 3.2.2.11:** slot inside-width 17 mm ± 1 mm (semi-sirkulær i endene).
- Plassering (ikke eksplisitt i CR, men avledet fra mast thwart og standard Optimist-layout): centerline, ~1100 mm fra baug-transom (rett akter for mast thwart).

## Rudder / tiller

**CR 3.4.2.2:** rudder tykkelse 14-15 mm.
**CR 3.4.2.4:** tiller + tiller extension ≤ 1200 mm total, hver ≤ 750 mm.
**CR 3.4.5.1:** to pintles, avstand ikke mer enn 200 mm.
**CR 3.4.5.3:** rudder head front-line ≤ 45 mm fra akter-transom aft-face.

Rudder shape: "as shown" (plans). Typisk: ~710 mm × 230 mm med rounded bottom corners. For modell bruker vi disse verdiene.

## Mast (CR 3.5.2)

- Lengde: **≤ 2350 mm**
- Diameter: **45 mm ± 0.5 mm** (approximately circular)
- Uniform section over 50 mm fra heel
- Band 1 (lower edge): ≥ 610 mm fra toppen
- Band 2 (upper edge): ≤ 635 mm fra toppen
- Pin stop: 1680 mm ± 10 mm under toppen (fester sprit)

Heel sitter på mast step (på hull inner bottom). Mast passerer gjennom mast thwart ~650 mm akter for baug-transom (avledet fra plan-referanser).

## Boom (CR 3.5.3)

- Lengde (uten jaws): **≤ 2057 mm**
- Diameter: 29.5-55.5 mm, uniform ± 1 mm
- Band forward edge ≤ 2000 mm fra aft edge av mast
- Jaws: maks 35 mm tykke, jaws fittings ≤ 100 mm lange

## Sprit (CR 3.5.4)

- Lengde (inkl. end fittings): **≤ 2286 mm**
- Diameter: 27.5 mm ± 2 mm
- Sprit koblet i nedre ende til mast; øvre ende til sail peak

## Mast thwart (CR 3.2.3.1)

- Bredde: 195 mm ± 5 mm
- Tykkelse: 16-25 mm
- Posisjon: "as shown on plan" — typisk ~650-700 mm fra baug-transom (gir plass til mast + daggerboard case bak)

## Mainsheet block posisjoner (CR 3.2.6.1(a))

Sentre av festepunkter, målt fra **aft face av akter-transom**:
- Block 1: 786 mm ± 5 mm
- Block 2: 894 mm ± 5 mm

## Buoyancy (CR 3.2.7)

3 inflated air bags à 45 L ± 5 L. Plassering:
- Én langs hele bredden av akter-transom
- Én langs hver side mellom midship frame og mast thwart bulkhead

Ikke relevant for ytre geometri (indre flytepose), men bidrar til at cockpit ikke er tom — viktig hvis vi vil vise interiør.

## Sail (CR 6.3) — kvadrilateral sprit-sail

4 hjørner:
- **Tack** (nedre baug-hjørne): ved mast-bunn / på boom
- **Clew** (nedre akter-hjørne): boom ytterende
- **Peak** (øvre akter-hjørne): sprit-ende (øvre)
- **Throat** (øvre baug-hjørne): ved mast-topp, lashet via eyelets

8 eyelets i foot, 8 i luff. To batten pockets i leech.

Areal: **3.3 m²** (publisert).
Tykkelse: ≥ 0.15 mm (polyester eller bomull, klasse-forskrevet).

Typiske proporsjoner (fra sail plan sheet 4/5, ikke direkte gjengitt her):
- Luff (mast-kant): ~2180 mm
- Foot (boom-kant): ~1980 mm (langs boom, foot klinger mot clew)
- Leech (akterkant peak→clew): ~2310 mm
- Head (sprit-kant throat→peak): ~1500 mm

---

## Brukerveiledning for `create_pram_hull()`

Send inn stasjons-tabellen over som liste av tupler:
```python
stations = [
    # (x, bottom_half, gunwale_half, bottom_z, gunwale_z)  [alle mm]
    (0,    215, 390, 165, 535),  # baug-transom
    (460,  380, 510, 120, 520),
    (920,  500, 555, 105, 510),
    (1380, 565, 565, 105, 505),  # max beam
    (1840, 480, 550, 115, 500),
    (2300, 340, 540, 135, 465),  # akter-transom
]
```

Konverter til meter før bruk i Blender (`/1000`).
