"use client";

import { useRef, useState } from "react";
import Link from "next/link";
import { Card } from "@/components/ui/Card";
import { RecBadge } from "@/components/ui/Badge";
import { sendChatMessage, type ChatResponse } from "@/lib/api";
import { Send, Bot, User, Sparkles, TrendingUp } from "lucide-react";

interface Message {
  id: number;
  role: "user" | "assistant";
  content: string;
  ticker?: string | null;
  recommendation?: string | null;
  score?: number | null;
}

const EXAMPLE_QUESTIONS = [
  "Bør jeg kjøpe Equinor?",
  "Hva er kursen på AAPL?",
  "Analyser NVDA teknisk",
  "Er Microsoft fundamental god?",
  "Hva med DNB?",
];

export default function ChatPage() {
  const [messages, setMessages] = useState<Message[]>([
    {
      id: 0,
      role: "assistant",
      content:
        "Hei! 👋 Jeg er din aksjeanalytiker. Still meg spørsmål om aksjer, så kjører jeg en analyse.\n\n" +
        "Prøv for eksempel:\n" +
        "• «Bør jeg kjøpe Equinor?»\n" +
        "• «Hva er kursen på AAPL?»\n" +
        "• «Analyser NVDA teknisk»\n" +
        "• «Er Microsoft fundamental god?»",
    },
  ]);
  const [input, setInput] = useState("");
  const [loading, setLoading] = useState(false);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const nextId = useRef(1);

  async function handleSend(text?: string) {
    const msg = (text || input).trim();
    if (!msg || loading) return;

    const userMsg: Message = {
      id: nextId.current++,
      role: "user",
      content: msg,
    };
    setMessages((prev) => [...prev, userMsg]);
    setInput("");
    setLoading(true);

    try {
      const response = await sendChatMessage(msg);
      const assistantMsg: Message = {
        id: nextId.current++,
        role: "assistant",
        content: response.reply,
        ticker: response.ticker,
        recommendation: response.recommendation,
        score: response.score,
      };
      setMessages((prev) => [...prev, assistantMsg]);
    } catch (e: any) {
      setMessages((prev) => [
        ...prev,
        {
          id: nextId.current++,
          role: "assistant",
          content: `Beklager, noe gikk galt: ${e.message}. Sørg for at backend kjører.`,
        },
      ]);
    } finally {
      setLoading(false);
      setTimeout(() => messagesEndRef.current?.scrollIntoView({ behavior: "smooth" }), 100);
    }
  }

  function handleKeyDown(e: React.KeyboardEvent) {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  }

  return (
    <div className="flex h-[calc(100vh-3rem)] flex-col">
      {/* Header */}
      <div className="mb-4">
        <div className="flex items-center gap-2">
          <Sparkles className="h-6 w-6 text-blue-400" />
          <h1 className="text-2xl font-bold text-white">AI Aksje-chat</h1>
        </div>
        <p className="text-slate-400">
          Still spørsmål om aksjer — jeg analyserer og gir anbefalinger
        </p>
      </div>

      {/* Messages area */}
      <div className="flex-1 overflow-y-auto rounded-xl border border-slate-800 bg-[#0d1117] p-4">
        <div className="space-y-4">
          {messages.map((msg) => (
            <div
              key={msg.id}
              className={`flex gap-3 ${
                msg.role === "user" ? "justify-end" : "justify-start"
              }`}
            >
              {msg.role === "assistant" && (
                <div className="flex h-8 w-8 flex-shrink-0 items-center justify-center rounded-full bg-blue-500/20">
                  <Bot className="h-4 w-4 text-blue-400" />
                </div>
              )}
              <div
                className={`max-w-[75%] rounded-xl px-4 py-3 ${
                  msg.role === "user"
                    ? "bg-blue-600 text-white"
                    : "bg-slate-800 text-slate-200"
                }`}
              >
                <pre className="whitespace-pre-wrap font-sans text-sm leading-relaxed">
                  {msg.content}
                </pre>

                {/* Metadata bar for assistant messages with analysis */}
                {msg.role === "assistant" && (msg.ticker || msg.recommendation) && (
                  <div className="mt-3 flex flex-wrap items-center gap-2 border-t border-slate-700 pt-2">
                    {msg.recommendation && (
                      <RecBadge type={msg.recommendation} />
                    )}
                    {msg.score != null && (
                      <span className="text-xs text-slate-400">
                        Score: {msg.score}/100
                      </span>
                    )}
                    {msg.ticker && (
                      <Link
                        href={`/aksjer/${encodeURIComponent(msg.ticker)}`}
                        className="flex items-center gap-1 rounded-md bg-slate-700 px-2 py-1 text-xs text-blue-400 hover:bg-slate-600"
                      >
                        <TrendingUp className="h-3 w-3" />
                        Se full analyse
                      </Link>
                    )}
                  </div>
                )}
              </div>
              {msg.role === "user" && (
                <div className="flex h-8 w-8 flex-shrink-0 items-center justify-center rounded-full bg-slate-700">
                  <User className="h-4 w-4 text-slate-300" />
                </div>
              )}
            </div>
          ))}

          {loading && (
            <div className="flex gap-3">
              <div className="flex h-8 w-8 flex-shrink-0 items-center justify-center rounded-full bg-blue-500/20">
                <Bot className="h-4 w-4 text-blue-400" />
              </div>
              <div className="rounded-xl bg-slate-800 px-4 py-3">
                <div className="flex items-center gap-2 text-sm text-slate-400">
                  <div className="h-2 w-2 animate-pulse rounded-full bg-blue-400" />
                  <div className="animation-delay-200 h-2 w-2 animate-pulse rounded-full bg-blue-400" />
                  <div className="animation-delay-400 h-2 w-2 animate-pulse rounded-full bg-blue-400" />
                  <span className="ml-1">Analyserer...</span>
                </div>
              </div>
            </div>
          )}

          <div ref={messagesEndRef} />
        </div>
      </div>

      {/* Quick question buttons */}
      {messages.length <= 1 && (
        <div className="mt-3 flex flex-wrap gap-2">
          {EXAMPLE_QUESTIONS.map((q) => (
            <button
              key={q}
              onClick={() => handleSend(q)}
              disabled={loading}
              className="rounded-full border border-slate-700 px-3 py-1.5 text-xs text-slate-400 transition-colors hover:border-blue-500/50 hover:text-blue-400 disabled:opacity-50"
            >
              {q}
            </button>
          ))}
        </div>
      )}

      {/* Input */}
      <div className="mt-3 flex gap-2">
        <input
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="Still et spørsmål om en aksje..."
          disabled={loading}
          className="flex-1 rounded-xl border border-slate-700 bg-slate-800/50 px-4 py-3 text-white placeholder:text-slate-500 focus:border-blue-500 focus:outline-none focus:ring-1 focus:ring-blue-500 disabled:opacity-50"
        />
        <button
          onClick={() => handleSend()}
          disabled={loading || !input.trim()}
          className="flex items-center justify-center rounded-xl bg-blue-600 px-4 py-3 text-white transition-colors hover:bg-blue-500 disabled:opacity-50"
        >
          <Send className="h-5 w-5" />
        </button>
      </div>
    </div>
  );
}
