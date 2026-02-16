"""Initial schema - all tables

Revision ID: e213bd719de9
Revises: 
Create Date: 2026-02-16 21:08:51.074340

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = 'e213bd719de9'
down_revision: Union[str, Sequence[str], None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    """Create all initial tables."""
    op.create_table(
        'stocks',
        sa.Column('id', sa.Integer(), autoincrement=True, nullable=False),
        sa.Column('ticker', sa.String(20), nullable=False),
        sa.Column('name', sa.String(200), nullable=False),
        sa.Column('sector', sa.String(100), nullable=True),
        sa.Column('industry', sa.String(200), nullable=True),
        sa.Column('market', sa.String(50), nullable=True),
        sa.Column('currency', sa.String(10), nullable=True),
        sa.Column('current_price', sa.Float(), nullable=True),
        sa.Column('market_cap', sa.Float(), nullable=True),
        sa.Column('last_updated', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.Column('created_at', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.PrimaryKeyConstraint('id'),
    )
    op.create_index('ix_stocks_ticker', 'stocks', ['ticker'], unique=True)

    op.create_table(
        'recommendations',
        sa.Column('id', sa.Integer(), autoincrement=True, nullable=False),
        sa.Column('stock_id', sa.Integer(), nullable=False),
        sa.Column('recommendation_type', sa.String(10), nullable=False),
        sa.Column('confidence', sa.Integer(), nullable=False),
        sa.Column('price_at_recommendation', sa.Float(), nullable=False),
        sa.Column('technical_score', sa.Float(), nullable=False),
        sa.Column('fundamental_score', sa.Float(), nullable=False),
        sa.Column('reasoning', sa.Text(), nullable=False),
        sa.Column('created_at', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.ForeignKeyConstraint(['stock_id'], ['stocks.id']),
        sa.PrimaryKeyConstraint('id'),
    )
    op.create_index('ix_recommendations_stock_id', 'recommendations', ['stock_id'])

    op.create_table(
        'recommendation_outcomes',
        sa.Column('id', sa.Integer(), autoincrement=True, nullable=False),
        sa.Column('recommendation_id', sa.Integer(), nullable=False),
        sa.Column('price_after_7d', sa.Float(), nullable=True),
        sa.Column('price_after_30d', sa.Float(), nullable=True),
        sa.Column('price_after_90d', sa.Float(), nullable=True),
        sa.Column('return_7d_pct', sa.Float(), nullable=True),
        sa.Column('return_30d_pct', sa.Float(), nullable=True),
        sa.Column('return_90d_pct', sa.Float(), nullable=True),
        sa.Column('was_correct_7d', sa.Boolean(), nullable=True),
        sa.Column('was_correct_30d', sa.Boolean(), nullable=True),
        sa.Column('was_correct_90d', sa.Boolean(), nullable=True),
        sa.Column('evaluated_at', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.ForeignKeyConstraint(['recommendation_id'], ['recommendations.id']),
        sa.PrimaryKeyConstraint('id'),
    )
    op.create_index('ix_recommendation_outcomes_recommendation_id', 'recommendation_outcomes', ['recommendation_id'])

    op.create_table(
        'portfolios',
        sa.Column('id', sa.Integer(), autoincrement=True, nullable=False),
        sa.Column('name', sa.String(100), nullable=False),
        sa.Column('created_at', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.PrimaryKeyConstraint('id'),
    )

    op.create_table(
        'portfolio_holdings',
        sa.Column('id', sa.Integer(), autoincrement=True, nullable=False),
        sa.Column('portfolio_id', sa.Integer(), nullable=False),
        sa.Column('stock_id', sa.Integer(), nullable=False),
        sa.Column('shares', sa.Float(), nullable=False),
        sa.Column('buy_price', sa.Float(), nullable=False),
        sa.Column('buy_date', sa.Date(), nullable=False),
        sa.Column('created_at', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.ForeignKeyConstraint(['portfolio_id'], ['portfolios.id']),
        sa.ForeignKeyConstraint(['stock_id'], ['stocks.id']),
        sa.PrimaryKeyConstraint('id'),
    )

    op.create_table(
        'watchlists',
        sa.Column('id', sa.Integer(), autoincrement=True, nullable=False),
        sa.Column('stock_id', sa.Integer(), nullable=False),
        sa.Column('added_at', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.ForeignKeyConstraint(['stock_id'], ['stocks.id']),
        sa.PrimaryKeyConstraint('id'),
    )

    op.create_table(
        'alerts',
        sa.Column('id', sa.Integer(), autoincrement=True, nullable=False),
        sa.Column('stock_id', sa.Integer(), nullable=False),
        sa.Column('condition_type', sa.String(50), nullable=False),
        sa.Column('threshold', sa.Float(), nullable=True),
        sa.Column('is_active', sa.Boolean(), nullable=False, server_default='1'),
        sa.Column('last_triggered', sa.DateTime(timezone=True), nullable=True),
        sa.Column('created_at', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.ForeignKeyConstraint(['stock_id'], ['stocks.id']),
        sa.PrimaryKeyConstraint('id'),
    )

    op.create_table(
        'paper_trades',
        sa.Column('id', sa.Integer(), autoincrement=True, nullable=False),
        sa.Column('stock_id', sa.Integer(), nullable=False),
        sa.Column('trade_type', sa.String(10), nullable=False),
        sa.Column('shares', sa.Float(), nullable=False),
        sa.Column('entry_price', sa.Float(), nullable=False),
        sa.Column('exit_price', sa.Float(), nullable=True),
        sa.Column('is_open', sa.Boolean(), nullable=False, server_default='1'),
        sa.Column('pnl', sa.Float(), nullable=True),
        sa.Column('pnl_pct', sa.Float(), nullable=True),
        sa.Column('note', sa.Text(), nullable=True),
        sa.Column('opened_at', sa.DateTime(timezone=True), server_default=sa.func.now()),
        sa.Column('closed_at', sa.DateTime(timezone=True), nullable=True),
        sa.ForeignKeyConstraint(['stock_id'], ['stocks.id']),
        sa.PrimaryKeyConstraint('id'),
    )


def downgrade() -> None:
    """Drop all tables."""
    op.drop_table('paper_trades')
    op.drop_table('alerts')
    op.drop_table('watchlists')
    op.drop_table('portfolio_holdings')
    op.drop_table('portfolios')
    op.drop_table('recommendation_outcomes')
    op.drop_index('ix_recommendations_stock_id', 'recommendations')
    op.drop_table('recommendations')
    op.drop_index('ix_stocks_ticker', 'stocks')
    op.drop_table('stocks')
