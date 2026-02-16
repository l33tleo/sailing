"use client";

import { useState } from "react";
import {
  AreaChart,
  Area,
  XAxis,
  YAxis,
  Tooltip,
  ResponsiveContainer,
  CartesianGrid,
} from "recharts";
import { cn } from "@/lib/utils";

interface PriceChartProps {
  dates: string[];
  close: number[];
  height?: number;
}

const periods = [
  { label: "1M", value: 22 },
  { label: "3M", value: 66 },
  { label: "6M", value: 132 },
  { label: "1Å", value: 252 },
  { label: "Alle", value: -1 },
];

export function PriceChart({ dates, close, height = 300 }: PriceChartProps) {
  const [selectedPeriod, setSelectedPeriod] = useState(3); // Default to 1Y

  const sliceCount =
    periods[selectedPeriod].value === -1
      ? dates.length
      : Math.min(periods[selectedPeriod].value, dates.length);

  const data = dates.slice(-sliceCount).map((date, i) => ({
    date,
    price: close[close.length - sliceCount + i],
  }));

  const firstPrice = data[0]?.price || 0;
  const lastPrice = data[data.length - 1]?.price || 0;
  const isUp = lastPrice >= firstPrice;

  return (
    <div>
      {/* Period selector */}
      <div className="mb-4 flex gap-1">
        {periods.map((p, i) => (
          <button
            key={p.label}
            onClick={() => setSelectedPeriod(i)}
            className={cn(
              "rounded-md px-3 py-1 text-xs font-medium transition-colors",
              i === selectedPeriod
                ? "bg-blue-500/20 text-blue-400"
                : "text-slate-500 hover:text-slate-300"
            )}
          >
            {p.label}
          </button>
        ))}
      </div>

      <ResponsiveContainer width="100%" height={height}>
        <AreaChart data={data}>
          <defs>
            <linearGradient id="priceGradient" x1="0" y1="0" x2="0" y2="1">
              <stop
                offset="0%"
                stopColor={isUp ? "#10b981" : "#ef4444"}
                stopOpacity={0.3}
              />
              <stop
                offset="100%"
                stopColor={isUp ? "#10b981" : "#ef4444"}
                stopOpacity={0}
              />
            </linearGradient>
          </defs>
          <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
          <XAxis
            dataKey="date"
            tick={{ fill: "#64748b", fontSize: 11 }}
            tickLine={false}
            axisLine={false}
            tickFormatter={(v) => {
              const d = new Date(v);
              return `${d.getDate()}.${d.getMonth() + 1}`;
            }}
            minTickGap={40}
          />
          <YAxis
            tick={{ fill: "#64748b", fontSize: 11 }}
            tickLine={false}
            axisLine={false}
            domain={["auto", "auto"]}
            tickFormatter={(v) => v.toFixed(0)}
            width={60}
          />
          <Tooltip
            contentStyle={{
              backgroundColor: "#1e293b",
              border: "1px solid #334155",
              borderRadius: "8px",
              color: "#e2e8f0",
              fontSize: 13,
            }}
            formatter={(value: number) => [`${value.toFixed(2)}`, "Kurs"]}
            labelFormatter={(label) => {
              const d = new Date(label);
              return d.toLocaleDateString("nb-NO", {
                year: "numeric",
                month: "long",
                day: "numeric",
              });
            }}
          />
          <Area
            type="monotone"
            dataKey="price"
            stroke={isUp ? "#10b981" : "#ef4444"}
            strokeWidth={2}
            fill="url(#priceGradient)"
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}
