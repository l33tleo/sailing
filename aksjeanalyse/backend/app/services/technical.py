"""Technical analysis service — calculates indicators and scores."""

import logging
import numpy as np
import pandas as pd

from app.schemas.recommendation import TechnicalAnalysisResult
from app.services.stock_data import get_historical_dataframe

logger = logging.getLogger(__name__)


def calculate_rsi(series: pd.Series, period: int = 14) -> pd.Series:
    """Calculate Relative Strength Index."""
    delta = series.diff()
    gain = delta.where(delta > 0, 0.0)
    loss = -delta.where(delta < 0, 0.0)
    avg_gain = gain.rolling(window=period, min_periods=period).mean()
    avg_loss = loss.rolling(window=period, min_periods=period).mean()
    rs = avg_gain / avg_loss
    rsi = 100 - (100 / (1 + rs))
    return rsi


def calculate_macd(
    series: pd.Series,
    fast: int = 12,
    slow: int = 26,
    signal: int = 9,
) -> tuple[pd.Series, pd.Series, pd.Series]:
    """Calculate MACD, signal line, and histogram."""
    ema_fast = series.ewm(span=fast, adjust=False).mean()
    ema_slow = series.ewm(span=slow, adjust=False).mean()
    macd_line = ema_fast - ema_slow
    signal_line = macd_line.ewm(span=signal, adjust=False).mean()
    histogram = macd_line - signal_line
    return macd_line, signal_line, histogram


def calculate_bollinger_bands(
    series: pd.Series, period: int = 20, std_dev: float = 2.0
) -> tuple[pd.Series, pd.Series, pd.Series]:
    """Calculate Bollinger Bands (upper, middle, lower)."""
    middle = series.rolling(window=period).mean()
    std = series.rolling(window=period).std()
    upper = middle + (std_dev * std)
    lower = middle - (std_dev * std)
    return upper, middle, lower


def analyze_technical(ticker: str) -> TechnicalAnalysisResult:
    """Perform full technical analysis on a stock.
    
    Returns a TechnicalAnalysisResult with all indicators and a score 0-100.
    """
    result = TechnicalAnalysisResult(ticker=ticker)

    df = get_historical_dataframe(ticker, period="1y")
    if df is None or len(df) < 200:
        # Try shorter period
        df = get_historical_dataframe(ticker, period="6mo")
        if df is None or len(df) < 30:
            logger.warning(f"Insufficient data for technical analysis of {ticker}")
            return result

    close = df["Close"]
    volume = df["Volume"]
    current_price = float(close.iloc[-1])
    result.current_price = round(current_price, 2)

    score = 0.0

    # --- RSI ---
    try:
        rsi_series = calculate_rsi(close)
        rsi_val = float(rsi_series.iloc[-1])
        if not np.isnan(rsi_val):
            result.rsi = round(rsi_val, 2)
            if rsi_val < 30:
                result.rsi_signal = "Oversolgt"
                score += 25  # Strong buy signal
            elif rsi_val < 40:
                result.rsi_signal = "Nær oversolgt"
                score += 18
            elif rsi_val > 70:
                result.rsi_signal = "Overkjøpt"
                score += 0  # Sell signal
            elif rsi_val > 60:
                result.rsi_signal = "Nær overkjøpt"
                score += 8
            else:
                result.rsi_signal = "Nøytral"
                score += 12
    except Exception as e:
        logger.error(f"RSI calculation error for {ticker}: {e}")

    # --- MACD ---
    try:
        macd_line, signal_line, histogram = calculate_macd(close)
        macd_val = float(macd_line.iloc[-1])
        signal_val = float(signal_line.iloc[-1])
        hist_val = float(histogram.iloc[-1])

        if not any(np.isnan(v) for v in [macd_val, signal_val, hist_val]):
            result.macd = round(macd_val, 4)
            result.macd_signal_line = round(signal_val, 4)
            result.macd_histogram = round(hist_val, 4)

            # Check for crossover
            prev_hist = float(histogram.iloc[-2]) if len(histogram) > 1 else 0
            if hist_val > 0 and prev_hist <= 0:
                result.macd_signal = "Bullish crossover"
                score += 25
            elif hist_val < 0 and prev_hist >= 0:
                result.macd_signal = "Bearish crossover"
                score += 0
            elif hist_val > 0:
                result.macd_signal = "Bullish"
                score += 18
            elif hist_val < 0:
                result.macd_signal = "Bearish"
                score += 5
            else:
                result.macd_signal = "Nøytral"
                score += 12
    except Exception as e:
        logger.error(f"MACD calculation error for {ticker}: {e}")

    # --- SMA (Moving Averages) ---
    try:
        sma_20 = close.rolling(window=20).mean()
        sma_50 = close.rolling(window=50).mean()
        sma_200 = close.rolling(window=200).mean() if len(close) >= 200 else None

        result.sma_20 = round(float(sma_20.iloc[-1]), 2) if not np.isnan(sma_20.iloc[-1]) else None
        result.sma_50 = round(float(sma_50.iloc[-1]), 2) if not np.isnan(sma_50.iloc[-1]) else None

        if sma_200 is not None and not np.isnan(sma_200.iloc[-1]):
            result.sma_200 = round(float(sma_200.iloc[-1]), 2)

        # Scoring: price vs moving averages
        if result.sma_50 and result.sma_200:
            if current_price > result.sma_50 > result.sma_200:
                result.sma_signal = "Golden Cross — sterk opptrend"
                score += 25
            elif current_price < result.sma_50 < result.sma_200:
                result.sma_signal = "Death Cross — sterk nedtrend"
                score += 0
            elif current_price > result.sma_50:
                result.sma_signal = "Over SMA50 — positiv trend"
                score += 18
            elif current_price > result.sma_200:
                result.sma_signal = "Over SMA200 — moderat"
                score += 12
            else:
                result.sma_signal = "Under glidende snitt"
                score += 5
        elif result.sma_50:
            if current_price > result.sma_50:
                result.sma_signal = "Over SMA50"
                score += 15
            else:
                result.sma_signal = "Under SMA50"
                score += 8
    except Exception as e:
        logger.error(f"SMA calculation error for {ticker}: {e}")

    # --- Volume ---
    try:
        vol_avg = volume.rolling(window=20).mean()
        vol_avg_val = float(vol_avg.iloc[-1])
        vol_current = float(volume.iloc[-1])

        if not np.isnan(vol_avg_val) and vol_avg_val > 0:
            result.volume_avg_20d = round(vol_avg_val, 0)
            result.volume_current = round(vol_current, 0)

            vol_ratio = vol_current / vol_avg_val
            price_change = (current_price - float(close.iloc[-2])) / float(close.iloc[-2]) if len(close) > 1 else 0

            if vol_ratio > 1.5 and price_change > 0:
                result.volume_signal = "Høyt volum + oppgang — bullish"
                score += 25
            elif vol_ratio > 1.5 and price_change < 0:
                result.volume_signal = "Høyt volum + nedgang — bearish"
                score += 5
            elif vol_ratio > 1.0:
                result.volume_signal = "Over gjennomsnitt"
                score += 15
            else:
                result.volume_signal = "Under gjennomsnitt"
                score += 10
    except Exception as e:
        logger.error(f"Volume calculation error for {ticker}: {e}")

    # --- Bollinger Bands ---
    try:
        upper, middle, lower = calculate_bollinger_bands(close)
        upper_val = float(upper.iloc[-1])
        middle_val = float(middle.iloc[-1])
        lower_val = float(lower.iloc[-1])

        if not any(np.isnan(v) for v in [upper_val, middle_val, lower_val]):
            result.bollinger_upper = round(upper_val, 2)
            result.bollinger_middle = round(middle_val, 2)
            result.bollinger_lower = round(lower_val, 2)

            if current_price <= lower_val:
                result.bollinger_signal = "Under nedre bånd — mulig oversolgt"
            elif current_price >= upper_val:
                result.bollinger_signal = "Over øvre bånd — mulig overkjøpt"
            else:
                result.bollinger_signal = "Innenfor båndene"
    except Exception as e:
        logger.error(f"Bollinger calculation error for {ticker}: {e}")

    result.technical_score = round(min(score, 100), 1)
    return result
