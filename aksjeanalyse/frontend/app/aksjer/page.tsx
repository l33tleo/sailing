"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { LoadingSpinner } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatLargeNumber } from "@/lib/utils";
import {
  searchStocks,
  getPopularStocks,
  type StockSearchResult,
} from "@/lib/api";
import { Search, TrendingUp } from "lucide-react";

export default function AksjerPage() {
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<StockSearchResult[]>([]);
  const [popular, setPopular] = useState<StockSearchResult[]>([]);
  const [searching, setSearching] = useState(false);
  const [loadingPopular, setLoadingPopular] = useState(true);
  const [market, setMarket] = useState<"all" | "oslo" | "global">("all");

  useEffect(() => {
    async function loadPopular() {
      try {
        const stocks = await getPopularStocks(market);
        setPopular(stocks);
      } catch {
        // Silently fail — popular stocks are optional
      } finally {
        setLoadingPopular(false);
      }
    }
    loadPopular();
  }, [market]);

  async function handleSearch(e: React.FormEvent) {
    e.preventDefault();
    if (!query.trim()) return;

    setSearching(true);
    try {
      const res = await searchStocks(query.trim());
      setResults(res);
    } catch {
      setResults([]);
    } finally {
      setSearching(false);
    }
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-white">Aksjesøk</h1>
        <p className="text-slate-400">
          Søk etter aksjer og generer analyser og anbefalinger
        </p>
      </div>

      {/* Search bar */}
      <Card>
        <form onSubmit={handleSearch} className="flex gap-3">
          <div className="relative flex-1">
            <Search className="absolute left-3 top-1/2 h-5 w-5 -translate-y-1/2 text-slate-500" />
            <input
              type="text"
              value={query}
              onChange={(e) => setQuery(e.target.value)}
              placeholder="Søk på ticker eller selskapsnavn (f.eks. EQNR, AAPL, DNB...)"
              className="w-full rounded-lg border border-slate-700 bg-slate-800/50 py-3 pl-10 pr-4 text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none focus:ring-1 focus:ring-blue-500"
            />
          </div>
          <button
            type="submit"
            disabled={searching}
            className="rounded-lg bg-blue-600 px-6 py-3 font-medium text-white transition-colors hover:bg-blue-500 disabled:opacity-50"
          >
            {searching ? "Søker..." : "Søk"}
          </button>
        </form>
      </Card>

      {/* Search results */}
      {results.length > 0 && (
        <Card>
          <CardHeader>
            <CardTitle>Søkeresultater</CardTitle>
            <CardDescription>
              Fant {results.length} treff for &quot;{query}&quot;
            </CardDescription>
          </CardHeader>
          <StockList stocks={results} />
        </Card>
      )}

      {/* Popular stocks */}
      <Card>
        <CardHeader>
          <div className="flex items-center justify-between">
            <div>
              <CardTitle>Populære aksjer</CardTitle>
              <CardDescription>Bla gjennom populære aksjer</CardDescription>
            </div>
            <div className="flex gap-1">
              {(["all", "oslo", "global"] as const).map((m) => (
                <button
                  key={m}
                  onClick={() => {
                    setMarket(m);
                    setLoadingPopular(true);
                  }}
                  className={`rounded-md px-3 py-1.5 text-xs font-medium transition-colors ${
                    market === m
                      ? "bg-blue-500/20 text-blue-400"
                      : "text-slate-500 hover:text-slate-300"
                  }`}
                >
                  {m === "all" ? "Alle" : m === "oslo" ? "Oslo Børs" : "Globale"}
                </button>
              ))}
            </div>
          </div>
        </CardHeader>

        {loadingPopular ? (
          <LoadingSpinner />
        ) : popular.length > 0 ? (
          <StockList stocks={popular} />
        ) : (
          <p className="py-8 text-center text-slate-500">
            Kunne ikke laste populære aksjer. Sørg for at backend kjører.
          </p>
        )}
      </Card>
    </div>
  );
}

function StockList({ stocks }: { stocks: StockSearchResult[] }) {
  return (
    <div className="grid grid-cols-1 gap-3 md:grid-cols-2 lg:grid-cols-3">
      {stocks.map((stock) => (
        <Link
          key={stock.ticker}
          href={`/aksjer/${encodeURIComponent(stock.ticker)}`}
          className="rounded-lg border border-slate-800 p-4 transition-colors hover:border-blue-500/50 hover:bg-slate-800/30"
        >
          <div className="flex items-center justify-between">
            <div>
              <p className="font-semibold text-white">{stock.ticker}</p>
              <p className="text-sm text-slate-400">{stock.name}</p>
            </div>
            <div className="text-right">
              {stock.current_price && (
                <p className="font-medium text-white">
                  {formatNumber(stock.current_price)}
                  <span className="ml-1 text-xs text-slate-500">
                    {stock.currency || ""}
                  </span>
                </p>
              )}
              {stock.market && (
                <p className="text-xs text-slate-500">{stock.market}</p>
              )}
            </div>
          </div>
          {stock.sector && (
            <p className="mt-2 text-xs text-slate-500">{stock.sector}</p>
          )}
        </Link>
      ))}
    </div>
  );
}
