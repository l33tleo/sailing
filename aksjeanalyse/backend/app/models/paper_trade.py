"""Paper trading models — simulate trades without real money."""

from datetime import datetime
from sqlalchemy import Boolean, DateTime, Float, ForeignKey, Integer, String, Text, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db.database import Base


class PaperTrade(Base):
    __tablename__ = "paper_trades"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    stock_id: Mapped[int] = mapped_column(ForeignKey("stocks.id"), nullable=False, index=True)
    trade_type: Mapped[str] = mapped_column(String(10), nullable=False)  # "BUY" or "SELL"
    shares: Mapped[float] = mapped_column(Float, nullable=False)
    entry_price: Mapped[float] = mapped_column(Float, nullable=False)
    exit_price: Mapped[float | None] = mapped_column(Float)
    is_open: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)
    pnl: Mapped[float | None] = mapped_column(Float)  # profit/loss in currency
    pnl_pct: Mapped[float | None] = mapped_column(Float)  # profit/loss in %
    note: Mapped[str | None] = mapped_column(Text)
    opened_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )
    closed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))

    # Relationships
    stock = relationship("Stock", lazy="selectin")

    def __repr__(self) -> str:
        return f"<PaperTrade(stock_id={self.stock_id}, type={self.trade_type}, shares={self.shares})>"
