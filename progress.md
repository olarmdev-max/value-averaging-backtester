# Progress

- Extracted requirements from the Google Doc, created the initial local repository, added README, docs, git history, and baseline project scaffold.
- Built a vertical-slice C++ executable plus high-level smoke tests covering generation, simulation, Monte Carlo, optimization, and strict one-minute caps everywhere.
- Split the original single-file prototype into reusable modules for config, IO, price generation, simulation, Monte Carlo, optimization, and commandline execution.
- Added a Nix flake, validated nix develop and nix build, and made reproducible toolchains the default development path for everyone.
- Replaced the placeholder optimizer with a real dlib-backed global search backend and verified activation through Nix-backed smoke tests end-to-end successfully.
- Removed non-Nix fallbacks, required dlib at build time, and forced smoke tests to fail outside nix develop intentionally for safety.
- Added the blessed test-in-nix script and GitHub Actions CI so repository checks always run through the flake environment in automation.
- Added a timed optimization budget, defaulted evaluations to 500 Monte Carlo simulations each, and enabled quick 30-second benchmarking runs easily.
- Implemented file-backed input mode so simulate, optimize, and batch evaluation can run directly on one or many close-only CSVs today.
- Documented the close-only CSV schema, added fixture tests, generated synthetic series scripts, and added Yahoo Finance download helpers for experimentation.
- Changed the optimization score from total-gain-over-drawdown to CAGR-over-drawdown, then retuned the same leveraged basket using daily Yahoo histories for comparison.
- Optimized the TSLA basket on daily adjusted closes and found parameters near 2.401 long-term ATR, 2.200 daily ATR, aggressiveness three.
- Replaced TSLA with TSLL, reran the CAGR-over-drawdown optimization, and confirmed the basket weakened materially because TSLL produced negative results overall.
- Ran a rolling six-month monthly-step median-parameter search under one total thirty-second budget, completing eleven passes across twelve common-history windows successfully.
- The final rolling-window median parameters were 6.6211, 2.0002, aggressiveness four, with median replay results of 115.90 CAGR and 27.19 DD.
- Published the repository to GitHub as olarmdev-max/value-averaging-backtester and kept the main branch clean after each major checkpoint commit and validation.
