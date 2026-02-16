"""Alerts API — manage stock alerts/notifications."""

from fastapi import APIRouter, Depends, HTTPException, Query
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.database import get_db
from app.models.alert import Alert
from app.models.stock import Stock
from app.services.stock_data import get_stock_quote

router = APIRouter(prefix="/alerts", tags=["Varsler"])


class AlertCreate(BaseModel):
    ticker: str
    condition_type: str  # PRICE_ABOVE, PRICE_BELOW, RSI_ABOVE, RSI_BELOW
    threshold: float


class AlertResponse(BaseModel):
    id: int
    ticker: str
    stock_name: str
    condition_type: str
    threshold: float
    is_active: bool
    last_triggered: str | None = None
    created_at: str

    model_config = {"from_attributes": True}


class AlertCheckResult(BaseModel):
    alert_id: int
    ticker: str
    condition_type: str
    threshold: float
    current_value: float
    triggered: bool
    message: str


CONDITION_LABELS = {
    "PRICE_ABOVE": "Kurs over",
    "PRICE_BELOW": "Kurs under",
    "RSI_ABOVE": "RSI over",
    "RSI_BELOW": "RSI under",
}


@router.post("/", response_model=AlertResponse)
async def create_alert(
    data: AlertCreate,
    db: AsyncSession = Depends(get_db),
):
    """Opprett et nytt varsel for en aksje."""
    if data.condition_type not in CONDITION_LABELS:
        raise HTTPException(
            status_code=400,
            detail=f"Ugyldig condition_type. Gyldige: {list(CONDITION_LABELS.keys())}",
        )

    # Get or create stock
    result = await db.execute(select(Stock).where(Stock.ticker == data.ticker.upper()))
    stock = result.scalar_one_or_none()

    if stock is None:
        quote = get_stock_quote(data.ticker)
        stock = Stock(
            ticker=data.ticker.upper(),
            name=quote.name if quote else data.ticker.upper(),
            current_price=quote.price if quote else None,
        )
        db.add(stock)
        await db.flush()

    alert = Alert(
        stock_id=stock.id,
        condition_type=data.condition_type,
        threshold=data.threshold,
        is_active=True,
    )
    db.add(alert)
    await db.flush()

    return AlertResponse(
        id=alert.id,
        ticker=stock.ticker,
        stock_name=stock.name,
        condition_type=alert.condition_type,
        threshold=alert.threshold,
        is_active=alert.is_active,
        created_at=str(alert.created_at),
    )


@router.get("/", response_model=list[AlertResponse])
async def list_alerts(
    active_only: bool = Query(True),
    db: AsyncSession = Depends(get_db),
):
    """Hent alle varsler."""
    query = select(Alert).order_by(Alert.created_at.desc())
    if active_only:
        query = query.where(Alert.is_active == True)

    result = await db.execute(query)
    alerts = result.scalars().all()

    return [
        AlertResponse(
            id=a.id,
            ticker=a.stock.ticker,
            stock_name=a.stock.name,
            condition_type=a.condition_type,
            threshold=a.threshold,
            is_active=a.is_active,
            last_triggered=str(a.last_triggered) if a.last_triggered else None,
            created_at=str(a.created_at),
        )
        for a in alerts
    ]


@router.delete("/{alert_id}")
async def delete_alert(alert_id: int, db: AsyncSession = Depends(get_db)):
    """Slett et varsel."""
    result = await db.execute(select(Alert).where(Alert.id == alert_id))
    alert = result.scalar_one_or_none()
    if not alert:
        raise HTTPException(status_code=404, detail="Varsel ikke funnet")
    await db.delete(alert)
    return {"message": "Varsel slettet"}


@router.post("/check", response_model=list[AlertCheckResult])
async def check_alerts(db: AsyncSession = Depends(get_db)):
    """Sjekk alle aktive varsler mot nåværende markedsdata."""
    result = await db.execute(select(Alert).where(Alert.is_active == True))
    alerts = result.scalars().all()

    from app.services.technical import analyze_technical
    from datetime import datetime, timezone

    results = []
    for alert in alerts:
        ticker = alert.stock.ticker
        current_value = 0.0
        triggered = False
        message = ""

        if alert.condition_type in ("PRICE_ABOVE", "PRICE_BELOW"):
            quote = get_stock_quote(ticker)
            if quote:
                current_value = quote.price
                if alert.condition_type == "PRICE_ABOVE" and current_value > alert.threshold:
                    triggered = True
                    message = f"{ticker} kurs {current_value:.2f} er over {alert.threshold:.2f}"
                elif alert.condition_type == "PRICE_BELOW" and current_value < alert.threshold:
                    triggered = True
                    message = f"{ticker} kurs {current_value:.2f} er under {alert.threshold:.2f}"
                else:
                    message = f"{ticker} kurs {current_value:.2f} — varsel ikke utløst"

        elif alert.condition_type in ("RSI_ABOVE", "RSI_BELOW"):
            tech = analyze_technical(ticker)
            if tech.rsi is not None:
                current_value = tech.rsi
                if alert.condition_type == "RSI_ABOVE" and current_value > alert.threshold:
                    triggered = True
                    message = f"{ticker} RSI {current_value:.1f} er over {alert.threshold:.0f}"
                elif alert.condition_type == "RSI_BELOW" and current_value < alert.threshold:
                    triggered = True
                    message = f"{ticker} RSI {current_value:.1f} er under {alert.threshold:.0f}"
                else:
                    message = f"{ticker} RSI {current_value:.1f} — varsel ikke utløst"

        if triggered:
            alert.last_triggered = datetime.now(timezone.utc)

        results.append(
            AlertCheckResult(
                alert_id=alert.id,
                ticker=ticker,
                condition_type=alert.condition_type,
                threshold=alert.threshold,
                current_value=round(current_value, 2),
                triggered=triggered,
                message=message,
            )
        )

    await db.commit()
    return results
