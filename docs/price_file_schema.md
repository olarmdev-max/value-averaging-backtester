# Price file schema

The file-backed input mode accepts one or more CSV files passed on the command line after the normal arguments.

## CLI shape

```bash
cpp_backtester <simulate|monte-carlo|optimize> <config.json> <output_dir> price1.csv [price2.csv ...]
```

## Expected schema

Preferred schema:

```csv
close
100.0
101.5
99.8
102.4
```

Rules:
- one close value per row
- a single column is expected
- the header `close` is supported and recommended
- blank lines are ignored
- values are parsed as floating-point closes

## What the engine does with this

Because the strategy currently consumes close and ATR-derived information, the loader converts the close-only series into internal bars by:
- using previous close as the next open
- deriving high/low from the open/close pair
- computing ATR from the resulting true-range series

## Use cases

- run on one historical ticker file
- run on a small basket of tickers
- optimize against a deterministic set of input histories instead of synthetic Monte Carlo paths
