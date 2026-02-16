"""Pydantic schemas for stock-related API requests and responses."""

from datetime import datetime
from pydantic import BaseModel


class StockBase(BaseModel):
    ticker: str
    name: str
    sector: str | None = None
    industry: str | None = None
    market: str | None = None
    currency: str | None = None


class StockCreate(StockBase):
    pass


class StockResponse(StockBase):
    id: int
    current_price: float | None = None
    market_cap: float | None = None
    last_updated: datetime
    created_at: datetime

    model_config = {"from_attributes": True}


class StockSearchResult(BaseModel):
    ticker: str
    name: str
    sector: str | None = None
    market: str | None = None
    current_price: float | None = None
    currency: str | None = None


class StockPriceHistory(BaseModel):
    dates: list[str]
    open: list[float]
    high: list[float]
    low: list[float]
    close: list[float]
    volume: list[int]


class StockQuote(BaseModel):
    ticker: str
    name: str
    price: float
    change: float
    change_percent: float
    volume: int
    market_cap: float | None = None
    pe_ratio: float | None = None
    dividend_yield: float | None = None
    fifty_two_week_high: float | None = None
    fifty_two_week_low: float | None = None
    currency: str | None = None


class StockKeyMetrics(BaseModel):
    """Fundamental key metrics for a stock."""
    ticker: str
    pe_ratio: float | None = None
    forward_pe: float | None = None
    pb_ratio: float | None = None
    ps_ratio: float | None = None
    ev_ebitda: float | None = None
    debt_to_equity: float | None = None
    current_ratio: float | None = None
    roe: float | None = None  # Return on Equity
    revenue_growth: float | None = None
    earnings_growth: float | None = None
    dividend_yield: float | None = None
    payout_ratio: float | None = None
    free_cash_flow: float | None = None
    market_cap: float | None = None
    beta: float | None = None
