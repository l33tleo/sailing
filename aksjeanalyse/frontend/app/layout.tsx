import type { Metadata } from "next";
import "./globals.css";
import Sidebar from "@/components/layout/Sidebar";
import { AuthProvider } from "@/lib/auth";

export const metadata: Metadata = {
  title: "AksjeAnalyse — Aksjeanalyse og Anbefalinger",
  description:
    "Analyser aksjer med teknisk og fundamental analyse. Få kjøp/hold/selg-anbefalinger og spor treffsikkerhet over tid.",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="nb">
      <body className="antialiased">
        <AuthProvider>
          <Sidebar />
          <main className="ml-64 min-h-screen p-6">{children}</main>
        </AuthProvider>
      </body>
    </html>
  );
}
