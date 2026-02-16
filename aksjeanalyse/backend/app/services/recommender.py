"""Recommendation engine — combines technical and fundamental analysis into buy/hold/sell."""

import logging
from app.schemas.recommendation import FullAnalysis
from app.services.technical import analyze_technical
from app.services.fundamental import analyze_fundamental
from app.services.stock_data import get_stock_quote

logger = logging.getLogger(__name__)


def generate_recommendation(ticker: str) -> FullAnalysis | None:
    """Generate a full analysis and recommendation for a stock.
    
    Combines technical score (0-100) and fundamental score (0-100)
    into a weighted combined score that determines the recommendation.
    
    Weights: 50% technical, 50% fundamental.
    """
    # Get current quote
    quote = get_stock_quote(ticker)
    if not quote:
        logger.error(f"Cannot get quote for {ticker}")
        return None

    # Run both analyses
    technical = analyze_technical(ticker)
    fundamental = analyze_fundamental(ticker)

    # Combined score (50/50 weighting)
    combined_score = (technical.technical_score * 0.5) + (fundamental.fundamental_score * 0.5)
    combined_score = round(combined_score, 1)

    # Determine recommendation
    if combined_score >= 70:
        rec_type = "KJØP"
    elif combined_score >= 40:
        rec_type = "HOLD"
    else:
        rec_type = "SELG"

    # Calculate confidence (how far from threshold boundaries)
    if rec_type == "KJØP":
        # Distance from 70 towards 100
        confidence = min(100, int(50 + (combined_score - 70) * (50 / 30)))
    elif rec_type == "SELG":
        # Distance from 40 towards 0
        confidence = min(100, int(50 + (40 - combined_score) * (50 / 40)))
    else:
        # HOLD — confidence based on how centered the score is
        mid = 55  # midpoint of HOLD range
        distance_from_mid = abs(combined_score - mid)
        confidence = max(30, int(70 - distance_from_mid * 2))

    # Generate reasoning in Norwegian
    reasoning = _generate_reasoning(ticker, technical, fundamental, combined_score, rec_type)

    return FullAnalysis(
        ticker=ticker,
        name=quote.name,
        current_price=quote.price,
        technical=technical,
        fundamental=fundamental,
        recommendation_type=rec_type,
        confidence=confidence,
        combined_score=combined_score,
        reasoning=reasoning,
    )


def _generate_reasoning(
    ticker: str,
    technical,
    fundamental,
    combined_score: float,
    rec_type: str,
) -> str:
    """Generate a human-readable reasoning text in Norwegian."""
    parts = []

    # Overall summary
    if rec_type == "KJØP":
        parts.append(f"Anbefaler KJØP av {ticker} med samlet score {combined_score}/100.")
    elif rec_type == "SELG":
        parts.append(f"Anbefaler SELG av {ticker} med samlet score {combined_score}/100.")
    else:
        parts.append(f"Anbefaler HOLD for {ticker} med samlet score {combined_score}/100.")

    # Technical summary
    parts.append(f"\nTeknisk analyse (score: {technical.technical_score}/100):")
    if technical.rsi_signal:
        parts.append(f"  • RSI ({technical.rsi}): {technical.rsi_signal}")
    if technical.macd_signal:
        parts.append(f"  • MACD: {technical.macd_signal}")
    if technical.sma_signal:
        parts.append(f"  • Glidende snitt: {technical.sma_signal}")
    if technical.volume_signal:
        parts.append(f"  • Volum: {technical.volume_signal}")
    if technical.bollinger_signal:
        parts.append(f"  • Bollinger: {technical.bollinger_signal}")

    # Fundamental summary
    parts.append(f"\nFundamental analyse (score: {fundamental.fundamental_score}/100):")
    if fundamental.pe_detail:
        parts.append(f"  • Verdsettelse: {fundamental.pe_detail}")
    if fundamental.debt_detail:
        parts.append(f"  • Gjeld: {fundamental.debt_detail}")
    if fundamental.growth_detail:
        parts.append(f"  • Vekst: {fundamental.growth_detail}")
    if fundamental.dividend_detail:
        parts.append(f"  • Utbytte: {fundamental.dividend_detail}")
    if fundamental.cashflow_detail:
        parts.append(f"  • Kontantstrøm: {fundamental.cashflow_detail}")

    return "\n".join(parts)
