"""APScheduler setup for daily recommendation evaluation."""

import logging
from apscheduler.schedulers.asyncio import AsyncIOScheduler
from apscheduler.triggers.cron import CronTrigger

from app.db.database import async_session
from app.services.evaluator import evaluate_recommendations

logger = logging.getLogger(__name__)

scheduler = AsyncIOScheduler()


async def daily_evaluation_job():
    """Run daily evaluation of all recommendation outcomes."""
    logger.info("Running scheduled recommendation evaluation...")
    try:
        async with async_session() as db:
            updated = await evaluate_recommendations(db)
            logger.info(f"Scheduled evaluation complete: {updated} outcomes updated")
    except Exception as e:
        logger.error(f"Scheduled evaluation failed: {e}")


def start_scheduler():
    """Start the APScheduler with daily evaluation job."""
    # Run every day at 18:00 UTC (after markets close)
    scheduler.add_job(
        daily_evaluation_job,
        CronTrigger(hour=18, minute=0),
        id="daily_evaluation",
        name="Daglig evaluering av anbefalinger",
        replace_existing=True,
    )
    scheduler.start()
    logger.info("Scheduler started — daily evaluation at 18:00 UTC")


def stop_scheduler():
    """Shut down the scheduler gracefully."""
    if scheduler.running:
        scheduler.shutdown()
        logger.info("Scheduler stopped")
