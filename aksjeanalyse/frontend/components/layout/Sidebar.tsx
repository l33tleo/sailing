"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { cn } from "@/lib/utils";
import { useAuth } from "@/lib/auth";
import {
  BarChart3,
  Search,
  TrendingUp,
  Target,
  Briefcase,
  LayoutDashboard,
  GitCompareArrows,
  Bell,
  CandlestickChart,
  Layers,
  SlidersHorizontal,
  MessageCircle,
} from "lucide-react";

const navItems = [
  { href: "/", label: "Dashboard", icon: LayoutDashboard },
  { href: "/chat", label: "AI Chat", icon: MessageCircle },
  { href: "/aksjer", label: "Aksjer", icon: Search },
  { href: "/screener", label: "Screener", icon: SlidersHorizontal },
  { href: "/anbefalinger", label: "Anbefalinger", icon: TrendingUp },
  { href: "/sammenligning", label: "Sammenligning", icon: GitCompareArrows },
  { href: "/sektorer", label: "Sektorer", icon: Layers },
  { href: "/scorecard", label: "Treffsikkerhet", icon: Target },
  { href: "/portefolje", label: "Portefølje", icon: Briefcase },
  { href: "/paper-trading", label: "Paper Trading", icon: CandlestickChart },
  { href: "/varsler", label: "Varsler", icon: Bell },
];

export default function Sidebar() {
  const pathname = usePathname();

  return (
    <aside className="fixed left-0 top-0 z-40 h-screen w-64 border-r border-slate-800 bg-[#0d1117]">
      {/* Logo */}
      <div className="flex h-16 items-center gap-3 border-b border-slate-800 px-6">
        <BarChart3 className="h-8 w-8 text-blue-500" />
        <div>
          <h1 className="text-lg font-bold text-white">AksjeAnalyse</h1>
          <p className="text-xs text-slate-500">Analyse &amp; Anbefalinger</p>
        </div>
      </div>

      {/* Navigation */}
      <nav className="mt-6 px-3">
        <ul className="space-y-1">
          {navItems.map((item) => {
            const isActive =
              pathname === item.href ||
              (item.href !== "/" && pathname.startsWith(item.href));
            return (
              <li key={item.href}>
                <Link
                  href={item.href}
                  className={cn(
                    "flex items-center gap-3 rounded-lg px-3 py-2.5 text-sm font-medium transition-colors",
                    isActive
                      ? "bg-blue-500/10 text-blue-400"
                      : "text-slate-400 hover:bg-slate-800 hover:text-slate-200"
                  )}
                >
                  <item.icon className="h-5 w-5" />
                  {item.label}
                </Link>
              </li>
            );
          })}
        </ul>
      </nav>

      {/* Footer with user */}
      <div className="absolute bottom-4 left-0 right-0 px-4 space-y-2">
        <UserFooter />
        <div className="rounded-lg border border-slate-800 bg-slate-900/50 p-2.5">
          <p className="text-[10px] text-slate-600">
            Data fra Yahoo Finance. Ikke finansiell rådgivning.
          </p>
        </div>
      </div>
    </aside>
  );
}

function UserFooter() {
  const { user, logout } = useAuth();

  if (!user) {
    return (
      <Link
        href="/login"
        className="flex items-center gap-2 rounded-lg border border-slate-800 bg-slate-900/50 p-3 text-sm text-slate-400 hover:border-blue-500/50 hover:text-blue-400"
      >
        <div className="flex h-7 w-7 items-center justify-center rounded-full bg-slate-800 text-xs">
          👤
        </div>
        Logg inn / Registrer
      </Link>
    );
  }

  return (
    <div className="rounded-lg border border-slate-800 bg-slate-900/50 p-3">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <div className="flex h-7 w-7 items-center justify-center rounded-full bg-blue-500/20 text-xs font-bold text-blue-400">
            {user.name.charAt(0).toUpperCase()}
          </div>
          <div>
            <p className="text-xs font-medium text-white">{user.name}</p>
            <p className="text-[10px] text-slate-500">{user.email}</p>
          </div>
        </div>
        <button
          onClick={logout}
          className="text-[10px] text-slate-600 hover:text-red-400"
        >
          Logg ut
        </button>
      </div>
    </div>
  );
}
