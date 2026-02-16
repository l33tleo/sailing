"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { PageLoading } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatPercent, formatCurrency } from "@/lib/utils";
import {
  getPortfolios,
  createPortfolio,
  getHoldings,
  addHolding,
  removeHolding,
  getWatchlist,
  addToWatchlist,
  removeFromWatchlist,
  type Portfolio,
  type Holding,
  type WatchlistItem,
} from "@/lib/api";
import { Plus, Trash2, Briefcase, Eye, TrendingUp, TrendingDown } from "lucide-react";

export default function PortefoljePage() {
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [activePortfolio, setActivePortfolio] = useState<Portfolio | null>(null);
  const [holdings, setHoldings] = useState<Holding[]>([]);
  const [watchlist, setWatchlist] = useState<WatchlistItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [showAddForm, setShowAddForm] = useState(false);
  const [showWatchlistForm, setShowWatchlistForm] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Form state
  const [formTicker, setFormTicker] = useState("");
  const [formShares, setFormShares] = useState("");
  const [formPrice, setFormPrice] = useState("");
  const [formDate, setFormDate] = useState(new Date().toISOString().split("T")[0]);
  const [watchlistTicker, setWatchlistTicker] = useState("");

  useEffect(() => {
    loadData();
  }, []);

  async function loadData() {
    try {
      const [p, wl] = await Promise.all([
        getPortfolios(),
        getWatchlist(),
      ]);
      setPortfolios(p);
      setWatchlist(wl);

      if (p.length > 0) {
        setActivePortfolio(p[0]);
        const h = await getHoldings(p[0].id);
        setHoldings(h);
      }
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  async function handleCreatePortfolio() {
    try {
      const p = await createPortfolio("Min portefølje");
      setPortfolios((prev) => [...prev, p]);
      setActivePortfolio(p);
      setHoldings([]);
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function handleAddHolding(e: React.FormEvent) {
    e.preventDefault();
    if (!activePortfolio) return;

    try {
      const h = await addHolding(activePortfolio.id, {
        ticker: formTicker.toUpperCase(),
        shares: parseFloat(formShares),
        buy_price: parseFloat(formPrice),
        buy_date: formDate,
      });
      setHoldings((prev) => [...prev, h]);
      setShowAddForm(false);
      setFormTicker("");
      setFormShares("");
      setFormPrice("");
      await loadData();
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function handleRemoveHolding(holdingId: number) {
    if (!activePortfolio) return;
    try {
      await removeHolding(activePortfolio.id, holdingId);
      setHoldings((prev) => prev.filter((h) => h.id !== holdingId));
      await loadData();
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function handleAddWatchlist(e: React.FormEvent) {
    e.preventDefault();
    try {
      const wl = await addToWatchlist(watchlistTicker.toUpperCase());
      setWatchlist((prev) => [wl, ...prev]);
      setShowWatchlistForm(false);
      setWatchlistTicker("");
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function handleRemoveWatchlist(id: number) {
    try {
      await removeFromWatchlist(id);
      setWatchlist((prev) => prev.filter((w) => w.id !== id));
    } catch (e: any) {
      setError(e.message);
    }
  }

  if (loading) return <PageLoading />;

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-white">Portefølje</h1>
        <p className="text-slate-400">Følg dine investeringer og watchlist</p>
      </div>

      {error && (
        <Card className="border-amber-500/30 bg-amber-500/10">
          <p className="text-sm text-amber-400">⚠️ {error}</p>
          <button onClick={() => setError(null)} className="mt-1 text-xs text-amber-300 underline">
            Lukk
          </button>
        </Card>
      )}

      {/* Portfolio section */}
      {!activePortfolio ? (
        <Card className="py-12 text-center">
          <Briefcase className="mx-auto h-12 w-12 text-slate-700" />
          <h2 className="mt-4 text-lg font-semibold text-white">
            Ingen portefølje ennå
          </h2>
          <p className="mt-1 text-slate-400">
            Opprett en portefølje for å spore dine investeringer.
          </p>
          <button
            onClick={handleCreatePortfolio}
            className="mt-4 inline-flex items-center gap-2 rounded-lg bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-500"
          >
            <Plus className="h-4 w-4" /> Opprett portefølje
          </button>
        </Card>
      ) : (
        <>
          {/* Portfolio summary */}
          <div className="grid grid-cols-1 gap-4 md:grid-cols-4">
            <Card>
              <p className="text-sm text-slate-400">Total verdi</p>
              <p className="text-2xl font-bold text-white">
                {formatNumber(activePortfolio.total_value)}
              </p>
            </Card>
            <Card>
              <p className="text-sm text-slate-400">Kostpris</p>
              <p className="text-2xl font-bold text-slate-300">
                {formatNumber(activePortfolio.total_cost)}
              </p>
            </Card>
            <Card>
              <p className="text-sm text-slate-400">Avkastning</p>
              <p
                className={`text-2xl font-bold ${
                  activePortfolio.total_return >= 0
                    ? "text-emerald-400"
                    : "text-red-400"
                }`}
              >
                {formatNumber(activePortfolio.total_return)} (
                {formatPercent(activePortfolio.total_return_pct)})
              </p>
            </Card>
            <Card>
              <p className="text-sm text-slate-400">Antall aksjer</p>
              <p className="text-2xl font-bold text-white">
                {activePortfolio.holdings_count}
              </p>
            </Card>
          </div>

          {/* Holdings */}
          <Card>
            <CardHeader>
              <div className="flex items-center justify-between">
                <CardTitle>Beholdning</CardTitle>
                <button
                  onClick={() => setShowAddForm(!showAddForm)}
                  className="flex items-center gap-1 rounded-lg bg-blue-600 px-3 py-2 text-sm font-medium text-white hover:bg-blue-500"
                >
                  <Plus className="h-4 w-4" /> Legg til aksje
                </button>
              </div>
            </CardHeader>

            {/* Add form */}
            {showAddForm && (
              <form
                onSubmit={handleAddHolding}
                className="mb-4 grid grid-cols-5 gap-3 rounded-lg border border-slate-800 bg-slate-900/50 p-4"
              >
                <input
                  type="text"
                  value={formTicker}
                  onChange={(e) => setFormTicker(e.target.value)}
                  placeholder="Ticker (f.eks. AAPL)"
                  required
                  className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
                />
                <input
                  type="number"
                  value={formShares}
                  onChange={(e) => setFormShares(e.target.value)}
                  placeholder="Antall aksjer"
                  required
                  step="any"
                  className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
                />
                <input
                  type="number"
                  value={formPrice}
                  onChange={(e) => setFormPrice(e.target.value)}
                  placeholder="Kjøpspris"
                  required
                  step="any"
                  className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
                />
                <input
                  type="date"
                  value={formDate}
                  onChange={(e) => setFormDate(e.target.value)}
                  required
                  className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2 text-sm text-white focus:border-blue-500 focus:outline-none"
                />
                <button
                  type="submit"
                  className="rounded-md bg-emerald-600 px-3 py-2 text-sm font-medium text-white hover:bg-emerald-500"
                >
                  Legg til
                </button>
              </form>
            )}

            {holdings.length === 0 ? (
              <p className="py-8 text-center text-slate-500">
                Ingen aksjer i porteføljen ennå.
              </p>
            ) : (
              <div className="overflow-x-auto">
                <table className="w-full text-sm">
                  <thead>
                    <tr className="border-b border-slate-800 text-left text-xs text-slate-500">
                      <th className="pb-2">Aksje</th>
                      <th className="pb-2">Antall</th>
                      <th className="pb-2">Kjøpspris</th>
                      <th className="pb-2">Nåverdi</th>
                      <th className="pb-2">Kostpris</th>
                      <th className="pb-2">Verdi</th>
                      <th className="pb-2">Avkastning</th>
                      <th className="pb-2"></th>
                    </tr>
                  </thead>
                  <tbody>
                    {holdings.map((h) => (
                      <tr
                        key={h.id}
                        className="border-b border-slate-800/50 hover:bg-slate-800/20"
                      >
                        <td className="py-3">
                          <Link
                            href={`/aksjer/${encodeURIComponent(h.ticker)}`}
                            className="font-medium text-white hover:text-blue-400"
                          >
                            {h.ticker}
                          </Link>
                          <span className="ml-2 text-xs text-slate-500">
                            {h.name}
                          </span>
                        </td>
                        <td className="text-slate-300">{h.shares}</td>
                        <td className="text-slate-300">
                          {formatNumber(h.buy_price)}
                        </td>
                        <td className="text-slate-300">
                          {formatNumber(h.current_price)}
                        </td>
                        <td className="text-slate-300">
                          {formatNumber(h.cost_basis)}
                        </td>
                        <td className="text-white font-medium">
                          {formatNumber(h.current_value)}
                        </td>
                        <td>
                          <span
                            className={
                              h.return_pct >= 0
                                ? "text-emerald-400"
                                : "text-red-400"
                            }
                          >
                            {formatPercent(h.return_pct)}
                          </span>
                        </td>
                        <td>
                          <button
                            onClick={() => handleRemoveHolding(h.id)}
                            className="rounded p-1 text-slate-600 hover:bg-red-500/10 hover:text-red-400"
                          >
                            <Trash2 className="h-4 w-4" />
                          </button>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </Card>
        </>
      )}

      {/* Watchlist */}
      <Card>
        <CardHeader>
          <div className="flex items-center justify-between">
            <div>
              <CardTitle>Watchlist</CardTitle>
              <CardDescription>Aksjer du følger med på</CardDescription>
            </div>
            <button
              onClick={() => setShowWatchlistForm(!showWatchlistForm)}
              className="flex items-center gap-1 rounded-lg border border-slate-700 px-3 py-2 text-sm text-slate-300 hover:bg-slate-800"
            >
              <Plus className="h-4 w-4" /> Legg til
            </button>
          </div>
        </CardHeader>

        {showWatchlistForm && (
          <form
            onSubmit={handleAddWatchlist}
            className="mb-4 flex gap-3"
          >
            <input
              type="text"
              value={watchlistTicker}
              onChange={(e) => setWatchlistTicker(e.target.value)}
              placeholder="Ticker (f.eks. EQNR.OL)"
              required
              className="flex-1 rounded-md border border-slate-700 bg-slate-800 px-3 py-2 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
            />
            <button
              type="submit"
              className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-500"
            >
              Legg til
            </button>
          </form>
        )}

        {watchlist.length === 0 ? (
          <p className="py-6 text-center text-slate-500">
            Ingen aksjer i watchlist.
          </p>
        ) : (
          <div className="grid grid-cols-1 gap-2 md:grid-cols-2 lg:grid-cols-3">
            {watchlist.map((wl) => (
              <div
                key={wl.id}
                className="flex items-center justify-between rounded-lg border border-slate-800 p-3"
              >
                <Link
                  href={`/aksjer/${encodeURIComponent(wl.ticker)}`}
                  className="font-medium text-white hover:text-blue-400"
                >
                  {wl.ticker}
                  <span className="ml-2 text-sm text-slate-500">{wl.name}</span>
                </Link>
                <div className="flex items-center gap-2">
                  {wl.current_price && (
                    <span className="text-sm text-slate-300">
                      {formatNumber(wl.current_price)}
                    </span>
                  )}
                  <button
                    onClick={() => handleRemoveWatchlist(wl.id)}
                    className="rounded p-1 text-slate-600 hover:bg-red-500/10 hover:text-red-400"
                  >
                    <Trash2 className="h-3 w-3" />
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </Card>
    </div>
  );
}
