"""Tests for the TTL cache system."""

import time
from app.cache import TTLCache


def test_cache_set_and_get():
    """Values are stored and retrievable."""
    cache = TTLCache(default_ttl=60)
    cache.set("key1", "value1")
    assert cache.get("key1") == "value1"


def test_cache_miss_returns_none():
    """Missing keys return None."""
    cache = TTLCache(default_ttl=60)
    assert cache.get("nonexistent") is None


def test_cache_expiration():
    """Values expire after TTL."""
    cache = TTLCache(default_ttl=1)
    cache.set("key1", "value1", ttl=1)
    assert cache.get("key1") == "value1"
    time.sleep(1.1)
    assert cache.get("key1") is None


def test_cache_custom_ttl():
    """Custom TTL overrides default."""
    cache = TTLCache(default_ttl=60)
    cache.set("short", "value", ttl=1)
    cache.set("long", "value", ttl=300)
    time.sleep(1.1)
    assert cache.get("short") is None
    assert cache.get("long") == "value"


def test_cache_delete():
    """Values can be explicitly deleted."""
    cache = TTLCache(default_ttl=60)
    cache.set("key1", "value1")
    cache.delete("key1")
    assert cache.get("key1") is None


def test_cache_clear():
    """Clear removes all entries."""
    cache = TTLCache(default_ttl=60)
    cache.set("a", 1)
    cache.set("b", 2)
    cache.set("c", 3)
    cache.clear()
    assert cache.get("a") is None
    assert cache.get("b") is None
    assert cache.get("c") is None


def test_cache_stats():
    """Stats track hits and misses."""
    cache = TTLCache(default_ttl=60)
    cache.set("key1", "value1")
    cache.get("key1")  # hit
    cache.get("key1")  # hit
    cache.get("missing")  # miss

    stats = cache.stats
    assert stats["hits"] == 2
    assert stats["misses"] == 1
    assert stats["hit_rate_pct"] == 66.7
    assert stats["size"] == 1


def test_cache_cleanup():
    """Cleanup removes expired entries."""
    cache = TTLCache(default_ttl=1)
    cache.set("a", 1, ttl=1)
    cache.set("b", 2, ttl=300)
    time.sleep(1.1)
    removed = cache.cleanup()
    assert removed == 1
    assert cache.get("a") is None
    assert cache.get("b") == 2


def test_cache_overwrite():
    """Setting same key overwrites."""
    cache = TTLCache(default_ttl=60)
    cache.set("key1", "old")
    cache.set("key1", "new")
    assert cache.get("key1") == "new"


def test_cache_stores_complex_objects():
    """Cache works with dicts, lists, objects."""
    cache = TTLCache(default_ttl=60)
    data = {"ticker": "AAPL", "price": 255.78, "nested": [1, 2, 3]}
    cache.set("complex", data)
    result = cache.get("complex")
    assert result == data
    assert result["ticker"] == "AAPL"
