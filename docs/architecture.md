# Architecture

## Current modular layout

- `include/cpp_backtester/types.hpp` — shared domain types
- `include/cpp_backtester/config.hpp` + `src/config.cpp` — JSON config loading
- `include/cpp_backtester/io.hpp` + `src/io.cpp` — file I/O helpers
- `include/cpp_backtester/price_generator.hpp` + `src/price_generator.cpp` — synthetic GBM + jump-diffusion path generation
- `include/cpp_backtester/simulator.hpp` + `src/simulator.cpp` — strategy engine and simulation artifact writing
- `include/cpp_backtester/monte_carlo.hpp` + `src/monte_carlo.cpp` — Monte Carlo aggregation
- `include/cpp_backtester/optimizer.hpp` + `src/optimizer.cpp` — optimization backend abstraction
- `include/cpp_backtester/rolling_optimizer.hpp` + `src/rolling_optimizer.cpp` — rolling-window pass controller with parallel window execution
- `src/main.cpp` — thin CLI only

## Why this split

This keeps the CLI thin and makes it easy to:
- swap in a stronger config system later
- replace the placeholder optimizer with a real Bayesian backend
- add modules without turning `main.cpp` into a giant file
- build reusable library targets for tests and future apps

## Planned next split points

- strategy rules into separate policy objects
- metrics/reporting into a dedicated module
- real JSON parsing dependency
- real dlib-backed optimizer backend behind the optimizer interface
