"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { cn } from "@/lib/utils";
import {
  BarChart3,
  Search,
  TrendingUp,
  Target,
  Briefcase,
  LayoutDashboard,
  GitCompareArrows,
  Bell,
} from "lucide-react";

const navItems = [
  { href: "/", label: "Dashboard", icon: LayoutDashboard },
  { href: "/aksjer", label: "Aksjer", icon: Search },
  { href: "/anbefalinger", label: "Anbefalinger", icon: TrendingUp },
  { href: "/sammenligning", label: "Sammenligning", icon: GitCompareArrows },
  { href: "/scorecard", label: "Treffsikkerhet", icon: Target },
  { href: "/portefolje", label: "Portefølje", icon: Briefcase },
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

      {/* Footer */}
      <div className="absolute bottom-4 left-0 right-0 px-6">
        <div className="rounded-lg border border-slate-800 bg-slate-900/50 p-3">
          <p className="text-xs text-slate-500">
            Data fra Yahoo Finance. Anbefalinger er ikke finansiell rådgivning.
          </p>
        </div>
      </div>
    </aside>
  );
}
