"""Fundamental analysis service — evaluates company financials and assigns scores."""

import logging
from app.schemas.recommendation import FundamentalAnalysisResult
from app.services.stock_data import get_ticker_info

logger = logging.getLogger(__name__)


def analyze_fundamental(ticker: str) -> FundamentalAnalysisResult:
    """Perform fundamental analysis on a stock.
    
    Scores 5 categories, each 0-20 points, total 0-100.
    """
    result = FundamentalAnalysisResult(ticker=ticker)
    info = get_ticker_info(ticker)

    if not info:
        logger.warning(f"No info available for fundamental analysis of {ticker}")
        return result

    total_score = 0.0

    # --- 1. P/E Ratio (0-20) ---
    try:
        pe = info.get("trailingPE")
        forward_pe = info.get("forwardPE")
        sector = info.get("sector", "Unknown")

        if pe is not None and pe > 0:
            if pe < 10:
                result.pe_score = 20
                result.pe_detail = f"P/E {pe:.1f} — svært lavt verdsatt"
            elif pe < 15:
                result.pe_score = 16
                result.pe_detail = f"P/E {pe:.1f} — rimelig priset"
            elif pe < 20:
                result.pe_score = 12
                result.pe_detail = f"P/E {pe:.1f} — moderat verdsatt"
            elif pe < 30:
                result.pe_score = 8
                result.pe_detail = f"P/E {pe:.1f} — relativt dyrt"
            else:
                result.pe_score = 4
                result.pe_detail = f"P/E {pe:.1f} — høyt verdsatt"

            # Bonus if forward P/E shows improvement
            if forward_pe and forward_pe < pe:
                result.pe_score = min(20, result.pe_score + 2)
                result.pe_detail += f" (forward P/E {forward_pe:.1f} — forbedring forventet)"
        else:
            result.pe_score = 5
            result.pe_detail = "P/E ikke tilgjengelig (negativt resultat?)"

        total_score += result.pe_score
    except Exception as e:
        logger.error(f"P/E analysis error for {ticker}: {e}")

    # --- 2. Debt / Gjeldsgrad (0-20) ---
    try:
        dte = info.get("debtToEquity")
        current_ratio = info.get("currentRatio")

        if dte is not None:
            if dte < 30:
                result.debt_score = 20
                result.debt_detail = f"D/E {dte:.0f}% — svært lav gjeld"
            elif dte < 60:
                result.debt_score = 16
                result.debt_detail = f"D/E {dte:.0f}% — moderat gjeld"
            elif dte < 100:
                result.debt_score = 12
                result.debt_detail = f"D/E {dte:.0f}% — akseptabel gjeld"
            elif dte < 200:
                result.debt_score = 8
                result.debt_detail = f"D/E {dte:.0f}% — høy gjeld"
            else:
                result.debt_score = 4
                result.debt_detail = f"D/E {dte:.0f}% — svært høy gjeld"

            if current_ratio and current_ratio > 1.5:
                result.debt_score = min(20, result.debt_score + 2)
                result.debt_detail += f" (likviditetsgrad {current_ratio:.1f} — god)"
        else:
            result.debt_score = 10
            result.debt_detail = "Gjeldsgrad ikke tilgjengelig"

        total_score += result.debt_score
    except Exception as e:
        logger.error(f"Debt analysis error for {ticker}: {e}")

    # --- 3. Revenue/Earnings Growth (0-20) ---
    try:
        rev_growth = info.get("revenueGrowth")
        earn_growth = info.get("earningsGrowth")

        if rev_growth is not None:
            rev_pct = rev_growth * 100
            if rev_pct > 20:
                result.growth_score = 18
                result.growth_detail = f"Inntektsvekst {rev_pct:.1f}% — sterk vekst"
            elif rev_pct > 10:
                result.growth_score = 15
                result.growth_detail = f"Inntektsvekst {rev_pct:.1f}% — god vekst"
            elif rev_pct > 0:
                result.growth_score = 12
                result.growth_detail = f"Inntektsvekst {rev_pct:.1f}% — moderat vekst"
            elif rev_pct > -10:
                result.growth_score = 8
                result.growth_detail = f"Inntektsvekst {rev_pct:.1f}% — svak nedgang"
            else:
                result.growth_score = 4
                result.growth_detail = f"Inntektsvekst {rev_pct:.1f}% — sterk nedgang"

            # Bonus for earnings growth
            if earn_growth is not None and earn_growth > 0:
                result.growth_score = min(20, result.growth_score + 2)
                result.growth_detail += f" (resultatvekst {earn_growth*100:.1f}%)"
        else:
            result.growth_score = 10
            result.growth_detail = "Vekstdata ikke tilgjengelig"

        total_score += result.growth_score
    except Exception as e:
        logger.error(f"Growth analysis error for {ticker}: {e}")

    # --- 4. Dividend (0-20) ---
    try:
        div_yield = info.get("dividendYield")
        payout_ratio = info.get("payoutRatio")

        if div_yield is not None and div_yield > 0:
            # yfinance inconsistency: some tickers return 0.041 (=4.1%), others 5.6 (=5.6%)
            # If the value is > 1, it's already a percentage; if < 1, multiply by 100
            div_pct = div_yield if div_yield > 1 else div_yield * 100
            if div_pct > 5:
                result.dividend_score = 18
                result.dividend_detail = f"Utbytterate {div_pct:.1f}% — svært høyt utbytte"
            elif div_pct > 3:
                result.dividend_score = 16
                result.dividend_detail = f"Utbytterate {div_pct:.1f}% — godt utbytte"
            elif div_pct > 1.5:
                result.dividend_score = 12
                result.dividend_detail = f"Utbytterate {div_pct:.1f}% — moderat utbytte"
            elif div_pct > 0:
                result.dividend_score = 8
                result.dividend_detail = f"Utbytterate {div_pct:.1f}% — lavt utbytte"

            # Check payout sustainability
            if payout_ratio is not None:
                if payout_ratio < 0.6:
                    result.dividend_score = min(20, result.dividend_score + 2)
                    result.dividend_detail += f" (utbetalingsandel {payout_ratio*100:.0f}% — bærekraftig)"
                elif payout_ratio > 0.9:
                    result.dividend_score = max(0, result.dividend_score - 4)
                    result.dividend_detail += f" (utbetalingsandel {payout_ratio*100:.0f}% — lite bærekraftig)"
        else:
            result.dividend_score = 8
            result.dividend_detail = "Ingen utbytte"

        total_score += result.dividend_score
    except Exception as e:
        logger.error(f"Dividend analysis error for {ticker}: {e}")

    # --- 5. Free Cash Flow (0-20) ---
    try:
        fcf = info.get("freeCashflow")
        market_cap = info.get("marketCap")

        if fcf is not None and market_cap and market_cap > 0:
            fcf_yield = (fcf / market_cap) * 100
            if fcf > 0 and fcf_yield > 8:
                result.cashflow_score = 20
                result.cashflow_detail = f"FCF yield {fcf_yield:.1f}% — svært attraktivt"
            elif fcf > 0 and fcf_yield > 5:
                result.cashflow_score = 16
                result.cashflow_detail = f"FCF yield {fcf_yield:.1f}% — godt"
            elif fcf > 0 and fcf_yield > 2:
                result.cashflow_score = 12
                result.cashflow_detail = f"FCF yield {fcf_yield:.1f}% — moderat"
            elif fcf > 0:
                result.cashflow_score = 8
                result.cashflow_detail = f"FCF yield {fcf_yield:.1f}% — lavt"
            else:
                result.cashflow_score = 4
                result.cashflow_detail = "Negativ fri kontantstrøm"
        else:
            result.cashflow_score = 10
            result.cashflow_detail = "Kontantstrøm-data ikke tilgjengelig"

        total_score += result.cashflow_score
    except Exception as e:
        logger.error(f"FCF analysis error for {ticker}: {e}")

    result.fundamental_score = round(total_score, 1)
    return result
