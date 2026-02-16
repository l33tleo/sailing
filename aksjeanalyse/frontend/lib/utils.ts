import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export function formatNumber(n: number | null | undefined, decimals = 2): string {
  if (n == null) return "–";
  return n.toLocaleString("nb-NO", {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
}

export function formatCurrency(n: number | null | undefined, currency = "NOK"): string {
  if (n == null) return "–";
  return n.toLocaleString("nb-NO", {
    style: "currency",
    currency,
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  });
}

export function formatPercent(n: number | null | undefined): string {
  if (n == null) return "–";
  const sign = n >= 0 ? "+" : "";
  return `${sign}${n.toFixed(2)}%`;
}

export function formatLargeNumber(n: number | null | undefined): string {
  if (n == null) return "–";
  if (Math.abs(n) >= 1e12) return `${(n / 1e12).toFixed(1)}T`;
  if (Math.abs(n) >= 1e9) return `${(n / 1e9).toFixed(1)}mrd`;
  if (Math.abs(n) >= 1e6) return `${(n / 1e6).toFixed(1)}M`;
  if (Math.abs(n) >= 1e3) return `${(n / 1e3).toFixed(1)}K`;
  return n.toFixed(0);
}

export function formatDate(dateStr: string): string {
  return new Date(dateStr).toLocaleDateString("nb-NO", {
    year: "numeric",
    month: "short",
    day: "numeric",
  });
}

export function recTypeColor(type: string): string {
  switch (type) {
    case "KJØP":
      return "text-emerald-400";
    case "SELG":
      return "text-red-400";
    case "HOLD":
      return "text-amber-400";
    default:
      return "text-slate-400";
  }
}

export function recTypeBg(type: string): string {
  switch (type) {
    case "KJØP":
      return "bg-emerald-500/20 text-emerald-400 border-emerald-500/30";
    case "SELG":
      return "bg-red-500/20 text-red-400 border-red-500/30";
    case "HOLD":
      return "bg-amber-500/20 text-amber-400 border-amber-500/30";
    default:
      return "bg-slate-500/20 text-slate-400 border-slate-500/30";
  }
}

export function scoreColor(score: number): string {
  if (score >= 70) return "text-emerald-400";
  if (score >= 40) return "text-amber-400";
  return "text-red-400";
}
