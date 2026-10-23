# Simulation trace version 1

The intermediate trace is UTF-8 JSON Lines. Every write is newline-terminated and flushed; a partial final record can be discarded after interruption. A malformed complete line is an error, not an invitation to skip history.

Records, in order:

1. `header`: `schemaVersion: 1` and `run`. The decimal `run.seed` is a **string** to preserve 64-bit precision. Agent, character, ascension, display seed and settings are run metadata.
2. Zero or more `catalog` / `frame` records. Catalog records have an `entries` object of immutable definitions; conflicting redefinitions are rejected.
3. `footer`: status is `victory`, `loss`, `paused`, or `interrupted`. No later records are permitted. A missing footer is always interpreted as interrupted.

A frame has consecutive integer `id` (starting at zero), `previousFrameId` (null initially, otherwise the previous ID), string `kind`, object `state`, array `effects`, and act/floor/combat/turn/action/pre-action-screen fields. Frame kinds are `run_start`, `action`, `combat_start`, `combat_end`, `run_state`, `run_end`. Unknown presentation kinds can be displayed as labeled fallback data rather than fabricated state.

Snapshots contain `screen`, authoritative `player`, `inventory`, `map`, and nullable `combat`. Combat contains enemy identities/intents, hand, piles and selection state. Card references are combat-scoped unique IDs; enemy references include combat, slot and lifetime generation. Detailed payloads are documented beside their snapshot adapters and their tests.

Each effect has an ordered sequence, kind, description, nullable known source/target, and before/after values. Damage distinguishes incoming attack value, Block absorbed, actual HP lost and known overkill. Healing is separate. Source attribution is never inferred from a net state delta.

`readTrace` produces `{schemaVersion, run, catalog, frames, status, warnings}`. The HTML exporter adds an embedded asset catalog. Browser viewing never executes the simulator or requires the intermediate file.
