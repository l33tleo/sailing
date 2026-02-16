"""Stock API endpoints — search, quotes, price history, and key metrics."""

from fastapi import APIRouter, HTTPException, Query

from app.schemas.stock import (
    StockKeyMetrics,
    StockPriceHistory,
    StockQuote,
    StockSearchResult,
)
from app.services.stock_data import (
    get_key_metrics,
    get_popular_stocks,
    get_price_history,
    get_stock_quote,
    search_stocks,
)

router = APIRouter(prefix="/stocks", tags=["Aksjer"])


@router.get("/search", response_model=list[StockSearchResult])
async def search(q: str = Query(..., min_length=1, description="Søkeord (ticker eller navn)")):
    """Søk etter aksjer på ticker eller navn."""
    results = search_stocks(q)
    if not results:
        return []
    return results


@router.get("/popular", response_model=list[StockSearchResult])
async def popular(market: str = Query("all", description="Market filter: oslo, global, all")):
    """Hent populære aksjer for oversikten."""
    return get_popular_stocks(market)


@router.get("/{ticker}/quote", response_model=StockQuote)
async def quote(ticker: str):
    """Hent siste kursdata for en aksje."""
    result = get_stock_quote(ticker)
    if not result:
        raise HTTPException(status_code=404, detail=f"Finner ikke aksje: {ticker}")
    return result


@router.get("/{ticker}/history", response_model=StockPriceHistory)
async def history(
    ticker: str,
    period: str = Query("1y", description="Periode: 1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, max"),
    interval: str = Query("1d", description="Intervall: 1m, 5m, 15m, 1h, 1d, 1wk, 1mo"),
):
    """Hent historiske kursdata for en aksje."""
    result = get_price_history(ticker, period=period, interval=interval)
    if not result:
        raise HTTPException(status_code=404, detail=f"Ingen historiske data for: {ticker}")
    return result


@router.get("/{ticker}/metrics", response_model=StockKeyMetrics)
async def metrics(ticker: str):
    """Hent fundamentale nøkkeltall for en aksje."""
    result = get_key_metrics(ticker)
    if not result:
        raise HTTPException(status_code=404, detail=f"Ingen nøkkeltall for: {ticker}")
    return result
