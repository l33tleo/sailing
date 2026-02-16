"""Sector analysis API — performance by sector."""

import logging
from fastapi import APIRouter
from pydantic import BaseModel
import yfinance as yf

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/sectors", tags=["Sektoranalyse"])


class SectorStock(BaseModel):
    ticker: str
    name: str
    price: float | None = None
    change_percent: float | None = None


class SectorData(BaseModel):
    name: str
    avg_change_pct: float = 0.0
    stock_count: int = 0
    stocks: list[SectorStock] = []


class SectorOverview(BaseModel):
    sectors: list[SectorData]


# Norwegian sectors with representative stocks
SECTOR_STOCKS = {
    "Energi": [
        "EQNR.OL",   # Equinor
        "AKRBP.OL",   # Aker BP
        "VAR.OL",     # Vår Energi
        "FRO.OL",     # Frontline
    ],
    "Finans": [
        "DNB.OL",     # DNB Bank
        "STB.OL",     # Storebrand
        "MORG.OL",    # SpareBank 1 SR-Bank
    ],
    "Sjømat": [
        "MOWI.OL",    # Mowi
        "SALM.OL",    # SalMar
        "BAKKA.OL",   # Bakkafrost
        "LSG.OL",     # Lerøy
    ],
    "Industri": [
        "NHY.OL",     # Norsk Hydro
        "YAR.OL",     # Yara
        "KOG.OL",     # Kongsberg Gruppen
        "SUBC.OL",    # Subsea 7
    ],
    "Telekom": [
        "TEL.OL",     # Telenor
    ],
    "Konsum": [
        "ORK.OL",     # Orkla
    ],
    "US Tech": [
        "AAPL",       # Apple
        "MSFT",       # Microsoft
        "GOOGL",      # Alphabet
        "AMZN",       # Amazon
        "NVDA",       # NVIDIA
        "META",       # Meta
    ],
    "US Finans": [
        "JPM",        # JPMorgan
        "V",          # Visa
        "BAC",        # Bank of America
    ],
    "US Helse": [
        "UNH",        # UnitedHealth
        "JNJ",        # Johnson & Johnson
        "PFE",        # Pfizer
    ],
}


@router.get("", response_model=SectorOverview)
async def get_sector_overview():
    """Hent sektoroversikt med gjennomsnittlig kursendring per sektor."""
    sectors = []

    for sector_name, tickers in SECTOR_STOCKS.items():
        sector = SectorData(name=sector_name)
        changes = []

        for ticker in tickers:
            try:
                t = yf.Ticker(ticker)
                fi = t.fast_info
                if fi:
                    price = getattr(fi, "last_price", None) or 0
                    prev = getattr(fi, "previous_close", None) or price
                    change_pct = ((price - prev) / prev * 100) if prev else 0

                    stock_info = t.info
                    name = ticker
                    if stock_info:
                        name = stock_info.get("shortName", stock_info.get("longName", ticker))

                    sector.stocks.append(
                        SectorStock(
                            ticker=ticker,
                            name=name,
                            price=round(price, 2),
                            change_percent=round(change_pct, 2),
                        )
                    )
                    changes.append(change_pct)
            except Exception as e:
                logger.warning(f"Sector error for {ticker}: {e}")
                continue

        sector.stock_count = len(sector.stocks)
        if changes:
            sector.avg_change_pct = round(sum(changes) / len(changes), 2)

        sectors.append(sector)

    # Sort by performance (best first)
    sectors.sort(key=lambda s: s.avg_change_pct, reverse=True)

    return SectorOverview(sectors=sectors)
