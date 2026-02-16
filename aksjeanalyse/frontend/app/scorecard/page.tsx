"use client";

import { useEffect, useState } from "react";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { RecBadge } from "@/components/ui/Badge";
import { ScoreGauge } from "@/components/ui/ScoreGauge";
import { PageLoading } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatPercent, formatDate } from "@/lib/utils";
import { getScorecard, runEvaluation, type ScorecardStats } from "@/lib/api";
import {
  BarChart,
  Bar,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
  CartesianGrid,
  PieChart,
  Pie,
  Cell,
} from "recharts";
import { Target, TrendingUp, RefreshCw, Award } from "lucide-react";
import Link from "next/link";

export default function ScorecardPage() {
  const [stats, setStats] = useState<ScorecardStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [evaluating, setEvaluating] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function load() {
    try {
      const sc = await getScorecard();
      setStats(sc);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    load();
  }, []);

  async function handleEvaluate() {
    setEvaluating(true);
    try {
      await runEvaluation();
      await load();
    } catch (e: any) {
      setError(e.message);
    } finally {
      setEvaluating(false);
    }
  }

  if (loading) return <PageLoading />;

  const hasData = stats && stats.total_recommendations > 0;

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-white">Treffsikkerhet</h1>
          <p className="text-slate-400">
            Evaluer hvor godt anbefalingene treffer over tid
          </p>
        </div>
        <button
          onClick={handleEvaluate}
          disabled={evaluating}
          className="flex items-center gap-2 rounded-lg border border-slate-700 px-4 py-2.5 text-sm font-medium text-slate-300 transition-colors hover:bg-slate-800 disabled:opacity-50"
        >
          <RefreshCw className={`h-4 w-4 ${evaluating ? "animate-spin" : ""}`} />
          Oppdater evaluering
        </button>
      </div>

      {error && (
        <Card className="border-amber-500/30 bg-amber-500/10">
          <p className="text-sm text-amber-400">⚠️ {error}</p>
        </Card>
      )}

      {!hasData ? (
        <Card className="py-16 text-center">
          <Target className="mx-auto h-16 w-16 text-slate-700" />
          <h2 className="mt-4 text-xl font-semibold text-white">
            Ingen anbefalinger å evaluere
          </h2>
          <p className="mt-2 text-slate-400">
            Generer anbefalinger for aksjer, og kom tilbake hit for å se
            treffsikkerheten over tid.
          </p>
          <Link
            href="/aksjer"
            className="mt-4 inline-block rounded-lg bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-500"
          >
            Gå til aksjesøk
          </Link>
        </Card>
      ) : (
        <>
          {/* Accuracy gauges */}
          <div className="grid grid-cols-1 gap-4 md:grid-cols-3">
            <Card className="flex flex-col items-center py-8">
              <ScoreGauge
                score={stats!.accuracy_7d}
                label="7-dagers treffsikkerhet"
                size="lg"
              />
              <p className="mt-3 text-sm text-slate-400">
                {stats!.correct_7d} av {stats!.total_recommendations} korrekte
              </p>
              <p className="text-sm text-slate-500">
                Snitt avkastning: {formatPercent(stats!.avg_return_7d)}
              </p>
            </Card>

            <Card className="flex flex-col items-center py-8">
              <ScoreGauge
                score={stats!.accuracy_30d}
                label="30-dagers treffsikkerhet"
                size="lg"
              />
              <p className="mt-3 text-sm text-slate-400">
                {stats!.correct_30d} av {stats!.total_recommendations} korrekte
              </p>
              <p className="text-sm text-slate-500">
                Snitt avkastning: {formatPercent(stats!.avg_return_30d)}
              </p>
            </Card>

            <Card className="flex flex-col items-center py-8">
              <ScoreGauge
                score={stats!.accuracy_90d}
                label="90-dagers treffsikkerhet"
                size="lg"
              />
              <p className="mt-3 text-sm text-slate-400">
                {stats!.correct_90d} av {stats!.total_recommendations} korrekte
              </p>
              <p className="text-sm text-slate-500">
                Snitt avkastning: {formatPercent(stats!.avg_return_90d)}
              </p>
            </Card>
          </div>

          {/* Charts row */}
          <div className="grid grid-cols-1 gap-4 md:grid-cols-2">
            {/* Accuracy bar chart */}
            <Card>
              <CardHeader>
                <CardTitle>Treffsikkerhet over tid</CardTitle>
              </CardHeader>
              <ResponsiveContainer width="100%" height={250}>
                <BarChart
                  data={[
                    { period: "7 dager", accuracy: stats!.accuracy_7d, returns: stats!.avg_return_7d },
                    { period: "30 dager", accuracy: stats!.accuracy_30d, returns: stats!.avg_return_30d },
                    { period: "90 dager", accuracy: stats!.accuracy_90d, returns: stats!.avg_return_90d },
                  ]}
                >
                  <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                  <XAxis dataKey="period" tick={{ fill: "#64748b", fontSize: 12 }} />
                  <YAxis tick={{ fill: "#64748b", fontSize: 12 }} unit="%" />
                  <Tooltip
                    contentStyle={{
                      backgroundColor: "#1e293b",
                      border: "1px solid #334155",
                      borderRadius: "8px",
                      color: "#e2e8f0",
                    }}
                  />
                  <Bar dataKey="accuracy" fill="#3b82f6" radius={[4, 4, 0, 0]} name="Treffsikkerhet %" />
                </BarChart>
              </ResponsiveContainer>
            </Card>

            {/* Recommendation distribution pie */}
            <Card>
              <CardHeader>
                <CardTitle>Fordeling av anbefalinger</CardTitle>
              </CardHeader>
              <ResponsiveContainer width="100%" height={250}>
                <PieChart>
                  <Pie
                    data={Object.entries(stats!.recommendations_by_type).map(
                      ([name, value]) => ({ name, value })
                    )}
                    cx="50%"
                    cy="50%"
                    innerRadius={60}
                    outerRadius={100}
                    dataKey="value"
                    label={({ name, value }) => `${name}: ${value}`}
                  >
                    {Object.keys(stats!.recommendations_by_type).map((type) => (
                      <Cell
                        key={type}
                        fill={
                          type === "KJØP"
                            ? "#10b981"
                            : type === "SELG"
                            ? "#ef4444"
                            : "#f59e0b"
                        }
                      />
                    ))}
                  </Pie>
                  <Tooltip
                    contentStyle={{
                      backgroundColor: "#1e293b",
                      border: "1px solid #334155",
                      borderRadius: "8px",
                      color: "#e2e8f0",
                    }}
                  />
                </PieChart>
              </ResponsiveContainer>
            </Card>
          </div>

          {/* Best/Worst recommendations */}
          <div className="grid grid-cols-1 gap-4 md:grid-cols-2">
            {stats!.best_recommendation && (
              <Card>
                <CardHeader>
                  <div className="flex items-center gap-2">
                    <Award className="h-5 w-5 text-emerald-400" />
                    <CardTitle>Beste anbefaling</CardTitle>
                  </div>
                </CardHeader>
                <RecSummary rec={stats!.best_recommendation} />
              </Card>
            )}
            {stats!.worst_recommendation && (
              <Card>
                <CardHeader>
                  <div className="flex items-center gap-2">
                    <TrendingUp className="h-5 w-5 text-red-400 rotate-180" />
                    <CardTitle>Verste anbefaling</CardTitle>
                  </div>
                </CardHeader>
                <RecSummary rec={stats!.worst_recommendation} />
              </Card>
            )}
          </div>
        </>
      )}
    </div>
  );
}

function RecSummary({ rec }: { rec: any }) {
  const ret = rec.return_90d_pct ?? rec.return_30d_pct ?? rec.return_7d_pct;
  return (
    <div className="space-y-2">
      <div className="flex items-center justify-between">
        <Link
          href={`/aksjer/${encodeURIComponent(rec.stock_ticker)}`}
          className="text-lg font-semibold text-white hover:text-blue-400"
        >
          {rec.stock_ticker}
        </Link>
        <RecBadge type={rec.recommendation_type} />
      </div>
      <div className="flex gap-4 text-sm text-slate-400">
        <span>Kurs: {formatNumber(rec.price_at_recommendation)}</span>
        <span>Dato: {formatDate(rec.created_at)}</span>
      </div>
      {ret != null && (
        <p
          className={`text-lg font-bold ${
            ret >= 0 ? "text-emerald-400" : "text-red-400"
          }`}
        >
          {formatPercent(ret)} avkastning
        </p>
      )}
    </div>
  );
}
