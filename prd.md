# PRD: ATR-Scaled Target-Path Strategy

## Purpose

Define the next-generation strategy model for the backtester so it is closer to the original value-averaging intent while keeping the implementation simple, tunable, and practical for high-volatility growth assets.

This PRD replaces the current heuristic-only interpretation with a path-based model that:

- uses an explicit target path
- scales that path with ATR
- resets the path after a full reset sell
- uses aggressiveness to control shortfall catch-up speed
- ignores tax-lot and wash-sale complexity

## Design goals

1. Keep the daily action hierarchy simple and deterministic.
2. Preserve ATR as the preferred normalization mechanism over raw percentages.
3. Move from heuristic trimming/buy sizing toward explicit target-path math.
4. Keep the first implementation simple enough to optimize and reason about.
5. Avoid tax-engine complexity; treat the account like a retirement account.

## Strategy summary

The strategy evaluates each allocation once per day and follows this action order:

1. **Long-term reset check**
   - If the position is far enough above baseline, execute a full reset sell.
   - After the sell, reset the target path as if starting from day 1.

2. **Daily overage check**
   - If the full reset condition is not met but the position is above the daily target path enough to qualify as overage, sell the overage amount.

3. **Shortfall catch-up buy**
   - If neither sell condition is met and actual invested value is below target, buy a fraction of the shortfall based on aggressiveness, limited by available cash.

4. **Otherwise wait**
   - No special handling for gaps.
   - No leverage.
   - No cross-asset rebalance.

## Core path model

### Target path philosophy

The original conceptual model is a rising expected-value path. If the invested value gets ahead of that path, it is reasonable to take profits. If it falls below the path, the system should buy to catch up.

### ATR-scaled slope

The agreed path model is:

`daily_rate = base_rate * (1 + k * ATR%)`

Where:

- `base_rate` is the baseline expected daily growth rate
- `k` controls how strongly ATR increases the slope
- `ATR% = ATR / close`

### Path reset behavior

After every `SELL_ALL_RESET`, the target path is reset from the new baseline. This means the next day starts a fresh target-path run rather than continuing the previous path.

### Simplicity decision

No upper cap is applied initially to the ATR multiplier. The reset threshold is treated as the practical release valve. If later testing shows that very high ATR names distort the path too much, a cap can be introduced in a later revision.

## Aggressiveness model

Aggressiveness no longer represents heuristic buy sizing directly. Instead, it represents the fraction of shortfall that should be bought on a given day.

### Shortfall logic

- `target_invested_value` = path-implied invested value for the current day
- `actual_invested_value` = current shares * close
- `shortfall = max(0, target_invested_value - actual_invested_value)`
- `desired_buy = shortfall * aggressiveness_fraction`
- `actual_buy = min(desired_buy, available_cash)`

### Initial aggressiveness mapping

Initial default mapping:

- aggressiveness 1 -> 25% of shortfall
- aggressiveness 2 -> 50% of shortfall
- aggressiveness 3 -> 75% of shortfall
- aggressiveness 4 -> 100% of shortfall

These fractions are part of the strategy definition and should themselves be treated as tunable parameters over time.

## Sell logic

### Long-term reset sell

This remains the top-priority action. If the position is sufficiently above its baseline / path-defined expectations, sell the full remaining position and reset the strategy state for that allocation.

### Daily overage sell

The daily sell should move away from the current fractional-share trim heuristic toward explicit overage math:

- compute how far actual invested value is above the target path
- sell that overage amount rather than using a fixed trim fraction

This is the intended direction for the next strategy version.

## Cash and utilization rules

### Out-of-cash behavior

The agreed rule is:

- compute desired buy from shortfall and aggressiveness
- buy up to available cash only
- if cash is zero, do nothing and wait

No leverage is used.

### Utilization

`cash_utilization_limit` remains a tunable control, but the strategy does not add any special rebalance logic once capital is fully deployed.

## Gap handling

No special gap policy will be introduced in the first implementation.

Large gap-ups and gap-downs are handled by the same normal daily rules:

- full reset sell
- daily overage sell
- shortfall catch-up buy
- or wait

## Tax and accounting model

Keep the accounting simple:

- no tax-lot tracking
- no wash-sale handling
- pooled / average-cost style accounting is acceptable
- assume retirement-account style behavior

This work is not intended to become a tax engine.

## Key differences from the current engine

The current engine is still an ATR-driven heuristic system. The target design documented here changes the model in these ways:

1. Introduces an explicit target path.
2. Uses ATR to scale the target-path slope.
3. Resets that path after each full reset sell.
4. Defines buys as a fraction of path shortfall.
5. Defines daily sells in terms of overage above path.
6. Keeps implementation simple by avoiding caps, lot tracking, rebalance logic, and gap-specific rules in v1.

## Tunable parameters

### Core strategy parameters

1. `base_rate`
   - baseline expected daily growth rate for the target path

2. `k`
   - sensitivity of path slope to `ATR%`

3. `long_term_reset_threshold`
   - threshold for triggering a full reset sell

4. `daily_overage_threshold`
   - threshold for triggering a daily overage sell

### Aggressiveness parameters

5. `aggressiveness_level_1_fraction`
6. `aggressiveness_level_2_fraction`
7. `aggressiveness_level_3_fraction`
8. `aggressiveness_level_4_fraction`

These define how much of the shortfall is bought each day for aggressiveness levels 1 through 4.

### Secondary parameters

9. `cash_utilization_limit`
10. `allow_sell_at_loss`

These can remain fixed initially if search-space size becomes a concern.

## Recommended first implementation scope

Implement the next version with:

- ATR-scaled target path using `base_rate` and `k`
- path reset after `SELL_ALL_RESET`
- aggressiveness as shortfall-catch-up fraction
- buy amount limited by available cash
- no special gap logic
- no tax/wash-sale logic
- pooled accounting

## Recommended optimization surface for first pass

To keep the search space manageable, prioritize optimization over:

1. `base_rate`
2. `k`
3. `long_term_reset_threshold`
4. `daily_overage_threshold`
5. aggressiveness selection or aggressiveness fraction set

## Open implementation note

The exact mathematical definition of:

- baseline for reset comparisons
- daily overage threshold behavior
- and the precise target-path state variables

should be implemented in a way that preserves the hierarchy and decisions above. The business logic is now sufficiently specified to build a first full version.
