# Requirements

## Goal
Build a C++ backend backtester/simulator for a value-averaging style strategy. All major behavior should be configurable via JSON so both the observed rules and the unknown corner cases can be tested as alternative modes.

## Core daily decision hierarchy

### 1) Long-term reset check
If current position value exceeds baseline cost by a configurable long-term target, the strategy should:
- sell all shares
- move proceeds to cash
- calculate profit
- route profit according to retention settings
- reset cost basis / restart the allocation

### 2) Daily value-average check
If the reset rule is not triggered, and current growth exceeds a configurable daily target path, the strategy should:
- sell only the overage
- compute the exact excess above target
- move captured profit to cash
- apply profit-routing rules
- keep the remaining invested shares

### 3) Daily incremental buy check
If neither sell rule triggers, and cash utilization is below its limit, the strategy should:
- execute an incremental buy
- reduce cash
- increase shares
- update average price / cost basis

### 4) Profit routing
After sell events, profit should be split into configurable buckets:
- taxes retained
- profit retained / withdrawal bucket
- reinvestable cash

## Required configurability
JSON settings should support at least:
- long-term target
- daily target / target-path growth
- cash utilization limit
- aggressiveness / deployment speed
- buy-sizing mode
- profit-routing percentages
- tax-retention percentage
- withdrawal / profit-retention percentage
- reinvestment behavior
- rebalancing behavior when fully utilized
- sell-at-loss policy / wash-sale-aware options
- extreme gap handling behavior
- one-buy-per-day vs catch-up behavior

## Open design questions to model explicitly
The simulator should expose options for unresolved behavior instead of hardcoding one assumption:
- what happens at 100% cash utilization
- how buy size scales with dip severity
- whether buys use fixed size or dynamic size
- whether the strategy may rebalance across allocations
- whether tax lots are tracked
- whether selling at a loss is allowed
- how to handle large gap-ups and gap-downs
- whether the engine can execute multiple catch-up buys or only one incremental buy per day

## Simulation requirements
- use synthetic price generation based on Geometric Brownian Motion (GBM) with jump diffusion
- parameterize thresholds in ATR units instead of raw percentages where possible
- support Monte Carlo runs
- target 500 simulations per run
- support comparing alternative policy choices across the unresolved corner cases

## Optimization requirements
- tune parameters with a Bayesian optimization library
- allow optimization over both numeric thresholds and discrete policy modes
- make the objective configurable (examples: CAGR, Sharpe, max drawdown, terminal wealth, risk-adjusted return)

## Suggested initial technical structure
- C++ core engine
- JSON config loader
- simulation module
- synthetic price generator
- metrics/reporting module
- optimization wrapper
- reproducible run outputs

## Inputs and outputs
Inputs:
- JSON strategy config
- JSON simulation config
- random seed settings
- synthetic price model parameters

Outputs:
- trade log
- daily portfolio/equity series
- summary metrics
- Monte Carlo aggregate statistics
- optimization results

## Immediate next implementation tasks
1. Define JSON schemas for strategy and simulation settings.
2. Define the daily engine state machine.
3. Implement synthetic GBM + jump-diffusion price generation.
4. Implement one baseline policy set that matches the known rules.
5. Add alternative policy switches for the unresolved corner cases.
6. Add Monte Carlo runner.
7. Add Bayesian optimization integration.
