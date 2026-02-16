import { cn } from "@/lib/utils";

export function Skeleton({ className }: { className?: string }) {
  return (
    <div
      className={cn(
        "animate-pulse rounded-md bg-slate-800",
        className
      )}
    />
  );
}

export function StockDetailSkeleton() {
  return (
    <div className="space-y-6">
      {/* Header skeleton */}
      <div className="flex items-center gap-4">
        <Skeleton className="h-10 w-10" />
        <div>
          <Skeleton className="h-7 w-32" />
          <Skeleton className="mt-1 h-4 w-48" />
        </div>
        <div className="ml-auto text-right">
          <Skeleton className="h-9 w-28" />
          <Skeleton className="mt-1 h-5 w-20" />
        </div>
      </div>

      {/* Chart skeleton */}
      <div className="rounded-xl border border-slate-800 bg-[#111827] p-6">
        <Skeleton className="mb-4 h-5 w-32" />
        <Skeleton className="h-[350px] w-full" />
      </div>

      {/* Metrics skeleton */}
      <div className="rounded-xl border border-slate-800 bg-[#111827] p-6">
        <Skeleton className="mb-4 h-5 w-24" />
        <div className="grid grid-cols-2 gap-4 md:grid-cols-4 lg:grid-cols-5">
          {Array.from({ length: 10 }).map((_, i) => (
            <div key={i} className="rounded-lg border border-slate-800 p-3">
              <Skeleton className="h-3 w-16" />
              <Skeleton className="mt-2 h-6 w-20" />
            </div>
          ))}
        </div>
      </div>

      {/* Analysis skeleton */}
      <div className="rounded-xl border border-slate-800 bg-[#111827] p-6">
        <Skeleton className="mb-4 h-5 w-40" />
        <Skeleton className="h-48 w-full" />
      </div>
    </div>
  );
}
