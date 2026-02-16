"""AksjeAnalyse API — Stock analysis and recommendation engine."""

import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.api import stocks, analysis, recommendations, portfolio, compare, news, export, market, alerts, screener, paper_trade, sectors
from app.db.database import init_db
from app.scheduler import start_scheduler, stop_scheduler

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan — init DB and scheduler on startup."""
    logger.info("Starting AksjeAnalyse API...")
    await init_db()
    logger.info("Database initialized")
    start_scheduler()
    yield
    stop_scheduler()
    logger.info("Shutting down AksjeAnalyse API...")


app = FastAPI(
    title="AksjeAnalyse API",
    description="Aksjeanalyse og anbefalingsmotor — teknisk og fundamental analyse med historisk treffsikkerhet",
    version="1.0.0",
    lifespan=lifespan,
)

# CORS — allow frontend to talk to the API
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:3000", "http://127.0.0.1:3000"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Register routers
app.include_router(stocks.router, prefix="/api")
app.include_router(analysis.router, prefix="/api")
app.include_router(recommendations.router, prefix="/api")
app.include_router(portfolio.router, prefix="/api")
app.include_router(compare.router, prefix="/api")
app.include_router(news.router, prefix="/api")
app.include_router(export.router, prefix="/api")
app.include_router(market.router, prefix="/api")
app.include_router(alerts.router, prefix="/api")
app.include_router(screener.router, prefix="/api")
app.include_router(paper_trade.router, prefix="/api")
app.include_router(sectors.router, prefix="/api")


@app.get("/api/health")
async def health_check():
    """Health check endpoint."""
    return {"status": "ok", "service": "AksjeAnalyse API"}


@app.get("/api/cache/stats")
async def cache_stats():
    """Cache statistics for monitoring."""
    from app.cache import quote_cache, info_cache, history_cache, news_cache
    return {
        "quote_cache": quote_cache.stats,
        "info_cache": info_cache.stats,
        "history_cache": history_cache.stats,
        "news_cache": news_cache.stats,
    }


@app.post("/api/cache/clear")
async def clear_caches():
    """Clear all caches."""
    from app.cache import quote_cache, info_cache, history_cache, news_cache
    quote_cache.clear()
    info_cache.clear()
    history_cache.clear()
    news_cache.clear()
    return {"message": "Alle cacher tømt"}
