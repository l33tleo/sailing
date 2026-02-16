import Link from "next/link";

export default function NotFound() {
  return (
    <div className="flex min-h-[60vh] items-center justify-center">
      <div className="max-w-md text-center">
        <div className="mx-auto mb-4 flex h-16 w-16 items-center justify-center rounded-full bg-slate-800">
          <span className="text-3xl font-bold text-slate-500">404</span>
        </div>
        <h2 className="mb-2 text-xl font-bold text-white">Siden finnes ikke</h2>
        <p className="mb-6 text-sm text-slate-400">
          Beklager, vi fant ikke siden du leter etter.
        </p>
        <Link
          href="/"
          className="rounded-lg bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-500"
        >
          Gå til dashboard
        </Link>
      </div>
    </div>
  );
}
