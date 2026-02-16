"use client";

import { useEffect } from "react";

export default function Error({
  error,
  reset,
}: {
  error: Error & { digest?: string };
  reset: () => void;
}) {
  useEffect(() => {
    console.error("Application error:", error);
  }, [error]);

  return (
    <div className="flex min-h-[60vh] items-center justify-center">
      <div className="max-w-md text-center">
        <div className="mx-auto mb-4 flex h-16 w-16 items-center justify-center rounded-full bg-red-500/20">
          <span className="text-2xl">⚠️</span>
        </div>
        <h2 className="mb-2 text-xl font-bold text-white">Noe gikk galt</h2>
        <p className="mb-6 text-sm text-slate-400">
          {error.message || "En uventet feil oppstod. Prøv igjen."}
        </p>
        <div className="flex justify-center gap-3">
          <button
            onClick={reset}
            className="rounded-lg bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-500"
          >
            Prøv igjen
          </button>
          <a
            href="/"
            className="rounded-lg border border-slate-700 px-4 py-2 text-sm font-medium text-slate-300 hover:bg-slate-800"
          >
            Gå til dashboard
          </a>
        </div>
        <p className="mt-4 text-xs text-slate-600">
          Sørg for at backend kjører på port 8000.
        </p>
      </div>
    </div>
  );
}
