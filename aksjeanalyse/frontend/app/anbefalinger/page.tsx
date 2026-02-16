"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { RecBadge } from "@/components/ui/Badge";
import { PageLoading } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatPercent, formatDate } from "@/lib/utils";
import {
  getRecommendations,
  runEvaluation,
  type Recommendation,
} from "@/lib/api";
import { RefreshCw, Filter } from "lucide-react";

export default function AnbefalingerPage() {
  const [recommendations, setRecommendations] = useState<Recommendation[]>([]);
  const [loading, setLoading] = useState(true);
  const [evaluating, setEvaluating] = useState(false);
  const [filter, setFilter] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  async function load() {
    setLoading(true);
    try {
      const recs = await getRecommendations({
        rec_type: filter || undefined,
        limit: 100,
      });
      setRecommendations(recs);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    load();
  }, [filter]);

  async function handleEvaluate() {
    setEvaluating(true);
    try {
      const result = await runEvaluation();
      alert(result.message);
      await load();
    } catch (e: any) {
      setError(e.message);
    } finally {
      setEvaluating(false);
    }
  }

  if (loading) return <PageLoading />;

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-white">Anbefalingshistorikk</h1>
          <p className="text-slate-400">
            Alle anbefalinger med utfall og treffsikkerhet
          </p>
        </div>
        <button
          onClick={handleEvaluate}
          disabled={evaluating}
          className="flex items-center gap-2 rounded-lg border border-slate-700 px-4 py-2.5 text-sm font-medium text-slate-300 transition-colors hover:bg-slate-800 disabled:opacity-50"
        >
          <RefreshCw
            className={`h-4 w-4 ${evaluating ? "animate-spin" : ""}`}
          />
          {evaluating ? "Evaluerer..." : "Evaluer anbefalinger"}
        </button>
      </div>

      {error && (
        <Card className="border-amber-500/30 bg-amber-500/10">
          <p className="text-sm text-amber-400">⚠️ {error}</p>
        </Card>
      )}

      {/* Filter */}
      <div className="flex items-center gap-2">
        <Filter className="h-4 w-4 text-slate-500" />
        <span className="text-sm text-slate-500">Filter:</span>
        {[null, "KJØP", "HOLD", "SELG"].map((f) => (
          <button
            key={f || "all"}
            onClick={() => setFilter(f)}
            className={`rounded-md px-3 py-1.5 text-xs font-medium transition-colors ${
              filter === f
                ? "bg-blue-500/20 text-blue-400"
                : "text-slate-500 hover:text-slate-300"
            }`}
          >
            {f || "Alle"}
          </button>
        ))}
      </div>

      {/* Recommendations table */}
      <Card>
        {recommendations.length === 0 ? (
          <div className="py-12 text-center">
            <p className="text-slate-500">Ingen anbefalinger funnet.</p>
            <Link
              href="/aksjer"
              className="mt-2 inline-block text-sm text-blue-400 hover:text-blue-300"
            >
              Gå til aksjesøk for å generere anbefalinger →
            </Link>
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full">
              <thead>
                <tr className="border-b border-slate-800 text-left text-xs text-slate-500">
                  <th className="pb-3 pr-4">Dato</th>
                  <th className="pb-3 pr-4">Aksje</th>
                  <th className="pb-3 pr-4">Anbefaling</th>
                  <th className="pb-3 pr-4">Konf.</th>
                  <th className="pb-3 pr-4">Score</th>
                  <th className="pb-3 pr-4">Kurs (da)</th>
                  <th className="pb-3 pr-4">Avk. 7d</th>
                  <th className="pb-3 pr-4">Avk. 30d</th>
                  <th className="pb-3 pr-4">Avk. 90d</th>
                  <th className="pb-3">Korrekt?</th>
                </tr>
              </thead>
              <tbody className="text-sm">
                {recommendations.map((rec) => (
                  <tr
                    key={rec.id}
                    className="border-b border-slate-800/50 hover:bg-slate-800/20"
                  >
                    <td className="py-3 pr-4 text-slate-400">
                      {formatDate(rec.created_at)}
                    </td>
                    <td className="pr-4">
                      <Link
                        href={`/aksjer/${encodeURIComponent(rec.stock_ticker)}`}
                        className="font-medium text-white hover:text-blue-400"
                      >
                        {rec.stock_ticker}
                      </Link>
                    </td>
                    <td className="pr-4">
                      <RecBadge type={rec.recommendation_type} />
                    </td>
                    <td className="pr-4 text-slate-300">{rec.confidence}%</td>
                    <td className="pr-4 text-slate-300">
                      {rec.combined_score.toFixed(0)}/100
                    </td>
                    <td className="pr-4 text-slate-300">
                      {formatNumber(rec.price_at_recommendation)}
                    </td>
                    <td className="pr-4">
                      <ReturnCell value={rec.return_7d_pct} />
                    </td>
                    <td className="pr-4">
                      <ReturnCell value={rec.return_30d_pct} />
                    </td>
                    <td className="pr-4">
                      <ReturnCell value={rec.return_90d_pct} />
                    </td>
                    <td>
                      <CorrectCell rec={rec} />
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </Card>
    </div>
  );
}

function ReturnCell({ value }: { value: number | null }) {
  if (value == null) return <span className="text-slate-600">–</span>;
  const color = value >= 0 ? "text-emerald-400" : "text-red-400";
  return <span className={color}>{formatPercent(value)}</span>;
}

function CorrectCell({ rec }: { rec: Recommendation }) {
  const checks = [rec.was_correct_90d, rec.was_correct_30d, rec.was_correct_7d];
  const result = checks.find((c) => c !== null);

  if (result === null || result === undefined) {
    return <span className="text-slate-600">Venter...</span>;
  }

  return result ? (
    <span className="text-emerald-400">✓ Ja</span>
  ) : (
    <span className="text-red-400">✗ Nei</span>
  );
}
