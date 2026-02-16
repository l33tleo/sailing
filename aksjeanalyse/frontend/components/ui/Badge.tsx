import { cn } from "@/lib/utils";

interface BadgeProps {
  children: React.ReactNode;
  variant?: "default" | "kjop" | "hold" | "selg" | "outline";
  className?: string;
}

export function Badge({ children, variant = "default", className }: BadgeProps) {
  const variants = {
    default: "bg-slate-700/50 text-slate-300 border-slate-600",
    kjop: "bg-emerald-500/20 text-emerald-400 border-emerald-500/30",
    hold: "bg-amber-500/20 text-amber-400 border-amber-500/30",
    selg: "bg-red-500/20 text-red-400 border-red-500/30",
    outline: "bg-transparent text-slate-400 border-slate-600",
  };

  return (
    <span
      className={cn(
        "inline-flex items-center rounded-full border px-2.5 py-0.5 text-xs font-medium",
        variants[variant],
        className
      )}
    >
      {children}
    </span>
  );
}

export function RecBadge({ type }: { type: string }) {
  const variant =
    type === "KJØP" ? "kjop" : type === "SELG" ? "selg" : "hold";
  return <Badge variant={variant}>{type}</Badge>;
}
