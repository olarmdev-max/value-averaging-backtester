#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path

import yfinance as yf


def main() -> int:
    parser = argparse.ArgumentParser(description="Download Yahoo Finance history to CSV")
    parser.add_argument("ticker", help="Ticker symbol, e.g. SPY")
    parser.add_argument("output", help="Output CSV path")
    parser.add_argument("--start", default=None, help="Optional start date YYYY-MM-DD")
    parser.add_argument("--end", default=None, help="Optional end date YYYY-MM-DD")
    parser.add_argument(
        "--include-date",
        action="store_true",
        help="Write date,close instead of close-only output for rolling-window optimization",
    )
    args = parser.parse_args()

    df = yf.download(
        args.ticker,
        start=args.start,
        end=args.end,
        auto_adjust=True,
        progress=False,
        threads=False,
    )
    if df.empty:
        raise SystemExit(f"No Yahoo data returned for {args.ticker}")

    close = df["Close"]
    if hasattr(close, "columns"):
        close = close.iloc[:, 0]
    close = close.dropna()

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", newline="") as f:
        writer = csv.writer(f)
        if args.include_date:
            writer.writerow(["date", "close"])
            for date_value, close_value in close.items():
                writer.writerow([date_value.date().isoformat(), f"{float(close_value):.6f}"])
        else:
            writer.writerow(["close"])
            for value in close.to_list():
                writer.writerow([f"{float(value):.6f}"])

    print(out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
