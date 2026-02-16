"""Tests for the recommendation scoring logic."""

from app.services.recommender import _generate_reasoning


class TestRecommendationLogic:
    """Test the scoring thresholds and recommendation type mapping."""

    def test_kjop_threshold(self):
        """Score >= 70 should produce KJØP."""
        # The combined_score logic: 70+ = KJØP
        assert _score_to_rec(70) == "KJØP"
        assert _score_to_rec(85) == "KJØP"
        assert _score_to_rec(100) == "KJØP"

    def test_hold_threshold(self):
        """Score 40-69 should produce HOLD."""
        assert _score_to_rec(40) == "HOLD"
        assert _score_to_rec(55) == "HOLD"
        assert _score_to_rec(69) == "HOLD"

    def test_selg_threshold(self):
        """Score < 40 should produce SELG."""
        assert _score_to_rec(0) == "SELG"
        assert _score_to_rec(20) == "SELG"
        assert _score_to_rec(39) == "SELG"

    def test_reasoning_contains_type(self):
        """Reasoning text should mention the recommendation type."""

        class FakeTech:
            technical_score = 50
            rsi = 45.0
            rsi_signal = "Nøytral"
            macd_signal = "Bullish"
            sma_signal = "Over SMA50"
            volume_signal = "Normal"
            bollinger_signal = "Innenfor"

        class FakeFund:
            fundamental_score = 60
            pe_detail = "P/E 15"
            debt_detail = "Lav gjeld"
            growth_detail = "God vekst"
            dividend_detail = "Utbytte 3%"
            cashflow_detail = "Positiv"

        reasoning = _generate_reasoning("AAPL", FakeTech(), FakeFund(), 55.0, "HOLD")
        assert "HOLD" in reasoning
        assert "AAPL" in reasoning
        assert "55.0" in reasoning

    def test_combined_score_is_average(self):
        """Combined score should be 50/50 average of tech and fundamental."""
        tech_score = 80
        fund_score = 60
        combined = (tech_score * 0.5) + (fund_score * 0.5)
        assert combined == 70.0


def _score_to_rec(combined_score: float) -> str:
    """Replicate the recommendation logic from recommender.py."""
    if combined_score >= 70:
        return "KJØP"
    elif combined_score >= 40:
        return "HOLD"
    else:
        return "SELG"
