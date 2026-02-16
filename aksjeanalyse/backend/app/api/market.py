"""Market overview API — indices and market summary."""

import logging
from fastapi import APIRouter
from pydantic import BaseModel
import yfinance as yf

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/market", tags=["Marked"])


class MarketIndex(BaseModel):
    name: str
    ticker: str
    price: float
    change: float
    change_percent: float
    currency: str | None = None


class MarketOverview(BaseModel):
    indices: list[MarketIndex]


INDICES = [
    ("OBX Total Return", "OBX.OL"),
    ("OSEBX", "OSEBX.OL"),
    ("S&P 500", "^GSPC"),
    ("Nasdaq", "^IXIC"),
    ("Dow Jones", "^DJI"),
    ("FTSE 100", "^FTSE"),
    ("DAX", "^GDAXI"),
    ("Nikkei 225", "^N225"),
]


@router.get("/indices", response_model=MarketOverview)
async def get_market_indices():
    """Hent siste kursdata for viktige markedsindekser."""
    results = []
    for name, ticker in INDICES:
        try:
            t = yf.Ticker(ticker)
            fi = t.fast_info
            if fi:
                price = getattr(fi, "last_price", None) or 0
                prev_close = getattr(fi, "previous_close", None) or price
                change = price - prev_close
                change_pct = (change / prev_close * 100) if prev_close else 0
                results.append(
                    MarketIndex(
                        name=name,
                        ticker=ticker,
                        price=round(price, 2),
                        change=round(change, 2),
                        change_percent=round(change_pct, 2),
                        currency=getattr(fi, "currency", None),
                    )
                )
        except Exception as e:
            logger.warning(f"Could not fetch index {ticker}: {e}")
            continue

    return MarketOverview(indices=results)
