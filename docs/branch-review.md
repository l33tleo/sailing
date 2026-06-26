# Branch-gjennomgang: gjenbruk fra cursor-branchene

> Laget for å vurdere hva som kan gjenbrukes fra to umergede cursor-brancher.
> Dato for analyse: 2026-06-25. Basert på git-historikk, ikke kjøretest.

## Nøkkelfunn først

Begge cursor-branchene skilte lag fra **`2955516 "first versjon"`** (13. feb 2026) — helt i
starten. **`main` har bare 7 commits siden da**, og *alle* er Optimist-båtmodellen + opprydding
(ingen endringer i spill-systemer/gameplay).

Konsekvens: **spillkoden på `main` er i praksis fortsatt «first versjon»** når det gjelder
gameplay. Følelsen av at «mye har endret seg» stemmer for *båtmodellen/assets*, men ikke for
selve spill-logikken. Det meste av gameplay-arbeidet ligger **ulevert** på `seilsimulator`-branchen.

---

## Branch 1: `cursor/seilsimulator-spill-plan-b40f`  ← relevant

**116 commits unikt arbeid. 146 filer endret (+9091 / −8424).** Dette er en stor utvidelse/
omskriving av spillet fra samme startpunkt som main. Ingen plandokumenter — «planen» ER koden.

### Nye systemer (filer som ikke finnes i main)
| System | Nye filer |
|---|---|
| Seilfysikk (uttrukket komponent) | `BoatSimulationComponent.{h,cpp}`, `Simulation/SailingSimulationMath.{h,cpp}` |
| Kjernesubsystemer (GameInstance) | `Systems/SailingCoreSubsystems.{h,cpp}` |
| Data-drevne missions/oppgraderinger | `Data/SailingMissionDataAsset.h`, `Data/BoatUpgradeDataAsset.h`, `Data/PortDataAsset.{h,cpp}`, `Data/SailingProgressionTypes.h` |
| Havn / mission board | `PortMarkerActor.{h,cpp}`, `MissionObjectiveActor.{h,cpp}`, `UI/PortMissionBoardWidget.{h,cpp}` |
| UMG HUD-overlay | `UI/SailingHUDOverlayWidget.{h,cpp}` |
| Automatiserte tester | `Tests/SailingProgressionTests.cpp`, `Tests/SailingSimulationMathTests.cpp` |

Tematisk dekker commit-historikken: docking-økonomi, port-oppgraderinger med vekting/priser,
mission-board med cooldown/refresh, telemetri-tellere, save-migrering, og oppdriftsfysikk.

### Gjenbruks-vurdering
- **Best gjenbrukbart (lav kollisjonsrisiko):** de *nye* filene over — særlig
  `SailingSimulationMath`, `BoatSimulationComponent` og data-asset-skjemaene. De er nye moduler
  som ikke finnes i main, så de kan cherry-pickes/kopieres relativt isolert.
- **Høy kollisjonsrisiko:** branchen **omskriver kjerneklassene** som også finnes i main
  (`SailboatPawn`, `SailingGameMode`, `SailingHUD`, `ChunkManager`, `IslandActor`,
  `SaveGameSailing`, `Sailing.Build.cs` m.fl. — 17 delte filer endret). En rett merge vil gi
  store konflikter, og main sin Optimist-mesh-lasting (`SailboatPawn.cpp` → `/Game/ModelsV2/Optimist3735`)
  må bevares manuelt.

### Anbefalt fremgang (hvis vi vil ha dette inn)
1. **Ikke** rett `git merge` — for mye divergens fra «first versjon».
2. Plukk system for system: start med de selvstendige nye modulene (sim-math, data-assets,
   subsystemer) via `git checkout seilsimulator -- <fil>`, bygg og test isolert.
3. Integrer kjerneklasse-endringene manuelt, og behold main sin Optimist-pipeline.
4. Behold branchen urørt til dette er gjort — den er eneste kilde til disse 116 commitene.

---

## Branch 2: `cursor/aksjeanalyse-appkonsept-ad1d`  ← ikke relevant for spillet

**24 commits.** Et **helt separat aksjeanalyse-webapp** under `aksjeanalyse/`:
FastAPI-backend (Alembic-migrasjoner, JWT-auth, yfinance-cache, AI-chat), Next.js-frontend,
docker-compose. Har ingenting med seilspillet å gjøre — havnet bare på en branch i samme repo.

### Gjenbruks-vurdering
- **Ingenting til seilspillet.** Hvis aksje-appen fortsatt er ønsket, hører den hjemme i et
  **eget repo**, ikke her. Hvis den er forlatt, kan branchen slettes — men det er et eget
  prosjekt, så bekreft før sletting.

---

## Beslutningspunkter (for deg)

1. **Seilsimulator-systemene** — vil du at jeg starter med å hente ut de selvstendige nye
   modulene (sim-math + data-assets + subsystemer) og få dem til å bygge mot dagens main?
   Det er den tryggeste første biten med høyest verdi.
2. **Aksje-appen** — eget repo, slett branchen, eller la den ligge?
3. **Inntil avklart:** ingen av de to branchene slettes (begge har unikt, ulevert arbeid).
