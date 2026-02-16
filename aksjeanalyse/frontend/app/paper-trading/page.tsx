"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { Badge } from "@/components/ui/Badge";
import { PageLoading } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatPercent, formatDate } from "@/lib/utils";
import {
  getPaperTrades,
  getPaperTradeSummary,
  openPaperTrade,
  closePaperTrade,
  deletePaperTrade,
  type PaperTrade,
  type PaperTradeSummary,
} from "@/lib/api";
import {
  Plus,
  X,
  Trash2,
  TrendingUp,
  TrendingDown,
  DollarSign,
  Target,
  BarChart3,
} from "lucide-react";

export default function PaperTradingPage() {
  const [trades, setTrades] = useState<PaperTrade[]>([]);
  const [summary, setSummary] = useState<PaperTradeSummary | null>(null);
  const [loading, setLoading] = useState(true);
  const [showForm, setShowForm] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Form
  const [formTicker, setFormTicker] = useState("");
  const [formType, setFormType] = useState<"BUY" | "SELL">("BUY");
  const [formShares, setFormShares] = useState("");
  const [formNote, setFormNote] = useState("");

  useEffect(() => {
    loadData();
  }, []);

  async function loadData() {
    try {
      const [t, s] = await Promise.all([getPaperTrades(), getPaperTradeSummary()]);
      setTrades(t);
      setSummary(s);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  async function handleOpenTrade(e: React.FormEvent) {
    e.preventDefault();
    try {
      await openPaperTrade({
        ticker: formTicker.toUpperCase(),
        trade_type: formType,
        shares: parseFloat(formShares),
        note: formNote || undefined,
      });
      setShowForm(false);
      setFormTicker("");
      setFormShares("");
      setFormNote("");
      await loadData();
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function handleCloseTrade(tradeId: number) {
    try {
      await closePaperTrade(tradeId);
      await loadData();
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function handleDeleteTrade(tradeId: number) {
    try {
      await deletePaperTrade(tradeId);
      await loadData();
    } catch (e: any) {
      setError(e.message);
    }
  }

  if (loading) return <PageLoading />;

  const openTrades = trades.filter((t) => t.is_open);
  const closedTrades = trades.filter((t) => !t.is_open);

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-white">Paper Trading</h1>
          <p className="text-slate-400">
            Simuler handler uten ekte penger — test strategier
          </p>
        </div>
        <button
          onClick={() => setShowForm(!showForm)}
          className="flex items-center gap-2 rounded-lg bg-blue-600 px-4 py-2.5 text-sm font-medium text-white hover:bg-blue-500"
        >
          <Plus className="h-4 w-4" /> Ny trade
        </button>
      </div>

      {error && (
        <Card className="border-red-500/30 bg-red-500/10">
          <p className="text-sm text-red-400">❌ {error}</p>
          <button onClick={() => setError(null)} className="text-xs text-red-300 underline">Lukk</button>
        </Card>
      )}

      {/* Summary cards */}
      {summary && (
        <div className="grid grid-cols-2 gap-4 md:grid-cols-4 lg:grid-cols-6">
          <Card>
            <p className="text-xs text-slate-500">Egenkapital</p>
            <p className={`text-xl font-bold ${summary.current_equity >= summary.starting_capital ? "text-emerald-400" : "text-red-400"}`}>
              {formatNumber(summary.current_equity, 0)}
            </p>
            <p className="text-xs text-slate-600">Start: {formatNumber(summary.starting_capital, 0)}</p>
          </Card>
          <Card>
            <p className="text-xs text-slate-500">Realisert P&L</p>
            <p className={`text-xl font-bold ${summary.total_realized_pnl >= 0 ? "text-emerald-400" : "text-red-400"}`}>
              {formatNumber(summary.total_realized_pnl)}
            </p>
          </Card>
          <Card>
            <p className="text-xs text-slate-500">Urealisert P&L</p>
            <p className={`text-xl font-bold ${summary.total_unrealized_pnl >= 0 ? "text-emerald-400" : "text-red-400"}`}>
              {formatNumber(summary.total_unrealized_pnl)}
            </p>
          </Card>
          <Card>
            <p className="text-xs text-slate-500">Win rate</p>
            <p className="text-xl font-bold text-white">{summary.win_rate}%</p>
          </Card>
          <Card>
            <p className="text-xs text-slate-500">Åpne / Lukkede</p>
            <p className="text-xl font-bold text-white">
              {summary.open_trades} / {summary.closed_trades}
            </p>
          </Card>
          <Card>
            <p className="text-xs text-slate-500">Snitt avkastning</p>
            <p className={`text-xl font-bold ${summary.avg_pnl_pct >= 0 ? "text-emerald-400" : "text-red-400"}`}>
              {formatPercent(summary.avg_pnl_pct)}
            </p>
          </Card>
        </div>
      )}

      {/* New trade form */}
      {showForm && (
        <Card>
          <CardHeader>
            <CardTitle>Åpne ny trade</CardTitle>
          </CardHeader>
          <form onSubmit={handleOpenTrade} className="grid grid-cols-5 gap-3">
            <input
              type="text"
              value={formTicker}
              onChange={(e) => setFormTicker(e.target.value)}
              placeholder="Ticker (f.eks. AAPL)"
              required
              className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
            />
            <div className="flex gap-1">
              <button
                type="button"
                onClick={() => setFormType("BUY")}
                className={`flex-1 rounded-md px-3 py-2.5 text-sm font-medium ${
                  formType === "BUY"
                    ? "bg-emerald-600 text-white"
                    : "border border-slate-700 text-slate-400"
                }`}
              >
                KJØP
              </button>
              <button
                type="button"
                onClick={() => setFormType("SELL")}
                className={`flex-1 rounded-md px-3 py-2.5 text-sm font-medium ${
                  formType === "SELL"
                    ? "bg-red-600 text-white"
                    : "border border-slate-700 text-slate-400"
                }`}
              >
                SHORT
              </button>
            </div>
            <input
              type="number"
              value={formShares}
              onChange={(e) => setFormShares(e.target.value)}
              placeholder="Antall aksjer"
              required
              step="any"
              min="0.01"
              className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
            />
            <input
              type="text"
              value={formNote}
              onChange={(e) => setFormNote(e.target.value)}
              placeholder="Notat (valgfritt)"
              className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
            />
            <button
              type="submit"
              className="rounded-md bg-blue-600 px-4 py-2.5 text-sm font-medium text-white hover:bg-blue-500"
            >
              Åpne trade
            </button>
          </form>
        </Card>
      )}

      {/* Open trades */}
      <Card>
        <CardHeader>
          <CardTitle>
            Åpne posisjoner{" "}
            <span className="text-sm font-normal text-slate-500">
              ({openTrades.length})
            </span>
          </CardTitle>
        </CardHeader>
        {openTrades.length === 0 ? (
          <p className="py-8 text-center text-slate-500">
            Ingen åpne trades. Klikk &quot;Ny trade&quot; for å starte.
          </p>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-slate-800 text-left text-xs text-slate-500">
                  <th className="pb-2">Aksje</th>
                  <th className="pb-2">Type</th>
                  <th className="pb-2">Antall</th>
                  <th className="pb-2">Inngangskurs</th>
                  <th className="pb-2">Nåkurs</th>
                  <th className="pb-2">Urealisert P&L</th>
                  <th className="pb-2">Åpnet</th>
                  <th className="pb-2">Notat</th>
                  <th className="pb-2"></th>
                </tr>
              </thead>
              <tbody>
                {openTrades.map((t) => (
                  <tr key={t.id} className="border-b border-slate-800/50 hover:bg-slate-800/20">
                    <td className="py-3">
                      <Link
                        href={`/aksjer/${encodeURIComponent(t.ticker)}`}
                        className="font-medium text-white hover:text-blue-400"
                      >
                        {t.ticker}
                      </Link>
                      <span className="ml-1 text-xs text-slate-500">{t.stock_name}</span>
                    </td>
                    <td>
                      <Badge variant={t.trade_type === "BUY" ? "kjop" : "selg"}>
                        {t.trade_type === "BUY" ? "KJØP" : "SHORT"}
                      </Badge>
                    </td>
                    <td className="text-slate-300">{t.shares}</td>
                    <td className="text-slate-300">{formatNumber(t.entry_price)}</td>
                    <td className="text-slate-300">{formatNumber(t.current_price)}</td>
                    <td>
                      <span className={t.unrealized_pnl != null && t.unrealized_pnl >= 0 ? "text-emerald-400" : "text-red-400"}>
                        {t.unrealized_pnl != null ? `${formatNumber(t.unrealized_pnl)} (${formatPercent(t.unrealized_pnl_pct)})` : "–"}
                      </span>
                    </td>
                    <td className="text-xs text-slate-500">{formatDate(t.opened_at)}</td>
                    <td className="max-w-[120px] truncate text-xs text-slate-600">{t.note || ""}</td>
                    <td>
                      <div className="flex gap-1">
                        <button
                          onClick={() => handleCloseTrade(t.id)}
                          className="rounded px-2 py-1 text-xs font-medium text-amber-400 hover:bg-amber-500/10"
                          title="Lukk trade"
                        >
                          Lukk
                        </button>
                        <button
                          onClick={() => handleDeleteTrade(t.id)}
                          className="rounded p-1 text-slate-600 hover:bg-red-500/10 hover:text-red-400"
                        >
                          <Trash2 className="h-3 w-3" />
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </Card>

      {/* Closed trades */}
      {closedTrades.length > 0 && (
        <Card>
          <CardHeader>
            <CardTitle>
              Lukkede trades{" "}
              <span className="text-sm font-normal text-slate-500">
                ({closedTrades.length})
              </span>
            </CardTitle>
          </CardHeader>
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-slate-800 text-left text-xs text-slate-500">
                  <th className="pb-2">Aksje</th>
                  <th className="pb-2">Type</th>
                  <th className="pb-2">Antall</th>
                  <th className="pb-2">Inngang</th>
                  <th className="pb-2">Utgang</th>
                  <th className="pb-2">P&L</th>
                  <th className="pb-2">P&L %</th>
                  <th className="pb-2">Åpnet</th>
                  <th className="pb-2">Lukket</th>
                  <th className="pb-2"></th>
                </tr>
              </thead>
              <tbody>
                {closedTrades.map((t) => (
                  <tr key={t.id} className="border-b border-slate-800/50">
                    <td className="py-2">
                      <Link
                        href={`/aksjer/${encodeURIComponent(t.ticker)}`}
                        className="font-medium text-white hover:text-blue-400"
                      >
                        {t.ticker}
                      </Link>
                    </td>
                    <td>
                      <Badge variant={t.trade_type === "BUY" ? "kjop" : "selg"}>
                        {t.trade_type === "BUY" ? "KJØP" : "SHORT"}
                      </Badge>
                    </td>
                    <td className="text-slate-300">{t.shares}</td>
                    <td className="text-slate-300">{formatNumber(t.entry_price)}</td>
                    <td className="text-slate-300">{formatNumber(t.exit_price)}</td>
                    <td className={t.pnl != null && t.pnl >= 0 ? "text-emerald-400" : "text-red-400"}>
                      {t.pnl != null ? formatNumber(t.pnl) : "–"}
                    </td>
                    <td className={t.pnl_pct != null && t.pnl_pct >= 0 ? "text-emerald-400" : "text-red-400"}>
                      {t.pnl_pct != null ? formatPercent(t.pnl_pct) : "–"}
                    </td>
                    <td className="text-xs text-slate-500">{formatDate(t.opened_at)}</td>
                    <td className="text-xs text-slate-500">{t.closed_at ? formatDate(t.closed_at) : "–"}</td>
                    <td>
                      <button
                        onClick={() => handleDeleteTrade(t.id)}
                        className="rounded p-1 text-slate-600 hover:bg-red-500/10 hover:text-red-400"
                      >
                        <Trash2 className="h-3 w-3" />
                      </button>
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
