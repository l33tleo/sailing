#!/usr/bin/env python3
"""Kalibrering av båtfart mot reell Optimist-polar.

Simulerer nøyaktig samme fremdriftsmodell som ASailboatPawn::Tick:
    a = SailForceAccelScale * (AWS * polar(AWA) - DragCoefficient * v^2) - LinearDamping * v
med tilsynelatende vind = sann vind + båtfart (AWindActor-konvensjonen), og skriver ut
likevektsfart (knop) per sann vindstyrke/-vinkel pluss responstid.

    python3 scripts/tuning/optimist_polar_fit.py          # tabell for dagens verdier
    python3 scripts/tuning/optimist_polar_fit.py --fit    # tilpass mot TARGET på nytt

Referanse (seiler, 2026-09-18): lens i 9 m/s = 3,5–5,5 kn; skrogfart ~3,5 kn, planing/surf 5–6+ kn.
Øvrige måltall er en omtrentlig Optimist-polar. Hold verdiene i PARAMS lik SailboatPawn.h.
"""
import math, random, sys

KN = 1 / 51.444          # cm/s -> knop
LINEAR_DAMPING = 0.5     # CapsuleComp->BodyInstance.LinearDamping
NOGO, CLOSE_ANGLE, BEAM = 25.0, 50.0, 1.0
PARAMS = {'ch': 0.75, 'broad': 1.0, 'run': 0.85, 'cd': 0.0043, 'scale': 0.3}

TWAS = (45, 90, 135, 180)
TARGET = {  # sann vind (cm/s) -> knop ved TWAS
    300: (2.2, 2.8, 2.5, 2.0), 500: (3.0, 3.7, 3.5, 2.9), 700: (3.4, 4.3, 4.3, 3.6),
    900: (3.7, 5.2, 5.5, 4.4), 1100: (3.8, 6.0, 6.5, 5.0), 1300: (3.9, 6.8, 7.3, 5.5),
}
WEIGHT = {(900, 180): 5, (1100, 180): 3, (1300, 180): 3}   # lens-tallene er de sikreste


def polar(awa, p):
    if awa < NOGO:
        return 0.0
    if awa < CLOSE_ANGLE:
        return p['ch'] * (awa - NOGO) / (CLOSE_ANGLE - NOGO)
    if awa < 90:
        return p['ch'] + (BEAM - p['ch']) * (awa - CLOSE_ANGLE) / (90 - CLOSE_ANGLE)
    if awa < 135:
        return BEAM + (p['broad'] - BEAM) * (awa - 90) / 45
    return p['broad'] + (p['run'] - p['broad']) * (awa - 135) / 45


def equilibrium(tws, twa, p, dt=0.02, steps=3000):
    """Returnerer (fart cm/s, AWA, tid til 63 % av sluttfart)."""
    v, c, s = 0.0, math.cos(math.radians(twa)), tws * math.sin(math.radians(twa))
    hist, awa = [], 0.0
    for _ in range(steps):
        f = tws * c + v
        awa, aws = math.degrees(math.atan2(s, f)), math.hypot(f, s)
        a = p['scale'] * (aws * polar(awa, p) - p['cd'] * v * v) - LINEAR_DAMPING * v
        v = max(0.0, v + a * dt)
        hist.append(v)
    t63 = next((i for i, x in enumerate(hist) if x >= 0.63 * v), 0) * dt
    return v, awa, t63


def cost(p):
    return sum(WEIGHT.get((w, twa), 1) * (equilibrium(w, twa, p)[0] * KN - tv) ** 2
               for w, t in TARGET.items() for twa, tv in zip(TWAS, t))


def fit(p):
    rng = {'ch': (0.1, 1.0), 'broad': (0.3, 1.0), 'run': (0.15, 1.0), 'cd': (0.0, 0.01)}
    random.seed(3)
    best, bc = dict(p), cost(p)
    for _ in range(1200):
        q = dict(best)
        k = random.choice(list(rng))
        lo, hi = rng[k]
        q[k] = min(hi, max(lo, q[k] + random.gauss(0, (hi - lo) * 0.06)))
        c = cost(q)
        if c < bc:
            best, bc = q, c
    print(f"kost={bc:.2f}")
    return best


if __name__ == '__main__':
    p = fit(PARAMS) if '--fit' in sys.argv else PARAMS
    print(p)
    cols = (45, 60, 90, 135, 160, 180)
    print("TWS m/s | " + " ".join(f"{t:5d}" for t in cols) + "   (knop, sann vindvinkel)")
    for w in (300, 500, 700, 900, 1100, 1300):
        print(f"  {w / 100:4.0f}  | " + " ".join(f"{equilibrium(w, t, p)[0] * KN:5.1f}" for t in cols))
    for twa in (45, 90, 180):
        v, awa, t63 = equilibrium(900, twa, p)
        print(f"9 m/s, TWA {twa:3d}: {v * KN:.1f} kn, AWA {awa:.0f}°, 63 % av farten etter {t63:.1f} s")
