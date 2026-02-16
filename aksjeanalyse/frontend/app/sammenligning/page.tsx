"use client";

import { useState } from "react";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { RecBadge } from "@/components/ui/Badge";
import { ScoreGauge } from "@/components/ui/ScoreGauge";
import { LoadingSpinner } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatPercent, formatLargeNumber } from "@/lib/utils";
import { compareStocks, type ComparisonStock } from "@/lib/api";
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
  CartesianGrid,
  Legend,
} from "recharts";
import { GitCompareArrows, Search } from "lucide-react";

const CHART_COLORS = ["#3b82f6", "#10b981", "#f59e0b", "#ef4444", "#8b5cf6"];

export default function SammenligningPage() {
  const [tickerInput, setTickerInput] = useState("AAPL, MSFT, GOOGL");
  const [stocks, setStocks] = useState<ComparisonStock[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function handleCompare(e: React.FormEvent) {
    e.preventDefault();
    const tickers = tickerInput
      .split(",")
      .map((t) => t.trim().toUpperCase())
      .filter(Boolean);

    if (tickers.length < 2) {
      setError("Minst 2 aksjer kreves for sammenligning");
      return;
    }

    setLoading(true);
    setError(null);
    try {
      const result = await compareStocks(tickers);
      setStocks(result.stocks);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  // Build normalized price chart data (base 100)
  const chartData = buildNormalizedChartData(stocks);

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-white">Sammenligning</h1>
        <p className="text-slate-400">
          Sammenlign aksjer side-om-side på nøkkeltall, analyse og kursutvikling
        </p>
      </div>

      {/* Input */}
      <Card>
        <form onSubmit={handleCompare} className="flex gap-3">
          <div className="relative flex-1">
            <Search className="absolute left-3 top-1/2 h-5 w-5 -translate-y-1/2 text-slate-500" />
            <input
              type="text"
              value={tickerInput}
              onChange={(e) => setTickerInput(e.target.value)}
              placeholder="Skriv inn 2-5 tickers, kommaseparert (f.eks. AAPL, MSFT, GOOGL)"
              className="w-full rounded-lg border border-slate-700 bg-slate-800/50 py-3 pl-10 pr-4 text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none focus:ring-1 focus:ring-blue-500"
            />
          </div>
          <button
            type="submit"
            disabled={loading}
            className="flex items-center gap-2 rounded-lg bg-blue-600 px-6 py-3 font-medium text-white transition-colors hover:bg-blue-500 disabled:opacity-50"
          >
            <GitCompareArrows className="h-4 w-4" />
            {loading ? "Analyserer..." : "Sammenlign"}
          </button>
        </form>
      </Card>

      {error && (
        <Card className="border-red-500/30 bg-red-500/10">
          <p className="text-sm text-red-400">❌ {error}</p>
        </Card>
      )}

      {loading && <LoadingSpinner />}

      {stocks.length >= 2 && (
        <>
          {/* Score overview */}
          <div className="grid grid-cols-2 gap-4 md:grid-cols-3 lg:grid-cols-5">
            {stocks.map((s, i) => (
              <Card key={s.ticker} className="flex flex-col items-center py-4">
                <p className="text-sm font-semibold text-white">{s.ticker}</p>
                <p className="mb-2 text-xs text-slate-500">{s.name}</p>
                <ScoreGauge
                  score={s.combined_score}
                  label=""
                  size="md"
                />
                <div className="mt-2">
                  <RecBadge type={s.recommendation} />
                </div>
              </Card>
            ))}
          </div>

          {/* Normalized price chart */}
          {chartData.length > 0 && (
            <Card>
              <CardHeader>
                <CardTitle>Kursutvikling (normalisert, base 100)</CardTitle>
                <CardDescription>
                  Sammenligner relativ prisutvikling de siste 6 månedene
                </CardDescription>
              </CardHeader>
              <ResponsiveContainer width="100%" height={350}>
                <LineChart data={chartData}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                  <XAxis
                    dataKey="date"
                    tick={{ fill: "#64748b", fontSize: 11 }}
                    tickFormatter={(v) => {
                      const d = new Date(v);
                      return `${d.getDate()}.${d.getMonth() + 1}`;
                    }}
                    minTickGap={40}
                  />
                  <YAxis
                    tick={{ fill: "#64748b", fontSize: 11 }}
                    domain={["auto", "auto"]}
                  />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: "#1e293b",
                      border: "1px solid #334155",
                      borderRadius: "8px",
                      color: "#e2e8f0",
                    }}
                    formatter={(v: number) => `${v.toFixed(1)}`}
                  />
                  <Legend />
                  {stocks.map((s, i) => (
                    <Line
                      key={s.ticker}
                      type="monotone"
                      dataKey={s.ticker}
                      stroke={CHART_COLORS[i % CHART_COLORS.length]}
                      strokeWidth={2}
                      dot={false}
                    />
                  ))}
                </LineChart>
              </ResponsiveContainer>
            </Card>
          )}

          {/* Comparison table */}
          <Card>
            <CardHeader>
              <CardTitle>Nøkkeltall-sammenligning</CardTitle>
            </CardHeader>
            <div className="overflow-x-auto">
              <table className="w-full text-sm">
                <thead>
                  <tr className="border-b border-slate-800 text-left">
                    <th className="pb-3 pr-4 text-xs text-slate-500">Metrikk</th>
                    {stocks.map((s) => (
                      <th
                        key={s.ticker}
                        className="pb-3 pr-4 text-xs font-semibold text-white"
                      >
                        {s.ticker}
                      </th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  <CompRow label="Kurs" stocks={stocks} getValue={(s) => s.price != null ? formatNumber(s.price) : "–"} />
                  <CompRow label="Endring %" stocks={stocks} getValue={(s) => s.change_percent != null ? formatPercent(s.change_percent) : "–"} colorize />
                  <CompRow label="Anbefaling" stocks={stocks} getValue={(s) => s.recommendation} badge />
                  <CompRow label="Samlet score" stocks={stocks} getValue={(s) => `${s.combined_score}/100`} />
                  <CompRow label="Teknisk score" stocks={stocks} getValue={(s) => `${s.technical_score}/100`} />
                  <CompRow label="Fundamental score" stocks={stocks} getValue={(s) => `${s.fundamental_score}/100`} />
                  <CompRow label="Markedsverdi" stocks={stocks} getValue={(s) => formatLargeNumber(s.market_cap)} />
                  <CompRow label="P/E" stocks={stocks} getValue={(s) => s.pe_ratio != null ? formatNumber(s.pe_ratio) : "–"} />
                  <CompRow label="Forward P/E" stocks={stocks} getValue={(s) => s.forward_pe != null ? formatNumber(s.forward_pe) : "–"} />
                  <CompRow label="P/B" stocks={stocks} getValue={(s) => s.pb_ratio != null ? formatNumber(s.pb_ratio) : "–"} />
                  <CompRow label="EV/EBITDA" stocks={stocks} getValue={(s) => s.ev_ebitda != null ? formatNumber(s.ev_ebitda) : "–"} />
                  <CompRow label="Gjeld/EK" stocks={stocks} getValue={(s) => s.debt_to_equity != null ? `${formatNumber(s.debt_to_equity)}%` : "–"} />
                  <CompRow label="Inntektsvekst" stocks={stocks} getValue={(s) => s.revenue_growth != null ? formatPercent(s.revenue_growth * 100) : "–"} colorize />
                  <CompRow label="Utbytte %" stocks={stocks} getValue={(s) => s.dividend_yield != null ? `${(s.dividend_yield > 1 ? s.dividend_yield : s.dividend_yield * 100).toFixed(2)}%` : "–"} />
                  <CompRow label="ROE" stocks={stocks} getValue={(s) => s.roe != null ? formatPercent(s.roe * 100) : "–"} />
                  <CompRow label="Beta" stocks={stocks} getValue={(s) => s.beta != null ? formatNumber(s.beta) : "–"} />
                  <CompRow label="RSI" stocks={stocks} getValue={(s) => s.rsi != null ? formatNumber(s.rsi) : "–"} />
                  <CompRow label="SMA-signal" stocks={stocks} getValue={(s) => s.sma_signal || "–"} />
                  <CompRow label="MACD-signal" stocks={stocks} getValue={(s) => s.macd_signal || "–"} />
                </tbody>
              </table>
            </div>
          </Card>
        </>
      )}
    </div>
  );
}

function CompRow({
  label,
  stocks,
  getValue,
  colorize = false,
  badge = false,
}: {
  label: string;
  stocks: ComparisonStock[];
  getValue: (s: ComparisonStock) => string;
  colorize?: boolean;
  badge?: boolean;
}) {
  return (
    <tr className="border-b border-slate-800/50">
      <td className="py-2 pr-4 text-slate-400">{label}</td>
      {stocks.map((s) => {
        const val = getValue(s);
        let className = "py-2 pr-4 text-slate-200";
        if (colorize && val.startsWith("+")) className = "py-2 pr-4 text-emerald-400";
        if (colorize && val.startsWith("-")) className = "py-2 pr-4 text-red-400";
        return (
          <td key={s.ticker} className={className}>
            {badge ? <RecBadge type={val} /> : val}
          </td>
        );
      })}
    </tr>
  );
}

function buildNormalizedChartData(stocks: ComparisonStock[]) {
  if (stocks.length === 0) return [];

  // Find the stock with the most data points as basis
  const maxLen = Math.max(...stocks.map((s) => s.price_history_dates.length));
  if (maxLen === 0) return [];

  const refStock = stocks.reduce((a, b) =>
    a.price_history_dates.length >= b.price_history_dates.length ? a : b
  );

  return refStock.price_history_dates.map((date, i) => {
    const point: Record<string, string | number> = { date };
    for (const s of stocks) {
      const idx = s.price_history_dates.indexOf(date);
      if (idx >= 0 && s.price_history_close[0] > 0) {
        point[s.ticker] = parseFloat(
          ((s.price_history_close[idx] / s.price_history_close[0]) * 100).toFixed(1)
        );
      }
    }
    return point;
  });
}
