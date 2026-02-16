"""Analysis API endpoints — technical, fundamental, and combined analysis."""

from fastapi import APIRouter, HTTPException

from app.schemas.recommendation import (
    FundamentalAnalysisResult,
    FullAnalysis,
    TechnicalAnalysisResult,
)
from app.services.fundamental import analyze_fundamental
from app.services.recommender import generate_recommendation
from app.services.technical import analyze_technical

router = APIRouter(prefix="/analysis", tags=["Analyse"])


@router.get("/{ticker}/technical", response_model=TechnicalAnalysisResult)
async def technical_analysis(ticker: str):
    """Kjør teknisk analyse for en aksje (RSI, MACD, SMA, Bollinger, volum)."""
    result = analyze_technical(ticker)
    if result.current_price is None:
        raise HTTPException(status_code=404, detail=f"Kan ikke analysere {ticker} — ingen data")
    return result


@router.get("/{ticker}/fundamental", response_model=FundamentalAnalysisResult)
async def fundamental_analysis(ticker: str):
    """Kjør fundamental analyse for en aksje (P/E, gjeld, vekst, utbytte, cashflow)."""
    result = analyze_fundamental(ticker)
    return result


@router.get("/{ticker}/full", response_model=FullAnalysis)
async def full_analysis(ticker: str):
    """Kjør full analyse og generer anbefaling for en aksje."""
    result = generate_recommendation(ticker)
    if not result:
        raise HTTPException(
            status_code=404,
            detail=f"Kan ikke generere anbefaling for {ticker}",
        )
    return result
