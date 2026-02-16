"""Stock data service — fetches real-time and historical data via yfinance.

All yfinance calls are cached via TTLCache to reduce API load and improve
response times. Cache TTLs:
  - Quotes: 60s
  - Info/metrics: 5 min
  - Historical data: 10 min
"""

import logging
from datetime import datetime, timedelta
from functools import lru_cache

import yfinance as yf
import pandas as pd

from app.cache import quote_cache, info_cache, history_cache
from app.schemas.stock import (
    StockKeyMetrics,
    StockPriceHistory,
    StockQuote,
    StockSearchResult,
)

logger = logging.getLogger(__name__)

# Popular Norwegian stocks for default listings
POPULAR_NORWEGIAN_STOCKS = [
    "EQNR.OL",   # Equinor
    "DNB.OL",     # DNB Bank
    "TEL.OL",     # Telenor
    "MOWI.OL",    # Mowi (Marine Harvest)
    "ORK.OL",     # Orkla
    "YAR.OL",     # Yara International
    "AKRBP.OL",   # Aker BP
    "NHY.OL",     # Norsk Hydro
    "SALM.OL",    # SalMar
    "SUBC.OL",    # Subsea 7
    "STB.OL",     # Storebrand
    "KOG.OL",     # Kongsberg Gruppen
    "AKER.OL",    # Aker
    "BAKKA.OL",   # Bakkafrost
    "FRO.OL",     # Frontline
]

POPULAR_GLOBAL_STOCKS = [
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA",
    "JPM", "V", "JNJ", "WMT", "PG", "UNH", "HD", "KO",
]

# Pre-mapped names to avoid slow info lookups for popular stocks
STOCK_NAMES = {
    "EQNR.OL": "Equinor", "DNB.OL": "DNB Bank", "TEL.OL": "Telenor",
    "MOWI.OL": "Mowi", "ORK.OL": "Orkla", "YAR.OL": "Yara International",
    "AKRBP.OL": "Aker BP", "NHY.OL": "Norsk Hydro", "SALM.OL": "SalMar",
    "SUBC.OL": "Subsea 7", "STB.OL": "Storebrand", "KOG.OL": "Kongsberg Gruppen",
    "AKER.OL": "Aker", "BAKKA.OL": "Bakkafrost", "FRO.OL": "Frontline",
    "AAPL": "Apple", "MSFT": "Microsoft", "GOOGL": "Alphabet",
    "AMZN": "Amazon", "NVDA": "NVIDIA", "META": "Meta Platforms",
    "TSLA": "Tesla", "JPM": "JPMorgan Chase", "V": "Visa",
    "JNJ": "Johnson & Johnson", "WMT": "Walmart", "PG": "Procter & Gamble",
    "UNH": "UnitedHealth", "HD": "Home Depot", "KO": "Coca-Cola",
}


def get_ticker_info(ticker: str) -> dict:
    """Fetch full info dict from yfinance for a ticker. Cached for 5 min."""
    cache_key = f"info:{ticker}"
    cached = info_cache.get(cache_key)
    if cached is not None:
        return cached
    try:
        t = yf.Ticker(ticker)
        info = t.info
        result = info if info else {}
        if result:
            info_cache.set(cache_key, result)
        return result
    except Exception as e:
        logger.error(f"Error fetching info for {ticker}: {e}")
        return {}


def get_stock_quote(ticker: str) -> StockQuote | None:
    """Get current quote for a single stock. Cached for 60s."""
    cache_key = f"quote:{ticker}"
    cached = quote_cache.get(cache_key)
    if cached is not None:
        return cached
    try:
        t = yf.Ticker(ticker)
        info = t.info
        result: StockQuote | None = None

        if not info or "regularMarketPrice" not in info:
            # Try fast_info as fallback
            fi = t.fast_info
            if fi:
                price = getattr(fi, "last_price", None) or 0
                prev_close = getattr(fi, "previous_close", None) or price
                change = price - prev_close
                change_pct = (change / prev_close * 100) if prev_close else 0
                result = StockQuote(
                    ticker=ticker,
                    name=ticker,
                    price=round(price, 2),
                    change=round(change, 2),
                    change_percent=round(change_pct, 2),
                    volume=int(getattr(fi, "last_volume", 0) or 0),
                    market_cap=getattr(fi, "market_cap", None),
                    currency=getattr(fi, "currency", None),
                )
        else:
            price = info.get("regularMarketPrice", 0) or info.get("currentPrice", 0) or 0
            prev_close = info.get("regularMarketPreviousClose", price) or price
            change = price - prev_close
            change_pct = (change / prev_close * 100) if prev_close else 0

            result = StockQuote(
                ticker=ticker,
                name=info.get("shortName", info.get("longName", ticker)),
                price=round(price, 2),
                change=round(change, 2),
                change_percent=round(change_pct, 2),
                volume=info.get("regularMarketVolume", 0) or 0,
                market_cap=info.get("marketCap"),
                pe_ratio=info.get("trailingPE"),
                dividend_yield=info.get("dividendYield"),
                fifty_two_week_high=info.get("fiftyTwoWeekHigh"),
                fifty_two_week_low=info.get("fiftyTwoWeekLow"),
                currency=info.get("currency"),
            )

        if result is not None:
            quote_cache.set(cache_key, result)
        return result
    except Exception as e:
        logger.error(f"Error fetching quote for {ticker}: {e}")
        return None


def get_price_history(
    ticker: str, period: str = "1y", interval: str = "1d"
) -> StockPriceHistory | None:
    """Get historical price data for a stock. Cached for 10 min."""
    cache_key = f"history:{ticker}:{period}:{interval}"
    cached = history_cache.get(cache_key)
    if cached is not None:
        return cached
    try:
        t = yf.Ticker(ticker)
        hist = t.history(period=period, interval=interval)
        if hist.empty:
            return None
        result = StockPriceHistory(
            dates=[d.strftime("%Y-%m-%d") for d in hist.index],
            open=[round(v, 2) for v in hist["Open"].tolist()],
            high=[round(v, 2) for v in hist["High"].tolist()],
            low=[round(v, 2) for v in hist["Low"].tolist()],
            close=[round(v, 2) for v in hist["Close"].tolist()],
            volume=[int(v) for v in hist["Volume"].tolist()],
        )
        history_cache.set(cache_key, result)
        return result
    except Exception as e:
        logger.error(f"Error fetching history for {ticker}: {e}")
        return None


def get_key_metrics(ticker: str) -> StockKeyMetrics | None:
    """Get fundamental key metrics for a stock. Uses cached info (5 min TTL)."""
    try:
        info = get_ticker_info(ticker)  # Already cached
        if not info:
            return None

        return StockKeyMetrics(
            ticker=ticker,
            pe_ratio=info.get("trailingPE"),
            forward_pe=info.get("forwardPE"),
            pb_ratio=info.get("priceToBook"),
            ps_ratio=info.get("priceToSalesTrailing12Months"),
            ev_ebitda=info.get("enterpriseToEbitda"),
            debt_to_equity=info.get("debtToEquity"),
            current_ratio=info.get("currentRatio"),
            roe=info.get("returnOnEquity"),
            revenue_growth=info.get("revenueGrowth"),
            earnings_growth=info.get("earningsGrowth"),
            dividend_yield=info.get("dividendYield"),
            payout_ratio=info.get("payoutRatio"),
            free_cash_flow=info.get("freeCashflow"),
            market_cap=info.get("marketCap"),
            beta=info.get("beta"),
        )
    except Exception as e:
        logger.error(f"Error fetching metrics for {ticker}: {e}")
        return None


def search_stocks(query: str) -> list[StockSearchResult]:
    """Search for stocks by name or ticker.
    
    Uses yfinance search functionality.
    """
    results = []
    try:
        # Try direct ticker lookup first
        t = yf.Ticker(query.upper())
        info = t.info
        if info and info.get("regularMarketPrice"):
            results.append(
                StockSearchResult(
                    ticker=query.upper(),
                    name=info.get("shortName", info.get("longName", query.upper())),
                    sector=info.get("sector"),
                    market=info.get("exchange"),
                    current_price=info.get("regularMarketPrice"),
                    currency=info.get("currency"),
                )
            )
    except Exception:
        pass

    # Also try with .OL suffix for Norwegian stocks
    if not query.upper().endswith(".OL"):
        try:
            oslo_ticker = f"{query.upper()}.OL"
            t = yf.Ticker(oslo_ticker)
            info = t.info
            if info and info.get("regularMarketPrice"):
                results.append(
                    StockSearchResult(
                        ticker=oslo_ticker,
                        name=info.get("shortName", info.get("longName", oslo_ticker)),
                        sector=info.get("sector"),
                        market="Oslo Børs",
                        current_price=info.get("regularMarketPrice"),
                        currency=info.get("currency"),
                    )
                )
        except Exception:
            pass

    return results


def get_popular_stocks(market: str = "all") -> list[StockSearchResult]:
    """Get a list of popular stocks for browsing."""
    tickers = []
    if market in ("oslo", "all"):
        tickers.extend(POPULAR_NORWEGIAN_STOCKS)
    if market in ("global", "all"):
        tickers.extend(POPULAR_GLOBAL_STOCKS)

    results = []
    for ticker in tickers:
        try:
            t = yf.Ticker(ticker)
            fi = t.fast_info
            if fi:
                price = getattr(fi, "last_price", None)
                results.append(
                    StockSearchResult(
                        ticker=ticker,
                        name=STOCK_NAMES.get(ticker, ticker.replace(".OL", "")),
                        sector=None,
                        market="Oslo Børs" if ticker.endswith(".OL") else "US",
                        current_price=round(price, 2) if price is not None else None,
                        currency=getattr(fi, "currency", None),
                    )
                )
        except Exception:
            continue

    return results


def get_historical_dataframe(ticker: str, period: str = "1y") -> pd.DataFrame | None:
    """Get raw pandas DataFrame of historical data for analysis services. Cached for 10 min."""
    cache_key = f"df:{ticker}:{period}"
    cached = history_cache.get(cache_key)
    if cached is not None:
        return cached
    try:
        t = yf.Ticker(ticker)
        hist = t.history(period=period)
        if hist.empty:
            return None
        history_cache.set(cache_key, hist)
        return hist
    except Exception as e:
        logger.error(f"Error fetching dataframe for {ticker}: {e}")
        return None
