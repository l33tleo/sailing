"""AI Chat API — answer stock questions using the analysis engine.

Parses natural-language Norwegian/English questions, identifies tickers,
runs the appropriate analysis, and returns a conversational response.
No external LLM needed — uses rule-based NLP + our own scoring engine.
"""

import re
import logging
from fastapi import APIRouter
from pydantic import BaseModel

from app.services.stock_data import (
    get_stock_quote,
    get_key_metrics,
    STOCK_NAMES,
    POPULAR_NORWEGIAN_STOCKS,
    POPULAR_GLOBAL_STOCKS,
)
from app.services.recommender import generate_recommendation
from app.services.technical import analyze_technical
from app.services.fundamental import analyze_fundamental

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/chat", tags=["AI-chat"])

# Reverse map: name → ticker
NAME_TO_TICKER: dict[str, str] = {}
for ticker, name in STOCK_NAMES.items():
    NAME_TO_TICKER[name.lower()] = ticker
# Also add common aliases
NAME_TO_TICKER.update({
    "equinor": "EQNR.OL", "dnb": "DNB.OL", "telenor": "TEL.OL",
    "mowi": "MOWI.OL", "orkla": "ORK.OL", "yara": "YAR.OL",
    "aker bp": "AKRBP.OL", "norsk hydro": "NHY.OL", "hydro": "NHY.OL",
    "salmar": "SALM.OL", "subsea": "SUBC.OL", "subsea 7": "SUBC.OL",
    "storebrand": "STB.OL", "kongsberg": "KOG.OL", "bakkafrost": "BAKKA.OL",
    "frontline": "FRO.OL", "apple": "AAPL", "microsoft": "MSFT",
    "google": "GOOGL", "alphabet": "GOOGL", "amazon": "AMZN",
    "nvidia": "NVDA", "meta": "META", "facebook": "META",
    "tesla": "TSLA", "jpmorgan": "JPM", "visa": "V",
    "walmart": "WMT", "coca-cola": "KO", "coca cola": "KO",
})

ALL_TICKERS = set(POPULAR_NORWEGIAN_STOCKS + POPULAR_GLOBAL_STOCKS)


class ChatMessage(BaseModel):
    message: str


class ChatResponse(BaseModel):
    reply: str
    ticker: str | None = None
    recommendation: str | None = None
    score: float | None = None


# Common Norwegian/English words that look like tickers but aren't
STOP_WORDS = {
    "HVA", "OG", "ER", "DET", "EN", "ET", "FOR", "HAR", "MED", "SOM",
    "PÅ", "TIL", "AV", "OM", "KAN", "VIL", "BLE", "MEG", "JEG",
    "DEN", "DE", "VI", "MAN", "NÅ", "HER", "DA", "NÅR", "HOW",
    "THE", "AND", "FOR", "ARE", "BUT", "NOT", "YOU", "ALL", "CAN",
    "HER", "WAS", "ONE", "OUR", "OUT", "BUY", "GOD", "NEW", "OLD",
}


def _extract_ticker(text: str) -> str | None:
    """Extract a stock ticker or company name from user text."""
    text_lower = text.lower()

    # 1) Check for company names first (most reliable)
    for name, ticker in sorted(NAME_TO_TICKER.items(), key=lambda x: -len(x[0])):
        if name in text_lower:
            return ticker

    # 2) Check for explicit tickers (AAPL, EQNR.OL, etc.)
    ticker_match = re.findall(r'\b([A-Z]{1,5}(?:\.OL)?)\b', text.upper())
    for candidate in ticker_match:
        if candidate in STOP_WORDS:
            continue
        if candidate in ALL_TICKERS:
            return candidate
        if f"{candidate}.OL" in ALL_TICKERS:
            return f"{candidate}.OL"

    # 3) Try remaining uppercase words as tickers (skip stop words)
    for candidate in ticker_match:
        if candidate in STOP_WORDS:
            continue
        if len(candidate) >= 2:
            return candidate

    return None


def _detect_intent(text: str) -> str:
    """Detect what the user is asking about."""
    text_lower = text.lower()

    buy_keywords = ["kjøpe", "kjøp", "buy", "bør jeg", "lønner", "investere", "anbefal"]
    sell_keywords = ["selge", "selg", "sell", "kvitte", "dumpe"]
    compare_keywords = ["sammenlign", "versus", "vs", "eller", "bedre enn", "compare"]
    price_keywords = ["kurs", "pris", "price", "hva koster", "hvor mye"]
    technical_keywords = ["rsi", "macd", "teknisk", "sma", "bollinger", "technical"]
    fundamental_keywords = ["fundamental", "p/e", "pe", "gjeld", "utbytte", "dividend", "vekst"]
    news_keywords = ["nyheter", "news", "hva skjer", "hendelser"]

    if any(k in text_lower for k in compare_keywords):
        return "compare"
    if any(k in text_lower for k in buy_keywords):
        return "recommendation"
    if any(k in text_lower for k in sell_keywords):
        return "recommendation"
    if any(k in text_lower for k in technical_keywords):
        return "technical"
    if any(k in text_lower for k in fundamental_keywords):
        return "fundamental"
    if any(k in text_lower for k in price_keywords):
        return "price"
    if any(k in text_lower for k in news_keywords):
        return "news_hint"

    # Default: full recommendation
    return "recommendation"


@router.post("", response_model=ChatResponse)
async def chat(msg: ChatMessage):
    """Svar på spørsmål om aksjer med analyse og anbefalinger.
    
    Eksempler:
    - "Bør jeg kjøpe Equinor?"
    - "Hva er kursen på AAPL?"
    - "Analyser NVDA teknisk"
    - "Er DNB fundamental god?"
    """
    text = msg.message.strip()
    if not text:
        return ChatResponse(reply="Skriv et spørsmål om en aksje, f.eks. 'Bør jeg kjøpe Equinor?'")

    ticker = _extract_ticker(text)
    intent = _detect_intent(text)

    if not ticker:
        return ChatResponse(
            reply="Jeg fant ikke en aksje i spørsmålet ditt. Prøv å nevne en ticker "
                  "(f.eks. AAPL, EQNR.OL) eller et selskapsnavn (f.eks. Equinor, Apple).\n\n"
                  "Eksempler:\n"
                  "• «Bør jeg kjøpe Equinor?»\n"
                  "• «Hva er RSI for AAPL?»\n"
                  "• «Analyser Microsoft fundamentalt»"
        )

    # --- Price query ---
    if intent == "price":
        quote = get_stock_quote(ticker)
        if not quote:
            return ChatResponse(reply=f"Beklager, fant ingen kursdata for {ticker}.", ticker=ticker)
        direction = "opp" if quote.change >= 0 else "ned"
        return ChatResponse(
            reply=f"📊 **{quote.name}** ({ticker})\n\n"
                  f"Kurs: **{quote.price}** {quote.currency or ''}\n"
                  f"Endring i dag: {'+' if quote.change >= 0 else ''}{quote.change} ({'+' if quote.change_percent >= 0 else ''}{quote.change_percent:.2f}%) — {direction}\n"
                  f"Volum: {quote.volume:,}\n"
                  f"52-ukers høy/lav: {quote.fifty_two_week_high} / {quote.fifty_two_week_low}",
            ticker=ticker,
        )

    # --- Technical analysis ---
    if intent == "technical":
        tech = analyze_technical(ticker)
        if tech.current_price is None:
            return ChatResponse(reply=f"Ikke nok data for teknisk analyse av {ticker}.", ticker=ticker)

        signals = []
        if tech.rsi_signal:
            signals.append(f"• RSI ({tech.rsi:.0f}): {tech.rsi_signal}")
        if tech.macd_signal:
            signals.append(f"• MACD: {tech.macd_signal}")
        if tech.sma_signal:
            signals.append(f"• Glidende snitt: {tech.sma_signal}")
        if tech.volume_signal:
            signals.append(f"• Volum: {tech.volume_signal}")
        if tech.bollinger_signal:
            signals.append(f"• Bollinger: {tech.bollinger_signal}")

        verdict = "positivt" if tech.technical_score >= 60 else "nøytralt" if tech.technical_score >= 40 else "negativt"

        return ChatResponse(
            reply=f"📈 **Teknisk analyse av {ticker}**\n"
                  f"Score: **{tech.technical_score}/100** — bildet er {verdict}\n\n"
                  + "\n".join(signals),
            ticker=ticker,
            score=tech.technical_score,
        )

    # --- Fundamental analysis ---
    if intent == "fundamental":
        fund = analyze_fundamental(ticker)

        details = []
        if fund.pe_detail:
            details.append(f"• {fund.pe_detail}")
        if fund.debt_detail:
            details.append(f"• {fund.debt_detail}")
        if fund.growth_detail:
            details.append(f"• {fund.growth_detail}")
        if fund.dividend_detail:
            details.append(f"• {fund.dividend_detail}")
        if fund.cashflow_detail:
            details.append(f"• {fund.cashflow_detail}")

        verdict = "sterk" if fund.fundamental_score >= 60 else "moderat" if fund.fundamental_score >= 40 else "svak"

        return ChatResponse(
            reply=f"🏢 **Fundamental analyse av {ticker}**\n"
                  f"Score: **{fund.fundamental_score}/100** — {verdict} fundamental profil\n\n"
                  + "\n".join(details),
            ticker=ticker,
            score=fund.fundamental_score,
        )

    # --- News hint ---
    if intent == "news_hint":
        return ChatResponse(
            reply=f"📰 For nyheter om {ticker}, gå til aksjesiden:\n"
                  f"/aksjer/{ticker}\n\n"
                  f"Der finner du de siste nyhetene fra Yahoo Finance.",
            ticker=ticker,
        )

    # --- Full recommendation (default) ---
    analysis = generate_recommendation(ticker)
    if not analysis:
        return ChatResponse(reply=f"Beklager, jeg klarte ikke å analysere {ticker}. Sjekk at tickeren er riktig.", ticker=ticker)

    emoji = {"KJØP": "🟢", "HOLD": "🟡", "SELG": "🔴"}.get(analysis.recommendation_type, "⚪")

    reply_parts = [
        f"{emoji} **Min anbefaling for {analysis.name} ({ticker}): {analysis.recommendation_type}**",
        f"Samlet score: **{analysis.combined_score}/100** (konfidens: {analysis.confidence}%)",
        "",
        f"📈 Teknisk score: {analysis.technical.technical_score}/100",
    ]

    if analysis.technical.rsi_signal:
        reply_parts.append(f"   RSI ({analysis.technical.rsi:.0f}): {analysis.technical.rsi_signal}")
    if analysis.technical.macd_signal:
        reply_parts.append(f"   MACD: {analysis.technical.macd_signal}")
    if analysis.technical.sma_signal:
        reply_parts.append(f"   Trend: {analysis.technical.sma_signal}")

    reply_parts.append(f"\n🏢 Fundamental score: {analysis.fundamental.fundamental_score}/100")
    if analysis.fundamental.pe_detail:
        reply_parts.append(f"   {analysis.fundamental.pe_detail}")
    if analysis.fundamental.growth_detail:
        reply_parts.append(f"   {analysis.fundamental.growth_detail}")
    if analysis.fundamental.dividend_detail:
        reply_parts.append(f"   {analysis.fundamental.dividend_detail}")

    reply_parts.append(f"\n💰 Kurs nå: {analysis.current_price} — ")
    if analysis.recommendation_type == "KJØP":
        reply_parts[-1] += "analysen tilsier at aksjen er attraktivt priset."
    elif analysis.recommendation_type == "SELG":
        reply_parts[-1] += "analysen tilsier at aksjen er overpriset eller i nedtrend."
    else:
        reply_parts[-1] += "analysen tilsier at det er best å avvente."

    return ChatResponse(
        reply="\n".join(reply_parts),
        ticker=ticker,
        recommendation=analysis.recommendation_type,
        score=analysis.combined_score,
    )
