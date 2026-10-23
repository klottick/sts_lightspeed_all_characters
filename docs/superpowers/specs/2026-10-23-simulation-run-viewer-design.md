# Read-only simulation run viewer

Status: written specification approved by the user; implementation planning in progress.

## Purpose and agreed constraints

Make an entire Slay the Spire simulation understandable without reconstructing it from terminal logs. Combat must be particularly easy to follow, but the viewer also covers the map, events, shops, rewards, rest sites, and card selections.

This is a utilitarian slideshow viewer, not a playable game or a simulation controller:

- One slide per chosen simulation action, with its automatic effects grouped underneath it.
- Initial states, combat transitions, and the final outcome remain inspectable.
- Static presentation: no animations, transitions, or decorative motion.
- Use actual Slay the Spire 1 artwork where available. Use labeled text when an asset is unavailable; do not create replacement illustrations.
- Navigation and inspection controls only. Cards, potions, relics, targets, and event options never execute gameplay actions.
- Export a self-contained HTML file that opens locally, without a server, account, browser extension, or internet connection.
- Record the actual run, not the search engine's hypothetical rollouts.

The approved visual direction is option 1, the tactical overview, refined to use original game artwork. The local reference mockup is `.superpowers/brainstorm/1681430-1792727473/content/tactical-overview-game-art.html`. It contains synthetic example data and is not the implementation.

## First release boundaries

Include complete single-run recording for `SimpleAgent` and `ScumSearchAgent2`, plus the offline exporter and browser viewer. A new single-run reporting entry point accepts a seed, ascension, agent configuration, and output path. Existing CLI commands and Python bindings keep their current behavior.

The report command runs one agent on one run. Batch dashboards, batch trace storage, manually played CLI sessions, loading existing encoded action logs, and standalone `mcts_save` search reports are not required for this release. These are additional input integrations, not reasons to restrict the viewer to combat.

Also exclude live streaming, gameplay controls, branching or alternate-action exploration, search-tree inspection, and explanations of why an agent selected an action. The viewer reports what occurred and the recorded effects, not an invented decision rationale.

## Viewer layout

### Persistent top inventory

The top run bar shows character, seed, ascension, act/floor, HP, gold, deck count, and recording status. Relics and potion slots sit directly below it, across the top of the main frame. Names and counters accompany their game icons. Empty potion slots remain visible.

During combat, current HP, gold, potions, and other changing combat values come from `BattleContext` where that is the authoritative state, rather than the stale run context awaiting combat exit. Deck identity and run-level inventory remain associated with the same run.

### Main frame

- Left: floor/combat/action outline, grouped by turn within combat.
- Center-left: player, HP bar, separate Block badge, and active statuses.
- Center: the card just played, retained after it leaves the hand, with target names and actual resolved effects. A potion use or non-card action receives its own labeled action presentation.
- Center-right: all enemies in stable positions, each with name, HP, Block, statuses, and clearly labeled next intent.
- Bottom: current hand, energy, and draw/discard/exhaust counts with inspectable pile contents.
- Right: an ordered resolution list and compact resulting-state summary.

Card images can contain printed base values. Current cost, upgrades, modified values, and resolved damage/Block must be separately labeled when they differ; the image must never be mistaken for authoritative current values. Do not calculate approximate card rules in browser JavaScript.

In an End Turn slide, the central action presentation shows the enemies' resolved moves in order, including source, targets, attacks, Block absorption, HP loss, and debuffs. The last played card remains available in that frame. Distinguish **just resolved** actions from **next intent**; they are not interchangeable.

Defeated, escaped, or transformed enemies retain enough prior identity and position to explain the selected action. Summoned entities receive new identities rather than inheriting an old target's history.

### HP changes

The solid red bar and its number show HP after resolution. A static hatched amber segment marks HP removed by the selected action, with explicit before/after numbers. Block is always a separate blue value. A fully blocked hit does not create an HP-loss segment. Overkill is not counted as removed HP.

Where damage and healing occur within the same action, report their gross recorded amounts in the effects list and the net before/after HP change on the bar. Do not fabricate a bar segment from attack damage or imply healing did not occur. A healing-only change uses an explicit positive label rather than a damage segment.

### Non-combat screens

Keep the same outline, inventory, navigation, and effects list, replacing the combat board with the relevant information:

| Screen | Main information |
| --- | --- |
| Map | Act map, traversed route, current node, and the node chosen by the agent |
| Event / Neow | Event name, available choices, chosen option, and actual costs/rewards |
| Shop | Visible stock and prices, chosen purchase/removal/exit, resulting gold and inventory |
| Rewards / boss relics | Offered options, selected or skipped rewards, resulting inventory |
| Rest site | Available options, chosen action, actual healing or upgrade |
| Card selection | Selection task, candidate cards, selected cards, and resulting changes |
| Run end | Victory/loss/paused status, final floor, HP, deck, relics, and run configuration |

The selected action's pre-action offer remains visible even when the engine immediately changes screens or removes the chosen item. These are historical options, not buttons for choosing another outcome.

### Navigation

Provide Previous/Next, an action scrubber, arrow-key navigation, and jumps by act, floor, combat, and turn. Optional autoplay advances slides at a selected interval without animating state changes. Autoplay stops at the end and when the user navigates manually.

Inspection may expand effects, item descriptions, or card piles. It never calls the simulator. Keep labels and numbers alongside icons, use visible keyboard focus, and do not communicate changes through color alone. On narrow screens, stack panels without hiding required combat information.

## Architecture and data flow

Use four focused components:

1. **Snapshot adapter:** converts engine state into value-only display data. No simulation decisions, RNG consumption, retained engine pointers, or serialization of queued functions.
2. **Run recorder:** captures the actual selected action before execution, ordered effects while it resolves, and the resulting state. It tracks run/combat/entity identity and writes a versioned trace.
3. **HTML exporter and asset catalog:** packages a completed or explicitly partial trace, a static viewer template, and only the referenced available images into one HTML document.
4. **Browser renderer:** navigates captured data and renders the selected slide. Plain HTML/CSS/JavaScript is sufficient; no frontend framework or live API is needed.

Flow: agent chooses an action → recorder captures its identity and pre-action context → engine executes it unchanged → recorder captures effects and final state → exporter embeds trace and artwork → browser displays the report.

### Engine integration

Both agents already have `takeAction(GameContext&, GameAction)` and `takeAction(BattleContext&, Action)` wrappers. Attach optional recording at these committed-action boundaries, not inside `BattleScumSearcher2::step`, random rollouts, or action enumeration.

Explicitly capture run initialization, combat initialization after opening effects resolve, combat completion before `exitBattle`, the updated run state after `exitBattle`, and the terminal or paused state. Associate lifecycle snapshots with their triggering action where possible; use labeled boundary frames where no agent action exists. Do not invent extra agent choices to represent automatic transitions.

Capture the action's card/item identity and description before execution: hand indices, target slots, and screen offers can change immediately afterward. Preserve selected card identity even when identical cards occur in a hand.

Combat effect capture requires more than adjacent full-state differences. Add optional observations at resolved damage/HP changes and enemy move boundaries, so attacks, absorbed Block, multiple hits, reactive effects, healing, and debuffs are not misreported as one unexplained net delta. Observe queue resolution, not merely the call that schedules an enemy move.

Activate effect collection only for the actual action and the specific battle being recorded. A scoped, thread-local observer with an explicit battle identity can avoid placing recorder ownership in copied search states. Restore the previous observer on scope exit. Do not retain it in queued callbacks or allow hypothetical search states to emit events.

Record source and target identity when the engine provides them. Label unattributed automatic effects as such; never guess a monster/card source from a net snapshot change. Damage figures and status results come from the engine, not from interpreting intent icons or printed card text.

Recording must not consume RNG, change policies, alter action ordering, or change existing console output. Keep unrelated engine fixes out of this work. Disabled recording must allocate no trace data and serialize no state; benchmark the disabled path to detect material slowdown.

### Trace contract

The trace has an integer schema version, run metadata, an entity/card reference catalog, initial state, ordered frames, and a completion status. Store the seed as a string so the browser cannot lose 64-bit integer precision.

Each frame contains:

- Monotonic frame ID, frame kind, act/floor, combat ID, turn, and screen kind.
- Chosen action type, stable source/target references, readable description, and pre-action card or offer data when applicable.
- A settled post-action snapshot, with a prior snapshot reference for comparison.
- Ordered typed effects, including known source/target, before/after values, damage/Block/HP information, and readable fallback text.
- Relevant pre-action screen contents when a transition would otherwise erase them.

Snapshots include run inventory and screen data; combat snapshots additionally include current player/enemy state, intents, statuses, hand, piles, potion state, and pending selection tasks. Include stance/orb data when present in the engine; do not silently omit meaningful state for non-Ironclad characters.

Use stable identities for card instances and monster lifetimes rather than matching by display name or current array position. Use lossless snapshot fields for display state, not a raw dump of `GameContext` or `BattleContext`. State that is uninitialized, irrelevant to the active screen, or not supported by the adapter must not be read.

Version 1 stores settled full display snapshots for straightforward random access, plus pre-action offers and effect data. Deduplicate immutable metadata and image bytes. Write intermediate trace frames incrementally so a run does not require keeping the complete trace in engine memory. The browser may load the final single-run report in memory; batch-scale analysis is outside this release.

## Export and artwork

Add a dedicated single-run reporting entry point rather than changing existing positional CLI commands. It accepts seed, ascension, agent choice, applicable search settings, output HTML path, and a local asset-catalog path. Document one reproducible command per agent. The report embeds the exact run settings.

The viewer template is maintained separately from C++ engine code and made available to the reporting entry point as a build resource. The exporter reads the completed trace and catalog and creates the final standalone report; the recipient does not need these resources to view it.

Use actual STS1 artwork. The approved mockup already establishes public wiki sources for its character, enemy, card, relic, potion, energy, and status/intent images. Maintain a source/identity manifest and an optional local cache. Asset acquisition is a separate explicit preparation step, not a network dependency during simulation or report viewing.

Support locally supplied game assets through the same manifest. Do not require the game to be installed merely to open a report. Do not commit the prototype's generated credentials, cached proprietary images, or generated reports as project source. Preserve source attribution and Mega Crit ownership notices; do not describe game artwork as covered by this repository's license.

Embed available referenced image bytes as data URLs and metadata as inert JSON. Missing artwork falls back to the item's name and numbers, without broken-image placeholders or newly invented art. Recognizable content matters more than decorative fidelity.

Escape JSON safely inside HTML, including script-closing sequences. Treat names, imported save data, and descriptions as text; do not interpolate them into executable markup. Report viewing must not execute externally supplied scripts or fetch remote resources.

## Failure behavior

- Validate report arguments and output destination before starting a run.
- Report an unwritable destination or failed final export clearly with a failing exit status; do not announce a successful report.
- Preserve completed trace frames when recording/export fails where possible. Label an exported interrupted or paused run explicitly; never present it as victory or a completed loss.
- Missing artwork does not fail a simulation or hide state.
- Unknown screen/entity/effect kinds receive readable fallback data. Never show fabricated zero values as if they were observed.
- Unsupported trace versions show an explicit error rather than rendering misleading state.
- An engine crash is not made recoverable by the viewer. Do not promise a finished HTML report after a fatal process failure; an incremental trace is the recovery artifact.

## Verification and acceptance

### Recorder and engine checks

- Same fixed seeds and agent settings produce the same chosen actions, RNG states, and final state with recording enabled and disabled.
- Actual trace frame count is independent of the number of internal search rollouts; increasing search effort does not dump hypothetical actions.
- Record a full run with every supported screen kind, plus victory, defeat, and paused examples.
- Capture combat entry, final lethal action, combat exit effects, and rewards without losing HP or inventory changes.
- Verify duplicate card identities, card selections, summons/replacements, multi-target attacks, multi-hit attacks, full/partial Block absorption, overkill, damage followed by healing, and status changes.
- Verify both agents use the same trace contract. Validate schema versioning, 64-bit seeds, unknown values, missing artwork, and safe HTML/JSON encoding.
- Measure recording-disabled overhead with a fixed workload; investigate material regression rather than accepting a slowdown based only on passing correctness tests.

### Browser and export checks

- Generate a report from real simulation output, not only synthetic fixtures.
- Open it directly using `file://`, with network disabled, and verify all embedded assets and navigation.
- The played card stays visible after leaving the hand; targets and effects match the selected frame.
- Multi-enemy frames preserve identity, defeated targets, per-enemy HP changes, and the distinction between resolved moves and next intents.
- Blocked damage and overkill do not overstate lost HP; healing is not mislabeled as damage.
- All non-combat views retain the pre-action options and distinguish selected/skipped items.
- Previous/Next, keyboard navigation, scrubber, jumps, pile inspection, and any autoplay operate only on the trace. No gameplay control is present.
- Check desktop and narrow-screen layouts, keyboard focus, text/icon fallback, missing assets, final-frame behavior, and browser console errors.

The feature is accepted when a generated offline report lets the user follow a complete run, including detailed multi-enemy combat, with the approved presentation and no change to simulation behavior.
