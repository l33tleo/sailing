"use client";

import { useState } from "react";
import { useRouter } from "next/navigation";
import { Card, CardHeader, CardTitle, CardDescription } from "@/components/ui/Card";
import { useAuth } from "@/lib/auth";
import { BarChart3 } from "lucide-react";

export default function LoginPage() {
  const router = useRouter();
  const { login, register, user } = useAuth();

  const [isRegister, setIsRegister] = useState(false);
  const [email, setEmail] = useState("");
  const [name, setName] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  // If already logged in, redirect
  if (user) {
    router.push("/");
    return null;
  }

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    setError(null);
    setLoading(true);

    try {
      if (isRegister) {
        await register(email, name, password);
      } else {
        await login(email, password);
      }
      router.push("/");
    } catch (e: any) {
      setError(e.message);
    } finally {
      setLoading(false);
    }
  }

  return (
    <div className="flex min-h-[80vh] items-center justify-center">
      <div className="w-full max-w-md">
        {/* Logo */}
        <div className="mb-8 flex items-center justify-center gap-3">
          <BarChart3 className="h-10 w-10 text-blue-500" />
          <div>
            <h1 className="text-2xl font-bold text-white">AksjeAnalyse</h1>
            <p className="text-sm text-slate-500">Analyse & Anbefalinger</p>
          </div>
        </div>

        <Card>
          <CardHeader>
            <CardTitle>
              {isRegister ? "Opprett konto" : "Logg inn"}
            </CardTitle>
            <CardDescription>
              {isRegister
                ? "Registrer deg for å lagre personlige porteføljer og anbefalinger"
                : "Logg inn for å få tilgang til dine porteføljer"}
            </CardDescription>
          </CardHeader>

          {error && (
            <div className="mb-4 rounded-lg border border-red-500/30 bg-red-500/10 p-3 text-sm text-red-400">
              {error}
            </div>
          )}

          <form onSubmit={handleSubmit} className="space-y-4">
            {isRegister && (
              <div>
                <label className="mb-1 block text-sm text-slate-400">
                  Navn
                </label>
                <input
                  type="text"
                  value={name}
                  onChange={(e) => setName(e.target.value)}
                  placeholder="Ola Nordmann"
                  required
                  className="w-full rounded-lg border border-slate-700 bg-slate-800 px-4 py-3 text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
                />
              </div>
            )}

            <div>
              <label className="mb-1 block text-sm text-slate-400">
                E-post
              </label>
              <input
                type="email"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                placeholder="din@epost.no"
                required
                className="w-full rounded-lg border border-slate-700 bg-slate-800 px-4 py-3 text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
              />
            </div>

            <div>
              <label className="mb-1 block text-sm text-slate-400">
                Passord
              </label>
              <input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder="Minst 6 tegn"
                required
                minLength={6}
                className="w-full rounded-lg border border-slate-700 bg-slate-800 px-4 py-3 text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none"
              />
            </div>

            <button
              type="submit"
              disabled={loading}
              className="w-full rounded-lg bg-blue-600 py-3 font-medium text-white transition-colors hover:bg-blue-500 disabled:opacity-50"
            >
              {loading
                ? "Venter..."
                : isRegister
                ? "Opprett konto"
                : "Logg inn"}
            </button>
          </form>

          <div className="mt-4 text-center">
            <button
              onClick={() => {
                setIsRegister(!isRegister);
                setError(null);
              }}
              className="text-sm text-slate-400 hover:text-blue-400"
            >
              {isRegister
                ? "Har du allerede konto? Logg inn"
                : "Ny bruker? Opprett konto"}
            </button>
          </div>
        </Card>
      </div>
    </div>
  );
}
