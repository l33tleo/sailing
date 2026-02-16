"""AksjeAnalyse API — Stock analysis and recommendation engine."""

import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.api import stocks, analysis, recommendations, portfolio, compare, news, export, market, alerts
from app.db.database import init_db

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan — init DB on startup."""
    logger.info("Starting AksjeAnalyse API...")
    await init_db()
    logger.info("Database initialized")
    yield
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


@app.get("/api/health")
async def health_check():
    """Health check endpoint."""
    return {"status": "ok", "service": "AksjeAnalyse API"}
