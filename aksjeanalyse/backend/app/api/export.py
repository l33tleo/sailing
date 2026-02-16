"""Export API endpoint — CSV export of recommendation history."""

import csv
import io
from datetime import datetime

from fastapi import APIRouter, Depends
from fastapi.responses import StreamingResponse
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.database import get_db
from app.models.recommendation import Recommendation, RecommendationOutcome
from app.models.stock import Stock

router = APIRouter(prefix="/export", tags=["Eksport"])


@router.get("/recommendations/csv")
async def export_recommendations_csv(db: AsyncSession = Depends(get_db)):
    """Last ned anbefalingshistorikk som CSV-fil."""
    result = await db.execute(
        select(Recommendation, RecommendationOutcome, Stock)
        .join(Stock, Recommendation.stock_id == Stock.id)
        .join(
            RecommendationOutcome,
            Recommendation.id == RecommendationOutcome.recommendation_id,
            isouter=True,
        )
        .order_by(Recommendation.created_at.desc())
    )
    rows = result.all()

    output = io.StringIO()
    writer = csv.writer(output, delimiter=";")

    # Header
    writer.writerow([
        "Dato",
        "Ticker",
        "Selskap",
        "Anbefaling",
        "Konfidens (%)",
        "Teknisk score",
        "Fundamental score",
        "Samlet score",
        "Kurs ved anbefaling",
        "Kurs etter 7d",
        "Kurs etter 30d",
        "Kurs etter 90d",
        "Avkastning 7d (%)",
        "Avkastning 30d (%)",
        "Avkastning 90d (%)",
        "Korrekt 7d",
        "Korrekt 30d",
        "Korrekt 90d",
        "Begrunnelse",
    ])

    for rec, outcome, stock in rows:
        combined = round((rec.technical_score * 0.5) + (rec.fundamental_score * 0.5), 1)
        writer.writerow([
            rec.created_at.strftime("%Y-%m-%d %H:%M") if rec.created_at else "",
            stock.ticker,
            stock.name,
            rec.recommendation_type,
            rec.confidence,
            rec.technical_score,
            rec.fundamental_score,
            combined,
            rec.price_at_recommendation,
            outcome.price_after_7d if outcome else "",
            outcome.price_after_30d if outcome else "",
            outcome.price_after_90d if outcome else "",
            outcome.return_7d_pct if outcome and outcome.return_7d_pct is not None else "",
            outcome.return_30d_pct if outcome and outcome.return_30d_pct is not None else "",
            outcome.return_90d_pct if outcome and outcome.return_90d_pct is not None else "",
            "Ja" if outcome and outcome.was_correct_7d else ("Nei" if outcome and outcome.was_correct_7d is False else ""),
            "Ja" if outcome and outcome.was_correct_30d else ("Nei" if outcome and outcome.was_correct_30d is False else ""),
            "Ja" if outcome and outcome.was_correct_90d else ("Nei" if outcome and outcome.was_correct_90d is False else ""),
            rec.reasoning.replace("\n", " | ") if rec.reasoning else "",
        ])

    output.seek(0)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"anbefalinger_{timestamp}.csv"

    return StreamingResponse(
        iter([output.getvalue()]),
        media_type="text/csv",
        headers={"Content-Disposition": f"attachment; filename={filename}"},
    )
