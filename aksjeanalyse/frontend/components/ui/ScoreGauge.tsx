"use client";

import { cn } from "@/lib/utils";

interface ScoreGaugeProps {
  score: number;
  label: string;
  size?: "sm" | "md" | "lg";
}

export function ScoreGauge({ score, label, size = "md" }: ScoreGaugeProps) {
  const sizes = {
    sm: { dim: 60, stroke: 5, fontSize: "text-sm" },
    md: { dim: 80, stroke: 6, fontSize: "text-lg" },
    lg: { dim: 120, stroke: 8, fontSize: "text-2xl" },
  };

  const { dim, stroke, fontSize } = sizes[size];
  const radius = (dim - stroke) / 2;
  const circumference = 2 * Math.PI * radius;
  const progress = (score / 100) * circumference;

  const color =
    score >= 70
      ? "stroke-emerald-500"
      : score >= 40
      ? "stroke-amber-500"
      : "stroke-red-500";

  const textColor =
    score >= 70
      ? "text-emerald-400"
      : score >= 40
      ? "text-amber-400"
      : "text-red-400";

  return (
    <div className="flex flex-col items-center gap-1">
      <div className="relative" style={{ width: dim, height: dim }}>
        <svg
          className="-rotate-90"
          width={dim}
          height={dim}
          viewBox={`0 0 ${dim} ${dim}`}
        >
          <circle
            cx={dim / 2}
            cy={dim / 2}
            r={radius}
            fill="none"
            stroke="currentColor"
            className="text-slate-800"
            strokeWidth={stroke}
          />
          <circle
            cx={dim / 2}
            cy={dim / 2}
            r={radius}
            fill="none"
            className={color}
            strokeWidth={stroke}
            strokeLinecap="round"
            strokeDasharray={circumference}
            strokeDashoffset={circumference - progress}
            style={{ transition: "stroke-dashoffset 0.5s ease" }}
          />
        </svg>
        <div className="absolute inset-0 flex items-center justify-center">
          <span className={cn("font-bold", fontSize, textColor)}>
            {Math.round(score)}
          </span>
        </div>
      </div>
      <span className="text-xs text-slate-500">{label}</span>
    </div>
  );
}
