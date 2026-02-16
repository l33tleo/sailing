"""Pydantic schemas for portfolio API requests and responses."""

from datetime import date, datetime
from pydantic import BaseModel


class PortfolioCreate(BaseModel):
    name: str = "Min portefølje"


class PortfolioResponse(BaseModel):
    id: int
    name: str
    total_value: float = 0.0
    total_cost: float = 0.0
    total_return: float = 0.0
    total_return_pct: float = 0.0
    holdings_count: int = 0
    created_at: datetime

    model_config = {"from_attributes": True}


class HoldingCreate(BaseModel):
    ticker: str
    shares: float
    buy_price: float
    buy_date: date


class HoldingResponse(BaseModel):
    id: int
    ticker: str
    name: str
    shares: float
    buy_price: float
    buy_date: date
    current_price: float | None = None
    current_value: float = 0.0
    cost_basis: float = 0.0
    return_value: float = 0.0
    return_pct: float = 0.0

    model_config = {"from_attributes": True}


class WatchlistAdd(BaseModel):
    ticker: str


class WatchlistResponse(BaseModel):
    id: int
    ticker: str
    name: str
    current_price: float | None = None
    change_pct: float | None = None
    added_at: datetime

    model_config = {"from_attributes": True}
