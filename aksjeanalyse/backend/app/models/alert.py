"""Alert model — user-configured notifications for stock conditions."""

from datetime import datetime

from sqlalchemy import Boolean, DateTime, Float, ForeignKey, String, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db.database import Base


class Alert(Base):
    __tablename__ = "alerts"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    stock_id: Mapped[int] = mapped_column(
        ForeignKey("stocks.id"), nullable=False, index=True
    )
    condition_type: Mapped[str] = mapped_column(
        String(50), nullable=False
    )  # e.g. "RSI_BELOW", "PRICE_ABOVE", "NEW_RECOMMENDATION"
    threshold: Mapped[float | None] = mapped_column(Float)
    is_active: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)
    last_triggered: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )

    # Relationships
    stock = relationship("Stock", lazy="selectin")

    def __repr__(self) -> str:
        return f"<Alert(stock_id={self.stock_id}, condition={self.condition_type})>"
