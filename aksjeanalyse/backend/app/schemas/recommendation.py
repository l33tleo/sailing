"""Pydantic schemas for recommendation API requests and responses."""

from datetime import datetime
from pydantic import BaseModel


class RecommendationResponse(BaseModel):
    id: int
    stock_ticker: str
    stock_name: str
    recommendation_type: str  # KJØP, HOLD, SELG
    confidence: int  # 0-100
    price_at_recommendation: float
    current_price: float | None = None
    technical_score: float
    fundamental_score: float
    combined_score: float = 0.0
    reasoning: str
    created_at: datetime

    model_config = {"from_attributes": True}


class RecommendationDetail(RecommendationResponse):
    """Extended recommendation with outcome data."""
    price_after_7d: float | None = None
    price_after_30d: float | None = None
    price_after_90d: float | None = None
    return_7d_pct: float | None = None
    return_30d_pct: float | None = None
    return_90d_pct: float | None = None
    was_correct_7d: bool | None = None
    was_correct_30d: bool | None = None
    was_correct_90d: bool | None = None


class GenerateRecommendationRequest(BaseModel):
    ticker: str


class TechnicalAnalysisResult(BaseModel):
    """Results from technical analysis."""
    ticker: str
    rsi: float | None = None
    rsi_signal: str | None = None  # "Overkjøpt", "Oversolgt", "Nøytral"
    macd: float | None = None
    macd_signal_line: float | None = None
    macd_histogram: float | None = None
    macd_signal: str | None = None  # "Bullish", "Bearish"
    sma_20: float | None = None
    sma_50: float | None = None
    sma_200: float | None = None
    sma_signal: str | None = None  # "Golden Cross", "Death Cross", "Nøytral"
    bollinger_upper: float | None = None
    bollinger_middle: float | None = None
    bollinger_lower: float | None = None
    bollinger_signal: str | None = None
    current_price: float | None = None
    volume_avg_20d: float | None = None
    volume_current: float | None = None
    volume_signal: str | None = None
    technical_score: float = 0.0


class FundamentalAnalysisResult(BaseModel):
    """Results from fundamental analysis."""
    ticker: str
    pe_score: float = 0.0
    pe_detail: str = ""
    debt_score: float = 0.0
    debt_detail: str = ""
    growth_score: float = 0.0
    growth_detail: str = ""
    dividend_score: float = 0.0
    dividend_detail: str = ""
    cashflow_score: float = 0.0
    cashflow_detail: str = ""
    fundamental_score: float = 0.0


class FullAnalysis(BaseModel):
    """Complete stock analysis combining technical and fundamental."""
    ticker: str
    name: str
    current_price: float | None = None
    technical: TechnicalAnalysisResult
    fundamental: FundamentalAnalysisResult
    recommendation_type: str
    confidence: int
    combined_score: float
    reasoning: str


class ScorecardStats(BaseModel):
    """Overall accuracy scorecard."""
    total_recommendations: int = 0
    correct_7d: int = 0
    correct_30d: int = 0
    correct_90d: int = 0
    accuracy_7d: float = 0.0  # percentage
    accuracy_30d: float = 0.0
    accuracy_90d: float = 0.0
    avg_return_7d: float = 0.0
    avg_return_30d: float = 0.0
    avg_return_90d: float = 0.0
    best_recommendation: RecommendationDetail | None = None
    worst_recommendation: RecommendationDetail | None = None
    recommendations_by_type: dict[str, int] = {}
