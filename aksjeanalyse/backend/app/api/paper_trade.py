"""Paper Trading API — simulate trades without real money."""

from datetime import datetime, timezone

from fastapi import APIRouter, Depends, HTTPException, Query
from pydantic import BaseModel
from sqlalchemy import select, func
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.database import get_db
from app.models.paper_trade import PaperTrade
from app.models.stock import Stock
from app.services.stock_data import get_stock_quote

router = APIRouter(prefix="/paper-trades", tags=["Paper Trading"])


class OpenTradeRequest(BaseModel):
    ticker: str
    trade_type: str  # BUY or SELL
    shares: float
    note: str | None = None


class CloseTradeRequest(BaseModel):
    note: str | None = None


class PaperTradeResponse(BaseModel):
    id: int
    ticker: str
    stock_name: str
    trade_type: str
    shares: float
    entry_price: float
    exit_price: float | None = None
    current_price: float | None = None
    is_open: bool
    pnl: float | None = None
    pnl_pct: float | None = None
    unrealized_pnl: float | None = None
    unrealized_pnl_pct: float | None = None
    note: str | None = None
    opened_at: str
    closed_at: str | None = None

    model_config = {"from_attributes": True}


class PaperTradeSummary(BaseModel):
    total_trades: int = 0
    open_trades: int = 0
    closed_trades: int = 0
    total_realized_pnl: float = 0.0
    total_unrealized_pnl: float = 0.0
    win_rate: float = 0.0  # % of closed trades that were profitable
    avg_pnl_pct: float = 0.0
    best_trade_pnl_pct: float | None = None
    worst_trade_pnl_pct: float | None = None
    starting_capital: float = 100000.0  # virtual starting capital
    current_equity: float = 100000.0


@router.post("", response_model=PaperTradeResponse)
async def open_trade(
    data: OpenTradeRequest,
    db: AsyncSession = Depends(get_db),
):
    """Åpne en ny simulert trade."""
    if data.trade_type not in ("BUY", "SELL"):
        raise HTTPException(status_code=400, detail="trade_type må være BUY eller SELL")
    if data.shares <= 0:
        raise HTTPException(status_code=400, detail="shares må være > 0")

    # Get current price
    quote = get_stock_quote(data.ticker)
    if not quote:
        raise HTTPException(status_code=404, detail=f"Finner ikke kurs for {data.ticker}")

    # Get or create stock
    result = await db.execute(select(Stock).where(Stock.ticker == data.ticker.upper()))
    stock = result.scalar_one_or_none()
    if stock is None:
        stock = Stock(
            ticker=data.ticker.upper(),
            name=quote.name,
            current_price=quote.price,
        )
        db.add(stock)
        await db.flush()
    else:
        stock.current_price = quote.price

    trade = PaperTrade(
        stock_id=stock.id,
        trade_type=data.trade_type,
        shares=data.shares,
        entry_price=quote.price,
        is_open=True,
        note=data.note,
    )
    db.add(trade)
    await db.flush()

    return PaperTradeResponse(
        id=trade.id,
        ticker=stock.ticker,
        stock_name=stock.name,
        trade_type=trade.trade_type,
        shares=trade.shares,
        entry_price=trade.entry_price,
        current_price=quote.price,
        is_open=True,
        unrealized_pnl=0.0,
        unrealized_pnl_pct=0.0,
        note=trade.note,
        opened_at=str(trade.opened_at),
    )


@router.post("/{trade_id}/close", response_model=PaperTradeResponse)
async def close_trade(
    trade_id: int,
    data: CloseTradeRequest | None = None,
    db: AsyncSession = Depends(get_db),
):
    """Lukk en åpen simulert trade til nåværende markedskurs."""
    result = await db.execute(select(PaperTrade).where(PaperTrade.id == trade_id))
    trade = result.scalar_one_or_none()
    if not trade:
        raise HTTPException(status_code=404, detail="Trade ikke funnet")
    if not trade.is_open:
        raise HTTPException(status_code=400, detail="Trade er allerede lukket")

    stock = trade.stock
    quote = get_stock_quote(stock.ticker)
    if not quote:
        raise HTTPException(status_code=500, detail=f"Kunne ikke hente kurs for {stock.ticker}")

    trade.exit_price = quote.price
    trade.is_open = False
    trade.closed_at = datetime.now(timezone.utc)

    if data and data.note:
        trade.note = (trade.note or "") + f" | Lukket: {data.note}"

    # Calculate P&L
    if trade.trade_type == "BUY":
        trade.pnl = (trade.exit_price - trade.entry_price) * trade.shares
        trade.pnl_pct = ((trade.exit_price - trade.entry_price) / trade.entry_price) * 100
    else:  # SELL (short)
        trade.pnl = (trade.entry_price - trade.exit_price) * trade.shares
        trade.pnl_pct = ((trade.entry_price - trade.exit_price) / trade.entry_price) * 100

    await db.flush()

    return PaperTradeResponse(
        id=trade.id,
        ticker=stock.ticker,
        stock_name=stock.name,
        trade_type=trade.trade_type,
        shares=trade.shares,
        entry_price=trade.entry_price,
        exit_price=trade.exit_price,
        current_price=quote.price,
        is_open=False,
        pnl=round(trade.pnl, 2),
        pnl_pct=round(trade.pnl_pct, 2),
        note=trade.note,
        opened_at=str(trade.opened_at),
        closed_at=str(trade.closed_at),
    )


@router.get("", response_model=list[PaperTradeResponse])
async def list_trades(
    open_only: bool = Query(False, description="Vis kun åpne trades"),
    db: AsyncSession = Depends(get_db),
):
    """Hent alle paper trades."""
    query = select(PaperTrade).order_by(PaperTrade.opened_at.desc())
    if open_only:
        query = query.where(PaperTrade.is_open == True)

    result = await db.execute(query)
    trades = result.scalars().all()

    responses = []
    for trade in trades:
        stock = trade.stock
        current_price = stock.current_price

        # Calculate unrealized P&L for open trades
        unrealized_pnl = None
        unrealized_pnl_pct = None
        if trade.is_open and current_price:
            if trade.trade_type == "BUY":
                unrealized_pnl = round((current_price - trade.entry_price) * trade.shares, 2)
                unrealized_pnl_pct = round(
                    ((current_price - trade.entry_price) / trade.entry_price) * 100, 2
                )
            else:
                unrealized_pnl = round((trade.entry_price - current_price) * trade.shares, 2)
                unrealized_pnl_pct = round(
                    ((trade.entry_price - current_price) / trade.entry_price) * 100, 2
                )

        responses.append(
            PaperTradeResponse(
                id=trade.id,
                ticker=stock.ticker if stock else "???",
                stock_name=stock.name if stock else "Ukjent",
                trade_type=trade.trade_type,
                shares=trade.shares,
                entry_price=trade.entry_price,
                exit_price=trade.exit_price,
                current_price=current_price,
                is_open=trade.is_open,
                pnl=round(trade.pnl, 2) if trade.pnl is not None else None,
                pnl_pct=round(trade.pnl_pct, 2) if trade.pnl_pct is not None else None,
                unrealized_pnl=unrealized_pnl,
                unrealized_pnl_pct=unrealized_pnl_pct,
                note=trade.note,
                opened_at=str(trade.opened_at),
                closed_at=str(trade.closed_at) if trade.closed_at else None,
            )
        )

    return responses


@router.get("/summary", response_model=PaperTradeSummary)
async def trade_summary(db: AsyncSession = Depends(get_db)):
    """Hent oppsummering av paper trading-resultat."""
    result = await db.execute(select(PaperTrade).order_by(PaperTrade.opened_at))
    trades = result.scalars().all()

    summary = PaperTradeSummary()
    summary.total_trades = len(trades)

    closed_pnls = []

    for trade in trades:
        stock = trade.stock
        if trade.is_open:
            summary.open_trades += 1
            # Calculate unrealized P&L
            if stock and stock.current_price:
                if trade.trade_type == "BUY":
                    upnl = (stock.current_price - trade.entry_price) * trade.shares
                else:
                    upnl = (trade.entry_price - stock.current_price) * trade.shares
                summary.total_unrealized_pnl += upnl
        else:
            summary.closed_trades += 1
            if trade.pnl is not None:
                summary.total_realized_pnl += trade.pnl
                closed_pnls.append(trade.pnl_pct or 0)

    summary.total_realized_pnl = round(summary.total_realized_pnl, 2)
    summary.total_unrealized_pnl = round(summary.total_unrealized_pnl, 2)

    if closed_pnls:
        wins = sum(1 for p in closed_pnls if p > 0)
        summary.win_rate = round((wins / len(closed_pnls)) * 100, 1)
        summary.avg_pnl_pct = round(sum(closed_pnls) / len(closed_pnls), 2)
        summary.best_trade_pnl_pct = round(max(closed_pnls), 2)
        summary.worst_trade_pnl_pct = round(min(closed_pnls), 2)

    summary.current_equity = round(
        summary.starting_capital + summary.total_realized_pnl + summary.total_unrealized_pnl, 2
    )

    return summary


@router.delete("/{trade_id}")
async def delete_trade(trade_id: int, db: AsyncSession = Depends(get_db)):
    """Slett en paper trade."""
    result = await db.execute(select(PaperTrade).where(PaperTrade.id == trade_id))
    trade = result.scalar_one_or_none()
    if not trade:
        raise HTTPException(status_code=404, detail="Trade ikke funnet")
    await db.delete(trade)
    return {"message": "Trade slettet"}
