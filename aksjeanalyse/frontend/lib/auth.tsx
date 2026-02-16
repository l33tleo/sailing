"use client";

import { createContext, useContext, useEffect, useState, ReactNode } from "react";

const API_BASE = "/api";

interface User {
  id: number;
  email: string;
  name: string;
  created_at: string;
}

interface AuthContextType {
  user: User | null;
  token: string | null;
  loading: boolean;
  login: (email: string, password: string) => Promise<void>;
  register: (email: string, name: string, password: string) => Promise<void>;
  logout: () => void;
}

const AuthContext = createContext<AuthContextType>({
  user: null,
  token: null,
  loading: true,
  login: async () => {},
  register: async () => {},
  logout: () => {},
});

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<User | null>(null);
  const [token, setToken] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  // Load token from localStorage on mount
  useEffect(() => {
    const saved = localStorage.getItem("aksjeanalyse_token");
    if (saved) {
      setToken(saved);
      fetchProfile(saved);
    } else {
      setLoading(false);
    }
  }, []);

  async function fetchProfile(t: string) {
    try {
      const res = await fetch(`${API_BASE}/auth/me`, {
        headers: { Authorization: `Bearer ${t}` },
      });
      if (res.ok) {
        const data = await res.json();
        setUser(data);
        setToken(t);
      } else {
        // Token expired/invalid
        localStorage.removeItem("aksjeanalyse_token");
        setToken(null);
        setUser(null);
      }
    } catch {
      // Network error — keep token, might work later
    } finally {
      setLoading(false);
    }
  }

  async function login(email: string, password: string) {
    const res = await fetch(`${API_BASE}/auth/login`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ email, password }),
    });
    if (!res.ok) {
      const err = await res.json().catch(() => ({ detail: "Innlogging feilet" }));
      throw new Error(err.detail || "Innlogging feilet");
    }
    const data = await res.json();
    localStorage.setItem("aksjeanalyse_token", data.access_token);
    setToken(data.access_token);
    setUser(data.user);
  }

  async function register(email: string, name: string, password: string) {
    const res = await fetch(`${API_BASE}/auth/register`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ email, name, password }),
    });
    if (!res.ok) {
      const err = await res.json().catch(() => ({ detail: "Registrering feilet" }));
      throw new Error(err.detail || "Registrering feilet");
    }
    const data = await res.json();
    localStorage.setItem("aksjeanalyse_token", data.access_token);
    setToken(data.access_token);
    setUser(data.user);
  }

  function logout() {
    localStorage.removeItem("aksjeanalyse_token");
    setToken(null);
    setUser(null);
  }

  return (
    <AuthContext.Provider value={{ user, token, loading, login, register, logout }}>
      {children}
    </AuthContext.Provider>
  );
}

export function useAuth() {
  return useContext(AuthContext);
}
