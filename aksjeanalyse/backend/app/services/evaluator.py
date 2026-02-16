"""Evaluation service — tracks recommendation accuracy over time."""

import logging
from datetime import datetime, timedelta, timezone

from sqlalchemy import select, and_
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.recommendation import Recommendation, RecommendationOutcome
from app.models.stock import Stock
from app.schemas.recommendation import RecommendationDetail, ScorecardStats
from app.services.stock_data import get_stock_quote

logger = logging.getLogger(__name__)


async def evaluate_recommendations(db: AsyncSession) -> int:
    """Evaluate all pending recommendation outcomes.
    
    Checks recommendations that are 7, 30, or 90 days old and records
    the actual price and whether the recommendation was correct.
    
    Returns the number of outcomes updated.
    """
    now = datetime.now(timezone.utc)
    updated = 0

    # Get all recommendations that need evaluation
    result = await db.execute(select(Recommendation).order_by(Recommendation.created_at))
    recommendations = result.scalars().all()

    for rec in recommendations:
        # Get or create outcome
        outcome_result = await db.execute(
            select(RecommendationOutcome).where(
                RecommendationOutcome.recommendation_id == rec.id
            )
        )
        outcome = outcome_result.scalar_one_or_none()

        if outcome is None:
            outcome = RecommendationOutcome(recommendation_id=rec.id)
            db.add(outcome)

        # Get stock ticker
        stock_result = await db.execute(select(Stock).where(Stock.id == rec.stock_id))
        stock = stock_result.scalar_one_or_none()
        if not stock:
            continue

        age = now - rec.created_at.replace(tzinfo=timezone.utc) if rec.created_at.tzinfo is None else now - rec.created_at

        quote = None

        # Check 7-day mark
        if age >= timedelta(days=7) and outcome.price_after_7d is None:
            quote = quote or get_stock_quote(stock.ticker)
            if quote:
                outcome.price_after_7d = quote.price
                outcome.return_7d_pct = round(
                    ((quote.price - rec.price_at_recommendation) / rec.price_at_recommendation) * 100, 2
                )
                outcome.was_correct_7d = _was_correct(
                    rec.recommendation_type, rec.price_at_recommendation, quote.price
                )
                updated += 1

        # Check 30-day mark
        if age >= timedelta(days=30) and outcome.price_after_30d is None:
            quote = quote or get_stock_quote(stock.ticker)
            if quote:
                outcome.price_after_30d = quote.price
                outcome.return_30d_pct = round(
                    ((quote.price - rec.price_at_recommendation) / rec.price_at_recommendation) * 100, 2
                )
                outcome.was_correct_30d = _was_correct(
                    rec.recommendation_type, rec.price_at_recommendation, quote.price
                )
                updated += 1

        # Check 90-day mark
        if age >= timedelta(days=90) and outcome.price_after_90d is None:
            quote = quote or get_stock_quote(stock.ticker)
            if quote:
                outcome.price_after_90d = quote.price
                outcome.return_90d_pct = round(
                    ((quote.price - rec.price_at_recommendation) / rec.price_at_recommendation) * 100, 2
                )
                outcome.was_correct_90d = _was_correct(
                    rec.recommendation_type, rec.price_at_recommendation, quote.price
                )
                updated += 1

        outcome.evaluated_at = now

    await db.commit()
    logger.info(f"Evaluated {updated} recommendation outcomes")
    return updated


def _was_correct(rec_type: str, price_at_rec: float, current_price: float) -> bool:
    """Determine if a recommendation was correct based on price movement."""
    if rec_type == "KJØP":
        return current_price > price_at_rec
    elif rec_type == "SELG":
        return current_price < price_at_rec
    else:  # HOLD
        # HOLD is correct if price didn't move more than 5% in either direction
        change_pct = abs((current_price - price_at_rec) / price_at_rec) * 100
        return change_pct < 5


async def get_scorecard(db: AsyncSession) -> ScorecardStats:
    """Calculate overall scorecard statistics."""
    stats = ScorecardStats()

    # Get all recommendations with outcomes
    result = await db.execute(
        select(Recommendation, RecommendationOutcome, Stock)
        .join(RecommendationOutcome, Recommendation.id == RecommendationOutcome.recommendation_id, isouter=True)
        .join(Stock, Recommendation.stock_id == Stock.id)
        .order_by(Recommendation.created_at.desc())
    )
    rows = result.all()

    if not rows:
        return stats

    stats.total_recommendations = len(rows)

    correct_7d, total_7d = 0, 0
    correct_30d, total_30d = 0, 0
    correct_90d, total_90d = 0, 0
    returns_7d, returns_30d, returns_90d = [], [], []

    best_return = float("-inf")
    worst_return = float("inf")
    best_rec = None
    worst_rec = None

    rec_type_counts: dict[str, int] = {}

    for rec, outcome, stock in rows:
        # Count by type
        rec_type_counts[rec.recommendation_type] = rec_type_counts.get(rec.recommendation_type, 0) + 1

        if outcome:
            if outcome.was_correct_7d is not None:
                total_7d += 1
                if outcome.was_correct_7d:
                    correct_7d += 1
                if outcome.return_7d_pct is not None:
                    returns_7d.append(outcome.return_7d_pct)

            if outcome.was_correct_30d is not None:
                total_30d += 1
                if outcome.was_correct_30d:
                    correct_30d += 1
                if outcome.return_30d_pct is not None:
                    returns_30d.append(outcome.return_30d_pct)

            if outcome.was_correct_90d is not None:
                total_90d += 1
                if outcome.was_correct_90d:
                    correct_90d += 1
                if outcome.return_90d_pct is not None:
                    returns_90d.append(outcome.return_90d_pct)

            # Track best/worst
            for ret in [outcome.return_90d_pct, outcome.return_30d_pct, outcome.return_7d_pct]:
                if ret is not None:
                    detail = _make_detail(rec, outcome, stock)
                    if ret > best_return:
                        best_return = ret
                        best_rec = detail
                    if ret < worst_return:
                        worst_return = ret
                        worst_rec = detail
                    break

    stats.accuracy_7d = round((correct_7d / total_7d * 100) if total_7d > 0 else 0, 1)
    stats.accuracy_30d = round((correct_30d / total_30d * 100) if total_30d > 0 else 0, 1)
    stats.accuracy_90d = round((correct_90d / total_90d * 100) if total_90d > 0 else 0, 1)
    stats.correct_7d = correct_7d
    stats.correct_30d = correct_30d
    stats.correct_90d = correct_90d
    stats.avg_return_7d = round(sum(returns_7d) / len(returns_7d), 2) if returns_7d else 0
    stats.avg_return_30d = round(sum(returns_30d) / len(returns_30d), 2) if returns_30d else 0
    stats.avg_return_90d = round(sum(returns_90d) / len(returns_90d), 2) if returns_90d else 0
    stats.best_recommendation = best_rec
    stats.worst_recommendation = worst_rec
    stats.recommendations_by_type = rec_type_counts

    return stats


def _make_detail(rec, outcome, stock) -> RecommendationDetail:
    """Create a RecommendationDetail from DB objects."""
    return RecommendationDetail(
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
