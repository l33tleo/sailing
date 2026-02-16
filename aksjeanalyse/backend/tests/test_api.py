"""Integration tests for API endpoints using httpx TestClient."""

import pytest
import pytest_asyncio
from httpx import AsyncClient, ASGITransport
from app.main import app
from app.db.database import Base, get_db
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

# Test database
TEST_DB_URL = "sqlite+aiosqlite:///:memory:"
test_engine = create_async_engine(TEST_DB_URL, echo=False)
TestSession = async_sessionmaker(test_engine, class_=AsyncSession, expire_on_commit=False)


async def override_get_db():
    async with TestSession() as session:
        try:
            yield session
            await session.commit()
        except Exception:
            await session.rollback()
            raise


# Override the DB dependency
app.dependency_overrides[get_db] = override_get_db


@pytest_asyncio.fixture(autouse=True)
async def setup_db():
    """Create tables before each test, drop after."""
    async with test_engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield
    async with test_engine.begin() as conn:
        await conn.run_sync(Base.metadata.drop_all)


@pytest.mark.asyncio
async def test_health_check():
    """Health endpoint returns ok."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/health")
    assert response.status_code == 200
    assert response.json()["status"] == "ok"


@pytest.mark.asyncio
async def test_stock_search():
    """Stock search returns results for valid ticker."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/stocks/search?q=AAPL")
    assert response.status_code == 200
    data = response.json()
    assert isinstance(data, list)
    assert len(data) >= 1
    assert data[0]["ticker"] == "AAPL"


@pytest.mark.asyncio
async def test_stock_quote():
    """Stock quote returns price data."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/stocks/AAPL/quote")
    assert response.status_code == 200
    data = response.json()
    assert data["ticker"] == "AAPL"
    assert data["price"] > 0
    assert "change_percent" in data


@pytest.mark.asyncio
async def test_stock_history():
    """Stock history returns date/price arrays."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/stocks/AAPL/history?period=1mo")
    assert response.status_code == 200
    data = response.json()
    assert len(data["dates"]) > 0
    assert len(data["close"]) == len(data["dates"])


@pytest.mark.asyncio
async def test_technical_analysis():
    """Technical analysis returns score and indicators."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/analysis/AAPL/technical")
    assert response.status_code == 200
    data = response.json()
    assert data["ticker"] == "AAPL"
    assert 0 <= data["technical_score"] <= 100
    assert data["rsi"] is not None


@pytest.mark.asyncio
async def test_fundamental_analysis():
    """Fundamental analysis returns score and details."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/analysis/AAPL/fundamental")
    assert response.status_code == 200
    data = response.json()
    assert data["ticker"] == "AAPL"
    assert 0 <= data["fundamental_score"] <= 100


@pytest.mark.asyncio
async def test_full_analysis():
    """Full analysis returns recommendation."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/analysis/AAPL/full")
    assert response.status_code == 200
    data = response.json()
    assert data["recommendation_type"] in ("KJØP", "HOLD", "SELG")
    assert 0 <= data["combined_score"] <= 100
    assert data["reasoning"] != ""


@pytest.mark.asyncio
async def test_generate_and_list_recommendations():
    """Generate a recommendation, then list it."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        # Generate
        resp = await ac.post(
            "/api/recommendations/generate",
            json={"ticker": "AAPL"},
        )
        assert resp.status_code == 200
        rec = resp.json()
        assert rec["stock_ticker"] == "AAPL"
        assert rec["recommendation_type"] in ("KJØP", "HOLD", "SELG")

        # List
        resp2 = await ac.get("/api/recommendations?limit=10")
        assert resp2.status_code == 200
        recs = resp2.json()
        assert len(recs) >= 1
        assert recs[0]["stock_ticker"] == "AAPL"


@pytest.mark.asyncio
async def test_scorecard():
    """Scorecard returns statistics."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/recommendations/scorecard")
    assert response.status_code == 200
    data = response.json()
    assert "total_recommendations" in data
    assert "accuracy_30d" in data


@pytest.mark.asyncio
async def test_portfolio_workflow():
    """Create portfolio, add holding, list."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        # Create
        resp = await ac.post("/api/portfolios", json={"name": "Test"})
        assert resp.status_code == 200
        portfolio = resp.json()
        pid = portfolio["id"]

        # Add holding
        resp2 = await ac.post(
            f"/api/portfolios/{pid}/holdings",
            json={"ticker": "AAPL", "shares": 10, "buy_price": 150, "buy_date": "2024-01-01"},
        )
        assert resp2.status_code == 200
        holding = resp2.json()
        assert holding["ticker"] == "AAPL"
        assert holding["shares"] == 10

        # List
        resp3 = await ac.get(f"/api/portfolios/{pid}/holdings")
        assert resp3.status_code == 200
        assert len(resp3.json()) == 1


@pytest.mark.asyncio
async def test_market_indices():
    """Market indices returns at least some data."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/market/indices")
    assert response.status_code == 200
    data = response.json()
    assert len(data["indices"]) > 0


@pytest.mark.asyncio
async def test_cache_stats():
    """Cache stats endpoint works."""
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        response = await ac.get("/api/cache/stats")
    assert response.status_code == 200
    data = response.json()
    assert "quote_cache" in data
    assert "info_cache" in data
