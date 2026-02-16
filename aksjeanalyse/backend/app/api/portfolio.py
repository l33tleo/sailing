"""Portfolio and watchlist API endpoints."""

from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.database import get_db
from app.models.portfolio import Portfolio, PortfolioHolding
from app.models.stock import Stock
from app.models.watchlist import Watchlist
from app.schemas.portfolio import (
    HoldingCreate,
    HoldingResponse,
    PortfolioCreate,
    PortfolioResponse,
    WatchlistAdd,
    WatchlistResponse,
)
from app.services.stock_data import get_stock_quote

router = APIRouter(tags=["Portefølje"])


# --- Portfolio ---

@router.post("/portfolios", response_model=PortfolioResponse)
async def create_portfolio(
    data: PortfolioCreate, db: AsyncSession = Depends(get_db)
):
    """Opprett en ny portefølje."""
    portfolio = Portfolio(name=data.name)
    db.add(portfolio)
    await db.flush()
    return PortfolioResponse(
        id=portfolio.id,
        name=portfolio.name,
        created_at=portfolio.created_at,
    )


@router.get("/portfolios", response_model=list[PortfolioResponse])
async def list_portfolios(db: AsyncSession = Depends(get_db)):
    """Hent alle porteføljer med verdi-oppsummering."""
    result = await db.execute(select(Portfolio).order_by(Portfolio.created_at))
    portfolios = result.scalars().all()

    responses = []
    for portfolio in portfolios:
        total_value = 0.0
        total_cost = 0.0
        for holding in portfolio.holdings:
            stock = holding.stock
            if stock and stock.current_price:
                total_value += holding.shares * stock.current_price
            total_cost += holding.shares * holding.buy_price

        total_return = total_value - total_cost
        total_return_pct = (total_return / total_cost * 100) if total_cost > 0 else 0

        responses.append(PortfolioResponse(
            id=portfolio.id,
            name=portfolio.name,
            total_value=round(total_value, 2),
            total_cost=round(total_cost, 2),
            total_return=round(total_return, 2),
            total_return_pct=round(total_return_pct, 2),
            holdings_count=len(portfolio.holdings),
            created_at=portfolio.created_at,
        ))

    return responses


@router.post("/portfolios/{portfolio_id}/holdings", response_model=HoldingResponse)
async def add_holding(
    portfolio_id: int,
    data: HoldingCreate,
    db: AsyncSession = Depends(get_db),
):
    """Legg til en aksje i porteføljen."""
    # Check portfolio exists
    result = await db.execute(select(Portfolio).where(Portfolio.id == portfolio_id))
    portfolio = result.scalar_one_or_none()
    if not portfolio:
        raise HTTPException(status_code=404, detail="Portefølje ikke funnet")

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

    holding = PortfolioHolding(
        portfolio_id=portfolio_id,
        stock_id=stock.id,
        shares=data.shares,
        buy_price=data.buy_price,
        buy_date=data.buy_date,
    )
    db.add(holding)
    await db.flush()

    current_price = stock.current_price or data.buy_price
    cost_basis = data.shares * data.buy_price
    current_value = data.shares * current_price
    return_value = current_value - cost_basis
    return_pct = (return_value / cost_basis * 100) if cost_basis > 0 else 0

    return HoldingResponse(
        id=holding.id,
        ticker=stock.ticker,
        name=stock.name,
        shares=holding.shares,
        buy_price=holding.buy_price,
        buy_date=holding.buy_date,
        current_price=current_price,
        current_value=round(current_value, 2),
        cost_basis=round(cost_basis, 2),
        return_value=round(return_value, 2),
        return_pct=round(return_pct, 2),
    )


@router.get("/portfolios/{portfolio_id}/holdings", response_model=list[HoldingResponse])
async def list_holdings(
    portfolio_id: int,
    db: AsyncSession = Depends(get_db),
):
    """Hent alle aksjer i en portefølje."""
    result = await db.execute(
        select(PortfolioHolding)
        .where(PortfolioHolding.portfolio_id == portfolio_id)
        .order_by(PortfolioHolding.buy_date)
    )
    holdings = result.scalars().all()

    responses = []
    for holding in holdings:
        stock = holding.stock
        current_price = (stock.current_price if stock else None) or holding.buy_price
        cost_basis = holding.shares * holding.buy_price
        current_value = holding.shares * current_price
        return_value = current_value - cost_basis
        return_pct = (return_value / cost_basis * 100) if cost_basis > 0 else 0

        responses.append(HoldingResponse(
            id=holding.id,
            ticker=stock.ticker if stock else "???",
            name=stock.name if stock else "Ukjent",
            shares=holding.shares,
            buy_price=holding.buy_price,
            buy_date=holding.buy_date,
            current_price=current_price,
            current_value=round(current_value, 2),
            cost_basis=round(cost_basis, 2),
            return_value=round(return_value, 2),
            return_pct=round(return_pct, 2),
        ))

    return responses


@router.delete("/portfolios/{portfolio_id}/holdings/{holding_id}")
async def remove_holding(
    portfolio_id: int,
    holding_id: int,
    db: AsyncSession = Depends(get_db),
):
    """Fjern en aksje fra porteføljen."""
    result = await db.execute(
        select(PortfolioHolding).where(
            PortfolioHolding.id == holding_id,
            PortfolioHolding.portfolio_id == portfolio_id,
        )
    )
    holding = result.scalar_one_or_none()
    if not holding:
        raise HTTPException(status_code=404, detail="Holding ikke funnet")
    await db.delete(holding)
    return {"message": "Holding slettet"}


# --- Watchlist ---

@router.post("/watchlist", response_model=WatchlistResponse)
async def add_to_watchlist(
    data: WatchlistAdd,
    db: AsyncSession = Depends(get_db),
):
    """Legg til en aksje i watchlist."""
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

    # Check if already in watchlist
    existing = await db.execute(
        select(Watchlist).where(Watchlist.stock_id == stock.id)
    )
    if existing.scalar_one_or_none():
        raise HTTPException(status_code=409, detail="Aksjen er allerede i watchlist")

    wl = Watchlist(stock_id=stock.id)
    db.add(wl)
    await db.flush()

    return WatchlistResponse(
        id=wl.id,
        ticker=stock.ticker,
        name=stock.name,
        current_price=stock.current_price,
        added_at=wl.added_at,
    )


@router.get("/watchlist", response_model=list[WatchlistResponse])
async def list_watchlist(db: AsyncSession = Depends(get_db)):
    """Hent alle aksjer i watchlist."""
    result = await db.execute(select(Watchlist).order_by(Watchlist.added_at.desc()))
    items = result.scalars().all()

    return [
        WatchlistResponse(
            id=wl.id,
            ticker=wl.stock.ticker,
            name=wl.stock.name,
            current_price=wl.stock.current_price,
            added_at=wl.added_at,
        )
        for wl in items
    ]


@router.delete("/watchlist/{watchlist_id}")
async def remove_from_watchlist(
    watchlist_id: int,
    db: AsyncSession = Depends(get_db),
):
    """Fjern en aksje fra watchlist."""
    result = await db.execute(select(Watchlist).where(Watchlist.id == watchlist_id))
    wl = result.scalar_one_or_none()
    if not wl:
        raise HTTPException(status_code=404, detail="Watchlist-element ikke funnet")
    await db.delete(wl)
    return {"message": "Fjernet fra watchlist"}
