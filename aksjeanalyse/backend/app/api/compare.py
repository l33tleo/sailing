"""Compare API endpoint — side-by-side stock comparison."""

from fastapi import APIRouter, HTTPException, Query
from pydantic import BaseModel

from app.services.stock_data import get_stock_quote, get_key_metrics, get_price_history
from app.services.technical import analyze_technical
from app.services.fundamental import analyze_fundamental
from app.services.recommender import generate_recommendation

router = APIRouter(prefix="/compare", tags=["Sammenligning"])


class ComparisonStock(BaseModel):
    ticker: str
    name: str
    price: float | None = None
    change_percent: float | None = None
    currency: str | None = None
    market_cap: float | None = None
    pe_ratio: float | None = None
    forward_pe: float | None = None
    pb_ratio: float | None = None
    ev_ebitda: float | None = None
    debt_to_equity: float | None = None
    revenue_growth: float | None = None
    dividend_yield: float | None = None
    roe: float | None = None
    beta: float | None = None
    free_cash_flow: float | None = None
    technical_score: float = 0
    fundamental_score: float = 0
    combined_score: float = 0
    recommendation: str = "–"
    rsi: float | None = None
    sma_signal: str | None = None
    macd_signal: str | None = None
    price_history_dates: list[str] = []
    price_history_close: list[float] = []


class ComparisonResponse(BaseModel):
    stocks: list[ComparisonStock]


@router.get("/", response_model=ComparisonResponse)
async def compare_stocks(
    tickers: str = Query(
        ...,
        description="Kommaseparerte tickers å sammenligne, f.eks. AAPL,MSFT,GOOGL",
    ),
):
    """Sammenlign 2-5 aksjer side om side med nøkkeltall, analyse og anbefaling."""
    ticker_list = [t.strip().upper() for t in tickers.split(",") if t.strip()]

    if len(ticker_list) < 2:
        raise HTTPException(status_code=400, detail="Minst 2 aksjer kreves for sammenligning")
    if len(ticker_list) > 5:
        raise HTTPException(status_code=400, detail="Maks 5 aksjer kan sammenlignes")

    results = []
    for ticker in ticker_list:
        stock = ComparisonStock(ticker=ticker, name=ticker)

        # Quote
        quote = get_stock_quote(ticker)
        if quote:
            stock.name = quote.name
            stock.price = quote.price
            stock.change_percent = quote.change_percent
            stock.currency = quote.currency
            stock.market_cap = quote.market_cap
            stock.pe_ratio = quote.pe_ratio
            stock.dividend_yield = quote.dividend_yield

        # Key metrics
        metrics = get_key_metrics(ticker)
        if metrics:
            stock.forward_pe = metrics.forward_pe
            stock.pb_ratio = metrics.pb_ratio
            stock.ev_ebitda = metrics.ev_ebitda
            stock.debt_to_equity = metrics.debt_to_equity
            stock.revenue_growth = metrics.revenue_growth
            stock.roe = metrics.roe
            stock.beta = metrics.beta
            stock.free_cash_flow = metrics.free_cash_flow
            if metrics.dividend_yield is not None:
                stock.dividend_yield = metrics.dividend_yield

        # Technical analysis
        tech = analyze_technical(ticker)
        stock.technical_score = tech.technical_score
        stock.rsi = tech.rsi
        stock.sma_signal = tech.sma_signal
        stock.macd_signal = tech.macd_signal

        # Fundamental analysis
        fund = analyze_fundamental(ticker)
        stock.fundamental_score = fund.fundamental_score

        # Combined
        stock.combined_score = round(
            (tech.technical_score * 0.5) + (fund.fundamental_score * 0.5), 1
        )
        if stock.combined_score >= 70:
            stock.recommendation = "KJØP"
        elif stock.combined_score >= 40:
            stock.recommendation = "HOLD"
        else:
            stock.recommendation = "SELG"

        # Price history (6 months for comparison chart)
        history = get_price_history(ticker, period="6mo")
        if history:
            stock.price_history_dates = history.dates
            stock.price_history_close = history.close

        results.append(stock)

    return ComparisonResponse(stocks=results)
