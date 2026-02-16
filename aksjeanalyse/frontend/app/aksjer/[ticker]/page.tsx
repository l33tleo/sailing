"use client";

import { useEffect, useState } from "react";
import { useParams } from "next/navigation";
import Link from "next/link";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { RecBadge } from "@/components/ui/Badge";
import { ScoreGauge } from "@/components/ui/ScoreGauge";
import { PageLoading } from "@/components/ui/LoadingSpinner";
import { PriceChart } from "@/components/charts/PriceChart";
import { ScoreBar } from "@/components/charts/ScoreBar";
import {
  formatNumber,
  formatPercent,
  formatLargeNumber,
  formatDate,
  recTypeBg,
} from "@/lib/utils";
import {
  getStockQuote,
  getStockHistory,
  getStockMetrics,
  getFullAnalysis,
  generateRecommendation,
  getRecommendations,
  getStockNews,
  type StockQuote,
  type StockPriceHistory,
  type StockKeyMetrics,
  type FullAnalysis,
  type Recommendation,
  type NewsItem,
} from "@/lib/api";
import {
  ArrowLeft,
  TrendingUp,
  TrendingDown,
  BarChart3,
  Zap,
  Newspaper,
  ExternalLink,
} from "lucide-react";

export default function StockDetailPage() {
  const params = useParams();
  const ticker = decodeURIComponent(params.ticker as string);

  const [quote, setQuote] = useState<StockQuote | null>(null);
  const [history, setHistory] = useState<StockPriceHistory | null>(null);
  const [metrics, setMetrics] = useState<StockKeyMetrics | null>(null);
  const [analysis, setAnalysis] = useState<FullAnalysis | null>(null);
  const [pastRecs, setPastRecs] = useState<Recommendation[]>([]);
  const [news, setNews] = useState<NewsItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [analyzing, setAnalyzing] = useState(false);
  const [saving, setSaving] = useState(false);
  const [saved, setSaved] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    async function load() {
      try {
        const [q, h, m, recs, n] = await Promise.all([
          getStockQuote(ticker),
          getStockHistory(ticker),
          getStockMetrics(ticker).catch(() => null),
          getRecommendations({ ticker, limit: 10 }).catch(() => []),
          getStockNews(ticker, 5).catch(() => []),
        ]);
        setQuote(q);
        setHistory(h);
        setMetrics(m);
        setPastRecs(recs);
        setNews(n);
      } catch (e: any) {
        setError(e.message);
      } finally {
        setLoading(false);
      }
    }
    load();
  }, [ticker]);

  async function handleAnalyze() {
    setAnalyzing(true);
    setError(null);
    try {
      const a = await getFullAnalysis(ticker);
      setAnalysis(a);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setAnalyzing(false);
    }
  }

  async function handleSaveRecommendation() {
    setSaving(true);
    try {
      const rec = await generateRecommendation(ticker);
      setPastRecs((prev) => [rec, ...prev]);
      setSaved(true);
      setTimeout(() => setSaved(false), 3000);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setSaving(false);
    }
  }

  if (loading) return <PageLoading />;

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center gap-4">
        <Link
          href="/aksjer"
          className="rounded-lg border border-slate-800 p-2 hover:bg-slate-800"
        >
          <ArrowLeft className="h-5 w-5 text-slate-400" />
        </Link>
        <div>
          <h1 className="text-2xl font-bold text-white">{ticker}</h1>
          <p className="text-slate-400">{quote?.name || ticker}</p>
        </div>
        {quote && (
          <div className="ml-auto text-right">
            <p className="text-3xl font-bold text-white">
              {formatNumber(quote.price)}
              <span className="ml-2 text-sm text-slate-500">
                {quote.currency || ""}
              </span>
            </p>
            <p
              className={
                quote.change >= 0 ? "text-emerald-400" : "text-red-400"
              }
            >
              {quote.change >= 0 ? "+" : ""}
              {formatNumber(quote.change)} ({formatPercent(quote.change_percent)})
            </p>
          </div>
        )}
      </div>

      {error && (
        <Card className="border-red-500/30 bg-red-500/10">
          <p className="text-sm text-red-400">❌ {error}</p>
        </Card>
      )}

      {/* Price chart */}
      {history && (
        <Card>
          <CardHeader>
            <CardTitle>Kursutvikling</CardTitle>
          </CardHeader>
          <PriceChart
            dates={history.dates}
            close={history.close}
            height={350}
          />
        </Card>
      )}

      {/* Key metrics */}
      {metrics && (
        <Card>
          <CardHeader>
            <CardTitle>Nøkkeltall</CardTitle>
          </CardHeader>
          <div className="grid grid-cols-2 gap-4 md:grid-cols-4 lg:grid-cols-5">
            <MetricItem label="P/E" value={metrics.pe_ratio} />
            <MetricItem label="Forward P/E" value={metrics.forward_pe} />
            <MetricItem label="P/B" value={metrics.pb_ratio} />
            <MetricItem label="EV/EBITDA" value={metrics.ev_ebitda} />
            <MetricItem
              label="Gjeld/EK"
              value={metrics.debt_to_equity}
              suffix="%"
            />
            <MetricItem
              label="ROE"
              value={metrics.roe ? metrics.roe * 100 : null}
              suffix="%"
            />
            <MetricItem
              label="Inntektsvekst"
              value={
                metrics.revenue_growth ? metrics.revenue_growth * 100 : null
              }
              suffix="%"
            />
            <MetricItem
              label="Utbytte"
              value={
                metrics.dividend_yield ? metrics.dividend_yield * 100 : null
              }
              suffix="%"
            />
            <MetricItem
              label="Markedsverdi"
              value={formatLargeNumber(metrics.market_cap)}
              raw
            />
            <MetricItem label="Beta" value={metrics.beta} />
          </div>
        </Card>
      )}

      {/* Analysis section */}
      <Card>
        <CardHeader>
          <div className="flex items-center justify-between">
            <div>
              <CardTitle>Analyse & Anbefaling</CardTitle>
              <CardDescription>
                Teknisk og fundamental analyse med kjøp/hold/selg-anbefaling
              </CardDescription>
            </div>
            <button
              onClick={handleAnalyze}
              disabled={analyzing}
              className="flex items-center gap-2 rounded-lg bg-blue-600 px-4 py-2.5 text-sm font-medium text-white transition-colors hover:bg-blue-500 disabled:opacity-50"
            >
              <Zap className="h-4 w-4" />
              {analyzing ? "Analyserer..." : "Kjør analyse"}
            </button>
          </div>
        </CardHeader>

        {analysis ? (
          <div className="space-y-6">
            {/* Recommendation badge */}
            <div className="flex items-center gap-6">
              <div
                className={`rounded-xl border p-6 text-center ${recTypeBg(
                  analysis.recommendation_type
                )}`}
              >
                <p className="text-3xl font-bold">
                  {analysis.recommendation_type}
                </p>
                <p className="mt-1 text-sm opacity-80">
                  Konfidens: {analysis.confidence}%
                </p>
              </div>

              <div className="flex gap-6">
                <ScoreGauge
                  score={analysis.technical.technical_score}
                  label="Teknisk"
                  size="md"
                />
                <ScoreGauge
                  score={analysis.fundamental.fundamental_score}
                  label="Fundamental"
                  size="md"
                />
                <ScoreGauge
                  score={analysis.combined_score}
                  label="Samlet"
                  size="lg"
                />
              </div>
            </div>

            {/* Technical details */}
            <div>
              <h4 className="mb-3 font-medium text-white">Teknisk analyse</h4>
              <div className="space-y-3">
                {analysis.technical.rsi != null && (
                  <ScoreBar
                    label={`RSI (${analysis.technical.rsi})`}
                    score={
                      analysis.technical.rsi < 30
                        ? 25
                        : analysis.technical.rsi > 70
                        ? 0
                        : 12
                    }
                    maxScore={25}
                    detail={analysis.technical.rsi_signal || ""}
                  />
                )}
                {analysis.technical.macd_signal && (
                  <ScoreBar
                    label="MACD"
                    score={
                      analysis.technical.macd_signal.includes("Bullish") ? 20 : 5
                    }
                    maxScore={25}
                    detail={analysis.technical.macd_signal}
                  />
                )}
                {analysis.technical.sma_signal && (
                  <ScoreBar
                    label="Glidende snitt"
                    score={
                      analysis.technical.sma_signal.includes("Golden") ? 25 : 12
                    }
                    maxScore={25}
                    detail={analysis.technical.sma_signal}
                  />
                )}
                {analysis.technical.volume_signal && (
                  <ScoreBar
                    label="Volum"
                    score={
                      analysis.technical.volume_signal.includes("bullish")
                        ? 25
                        : 10
                    }
                    maxScore={25}
                    detail={analysis.technical.volume_signal}
                  />
                )}
              </div>
            </div>

            {/* Fundamental details */}
            <div>
              <h4 className="mb-3 font-medium text-white">
                Fundamental analyse
              </h4>
              <div className="space-y-3">
                <ScoreBar
                  label="Verdsettelse (P/E)"
                  score={analysis.fundamental.pe_score}
                  maxScore={20}
                  detail={analysis.fundamental.pe_detail}
                />
                <ScoreBar
                  label="Gjeld"
                  score={analysis.fundamental.debt_score}
                  maxScore={20}
                  detail={analysis.fundamental.debt_detail}
                />
                <ScoreBar
                  label="Vekst"
                  score={analysis.fundamental.growth_score}
                  maxScore={20}
                  detail={analysis.fundamental.growth_detail}
                />
                <ScoreBar
                  label="Utbytte"
                  score={analysis.fundamental.dividend_score}
                  maxScore={20}
                  detail={analysis.fundamental.dividend_detail}
                />
                <ScoreBar
                  label="Kontantstrøm"
                  score={analysis.fundamental.cashflow_score}
                  maxScore={20}
                  detail={analysis.fundamental.cashflow_detail}
                />
              </div>
            </div>

            {/* Save recommendation button */}
            <div className="flex items-center gap-4 border-t border-slate-800 pt-4">
              <button
                onClick={handleSaveRecommendation}
                disabled={saving}
                className="rounded-lg bg-emerald-600 px-4 py-2.5 text-sm font-medium text-white transition-colors hover:bg-emerald-500 disabled:opacity-50"
              >
                {saving
                  ? "Lagrer..."
                  : saved
                  ? "✓ Lagret!"
                  : "Lagre anbefaling"}
              </button>
              <p className="text-xs text-slate-500">
                Anbefalingen lagres med tidsstempel og kurs slik at du kan
                evaluere den i ettertid.
              </p>
            </div>

            {/* Reasoning */}
            <div className="rounded-lg bg-slate-800/50 p-4">
              <h4 className="mb-2 text-sm font-medium text-slate-300">
                Fullstendig begrunnelse
              </h4>
              <pre className="whitespace-pre-wrap text-sm text-slate-400">
                {analysis.reasoning}
              </pre>
            </div>
          </div>
        ) : (
          <div className="py-12 text-center">
            <BarChart3 className="mx-auto h-12 w-12 text-slate-700" />
            <p className="mt-4 text-slate-500">
              Klikk &quot;Kjør analyse&quot; for å generere en full analyse og
              anbefaling for {ticker}.
            </p>
          </div>
        )}
      </Card>

      {/* News */}
      {news.length > 0 && (
        <Card>
          <CardHeader>
            <div className="flex items-center gap-2">
              <Newspaper className="h-5 w-5 text-blue-400" />
              <CardTitle>Nyheter</CardTitle>
            </div>
          </CardHeader>
          <div className="space-y-3">
            {news.map((n, i) => (
              <a
                key={i}
                href={n.link}
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-start gap-3 rounded-lg border border-slate-800 p-3 transition-colors hover:border-blue-500/50 hover:bg-slate-800/30"
              >
                {n.thumbnail && (
                  <img
                    src={n.thumbnail}
                    alt=""
                    className="h-16 w-24 rounded object-cover"
                  />
                )}
                <div className="flex-1">
                  <p className="text-sm font-medium text-white">{n.title}</p>
                  <p className="mt-1 text-xs text-slate-500">
                    {n.publisher}
                    {n.published && ` · ${formatDate(n.published)}`}
                  </p>
                </div>
                <ExternalLink className="mt-1 h-4 w-4 flex-shrink-0 text-slate-600" />
              </a>
            ))}
          </div>
        </Card>
      )}

      {/* Past recommendations for this stock */}
      {pastRecs.length > 0 && (
        <Card>
          <CardHeader>
            <CardTitle>Anbefalingshistorikk for {ticker}</CardTitle>
            <CardDescription>
              Tidligere anbefalinger og deres utfall
            </CardDescription>
          </CardHeader>
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-slate-800 text-left text-xs text-slate-500">
                  <th className="pb-2">Dato</th>
                  <th className="pb-2">Anbefaling</th>
                  <th className="pb-2">Score</th>
                  <th className="pb-2">Kurs</th>
                  <th className="pb-2">Avk. 7d</th>
                  <th className="pb-2">Avk. 30d</th>
                  <th className="pb-2">Avk. 90d</th>
                </tr>
              </thead>
              <tbody>
                {pastRecs.map((rec) => (
                  <tr
                    key={rec.id}
                    className="border-b border-slate-800/50"
                  >
                    <td className="py-2 text-slate-400">
                      {formatDate(rec.created_at)}
                    </td>
                    <td>
                      <RecBadge type={rec.recommendation_type} />
                    </td>
                    <td className="text-slate-300">
                      {rec.combined_score.toFixed(0)}
                    </td>
                    <td className="text-slate-300">
                      {formatNumber(rec.price_at_recommendation)}
                    </td>
                    <td>
                      <ReturnCell value={rec.return_7d_pct} correct={rec.was_correct_7d} />
                    </td>
                    <td>
                      <ReturnCell value={rec.return_30d_pct} correct={rec.was_correct_30d} />
                    </td>
                    <td>
                      <ReturnCell value={rec.return_90d_pct} correct={rec.was_correct_90d} />
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </Card>
      )}
    </div>
  );
}

function MetricItem({
  label,
  value,
  suffix = "",
  raw = false,
}: {
  label: string;
  value: number | string | null | undefined;
  suffix?: string;
  raw?: boolean;
}) {
  const display = raw
    ? (value as string) || "–"
    : value != null
    ? `${typeof value === "number" ? formatNumber(value) : value}${suffix}`
    : "–";

  return (
    <div className="rounded-lg border border-slate-800 p-3">
      <p className="text-xs text-slate-500">{label}</p>
      <p className="mt-1 text-lg font-semibold text-white">{display}</p>
    </div>
  );
}

function ReturnCell({
  value,
  correct,
}: {
  value: number | null;
  correct: boolean | null;
}) {
  if (value == null) return <span className="text-slate-600">–</span>;
  const color = value >= 0 ? "text-emerald-400" : "text-red-400";
  const icon = correct === true ? " ✓" : correct === false ? " ✗" : "";
  return (
    <span className={color}>
      {formatPercent(value)}
      {icon && (
        <span className={correct ? "text-emerald-400" : "text-red-400"}>
          {icon}
        </span>
      )}
    </span>
  );
}
