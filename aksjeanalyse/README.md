# AksjeAnalyse 📊

Aksjeanalyse- og anbefalingsapp — analyserer aksjer med teknisk og fundamental analyse, gir KJØP/HOLD/SELG-anbefalinger, og sporer treffsikkerhet over tid.

## Funksjoner

- **Aksjesøk** — Søk etter norske (Oslo Børs) og internasjonale aksjer
- **Teknisk analyse** — RSI, MACD, SMA/EMA, Bollinger Bands, volum
- **Fundamental analyse** — P/E, gjeld, vekst, utbytte, kontantstrøm
- **Anbefalingsmotor** — Scorer 0-100 og gir KJØP/HOLD/SELG med begrunnelse
- **Anbefalingshistorikk** — Lagrer alle anbefalinger med tidsstempel og kurs
- **Treffsikkerhet** — Evaluerer anbefalinger etter 7, 30 og 90 dager
- **Portefølje-tracker** — Spor beholdning og avkastning
- **Watchlist** — Følg aksjer du er interessert i

## Tech Stack

| Komponent | Teknologi |
|-----------|-----------|
| Frontend | Next.js 15 + TypeScript + Tailwind CSS v4 |
| Backend | Python FastAPI |
| Database | PostgreSQL (prod) / SQLite (dev) |
| Aksjedata | yfinance |
| Analyse | pandas + numpy |
| Grafer | Recharts |

## Kom i gang

### Backend

```bash
cd backend
pip install -r requirements.txt
python3 -m uvicorn app.main:app --reload --port 8000
```

API docs: http://localhost:8000/docs

### Frontend

```bash
cd frontend
npm install
npm run dev
```

App: http://localhost:3000

### Docker (produksjon)

```bash
docker-compose up -d
```

## API-endepunkter

| Metode | Sti | Beskrivelse |
|--------|-----|-------------|
| GET | `/api/stocks/search?q=AAPL` | Søk etter aksjer |
| GET | `/api/stocks/{ticker}/quote` | Hent kursinformasjon |
| GET | `/api/stocks/{ticker}/history` | Historiske kursdata |
| GET | `/api/stocks/{ticker}/metrics` | Fundamentale nøkkeltall |
| GET | `/api/analysis/{ticker}/technical` | Teknisk analyse |
| GET | `/api/analysis/{ticker}/fundamental` | Fundamental analyse |
| GET | `/api/analysis/{ticker}/full` | Full analyse med anbefaling |
| POST | `/api/recommendations/generate` | Generer og lagre anbefaling |
| GET | `/api/recommendations/` | Hent alle anbefalinger |
| GET | `/api/recommendations/scorecard` | Treffsikkerhet-statistikk |
| POST | `/api/recommendations/evaluate` | Evaluer gamle anbefalinger |
| POST | `/api/portfolios` | Opprett portefølje |
| POST | `/api/portfolios/{id}/holdings` | Legg til aksje |
| POST | `/api/watchlist` | Legg til i watchlist |

## Anbefalingsmotor

Kombinerer teknisk score (0-100) og fundamental score (0-100) til en samlet vurdering:

| Score | Anbefaling |
|-------|-----------|
| 70-100 | **KJØP** 🟢 |
| 40-69 | **HOLD** 🟡 |
| 0-39 | **SELG** 🔴 |

### Teknisk scoring
- RSI (0-25 poeng)
- MACD (0-25 poeng)
- Glidende snitt / SMA-trend (0-25 poeng)
- Volum (0-25 poeng)

### Fundamental scoring
- Verdsettelse / P/E (0-20 poeng)
- Gjeldsgrad (0-20 poeng)
- Inntektsvekst (0-20 poeng)
- Utbytte (0-20 poeng)
- Fri kontantstrøm (0-20 poeng)

## Ansvarsfraskrivelse

⚠️ Denne appen gir **ikke finansiell rådgivning**. Anbefalingene er basert på automatiserte beregninger og bør brukes som ett av flere verktøy i investeringsbeslutninger. Gjør alltid din egen research.
