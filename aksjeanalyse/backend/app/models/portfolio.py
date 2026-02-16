"""Portfolio models — track user stock holdings."""

from datetime import date, datetime

from sqlalchemy import Date, DateTime, Float, ForeignKey, Integer, String, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db.database import Base


class Portfolio(Base):
    __tablename__ = "portfolios"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String(100), nullable=False, default="Min portefølje")
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )

    # Relationships
    holdings = relationship("PortfolioHolding", back_populates="portfolio", lazy="selectin")

    def __repr__(self) -> str:
        return f"<Portfolio(name={self.name})>"


class PortfolioHolding(Base):
    __tablename__ = "portfolio_holdings"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    portfolio_id: Mapped[int] = mapped_column(
        ForeignKey("portfolios.id"), nullable=False, index=True
    )
    stock_id: Mapped[int] = mapped_column(
        ForeignKey("stocks.id"), nullable=False, index=True
    )
    shares: Mapped[float] = mapped_column(Float, nullable=False)
    buy_price: Mapped[float] = mapped_column(Float, nullable=False)
    buy_date: Mapped[date] = mapped_column(Date, nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )

    # Relationships
    portfolio = relationship("Portfolio", back_populates="holdings")
    stock = relationship("Stock", lazy="selectin")

    def __repr__(self) -> str:
        return f"<PortfolioHolding(stock_id={self.stock_id}, shares={self.shares})>"
