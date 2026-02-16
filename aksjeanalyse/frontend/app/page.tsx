"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { Card, CardHeader, CardTitle } from "@/components/ui/Card";
import { RecBadge } from "@/components/ui/Badge";
import { PageLoading } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatPercent, formatLargeNumber, formatDate } from "@/lib/utils";
import {
  getRecommendations,
  getScorecard,
  type Recommendation,
  type ScorecardStats,
} from "@/lib/api";
import { TrendingUp, TrendingDown, Target, BarChart3, ArrowRight } from "lucide-react";

export default function DashboardPage() {
  const [recommendations, setRecommendations] = useState<Recommendation[]>([]);
  const [scorecard, setScorecard] = useState<ScorecardStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    async function load() {
      try {
        const [recs, sc] = await Promise.all([
          getRecommendations({ limit: 5 }),
          getScorecard(),
        ]);
        setRecommendations(recs);
        setScorecard(sc);
      } catch (e: any) {
        setError(e.message || "Kunne ikke laste data");
      } finally {
        setLoading(false);
      }
    }
    load();
  }, []);

  if (loading) return <PageLoading />;

  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold text-white">Dashboard</h1>
        <p className="text-slate-400">
          Oversikt over anbefalinger og treffsikkerhet
        </p>
      </div>

      {error && (
        <Card className="border-amber-500/30 bg-amber-500/10">
          <p className="text-amber-400 text-sm">
            ⚠️ {error} — Sørg for at backend kjører på port 8000.
          </p>
        </Card>
      )}

      {/* Stats cards */}
      <div className="grid grid-cols-1 gap-4 md:grid-cols-2 lg:grid-cols-4">
        <Card>
          <div className="flex items-center gap-3">
            <div className="rounded-lg bg-blue-500/20 p-2">
              <BarChart3 className="h-5 w-5 text-blue-400" />
            </div>
            <div>
              <p className="text-sm text-slate-400">Totale anbefalinger</p>
              <p className="text-2xl font-bold text-white">
                {scorecard?.total_recommendations || 0}
              </p>
            </div>
          </div>
        </Card>

        <Card>
          <div className="flex items-center gap-3">
            <div className="rounded-lg bg-emerald-500/20 p-2">
              <Target className="h-5 w-5 text-emerald-400" />
            </div>
            <div>
              <p className="text-sm text-slate-400">Treffsikkerhet (30d)</p>
              <p className="text-2xl font-bold text-emerald-400">
                {scorecard?.accuracy_30d
                  ? `${scorecard.accuracy_30d.toFixed(1)}%`
                  : "–"}
              </p>
            </div>
          </div>
        </Card>

        <Card>
          <div className="flex items-center gap-3">
            <div className="rounded-lg bg-emerald-500/20 p-2">
              <TrendingUp className="h-5 w-5 text-emerald-400" />
            </div>
            <div>
              <p className="text-sm text-slate-400">Snitt avkastning (30d)</p>
              <p className="text-2xl font-bold text-white">
                {scorecard?.avg_return_30d
                  ? formatPercent(scorecard.avg_return_30d)
                  : "–"}
              </p>
            </div>
          </div>
        </Card>

        <Card>
          <div className="flex items-center gap-3">
            <div className="rounded-lg bg-amber-500/20 p-2">
              <TrendingDown className="h-5 w-5 text-amber-400" />
            </div>
            <div>
              <p className="text-sm text-slate-400">Fordeling</p>
              <div className="flex gap-2 text-sm">
                {scorecard?.recommendations_by_type &&
                  Object.entries(scorecard.recommendations_by_type).map(
                    ([type, count]) => (
                      <span key={type}>
                        <RecBadge type={type} /> {count}
                      </span>
                    )
                  )}
                {(!scorecard?.recommendations_by_type ||
                  Object.keys(scorecard.recommendations_by_type).length === 0) && (
                  <span className="text-slate-500">Ingen data ennå</span>
                )}
              </div>
            </div>
          </div>
        </Card>
      </div>

      {/* Recent recommendations */}
      <Card>
        <CardHeader>
          <div className="flex items-center justify-between">
            <CardTitle>Siste anbefalinger</CardTitle>
            <Link
              href="/anbefalinger"
              className="flex items-center gap-1 text-sm text-blue-400 hover:text-blue-300"
            >
              Se alle <ArrowRight className="h-4 w-4" />
            </Link>
          </div>
        </CardHeader>

        {recommendations.length === 0 ? (
          <div className="py-8 text-center">
            <p className="text-slate-500">Ingen anbefalinger ennå.</p>
            <Link
              href="/aksjer"
              className="mt-2 inline-block text-sm text-blue-400 hover:text-blue-300"
            >
              Gå til aksjesøk for å generere en anbefaling →
            </Link>
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full">
              <thead>
                <tr className="border-b border-slate-800 text-left text-xs text-slate-500">
                  <th className="pb-2">Aksje</th>
                  <th className="pb-2">Anbefaling</th>
                  <th className="pb-2">Score</th>
                  <th className="pb-2">Kurs (da)</th>
                  <th className="pb-2">Avk. 7d</th>
                  <th className="pb-2">Avk. 30d</th>
                  <th className="pb-2">Dato</th>
                </tr>
              </thead>
              <tbody className="text-sm">
                {recommendations.map((rec) => (
                  <tr
                    key={rec.id}
                    className="border-b border-slate-800/50 hover:bg-slate-800/30"
                  >
                    <td className="py-3">
                      <Link
                        href={`/aksjer/${encodeURIComponent(rec.stock_ticker)}`}
                        className="font-medium text-white hover:text-blue-400"
                      >
                        {rec.stock_ticker}
                      </Link>
                      <span className="ml-2 text-xs text-slate-500">
                        {rec.stock_name}
                      </span>
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
                      <ReturnCell value={rec.return_7d_pct} />
                    </td>
                    <td>
                      <ReturnCell value={rec.return_30d_pct} />
                    </td>
                    <td className="text-slate-500">
                      {formatDate(rec.created_at)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </Card>

      {/* Quick actions */}
      <div className="grid grid-cols-1 gap-4 md:grid-cols-3">
        <Link href="/aksjer">
          <Card className="cursor-pointer transition-colors hover:border-blue-500/50">
            <div className="flex items-center gap-3">
              <div className="rounded-lg bg-blue-500/20 p-3">
                <BarChart3 className="h-6 w-6 text-blue-400" />
              </div>
              <div>
                <p className="font-medium text-white">Søk etter aksjer</p>
                <p className="text-sm text-slate-400">
                  Finn aksjer å analysere
                </p>
              </div>
            </div>
          </Card>
        </Link>

        <Link href="/scorecard">
          <Card className="cursor-pointer transition-colors hover:border-emerald-500/50">
            <div className="flex items-center gap-3">
              <div className="rounded-lg bg-emerald-500/20 p-3">
                <Target className="h-6 w-6 text-emerald-400" />
              </div>
              <div>
                <p className="font-medium text-white">Treffsikkerhet</p>
                <p className="text-sm text-slate-400">
                  Se hvor godt anbefalingene treffer
                </p>
              </div>
            </div>
          </Card>
        </Link>

        <Link href="/portefolje">
          <Card className="cursor-pointer transition-colors hover:border-amber-500/50">
            <div className="flex items-center gap-3">
              <div className="rounded-lg bg-amber-500/20 p-3">
                <TrendingUp className="h-6 w-6 text-amber-400" />
              </div>
              <div>
                <p className="font-medium text-white">Portefølje</p>
                <p className="text-sm text-slate-400">
                  Følg dine investeringer
                </p>
              </div>
            </div>
          </Card>
        </Link>
      </div>
    </div>
  );
}

function ReturnCell({ value }: { value: number | null }) {
  if (value == null) return <span className="text-slate-600">–</span>;
  const color = value >= 0 ? "text-emerald-400" : "text-red-400";
  return <span className={color}>{formatPercent(value)}</span>;
}
