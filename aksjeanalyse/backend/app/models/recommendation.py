"""Recommendation and outcome models — core of the analysis tracking system."""

from datetime import datetime
from enum import Enum as PyEnum

from sqlalchemy import DateTime, Enum, Float, ForeignKey, Integer, String, Text, func, Boolean
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db.database import Base


class RecommendationType(str, PyEnum):
    KJOP = "KJØP"
    HOLD = "HOLD"
    SELG = "SELG"


class Recommendation(Base):
    __tablename__ = "recommendations"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    stock_id: Mapped[int] = mapped_column(ForeignKey("stocks.id"), nullable=False, index=True)
    recommendation_type: Mapped[str] = mapped_column(String(10), nullable=False)
    confidence: Mapped[int] = mapped_column(Integer, nullable=False)  # 0-100
    price_at_recommendation: Mapped[float] = mapped_column(Float, nullable=False)
    technical_score: Mapped[float] = mapped_column(Float, nullable=False)  # 0-100
    fundamental_score: Mapped[float] = mapped_column(Float, nullable=False)  # 0-100
    reasoning: Mapped[str] = mapped_column(Text, nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )

    # Relationships
    stock = relationship("Stock", back_populates="recommendations", lazy="selectin")
    outcomes = relationship("RecommendationOutcome", back_populates="recommendation", lazy="selectin")

    def __repr__(self) -> str:
        return f"<Recommendation(stock_id={self.stock_id}, type={self.recommendation_type}, confidence={self.confidence})>"


class RecommendationOutcome(Base):
    __tablename__ = "recommendation_outcomes"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    recommendation_id: Mapped[int] = mapped_column(
        ForeignKey("recommendations.id"), nullable=False, index=True
    )
    price_after_7d: Mapped[float | None] = mapped_column(Float)
    price_after_30d: Mapped[float | None] = mapped_column(Float)
    price_after_90d: Mapped[float | None] = mapped_column(Float)
    return_7d_pct: Mapped[float | None] = mapped_column(Float)
    return_30d_pct: Mapped[float | None] = mapped_column(Float)
    return_90d_pct: Mapped[float | None] = mapped_column(Float)
    was_correct_7d: Mapped[bool | None] = mapped_column(Boolean)
    was_correct_30d: Mapped[bool | None] = mapped_column(Boolean)
    was_correct_90d: Mapped[bool | None] = mapped_column(Boolean)
    evaluated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now()
    )

    # Relationships
    recommendation = relationship("Recommendation", back_populates="outcomes")

    def __repr__(self) -> str:
        return f"<RecommendationOutcome(recommendation_id={self.recommendation_id})>"
