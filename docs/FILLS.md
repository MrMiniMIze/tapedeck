# Fill Model Assumptions

> **Status: TEMPLATE (filled at Phase 2 to 3).** The credibility of every P&L and
> Sharpe number lives here. State *every* assumption, then show the sensitivity.

## Queue-position model

- How a resting order's queue rank is assigned when it joins a level: _TODO_
- How rank advances (cancels ahead, partial fills ahead): _TODO_
- **Data caveat:** queue position is only *real* on L3/MBO feeds. On L2 data
  (e.g. Binance depth) we never observe our own rank, any such fill is an
  explicit, labelled assumption, not an observation. Affected fills: _TODO_

## When a resting order fills

- Rule: _TODO_ (e.g. only once the level trades fully through our rank,
  conservative; may understate fills).

## Aggressive orders

- Slippage / market-impact model: _TODO_

## Latency assumption

- Assumed signal-to-order-arrival latency: _TODO_

## Sensitivity: the "kill" curves (the headline research artifact)

These show **where the edge crosses zero**, which is the honest deliverable.

- Net Sharpe vs fill-aggressiveness (optimistic touch / realistic queue+latency+adverse-selection / pessimistic): _TODO_
- Net Sharpe vs assumed latency: _TODO_
- Net Sharpe vs holding horizon: _TODO_
- Crossover point(s): _TODO_
