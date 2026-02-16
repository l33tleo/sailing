"""News API endpoint — stock news via yfinance. Cached for 15 min."""

import logging
from fastapi import APIRouter, HTTPException, Query
from pydantic import BaseModel
import yfinance as yf

from app.cache import news_cache

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/news", tags=["Nyheter"])


class NewsItem(BaseModel):
    title: str
    publisher: str
    link: str
    published: str
    thumbnail: str | None = None
    related_tickers: list[str] = []


@router.get("/{ticker}", response_model=list[NewsItem])
async def get_stock_news(
    ticker: str,
    limit: int = Query(10, ge=1, le=50),
):
    """Hent nyheter for en aksje via yfinance. Cached for 15 min."""
    cache_key = f"news:{ticker}:{limit}"
    cached = news_cache.get(cache_key)
    if cached is not None:
        return cached
    try:
        t = yf.Ticker(ticker)
        news = t.news
        if not news:
            return []

        items = []
        for n in news[:limit]:
            content = n.get("content", {}) if isinstance(n, dict) else {}
            # yfinance 0.2.x returns different structures
            title = (
                n.get("title")
                or content.get("title")
                or "Uten tittel"
            )
            publisher = (
                n.get("publisher")
                or content.get("provider", {}).get("displayName", "")
                or "Ukjent"
            )
            link = (
                n.get("link")
                or content.get("canonicalUrl", {}).get("url", "")
                or "#"
            )
            pub_date = (
                n.get("providerPublishTime")
                or content.get("pubDate", "")
                or ""
            )

            # Handle epoch timestamp
            if isinstance(pub_date, (int, float)):
                from datetime import datetime, timezone
                pub_date = datetime.fromtimestamp(pub_date, tz=timezone.utc).isoformat()

            thumbnail = None
            if "thumbnail" in n and n["thumbnail"]:
                resolutions = n["thumbnail"].get("resolutions", [])
                if resolutions:
                    thumbnail = resolutions[0].get("url")
            elif "content" in n:
                thumb = content.get("thumbnail", {})
                if thumb and "resolutions" in thumb:
                    resolutions = thumb["resolutions"]
                    if resolutions:
                        thumbnail = resolutions[0].get("url")

            related = n.get("relatedTickers", []) or []

            items.append(
                NewsItem(
                    title=title,
                    publisher=publisher,
                    link=link,
                    published=str(pub_date),
                    thumbnail=thumbnail,
                    related_tickers=related,
                )
            )

        news_cache.set(cache_key, items)
        return items
    except Exception as e:
        logger.error(f"Error fetching news for {ticker}: {e}")
        return []
