# Bayesian optimization library decision

## Recommendation

For this project, the best practical default is **dlib**.

Why:
- already available in nixpkgs
- permissive license
- good long-term maintenance story
- strong fit for bounded expensive black-box tuning in a modular C++ app
- much easier to live with operationally than older BO-specific libraries

## Important nuance

`dlib` is the best **practical default**, not the most academically pure Bayesian-optimization toolkit.

If later we need deep control over Gaussian-process internals, acquisition functions, or BO-specific research features:
- **Limbo** is the stronger specialized BO candidate
- **OpenTURNS** is another real BO option, but it is much heavier

## Why BayesOpt is not the default

BayesOpt is technically relevant, but its AGPL/commercial licensing story makes it a poor default choice for this repo.

## Current implementation state

- the optimizer is isolated behind `optimizer.hpp`
- the current runtime backend is still a smoke-test placeholder
- the Nix flake now provisions `dlib`
- the codebase is structured so a real dlib-backed optimizer can be added without another repo-wide rewrite
