/**
 * API client for the AksjeAnalyse backend.
 * All requests are proxied through Next.js rewrites to avoid CORS issues.
 */

const API_BASE = "/api";

async function fetchAPI<T>(path: string, options?: RequestInit): Promise<T> {
  const res = await fetch(`${API_BASE}${path}`, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...options?.headers,
    },
  });

  if (!res.ok) {
    const error = await res.json().catch(() => ({ detail: res.statusText }));
    throw new Error(error.detail || `API error: ${res.status}`);
  }

  return res.json();
}

// --- Stock endpoints ---

export interface StockSearchResult {
  ticker: string;
  name: string;
  sector: string | null;
  market: string | null;
  current_price: number | null;
  currency: string | null;
}

export interface StockQuote {
  ticker: string;
  name: string;
  price: number;
  change: number;
  change_percent: number;
  volume: number;
  market_cap: number | null;
  pe_ratio: number | null;
  dividend_yield: number | null;
  fifty_two_week_high: number | null;
  fifty_two_week_low: number | null;
  currency: string | null;
}

export interface StockPriceHistory {
  dates: string[];
  open: number[];
  high: number[];
  low: number[];
  close: number[];
  volume: number[];
}

export interface StockKeyMetrics {
  ticker: string;
  pe_ratio: number | null;
  forward_pe: number | null;
  pb_ratio: number | null;
  ps_ratio: number | null;
  ev_ebitda: number | null;
  debt_to_equity: number | null;
  current_ratio: number | null;
  roe: number | null;
  revenue_growth: number | null;
  earnings_growth: number | null;
  dividend_yield: number | null;
  payout_ratio: number | null;
  free_cash_flow: number | null;
  market_cap: number | null;
  beta: number | null;
}

export async function searchStocks(q: string): Promise<StockSearchResult[]> {
  return fetchAPI(`/stocks/search?q=${encodeURIComponent(q)}`);
}

export async function getPopularStocks(market = "all"): Promise<StockSearchResult[]> {
  return fetchAPI(`/stocks/popular?market=${market}`);
}

export async function getStockQuote(ticker: string): Promise<StockQuote> {
  return fetchAPI(`/stocks/${encodeURIComponent(ticker)}/quote`);
}

export async function getStockHistory(
  ticker: string,
  period = "1y",
  interval = "1d"
): Promise<StockPriceHistory> {
  return fetchAPI(
    `/stocks/${encodeURIComponent(ticker)}/history?period=${period}&interval=${interval}`
  );
}

export async function getStockMetrics(ticker: string): Promise<StockKeyMetrics> {
  return fetchAPI(`/stocks/${encodeURIComponent(ticker)}/metrics`);
}

// --- Analysis endpoints ---

export interface TechnicalAnalysis {
  ticker: string;
  rsi: number | null;
  rsi_signal: string | null;
  macd: number | null;
  macd_signal_line: number | null;
  macd_histogram: number | null;
  macd_signal: string | null;
  sma_20: number | null;
  sma_50: number | null;
  sma_200: number | null;
  sma_signal: string | null;
  bollinger_upper: number | null;
  bollinger_middle: number | null;
  bollinger_lower: number | null;
  bollinger_signal: string | null;
  current_price: number | null;
  volume_avg_20d: number | null;
  volume_current: number | null;
  volume_signal: string | null;
  technical_score: number;
}

export interface FundamentalAnalysis {
  ticker: string;
  pe_score: number;
  pe_detail: string;
  debt_score: number;
  debt_detail: string;
  growth_score: number;
  growth_detail: string;
  dividend_score: number;
  dividend_detail: string;
  cashflow_score: number;
  cashflow_detail: string;
  fundamental_score: number;
}

export interface FullAnalysis {
  ticker: string;
  name: string;
  current_price: number | null;
  technical: TechnicalAnalysis;
  fundamental: FundamentalAnalysis;
  recommendation_type: string;
  confidence: number;
  combined_score: number;
  reasoning: string;
}

export async function getTechnicalAnalysis(ticker: string): Promise<TechnicalAnalysis> {
  return fetchAPI(`/analysis/${encodeURIComponent(ticker)}/technical`);
}

export async function getFundamentalAnalysis(ticker: string): Promise<FundamentalAnalysis> {
  return fetchAPI(`/analysis/${encodeURIComponent(ticker)}/fundamental`);
}

export async function getFullAnalysis(ticker: string): Promise<FullAnalysis> {
  return fetchAPI(`/analysis/${encodeURIComponent(ticker)}/full`);
}

// --- Recommendation endpoints ---

export interface Recommendation {
  id: number;
  stock_ticker: string;
  stock_name: string;
  recommendation_type: string;
  confidence: number;
  price_at_recommendation: number;
  current_price: number | null;
  technical_score: number;
  fundamental_score: number;
  combined_score: number;
  reasoning: string;
  created_at: string;
  price_after_7d: number | null;
  price_after_30d: number | null;
  price_after_90d: number | null;
  return_7d_pct: number | null;
  return_30d_pct: number | null;
  return_90d_pct: number | null;
  was_correct_7d: boolean | null;
  was_correct_30d: boolean | null;
  was_correct_90d: boolean | null;
}

export interface ScorecardStats {
  total_recommendations: number;
  correct_7d: number;
  correct_30d: number;
  correct_90d: number;
  accuracy_7d: number;
  accuracy_30d: number;
  accuracy_90d: number;
  avg_return_7d: number;
  avg_return_30d: number;
  avg_return_90d: number;
  best_recommendation: Recommendation | null;
  worst_recommendation: Recommendation | null;
  recommendations_by_type: Record<string, number>;
}

export async function generateRecommendation(ticker: string): Promise<Recommendation> {
  return fetchAPI("/recommendations/generate", {
    method: "POST",
    body: JSON.stringify({ ticker }),
  });
}

export async function getRecommendations(params?: {
  ticker?: string;
  rec_type?: string;
  limit?: number;
  offset?: number;
}): Promise<Recommendation[]> {
  const searchParams = new URLSearchParams();
  if (params?.ticker) searchParams.set("ticker", params.ticker);
  if (params?.rec_type) searchParams.set("rec_type", params.rec_type);
  if (params?.limit) searchParams.set("limit", String(params.limit));
  if (params?.offset) searchParams.set("offset", String(params.offset));
  const qs = searchParams.toString();
  return fetchAPI(`/recommendations/${qs ? `?${qs}` : ""}`);
}

export async function getScorecard(): Promise<ScorecardStats> {
  return fetchAPI("/recommendations/scorecard");
}

export async function runEvaluation(): Promise<{ message: string; updated_count: number }> {
  return fetchAPI("/recommendations/evaluate", { method: "POST" });
}

// --- Portfolio endpoints ---

export interface Portfolio {
  id: number;
  name: string;
  total_value: number;
  total_cost: number;
  total_return: number;
  total_return_pct: number;
  holdings_count: number;
  created_at: string;
}

export interface Holding {
  id: number;
  ticker: string;
  name: string;
  shares: number;
  buy_price: number;
  buy_date: string;
  current_price: number | null;
  current_value: number;
  cost_basis: number;
  return_value: number;
  return_pct: number;
}

export async function createPortfolio(name = "Min portefølje"): Promise<Portfolio> {
  return fetchAPI("/portfolios", {
    method: "POST",
    body: JSON.stringify({ name }),
  });
}

export async function getPortfolios(): Promise<Portfolio[]> {
  return fetchAPI("/portfolios");
}

export async function addHolding(
  portfolioId: number,
  data: { ticker: string; shares: number; buy_price: number; buy_date: string }
): Promise<Holding> {
  return fetchAPI(`/portfolios/${portfolioId}/holdings`, {
    method: "POST",
    body: JSON.stringify(data),
  });
}

export async function getHoldings(portfolioId: number): Promise<Holding[]> {
  return fetchAPI(`/portfolios/${portfolioId}/holdings`);
}

export async function removeHolding(portfolioId: number, holdingId: number): Promise<void> {
  return fetchAPI(`/portfolios/${portfolioId}/holdings/${holdingId}`, {
    method: "DELETE",
  });
}

// --- Watchlist endpoints ---

export interface WatchlistItem {
  id: number;
  ticker: string;
  name: string;
  current_price: number | null;
  change_pct: number | null;
  added_at: string;
}

export async function addToWatchlist(ticker: string): Promise<WatchlistItem> {
  return fetchAPI("/watchlist", {
    method: "POST",
    body: JSON.stringify({ ticker }),
  });
}

export async function getWatchlist(): Promise<WatchlistItem[]> {
  return fetchAPI("/watchlist");
}

export async function removeFromWatchlist(id: number): Promise<void> {
  return fetchAPI(`/watchlist/${id}`, { method: "DELETE" });
}

// --- Compare endpoints ---

export interface ComparisonStock {
  ticker: string;
  name: string;
  price: number | null;
  change_percent: number | null;
  currency: string | null;
  market_cap: number | null;
  pe_ratio: number | null;
  forward_pe: number | null;
  pb_ratio: number | null;
  ev_ebitda: number | null;
  debt_to_equity: number | null;
  revenue_growth: number | null;
  dividend_yield: number | null;
  roe: number | null;
  beta: number | null;
  free_cash_flow: number | null;
  technical_score: number;
  fundamental_score: number;
  combined_score: number;
  recommendation: string;
  rsi: number | null;
  sma_signal: string | null;
  macd_signal: string | null;
  price_history_dates: string[];
  price_history_close: number[];
}

export async function compareStocks(tickers: string[]): Promise<{ stocks: ComparisonStock[] }> {
  return fetchAPI(`/compare/?tickers=${tickers.join(",")}`);
}

// --- News endpoints ---

export interface NewsItem {
  title: string;
  publisher: string;
  link: string;
  published: string;
  thumbnail: string | null;
  related_tickers: string[];
}

export async function getStockNews(ticker: string, limit = 10): Promise<NewsItem[]> {
  return fetchAPI(`/news/${encodeURIComponent(ticker)}?limit=${limit}`);
}

// --- Market endpoints ---

export interface MarketIndex {
  name: string;
  ticker: string;
  price: number;
  change: number;
  change_percent: number;
  currency: string | null;
}

export async function getMarketIndices(): Promise<{ indices: MarketIndex[] }> {
  return fetchAPI("/market/indices");
}

// --- Alert endpoints ---

export interface AlertItem {
  id: number;
  ticker: string;
  stock_name: string;
  condition_type: string;
  threshold: number;
  is_active: boolean;
  last_triggered: string | null;
  created_at: string;
}

export interface AlertCheckResult {
  alert_id: number;
  ticker: string;
  condition_type: string;
  threshold: number;
  current_value: number;
  triggered: boolean;
  message: string;
}

export async function createAlert(data: {
  ticker: string;
  condition_type: string;
  threshold: number;
}): Promise<AlertItem> {
  return fetchAPI("/alerts", { method: "POST", body: JSON.stringify(data) });
}

export async function getAlerts(activeOnly = true): Promise<AlertItem[]> {
  return fetchAPI(`/alerts?active_only=${activeOnly}`);
}

export async function deleteAlert(id: number): Promise<void> {
  return fetchAPI(`/alerts/${id}`, { method: "DELETE" });
}

export async function checkAlerts(): Promise<AlertCheckResult[]> {
  return fetchAPI("/alerts/check", { method: "POST" });
}

// --- Export ---

export function getExportCsvUrl(): string {
  return `${API_BASE}/export/recommendations/csv`;
}

// --- Paper Trading endpoints ---

export interface PaperTrade {
  id: number;
  ticker: string;
  stock_name: string;
  trade_type: string;
  shares: number;
  entry_price: number;
  exit_price: number | null;
  current_price: number | null;
  is_open: boolean;
  pnl: number | null;
  pnl_pct: number | null;
  unrealized_pnl: number | null;
  unrealized_pnl_pct: number | null;
  note: string | null;
  opened_at: string;
  closed_at: string | null;
}

export interface PaperTradeSummary {
  total_trades: number;
  open_trades: number;
  closed_trades: number;
  total_realized_pnl: number;
  total_unrealized_pnl: number;
  win_rate: number;
  avg_pnl_pct: number;
  best_trade_pnl_pct: number | null;
  worst_trade_pnl_pct: number | null;
  starting_capital: number;
  current_equity: number;
}

export async function openPaperTrade(data: {
  ticker: string;
  trade_type: string;
  shares: number;
  note?: string;
}): Promise<PaperTrade> {
  return fetchAPI("/paper-trades", { method: "POST", body: JSON.stringify(data) });
}

export async function closePaperTrade(
  tradeId: number,
  note?: string
): Promise<PaperTrade> {
  return fetchAPI(`/paper-trades/${tradeId}/close`, {
    method: "POST",
    body: JSON.stringify({ note }),
  });
}

export async function getPaperTrades(openOnly = false): Promise<PaperTrade[]> {
  return fetchAPI(`/paper-trades?open_only=${openOnly}`);
}

export async function getPaperTradeSummary(): Promise<PaperTradeSummary> {
  return fetchAPI("/paper-trades/summary");
}

export async function deletePaperTrade(id: number): Promise<void> {
  return fetchAPI(`/paper-trades/${id}`, { method: "DELETE" });
}

// --- Sector endpoints ---

export interface SectorStock {
  ticker: string;
  name: string;
  price: number | null;
  change_percent: number | null;
}

export interface SectorData {
  name: string;
  avg_change_pct: number;
  stock_count: number;
  stocks: SectorStock[];
}

export async function getSectors(): Promise<{ sectors: SectorData[] }> {
  return fetchAPI("/sectors");
}
