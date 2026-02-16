"use client";

import { useEffect, useState } from "react";
import Link from "next/link";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { PageLoading } from "@/components/ui/LoadingSpinner";
import { formatNumber, formatPercent } from "@/lib/utils";
import { getSectors, type SectorData } from "@/lib/api";
import {
  BarChart,
  Bar,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
  CartesianGrid,
  Cell,
} from "recharts";
import { TrendingUp, TrendingDown, Layers } from "lucide-react";

export default function SektorerPage() {
  const [sectors, setSectors] = useState<SectorData[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    async function load() {
      try {
        const data = await getSectors();
        setSectors(data.sectors);
      } catch (e: any) {
        setError(e.message);
      } finally {
        setLoading(false);
      }
    }
    load();
  }, []);

  if (loading) return <PageLoading />;

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-white">Sektoranalyse</h1>
        <p className="text-slate-400">
          Se hvordan ulike sektorer presterer i dag
        </p>
      </div>

      {error && (
        <Card className="border-amber-500/30 bg-amber-500/10">
          <p className="text-sm text-amber-400">⚠️ {error}</p>
        </Card>
      )}

      {/* Sector performance bar chart */}
      {sectors.length > 0 && (
        <Card>
          <CardHeader>
            <CardTitle>Sektorprestasjon i dag</CardTitle>
            <CardDescription>
              Gjennomsnittlig kursendring per sektor
            </CardDescription>
          </CardHeader>
          <ResponsiveContainer width="100%" height={350}>
            <BarChart
              data={sectors.map((s) => ({
                name: s.name,
                change: s.avg_change_pct,
              }))}
              layout="vertical"
              margin={{ left: 80 }}
            >
              <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
              <XAxis
                type="number"
                tick={{ fill: "#64748b", fontSize: 12 }}
                tickFormatter={(v) => `${v.toFixed(2)}%`}
              />
              <YAxis
                type="category"
                dataKey="name"
                tick={{ fill: "#e2e8f0", fontSize: 12 }}
                width={80}
              />
              <Tooltip
                contentStyle={{
                  backgroundColor: "#1e293b",
                  border: "1px solid #334155",
                  borderRadius: "8px",
                  color: "#e2e8f0",
                }}
                formatter={(v: number) => [`${v.toFixed(2)}%`, "Endring"]}
              />
              <Bar dataKey="change" radius={[0, 4, 4, 0]}>
                {sectors.map((s, i) => (
                  <Cell
                    key={s.name}
                    fill={s.avg_change_pct >= 0 ? "#10b981" : "#ef4444"}
                  />
                ))}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        </Card>
      )}

      {/* Sector details */}
      <div className="grid grid-cols-1 gap-4 md:grid-cols-2 lg:grid-cols-3">
        {sectors.map((sector) => (
          <Card key={sector.name}>
            <CardHeader>
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <Layers className="h-5 w-5 text-blue-400" />
                  <CardTitle className="text-base">{sector.name}</CardTitle>
                </div>
                <span
                  className={`text-lg font-bold ${
                    sector.avg_change_pct >= 0
                      ? "text-emerald-400"
                      : "text-red-400"
                  }`}
                >
                  {sector.avg_change_pct >= 0 ? "+" : ""}
                  {sector.avg_change_pct.toFixed(2)}%
                </span>
              </div>
              <CardDescription>
                {sector.stock_count} aksje{sector.stock_count !== 1 ? "r" : ""}
              </CardDescription>
            </CardHeader>
            <div className="space-y-2">
              {sector.stocks.map((stock) => (
                <div
                  key={stock.ticker}
                  className="flex items-center justify-between rounded-md border border-slate-800/50 px-3 py-2"
                >
                  <div>
                    <Link
                      href={`/aksjer/${encodeURIComponent(stock.ticker)}`}
                      className="text-sm font-medium text-white hover:text-blue-400"
                    >
                      {stock.ticker}
                    </Link>
                    <p className="text-xs text-slate-500">{stock.name}</p>
                  </div>
                  <div className="text-right">
                    <p className="text-sm text-slate-300">
                      {stock.price != null ? formatNumber(stock.price) : "–"}
                    </p>
                    <p
                      className={`text-xs font-medium ${
                        stock.change_percent != null && stock.change_percent >= 0
                          ? "text-emerald-400"
                          : "text-red-400"
                      }`}
                    >
                      {stock.change_percent != null
                        ? formatPercent(stock.change_percent)
                        : "–"}
                    </p>
                  </div>
                </div>
              ))}
            </div>
          </Card>
        ))}
      </div>
    </div>
  );
}
