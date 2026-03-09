#!/usr/bin/env python3
"""Manual test script for the Python memory reader MVP."""

import argparse
import logging
import sys
import time
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from core.memory_reader import IL2CPPMemoryReader


logging.basicConfig(
    level=logging.INFO,
    format="[%(levelname)s] %(message)s",
)
logger = logging.getLogger(__name__)


def parse_address(value: str) -> int:
    """Parse a decimal or hex address string.

    Args:
        value: Address string from the CLI.

    Returns:
        Parsed absolute address.

    Raises:
        argparse.ArgumentTypeError: If the value is invalid.
    """
    try:
        parsed = int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid address: {value}") from exc

    if parsed <= 0:
        raise argparse.ArgumentTypeError("address must be a positive integer")

    return parsed


def build_parser() -> argparse.ArgumentParser:
    """Create the CLI parser for the manual test script."""
    parser = argparse.ArgumentParser(description="Manual Monopoly GO memory reader test")
    parser.add_argument(
        "--dice-line-item-addr",
        required=True,
        type=parse_address,
        help=(
            "Absolute address of the live WithBuddies.Common.UserInventoryLineItemDto "
            "for Tophat.Common.Economy.TophatCommodityKeys.ROLLS"
        ),
    )
    parser.add_argument(
        "--reads",
        type=int,
        default=10,
        help="Number of repeated dice reads to perform after the first read",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=0.5,
        help="Seconds to wait between repeated reads",
    )
    return parser


def main() -> int:
    """Run the manual MVP test sequence."""
    args = build_parser().parse_args()

    logger.info("=== Memory Reader MVP Test ===")
    logger.info("Manual dice line item address: 0x%x", args.dice_line_item_addr)

    reader = IL2CPPMemoryReader(dice_line_item_address=args.dice_line_item_addr)

    logger.info("Initializing memory reader...")
    if not reader.initialize():
        logger.error("Initialization failed.")
        logger.error("Check that Monopoly GO is running and root access is available.")
        return 1

    logger.info("Initialization succeeded.")
    logger.info("PID: %s", reader.pid)
    logger.info("Base address: 0x%x", reader.base_address)

    try:
        logger.info("--- Single Read Test ---")
        dice = reader.get_dice_count()
        if dice is None:
            logger.error("Dice read failed. Verify the manual line-item pointer.")
        else:
            logger.info("Dice count: %s", dice)
            if 0 <= dice <= 999999:
                logger.info("Value looks plausible.")
            else:
                logger.warning("Value looks suspicious. Re-check the anchor pointer.")

        logger.info("--- Continuous Read Test ---")
        for index in range(args.reads):
            dice = reader.get_dice_count()
            if dice is None:
                logger.warning("[%s/%s] Dice read failed", index + 1, args.reads)
            else:
                logger.info("[%s/%s] Dice: %s", index + 1, args.reads, dice)
            time.sleep(args.interval)

    except KeyboardInterrupt:
        logger.warning("Interrupted by user.")
    except Exception as exc:  # pragma: no cover - manual script safety net
        logger.error("Unexpected error: %s", exc)
        return 1
    finally:
        reader.close()
        logger.info("Memory reader closed.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
