"use client";

import { useEffect, useState } from "react";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { Badge } from "@/components/ui/Badge";
import { PageLoading } from "@/components/ui/LoadingSpinner";
import { formatNumber } from "@/lib/utils";
import {
  getAlerts,
  createAlert,
  deleteAlert,
  checkAlerts,
  type AlertItem,
  type AlertCheckResult,
} from "@/lib/api";
import { Bell, Plus, Trash2, RefreshCw, CheckCircle, XCircle } from "lucide-react";

const CONDITION_LABELS: Record<string, string> = {
  PRICE_ABOVE: "Kurs over",
  PRICE_BELOW: "Kurs under",
  RSI_ABOVE: "RSI over",
  RSI_BELOW: "RSI under",
};

const CONDITION_OPTIONS = Object.entries(CONDITION_LABELS);

export default function VarslerPage() {
  const [alerts, setAlerts] = useState<AlertItem[]>([]);
  const [checkResults, setCheckResults] = useState<AlertCheckResult[]>([]);
  const [loading, setLoading] = useState(true);
  const [checking, setChecking] = useState(false);
  const [showForm, setShowForm] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Form
  const [formTicker, setFormTicker] = useState("");
  const [formCondition, setFormCondition] = useState("PRICE_BELOW");
  const [formThreshold, setFormThreshold] = useState("");

  useEffect(() => {
    loadAlerts();
  }, []);

  async function loadAlerts() {
    try {
      const data = await getAlerts();
      setAlerts(data);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  async function handleCreate(e: React.FormEvent) {
    e.preventDefault();
    try {
      const alert = await createAlert({
        ticker: formTicker.toUpperCase(),
        condition_type: formCondition,
        threshold: parseFloat(formThreshold),
      });
      setAlerts((prev) => [alert, ...prev]);
      setShowForm(false);
      setFormTicker("");
      setFormThreshold("");
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function handleDelete(id: number) {
    try {
      await deleteAlert(id);
      setAlerts((prev) => prev.filter((a) => a.id !== id));
    } catch (e: any) {
      setError(e.message);
    }
  }

  async function handleCheck() {
    setChecking(true);
    try {
      const results = await checkAlerts();
      setCheckResults(results);
    } catch (e: any) {
      setError(e.message);
    } finally {
      setChecking(false);
    }
  }

  if (loading) return <PageLoading />;

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-white">Varsler</h1>
          <p className="text-slate-400">
            Sett opp varsler for kurs- og RSI-nivåer
          </p>
        </div>
        <div className="flex gap-2">
          <button
            onClick={handleCheck}
            disabled={checking}
            className="flex items-center gap-2 rounded-lg border border-slate-700 px-4 py-2.5 text-sm font-medium text-slate-300 hover:bg-slate-800 disabled:opacity-50"
          >
            <RefreshCw className={`h-4 w-4 ${checking ? "animate-spin" : ""}`} />
            Sjekk varsler
          </button>
          <button
            onClick={() => setShowForm(!showForm)}
            className="flex items-center gap-2 rounded-lg bg-blue-600 px-4 py-2.5 text-sm font-medium text-white hover:bg-blue-500"
          >
            <Plus className="h-4 w-4" /> Nytt varsel
          </button>
        </div>
      </div>

      {error && (
        <Card className="border-amber-500/30 bg-amber-500/10">
          <p className="text-sm text-amber-400">⚠️ {error}</p>
          <button onClick={() => setError(null)} className="text-xs text-amber-300 underline">Lukk</button>
        </Card>
      )}

      {/* Create form */}
      {showForm && (
        <Card>
          <CardHeader>
            <CardTitle>Opprett nytt varsel</CardTitle>
          </CardHeader>
          <form onSubmit={handleCreate} className="grid grid-cols-4 gap-3">
            <input
              type="text"
              value={formTicker}
              onChange={(e) => setFormTicker(e.target.value)}
              placeholder="Ticker (f.eks. AAPL)"
              required
              className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
            />
            <select
              value={formCondition}
              onChange={(e) => setFormCondition(e.target.value)}
              className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white focus:border-blue-500 focus:outline-none"
            >
              {CONDITION_OPTIONS.map(([value, label]) => (
                <option key={value} value={value}>
                  {label}
                </option>
              ))}
            </select>
            <input
              type="number"
              value={formThreshold}
              onChange={(e) => setFormThreshold(e.target.value)}
              placeholder="Terskelverdi"
              required
              step="any"
              className="rounded-md border border-slate-700 bg-slate-800 px-3 py-2.5 text-sm text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
            />
            <button
              type="submit"
              className="rounded-md bg-emerald-600 px-4 py-2.5 text-sm font-medium text-white hover:bg-emerald-500"
            >
              Opprett
            </button>
          </form>
        </Card>
      )}

      {/* Check results */}
      {checkResults.length > 0 && (
        <Card>
          <CardHeader>
            <CardTitle>Sjekkresultat</CardTitle>
          </CardHeader>
          <div className="space-y-2">
            {checkResults.map((r) => (
              <div
                key={r.alert_id}
                className={`flex items-center gap-3 rounded-lg border p-3 ${
                  r.triggered
                    ? "border-amber-500/30 bg-amber-500/10"
                    : "border-slate-800 bg-slate-900/50"
                }`}
              >
                {r.triggered ? (
                  <Bell className="h-5 w-5 text-amber-400" />
                ) : (
                  <CheckCircle className="h-5 w-5 text-slate-500" />
                )}
                <div className="flex-1">
                  <p className={`text-sm font-medium ${r.triggered ? "text-amber-300" : "text-slate-400"}`}>
                    {r.message}
                  </p>
                  <p className="text-xs text-slate-500">
                    Nåverdi: {formatNumber(r.current_value)} | Terskel: {formatNumber(r.threshold)}
                  </p>
                </div>
                {r.triggered && (
                  <Badge variant="hold">UTLØST</Badge>
                )}
              </div>
            ))}
          </div>
        </Card>
      )}

      {/* Alerts list */}
      <Card>
        <CardHeader>
          <CardTitle>Aktive varsler</CardTitle>
          <CardDescription>
            {alerts.length} varsel{alerts.length !== 1 ? "er" : ""} konfigurert
          </CardDescription>
        </CardHeader>

        {alerts.length === 0 ? (
          <div className="py-12 text-center">
            <Bell className="mx-auto h-12 w-12 text-slate-700" />
            <p className="mt-4 text-slate-500">Ingen varsler satt opp ennå.</p>
            <p className="text-sm text-slate-600">
              Klikk &quot;Nytt varsel&quot; for å sette opp et varsel.
            </p>
          </div>
        ) : (
          <div className="space-y-2">
            {alerts.map((a) => (
              <div
                key={a.id}
                className="flex items-center justify-between rounded-lg border border-slate-800 p-4"
              >
                <div className="flex items-center gap-4">
                  <Bell className="h-5 w-5 text-blue-400" />
                  <div>
                    <p className="font-medium text-white">
                      {a.ticker}{" "}
                      <span className="text-slate-400">
                        — {CONDITION_LABELS[a.condition_type] || a.condition_type}{" "}
                        {formatNumber(a.threshold)}
                      </span>
                    </p>
                    <p className="text-xs text-slate-500">
                      {a.stock_name}
                      {a.last_triggered && ` | Sist utløst: ${a.last_triggered}`}
                    </p>
                  </div>
                </div>
                <button
                  onClick={() => handleDelete(a.id)}
                  className="rounded p-2 text-slate-600 hover:bg-red-500/10 hover:text-red-400"
                >
                  <Trash2 className="h-4 w-4" />
                </button>
              </div>
            ))}
          </div>
        )}
      </Card>
    </div>
  );
}
