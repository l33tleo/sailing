"""Recommendation API endpoints — generate, list, and evaluate recommendations."""

from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy import select, func
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.database import get_db
from app.models.recommendation import Recommendation, RecommendationOutcome
from app.models.stock import Stock
from app.schemas.recommendation import (
    GenerateRecommendationRequest,
    RecommendationDetail,
    RecommendationResponse,
    ScorecardStats,
)
from app.services.evaluator import evaluate_recommendations, get_scorecard
from app.services.recommender import generate_recommendation

router = APIRouter(prefix="/recommendations", tags=["Anbefalinger"])


@router.post("/generate", response_model=RecommendationResponse)
async def generate_and_save(
    request: GenerateRecommendationRequest,
    db: AsyncSession = Depends(get_db),
):
    """Generer en ny anbefaling for en aksje og lagre den i databasen."""
    analysis = generate_recommendation(request.ticker)
    if not analysis:
        raise HTTPException(
            status_code=404,
            detail=f"Kan ikke generere anbefaling for {request.ticker}",
        )

    # Get or create stock
    result = await db.execute(select(Stock).where(Stock.ticker == request.ticker))
    stock = result.scalar_one_or_none()

    if stock is None:
        stock = Stock(
            ticker=request.ticker,
            name=analysis.name,
            current_price=analysis.current_price,
        )
        db.add(stock)
        await db.flush()

    # Update stock price
    stock.current_price = analysis.current_price

    # Create recommendation
    rec = Recommendation(
        stock_id=stock.id,
        recommendation_type=analysis.recommendation_type,
        confidence=analysis.confidence,
        price_at_recommendation=analysis.current_price,
        technical_score=analysis.technical.technical_score,
        fundamental_score=analysis.fundamental.fundamental_score,
        reasoning=analysis.reasoning,
    )
    db.add(rec)
    await db.flush()

    return RecommendationResponse(
        id=rec.id,
        stock_ticker=stock.ticker,
        stock_name=stock.name,
        recommendation_type=rec.recommendation_type,
        confidence=rec.confidence,
        price_at_recommendation=rec.price_at_recommendation,
        current_price=analysis.current_price,
        technical_score=rec.technical_score,
        fundamental_score=rec.fundamental_score,
        combined_score=analysis.combined_score,
        reasoning=rec.reasoning,
        created_at=rec.created_at,
    )


@router.get("", response_model=list[RecommendationDetail])
async def list_recommendations(
    ticker: str | None = Query(None, description="Filtrer på ticker"),
    rec_type: str | None = Query(None, description="Filtrer på type: KJØP, HOLD, SELG"),
    limit: int = Query(50, ge=1, le=200),
    offset: int = Query(0, ge=0),
    db: AsyncSession = Depends(get_db),
):
    """Hent alle lagrede anbefalinger med utfallsdata."""
    query = (
        select(Recommendation, RecommendationOutcome, Stock)
        .join(Stock, Recommendation.stock_id == Stock.id)
        .join(
            RecommendationOutcome,
            Recommendation.id == RecommendationOutcome.recommendation_id,
            isouter=True,
        )
        .order_by(Recommendation.created_at.desc())
        .limit(limit)
        .offset(offset)
    )

    if ticker:
        query = query.where(Stock.ticker == ticker.upper())
    if rec_type:
        query = query.where(Recommendation.recommendation_type == rec_type)

    result = await db.execute(query)
    rows = result.all()

    return [
        RecommendationDetail(
            id=rec.id,
            stock_ticker=stock.ticker,
            stock_name=stock.name,
            recommendation_type=rec.recommendation_type,
            confidence=rec.confidence,
            price_at_recommendation=rec.price_at_recommendation,
            technical_score=rec.technical_score,
            fundamental_score=rec.fundamental_score,
            combined_score=(rec.technical_score * 0.5 + rec.fundamental_score * 0.5),
            reasoning=rec.reasoning,
            created_at=rec.created_at,
            price_after_7d=outcome.price_after_7d if outcome else None,
            price_after_30d=outcome.price_after_30d if outcome else None,
            price_after_90d=outcome.price_after_90d if outcome else None,
            return_7d_pct=outcome.return_7d_pct if outcome else None,
            return_30d_pct=outcome.return_30d_pct if outcome else None,
            return_90d_pct=outcome.return_90d_pct if outcome else None,
            was_correct_7d=outcome.was_correct_7d if outcome else None,
            was_correct_30d=outcome.was_correct_30d if outcome else None,
            was_correct_90d=outcome.was_correct_90d if outcome else None,
        )
        for rec, outcome, stock in rows
    ]


@router.get("/scorecard", response_model=ScorecardStats)
async def scorecard(db: AsyncSession = Depends(get_db)):
    """Hent samlet treffsikkerhet-statistikk for alle anbefalinger."""
    return await get_scorecard(db)


@router.post("/evaluate")
async def run_evaluation(db: AsyncSession = Depends(get_db)):
    """Kjør evaluering av alle utestående anbefalinger mot faktiske kurser."""
    updated = await evaluate_recommendations(db)
    return {"message": f"Evaluerte {updated} anbefalinger", "updated_count": updated}
