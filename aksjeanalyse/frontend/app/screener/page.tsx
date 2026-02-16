"use client";

import { useState } from "react";
import Link from "next/link";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { LoadingSpinner } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatPercent, formatLargeNumber } from "@/lib/utils";
import { screenStocks, type ScreenerStock } from "@/lib/api";
import { Filter, Search, SlidersHorizontal } from "lucide-react";

export default function ScreenerPage() {
  const [results, setResults] = useState<ScreenerStock[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [hasSearched, setHasSearched] = useState(false);

  // Filter state
  const [market, setMarket] = useState("all");
  const [maxPe, setMaxPe] = useState("");
  const [minPe, setMinPe] = useState("");
  const [minDividend, setMinDividend] = useState("");
  const [maxDebt, setMaxDebt] = useState("");

  async function handleScreen(e: React.FormEvent) {
    e.preventDefault();
    setLoading(true);
    setError(null);
    setHasSearched(true);

    try {
      const data = await screenStocks({
        market,
        min_pe: minPe ? parseFloat(minPe) : undefined,
        max_pe: maxPe ? parseFloat(maxPe) : undefined,
        min_dividend: minDividend ? parseFloat(minDividend) : undefined,
        max_debt: maxDebt ? parseFloat(maxDebt) : undefined,
      });
      setResults(data);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  function handleReset() {
    setMarket("all");
    setMaxPe("");
    setMinPe("");
    setMinDividend("");
    setMaxDebt("");
    setResults([]);
    setHasSearched(false);
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-white">Aksjescreener</h1>
        <p className="text-slate-400">
          Filtrer aksjer basert på nøkkeltall — finn aksjer som matcher dine kriterier
        </p>
      </div>

      {/* Filter form */}
      <Card>
        <CardHeader>
          <div className="flex items-center gap-2">
            <SlidersHorizontal className="h-5 w-5 text-blue-400" />
            <CardTitle>Filtere</CardTitle>
          </div>
        </CardHeader>
        <form onSubmit={handleScreen} className="space-y-4">
          <div className="grid grid-cols-2 gap-4 md:grid-cols-5">
            {/* Market */}
            <div>
              <label className="mb-1 block text-xs text-slate-500">Marked</label>
              <select
                value={market}
                onChange={(e) => setMarket(e.target.value)}
                className="w-full rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white focus:border-blue-500 focus:outline-none"
              >
                <option value="all">Alle</option>
                <option value="oslo">Oslo Børs</option>
                <option value="global">Globale</option>
              </select>
            </div>

            {/* P/E range */}
            <div>
              <label className="mb-1 block text-xs text-slate-500">Min P/E</label>
              <input
                type="number"
                value={minPe}
                onChange={(e) => setMinPe(e.target.value)}
                placeholder="f.eks. 5"
                step="any"
                className="w-full rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-600 focus:border-blue-500 focus:outline-none"
              />
            </div>
            <div>
              <label className="mb-1 block text-xs text-slate-500">Maks P/E</label>
              <input
                type="number"
                value={maxPe}
                onChange={(e) => setMaxPe(e.target.value)}
                placeholder="f.eks. 20"
                step="any"
                className="w-full rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-600 focus:border-blue-500 focus:outline-none"
              />
            </div>

            {/* Dividend */}
            <div>
              <label className="mb-1 block text-xs text-slate-500">Min utbytte (%)</label>
              <input
                type="number"
                value={minDividend}
                onChange={(e) => setMinDividend(e.target.value)}
                placeholder="f.eks. 3"
                step="any"
                className="w-full rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-600 focus:border-blue-500 focus:outline-none"
              />
            </div>

            {/* Debt */}
            <div>
              <label className="mb-1 block text-xs text-slate-500">Maks gjeld (D/E %)</label>
              <input
                type="number"
                value={maxDebt}
                onChange={(e) => setMaxDebt(e.target.value)}
                placeholder="f.eks. 100"
                step="any"
                className="w-full rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-600 focus:border-blue-500 focus:outline-none"
              />
            </div>
          </div>

          <div className="flex gap-2">
            <button
              type="submit"
              disabled={loading}
              className="flex items-center gap-2 rounded-lg bg-blue-600 px-5 py-2.5 text-sm font-medium text-white transition-colors hover:bg-blue-500 disabled:opacity-50"
            >
              <Filter className="h-4 w-4" />
              {loading ? "Filtrerer..." : "Filtrer aksjer"}
            </button>
            <button
              type="button"
              onClick={handleReset}
              className="rounded-lg border border-slate-700 px-4 py-2.5 text-sm text-slate-400 hover:bg-slate-800"
            >
              Nullstill
            </button>
          </div>
        </form>
      </Card>

      {error && (
        <Card className="border-red-500/30 bg-red-500/10">
          <p className="text-sm text-red-400">❌ {error}</p>
        </Card>
      )}

      {loading && <LoadingSpinner />}

      {/* Results */}
      {hasSearched && !loading && (
        <Card>
          <CardHeader>
            <CardTitle>
              Resultater{" "}
              <span className="text-sm font-normal text-slate-500">
                ({results.length} aksjer funnet)
              </span>
            </CardTitle>
          </CardHeader>

          {results.length === 0 ? (
            <p className="py-8 text-center text-slate-500">
              Ingen aksjer matchet filtrene. Prøv å lempe på kriteriene.
            </p>
          ) : (
            <div className="overflow-x-auto">
              <table className="w-full text-sm">
                <thead>
                  <tr className="border-b border-slate-800 text-left text-xs text-slate-500">
                    <th className="pb-2 pr-3">Aksje</th>
                    <th className="pb-2 pr-3">Kurs</th>
                    <th className="pb-2 pr-3">Endring</th>
                    <th className="pb-2 pr-3">Markedsverdi</th>
                    <th className="pb-2 pr-3">P/E</th>
                    <th className="pb-2 pr-3">Fwd P/E</th>
                    <th className="pb-2 pr-3">Utbytte</th>
                    <th className="pb-2 pr-3">D/E</th>
                    <th className="pb-2 pr-3">Vekst</th>
                    <th className="pb-2 pr-3">ROE</th>
                    <th className="pb-2 pr-3">Beta</th>
                    <th className="pb-2">Marked</th>
                  </tr>
                </thead>
                <tbody>
                  {results.map((s) => (
                    <tr
                      key={s.ticker}
                      className="border-b border-slate-800/50 hover:bg-slate-800/20"
                    >
                      <td className="py-3 pr-3">
                        <Link
                          href={`/aksjer/${encodeURIComponent(s.ticker)}`}
                          className="font-medium text-white hover:text-blue-400"
                        >
                          {s.ticker}
                        </Link>
                        <p className="text-xs text-slate-500">{s.name}</p>
                      </td>
                      <td className="pr-3 text-slate-300">
                        {formatNumber(s.price)}
                        <span className="ml-1 text-xs text-slate-600">{s.currency}</span>
                      </td>
                      <td className="pr-3">
                        <span className={s.change_percent >= 0 ? "text-emerald-400" : "text-red-400"}>
                          {formatPercent(s.change_percent)}
                        </span>
                      </td>
                      <td className="pr-3 text-slate-300">{formatLargeNumber(s.market_cap)}</td>
                      <td className="pr-3 text-slate-300">
                        {s.pe_ratio != null ? formatNumber(s.pe_ratio) : "–"}
                      </td>
                      <td className="pr-3 text-slate-300">
                        {s.forward_pe != null ? formatNumber(s.forward_pe) : "–"}
                      </td>
                      <td className="pr-3 text-slate-300">
                        {s.dividend_yield != null
                          ? `${(s.dividend_yield > 1 ? s.dividend_yield : s.dividend_yield * 100).toFixed(2)}%`
                          : "–"}
                      </td>
                      <td className="pr-3 text-slate-300">
                        {s.debt_to_equity != null ? `${formatNumber(s.debt_to_equity)}%` : "–"}
                      </td>
                      <td className="pr-3">
                        {s.revenue_growth != null ? (
                          <span className={s.revenue_growth >= 0 ? "text-emerald-400" : "text-red-400"}>
                            {formatPercent(s.revenue_growth * 100)}
                          </span>
                        ) : "–"}
                      </td>
                      <td className="pr-3 text-slate-300">
                        {s.roe != null ? formatPercent(s.roe * 100) : "–"}
                      </td>
                      <td className="pr-3 text-slate-300">
                        {s.beta != null ? formatNumber(s.beta) : "–"}
                      </td>
                      <td className="text-xs text-slate-500">{s.market}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </Card>
      )}

      {/* Tips */}
      {!hasSearched && (
        <Card className="border-blue-500/20 bg-blue-500/5">
          <div className="flex items-start gap-3">
            <Search className="mt-0.5 h-5 w-5 text-blue-400" />
            <div>
              <p className="font-medium text-blue-300">Tips for screening</p>
              <ul className="mt-1 space-y-1 text-sm text-slate-400">
                <li>• <strong>Verdiaksjer:</strong> P/E under 15, utbytte over 3%</li>
                <li>• <strong>Lav risiko:</strong> Gjeld under 50%, beta under 1</li>
                <li>• <strong>Vekstaksjer:</strong> Ingen P/E-filter, se på inntektsvekst i resultatene</li>
                <li>• <strong>Oslo Børs:</strong> Filtrer på &quot;Oslo Børs&quot; for norske aksjer</li>
              </ul>
            </div>
          </div>
        </Card>
      )}
    </div>
  );
}
