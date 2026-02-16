"""In-memory TTL cache for yfinance API responses.

Reduces API calls and improves response times. Each cache entry has a
configurable TTL (time-to-live) after which it expires automatically.

For production, replace with Redis using the same interface.
"""

import logging
import time
from threading import Lock
from typing import Any

logger = logging.getLogger(__name__)


class TTLCache:
    """Thread-safe in-memory cache with per-key TTL."""

    def __init__(self, default_ttl: int = 300):
        """
        Args:
            default_ttl: Default time-to-live in seconds (5 min).
        """
        self._store: dict[str, tuple[Any, float]] = {}
        self._lock = Lock()
        self.default_ttl = default_ttl
        self._hits = 0
        self._misses = 0

    def get(self, key: str) -> Any | None:
        """Get a value from cache. Returns None if expired or missing."""
        with self._lock:
            if key in self._store:
                value, expires_at = self._store[key]
                if time.time() < expires_at:
                    self._hits += 1
                    return value
                else:
                    # Expired — remove it
                    del self._store[key]
            self._misses += 1
            return None

    def set(self, key: str, value: Any, ttl: int | None = None) -> None:
        """Store a value with TTL."""
        ttl = ttl if ttl is not None else self.default_ttl
        with self._lock:
            self._store[key] = (value, time.time() + ttl)

    def delete(self, key: str) -> None:
        """Remove a key from cache."""
        with self._lock:
            self._store.pop(key, None)

    def clear(self) -> None:
        """Clear all cached entries."""
        with self._lock:
            self._store.clear()
            self._hits = 0
            self._misses = 0

    def cleanup(self) -> int:
        """Remove all expired entries. Returns count of removed items."""
        now = time.time()
        removed = 0
        with self._lock:
            expired_keys = [k for k, (_, exp) in self._store.items() if now >= exp]
            for k in expired_keys:
                del self._store[k]
                removed += 1
        return removed

    @property
    def stats(self) -> dict:
        """Return cache statistics."""
        with self._lock:
            total = self._hits + self._misses
            hit_rate = (self._hits / total * 100) if total > 0 else 0
            return {
                "size": len(self._store),
                "hits": self._hits,
                "misses": self._misses,
                "hit_rate_pct": round(hit_rate, 1),
            }


# Global cache instances with different TTLs
# Quote data: 60s (changes frequently)
quote_cache = TTLCache(default_ttl=60)

# Info/metrics data: 5 min (fundamental data is slow-changing)
info_cache = TTLCache(default_ttl=300)

# Historical data: 10 min (daily bars don't change intraday)
history_cache = TTLCache(default_ttl=600)

# News: 15 min
news_cache = TTLCache(default_ttl=900)
