"""Tests for the technical analysis service."""

import numpy as np
import pandas as pd
from app.services.technical import calculate_rsi, calculate_macd, calculate_bollinger_bands


def _make_price_series(prices: list[float]) -> pd.Series:
    """Helper to create a pandas Series from a list of prices."""
    return pd.Series(prices, dtype=float)


class TestRSI:
    def test_rsi_returns_series(self):
        """RSI returns a pandas Series."""
        prices = _make_price_series([44 + i * 0.5 for i in range(30)])
        rsi = calculate_rsi(prices, period=14)
        assert isinstance(rsi, pd.Series)
        assert len(rsi) == len(prices)

    def test_rsi_range(self):
        """RSI values should be between 0 and 100."""
        prices = _make_price_series([100 + i * (-1)**i for i in range(50)])
        rsi = calculate_rsi(prices, period=14)
        valid = rsi.dropna()
        assert all(0 <= v <= 100 for v in valid)

    def test_rsi_uptrend_high(self):
        """Steadily rising prices should produce high RSI."""
        prices = _make_price_series([100 + i * 2 for i in range(30)])
        rsi = calculate_rsi(prices, period=14)
        last_rsi = rsi.iloc[-1]
        assert last_rsi > 70  # Strong uptrend → overbought

    def test_rsi_downtrend_low(self):
        """Steadily falling prices should produce low RSI."""
        prices = _make_price_series([200 - i * 2 for i in range(30)])
        rsi = calculate_rsi(prices, period=14)
        last_rsi = rsi.iloc[-1]
        assert last_rsi < 30  # Strong downtrend → oversold


class TestMACD:
    def test_macd_returns_three_series(self):
        """MACD returns macd_line, signal_line, histogram."""
        prices = _make_price_series([100 + i * 0.3 for i in range(50)])
        macd_line, signal_line, histogram = calculate_macd(prices)
        assert len(macd_line) == len(prices)
        assert len(signal_line) == len(prices)
        assert len(histogram) == len(prices)

    def test_histogram_is_macd_minus_signal(self):
        """Histogram should equal MACD line minus signal line."""
        prices = _make_price_series([100 + i * 0.5 * (-1)**i for i in range(50)])
        macd_line, signal_line, histogram = calculate_macd(prices)
        # Check last value (where all EMAs have converged)
        expected = macd_line.iloc[-1] - signal_line.iloc[-1]
        assert abs(histogram.iloc[-1] - expected) < 0.0001

    def test_macd_uptrend_positive(self):
        """Strong uptrend should produce positive MACD."""
        prices = _make_price_series([50 + i * 1.5 for i in range(50)])
        macd_line, _, _ = calculate_macd(prices)
        assert macd_line.iloc[-1] > 0


class TestBollingerBands:
    def test_bands_structure(self):
        """Returns upper, middle, lower bands."""
        prices = _make_price_series([100 + i * 0.2 for i in range(30)])
        upper, middle, lower = calculate_bollinger_bands(prices, period=20)
        assert len(upper) == len(prices)
        # Upper > middle > lower
        last_idx = -1
        if not np.isnan(upper.iloc[last_idx]):
            assert upper.iloc[last_idx] > middle.iloc[last_idx] > lower.iloc[last_idx]

    def test_middle_is_sma(self):
        """Middle band should be the SMA."""
        prices = _make_price_series([100 + i * 0.1 for i in range(30)])
        _, middle, _ = calculate_bollinger_bands(prices, period=20)
        sma = prices.rolling(window=20).mean()
        # Compare last values
        assert abs(middle.iloc[-1] - sma.iloc[-1]) < 0.0001

    def test_wider_stddev_means_wider_bands(self):
        """Higher std_dev parameter should give wider bands."""
        prices = _make_price_series([100 + i * 0.5 * (-1)**i for i in range(30)])
        upper1, _, lower1 = calculate_bollinger_bands(prices, period=20, std_dev=1.0)
        upper2, _, lower2 = calculate_bollinger_bands(prices, period=20, std_dev=3.0)
        # Wider bands with higher std_dev
        width1 = upper1.iloc[-1] - lower1.iloc[-1]
        width2 = upper2.iloc[-1] - lower2.iloc[-1]
        assert width2 > width1
