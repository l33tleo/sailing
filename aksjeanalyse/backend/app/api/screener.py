"""Screener API — filter stocks by financial criteria."""

import logging
from fastapi import APIRouter, Query
from app.services.stock_data import get_stock_quote, get_key_metrics, POPULAR_NORWEGIAN_STOCKS, POPULAR_GLOBAL_STOCKS

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/screener", tags=["Screener"])


@router.get("/")
async def screen_stocks(
    market: str = Query("all", description="Market: oslo, global, all"),
    min_pe: float | None = Query(None, description="Minimum P/E ratio"),
    max_pe: float | None = Query(None, description="Maximum P/E ratio"),
    min_dividend: float | None = Query(None, description="Minimum utbytterate (%)"),
    max_debt: float | None = Query(None, description="Maks gjeldsgrad (D/E %)"),
    min_market_cap: float | None = Query(None, description="Minimum markedsverdi"),
    sector: str | None = Query(None, description="Sektor-filter"),
):
    """Filtrer aksjer basert på finansielle kriterier."""
    tickers = []
    if market in ("oslo", "all"):
        tickers.extend(POPULAR_NORWEGIAN_STOCKS)
    if market in ("global", "all"):
        tickers.extend(POPULAR_GLOBAL_STOCKS)

    results = []
    for ticker in tickers:
        try:
            quote = get_stock_quote(ticker)
            if not quote:
                continue

            metrics = get_key_metrics(ticker)
            if not metrics:
                continue

            # Apply filters
            if min_pe is not None and (metrics.pe_ratio is None or metrics.pe_ratio < min_pe):
                continue
            if max_pe is not None and (metrics.pe_ratio is None or metrics.pe_ratio > max_pe):
                continue
            if min_dividend is not None:
                div = metrics.dividend_yield
                if div is None:
                    continue
                div_pct = div if div > 1 else div * 100
                if div_pct < min_dividend:
                    continue
            if max_debt is not None and (
                metrics.debt_to_equity is None or metrics.debt_to_equity > max_debt
            ):
                continue
            if min_market_cap is not None and (
                metrics.market_cap is None or metrics.market_cap < min_market_cap
            ):
                continue

            results.append({
                "ticker": ticker,
                "name": quote.name,
                "price": quote.price,
                "change_percent": quote.change_percent,
                "currency": quote.currency,
                "market_cap": metrics.market_cap,
                "pe_ratio": metrics.pe_ratio,
                "forward_pe": metrics.forward_pe,
                "dividend_yield": metrics.dividend_yield,
                "debt_to_equity": metrics.debt_to_equity,
                "revenue_growth": metrics.revenue_growth,
                "roe": metrics.roe,
                "beta": metrics.beta,
                "sector": None,  # Would need ticker info for sector
                "market": "Oslo Børs" if ticker.endswith(".OL") else "US",
            })
        except Exception as e:
            logger.error(f"Screener error for {ticker}: {e}")
            continue

    return results


@router.get("/indices")
async def get_market_indices():
    """Hent hovedindekser (OBX, S&P 500, Nasdaq, etc.)."""
    indices = [
        ("^GSPC", "S&P 500"),
        ("^IXIC", "Nasdaq Composite"),
        ("^DJI", "Dow Jones"),
        ("^OBX", "Oslo Børs OBX"),
        ("^STOXX50E", "Euro Stoxx 50"),
    ]

    results = []
    for ticker, display_name in indices:
        try:
            quote = get_stock_quote(ticker)
            if quote:
                results.append({
                    "ticker": ticker,
                    "name": display_name,
                    "price": quote.price,
                    "change": quote.change,
                    "change_percent": quote.change_percent,
                    "currency": quote.currency,
                })
        except Exception as e:
            logger.error(f"Index error for {ticker}: {e}")
            continue

    return results
