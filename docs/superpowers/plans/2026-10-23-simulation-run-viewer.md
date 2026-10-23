# Simulation Run Viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate an offline, read-only HTML slideshow of a complete simulation, using original STS1 artwork and accurately recorded combat effects.

**Architecture:** Capture selected actions and value-only snapshots at the two agents' committed-action boundaries. Collect combat effects only while the actual battle resolves, stream a versioned JSONL trace, then package it with a small static browser renderer and locally available artwork. Keep the exporter outside the engine source glob so existing executables do not acquire a viewer-resource dependency.

**Tech Stack:** Existing C++17 engine and vendored nlohmann/json; CMake/CTest; Python 3 standard library for resource embedding and explicit asset preparation; plain HTML/CSS/JavaScript. Playwright is a development-only browser-test dependency.

**Spec:** `docs/superpowers/specs/2026-10-23-simulation-run-viewer-design.md`

## Global Constraints

- One slide per chosen simulation action, with its automatic effects grouped underneath it.
- Static presentation: no animations, transitions, or decorative motion.
- Use actual Slay the Spire 1 artwork where available. Use labeled text when an asset is unavailable; do not create replacement illustrations.
- Navigation and inspection controls only. Cards, potions, relics, targets, and event options never execute gameplay actions.
- Export a self-contained HTML file that opens locally, without a server, account, browser extension, or internet connection.
- Record the actual run, not the search engine's hypothetical rollouts.
- Recording must not consume RNG, change policies, alter action ordering, or change existing console output.
- Disabled recording must allocate no trace data and serialize no state; benchmark the disabled path to detect material slowdown.
- Do not commit the prototype's generated credentials, cached proprietary images, or generated reports as project source.
- Preserve C++17, existing entry points, and Python bindings; use existing dependencies rather than a new runtime framework.

## Review Focus

- A card opens a selection screen before its queue drains: preserve its identity and resume recording effects under the subsequent selection, without losing or duplicating the card's effects. Tasks 4–5 test this explicitly.
- A summon replaces a monster with the same species in the same array slot: assign a new lifetime reference and retain the old target in historical effects. Tasks 2 and 4 test this explicitly.
- A hit triggers healing/revival or another nested HP change: retain gross HP loss and healing, avoid duplicate damage events, and render the net bar honestly. Tasks 4 and 8 test this explicitly.
- An interrupted JSONL file, malformed asset catalog, or failed output write: retain valid trace data, label partial results, and never silently replace a good report or claim completion. Tasks 1, 6, and 10 test these explicitly.
- Free/X-cost cards, temporary costs, and multi-upgrade cards: retain exact current metadata and distinguish printed artwork from authoritative values. Tasks 2 and 8 test these explicitly.

---

## File structure and shared contracts

New engine reporting code lives under `include/sim/report/` and `src/sim/report/`: `Trace`, `IdentityRegistry`, `SnapshotAdapter`, `ScreenAdapter`, `CombatObserver`, and `RunRecorder`, each with a matching `.h`/`.cpp`. These are distinct responsibilities, not one growing serializer.

`report/` contains `HtmlExporter.cpp`, `AssetCatalog.cpp`, `ReportOptions.cpp`, `viewer/{viewer.html,viewer.css,model.js,dom.js,combat.js,screens.js,viewer.js}`, and `assets/sources.json`. Their public headers live under `include/sim/report/`. `apps/report.cpp` is the new executable entry point. `tools/embed_report_resources.py` generates a build-only resource header; `tools/prepare_report_assets.py` explicitly populates a user-selected cache.

Tests live under `tests/report/`: a small always-on C++ assertion harness, focused `*Tests.cpp` files, JSON fixtures, Python exporter/tool tests, and `browser/` with an isolated Playwright package and tests. No game artwork is required in checked-in test fixtures: use a generated tiny PNG for packaging tests and verify optional real assets locally.

All C++ interfaces below are in `sts::report`, use `Json = nlohmann::json`, and use `std::filesystem::path` for paths. JSON records contain display values, never engine pointers or queued functions.

### Wire format decisions

- JSONL record types: `header`, `catalog`, `frame`, `footer`. Header is first and contains `schemaVersion: 1` and run metadata. Catalog records add immutable definitions before their first use. Footer status is `victory`, `loss`, `paused`, or `interrupted`.
- Run metadata: decimal `seed` string, `seedDisplay`, `character`, `ascension`, `agent`, exact agent settings, and recorder schema version. JSON numeric seeds are forbidden.
- Frame: `id`, `kind`, `previousFrameId`, `act`, `floor`, `combatId` (nullable), `turn` (nullable), `action` (nullable), `beforeScreen` (nullable), `state`, and ordered `effects`. IDs are monotonic integers starting at zero; prior-frame references are nullable on the first frame.
- Kinds: `run_start`, `action`, `combat_start`, `combat_end`, `run_state`, `run_end`. Boundaries are labeled, not invented agent decisions. A returned `CARD_SELECT` state is a settled input boundary even if more automatic work awaits that selection.
- Snapshot: `screen`, `player`, `inventory`, `map`, and nullable `combat`. Combat overrides current player/inventory fields with battle-authoritative values. Unsupported values are `null`, not invented zeroes.
- Card instance: `ref`, `cardId`, `name`, `upgrades`, `baseCost`, nullable `currentCost`, `freeToPlay`, `specialData`, `assetKey`. References use `combat-N/card-<uniqueId>`; preserve the engine's unique ID across pile movement. Run-deck entries use run-scoped occurrence references, not names.
- Enemy: `ref`, `slot`, `monsterId`, `name`, `hp`, `maxHp`, `block`, `statuses`, `lifeState`, and `intent`. References use `combat-N/enemy-<slot>-<generation>`. Construction/replacement advances generation; ordinary move changes and a phase revival do not.
- Intent: `moveId`, readable `name`, `kind`, nullable `damagePerHit`, nullable `hitCount`, `assetKey`. Use engine move/damage helpers without RNG. Unsupported intent classification remains explicit; do not derive damage from artwork.
- Action: type, encoded `bits` when available, description, source/target references, pre-action card/item/selection, and nullable `parentActionId` for interrupted card selections.
- Effect: `sequence`, `kind`, nullable source/target references, description, and numeric `before`/`after` data. A damage event separates `incoming`, `absorbedBlock`, `hpLost`, and nullable `overkill`; never derive these four fields from one final HP delta. Enemy move groups distinguish `resolvedMove` from the post-state `intent`.
- Packaged document: `{schemaVersion: 1, run, catalog, frames, status, warnings, assets}`. Assets are `{byKey, blobs, sources}`; repeated image bytes share one data URL in `blobs`.

## Task 1: Versioned streaming trace and executable test harness

**Files:** Create `include/sim/report/Trace.h`, `src/sim/report/Trace.cpp`, `tests/report/{TestSupport.h,ReportTestMain.cpp,TraceTests.cpp}`, `docs/report-trace-v1.md`; modify `CMakeLists.txt`.

**Interfaces:** Produce `TraceWriter(std::ostream&, const Json& run)`, `void appendCatalog(const Json&)`, `void appendFrame(const Json&)`, `void finish(const std::string& status)`, and `Json readTrace(std::istream&)`. Produce the wire format above and an always-on `CHECK(condition)` test macro. `report-tests --case <name>` runs one registered case; CTest names use the `report.` prefix.

- [ ] **Step 1 — Establish the baseline.** Initialize existing submodules with `git submodule update --init --recursive`; if the relative pybind11 URL fails, retry with a command-local URL override to `https://github.com/pybind/pybind11.git`, without changing the pinned commit or `.gitmodules`. Run `cmake -S . -B cmake-build-report -DCMAKE_BUILD_TYPE=Release` and `cmake --build cmake-build-report --target main test -j2`. Record baseline failures and stop for a decision if compatibility changes are needed; do not hide unrelated repairs in this task.
- [ ] **Step 2 — Add failing tests.** `trace_round_trip` writes a run with seed `"18446744073709551615"`, one frame, and footer `paused`; assert schema version `1`, seed string equality, frame ID `0`, and `status == "paused"`. `trace_partial_and_invalid` asserts that a missing footer becomes `interrupted`, a final incomplete line yields a warning and retains prior complete frames, a malformed complete middle line fails, duplicate IDs fail, and schema version `2` fails. `trace_stream_failure` uses a failing stream buffer and asserts an error, not a success footer. Register cases with CTest; assertions must still execute with `NDEBUG`.
- [ ] **Step 3 — Verify red.** Build `report-tests` and run `ctest --test-dir cmake-build-report -R '^report.trace_' --output-on-failure`; expect missing implementation failures, not unrelated dependency errors.
- [ ] **Step 4 — Implement the writer/reader.** Flush each complete JSONL record, validate record order/references, and retain only current writer counters rather than all frames. Define the wire format in `docs/report-trace-v1.md`. Add static target `sts-report-engine` for new test/report executables using the existing source list and existing JSON include/dependency; leave existing executable construction intact. Link `report-tests` against this target.
- [ ] **Step 5 — Verify green and commit.** The same trace tests pass. Commit the focused files as `feat: add versioned streaming simulation traces`.

## Task 2: Stable combat identities and safe snapshots

**Files:** Create `include/sim/report/{IdentityRegistry,SnapshotAdapter}.h`, matching files under `src/sim/report/`, and `tests/report/{SnapshotTests.cpp,EngineFixtures.h,EngineFixtures.cpp}`.

**Interfaces:** Produce `void IdentityRegistry::beginCombat()`, `std::string cardRef(int uniqueId) const`, `std::string enemyRef(int slot)`, `void replaceEnemy(int slot)`. Produce `SnapshotAdapter(IdentityRegistry&)`, `Json game(const GameContext&)`, `Json battle(const Json& runSnapshot, const BattleContext&)`, `Json card(const CardInstance&)`, and `Json catalogDelta()`. `game()` delegates active-screen details to Task 3; initially provide an explicit unsupported-screen object. Test helper `BattleFixture twoLouses(std::uint64_t seed = 1)` owns an initialized `GameContext gc` and `BattleContext bc` using `MonsterEncounter::TWO_LOUSE`.

- [ ] **Step 1 — Write failing snapshot cases.** `snapshot_authoritative_state`: set `gc.curHp = 80`, `bc.player.curHp = 17`, battle gold `23`, and one battle potion empty; assert displayed HP `17`, gold `23`, and that the empty slot is retained. `snapshot_card_identity`: two Strikes with unique IDs `10` and `11` must differ; moving ID `10` to discard preserves its reference. `snapshot_replacement_identity`: two `replaceEnemy(0)` calls produce different refs despite unchanged species/slot. `snapshot_special_cards`: assert X-cost remains `-1`, free-to-play is separate from base cost, temporary cost `0` stays `0`, and Searing Blow upgrade count `3` survives serialization.
- [ ] **Step 2 — Verify red.** Run `ctest --test-dir cmake-build-report -R '^report.snapshot_' --output-on-failure`; expect absent adapters/identity behavior.
- [ ] **Step 3 — Implement snapshots.** Read only live hand entries, active screen fields, present status values, and initialized monsters. Use existing card IDs/unique IDs and engine damage helpers; include stance and `orbSlots`, and explicitly mark unavailable orb contents as unsupported (the current engine does not store an orb array). Preserve relic counters, potion capacity, pile contents, selection task, status names/amounts, and observable special card data. Attach asset keys by stable engine ID, not English text. Keep output metadata deduplicated through `catalogDelta()`.
- [ ] **Step 4 — Add and run `snapshot_no_rng_mutation`.** Compare every `Random::{seed0,seed1,counter}` before/after repeated game/battle snapshots; assert exact equality. Run all snapshot cases plus trace tests; expect all pass.
- [ ] **Step 5 — Commit.** `feat: capture stable combat display snapshots`.

## Task 3: Structured non-combat screen and action adapters

**Files:** Create `include/sim/report/ScreenAdapter.h`, `src/sim/report/ScreenAdapter.cpp`, `tests/report/ScreenTests.cpp`; extend `SnapshotAdapter.cpp` and `docs/report-trace-v1.md`.

**Interfaces:** Produce `Json screenSnapshot(const GameContext&, IdentityRegistry&)`, `Json describeGameAction(const GameContext&, const search::GameAction&, IdentityRegistry&)`, and `Json describeBattleAction(const BattleContext&, const search::Action&, IdentityRegistry&)`. Options carry stable option keys, labels, structured card/item/cost data, and availability; selected option identity is captured before execution. No gameplay execution occurs in these functions.

- [ ] **Step 1 — Write failing cases.** `screen_all_kinds` checks map, Neow/event, shop, normal/boss rewards, rest site, and card-select fixtures against their active `ScreenState` and exact option names/values. `screen_transition_offer` purchases a card with price `50` from gold `99`; assert the action retains the purchased card and price while the after snapshot shows gold `49`. `screen_unknown` retains unknown IDs with an explicit label and `null` unsupported data. `screen_descriptions_nonempty` asserts every enumerated option receives a nonempty description despite the existing empty `GameAction::printDesc` implementation.
- [ ] **Step 2 — Verify red.** Run CTest filter `'^report.screen_'`; expect unsupported-screen/description assertions.
- [ ] **Step 3 — Implement active-screen switches.** Read only fields belonging to the current screen. Reuse enum names, screen constants, legal-action enumeration, and existing console presentation as references; do not parse stdout, attach a fake `ConsoleSimulator`, mutate policies, or globally change the stub `printDesc`. Encode skip, leave, potion discard, upgrade/remove/transform, route selection, offered rewards, and selected multi-card choices explicitly.
- [ ] **Step 4 — Verify green.** All screen cases pass, and repeated option enumeration preserves every run RNG state. Extend contract documentation with each screen payload shape.
- [ ] **Step 5 — Commit.** `feat: describe recorded run screens and choices`.

## Task 4: Observe resolved combat effects without changing search states

**Files:** Create `include/sim/report/CombatObserver.h`, `src/sim/report/CombatObserver.cpp`, `tests/report/CombatObserverTests.cpp`; modify `src/combat/{BattleContext,Player,Monster,MonsterGroup,MonsterSpecific}.cpp`. Add observation to template status mutators in `include/combat/{Player,Monster}.h` only where resolved mutations otherwise have no observation boundary.

**Interfaces:** Produce `CombatObserver(BattleContext&, IdentityRegistry&)`, `Json takeEffects()`, and noncopyable `ScopedCombatObserver(CombatObserver&)`. Hot hooks use `CombatObserver* observerFor(const BattleContext*) noexcept`, `CombatObserver* observerFor(const Player*) noexcept`, and `CombatObserver* observerFor(const Monster*) noexcept`; lookup checks the actual object inside the bound battle. Observer methods: `beginEnemyMove(int slot, const Json& move)`, `endEnemyMove()`, `recordDamage(const Json&)`, `recordMutation(const Json&)`, and `enemyReplaced(int slot)`. Observer ownership is external; `BattleContext`, queues, and search copies receive no observer member.

- [ ] **Step 1 — Write failing resolution cases.** `effects_damage_math`: a `7`-damage hit into `5` Block records `absorbedBlock == 5` and `hpLost == 2`; a fully blocked hit records zero HP loss; a `9` hit against `2` HP records `hpLost == 2`, `overkill == 7`. `effects_multihit_reactive` records each hit and reactive HP change once, in resolution order. `effects_damage_then_heal` starts at HP `10`, loses `7`, heals `4`, and asserts gross loss `7`, gross heal `4`, final HP `7`. Include a revival case that does not collapse death prevention into net zero damage.
- [ ] **Step 2 — Verify red.** Run CTest filter `'^report.effects_'`; expect absent events, not changed engine outcomes.
- [ ] **Step 3 — Implement scoped observation.** Restore the previous thread-local observer on every exit, including exceptions. Add no-op-when-disabled hooks around actual state changes. Observe damage at the layer where defenses have resolved, before nested healing/revival can erase HP loss; avoid emitting it again in callers. Use scoped damage attribution for player attacks with explicit enemy index; label unknown reactive origins `automatic`. Capture healing, direct HP loss, Block/energy changes, and status mutations separately. Mutators without a battle argument must validate the bound player/monster pointer, not assume any thread-local observer applies.
- [ ] **Step 4 — Add failing queue/lifetime tests, then implement their boundaries.** `effects_queued_enemy_moves` schedules Red Louse Bite and Green Louse debuff; assert two distinct resolved move groups, effects after scheduling, and no assignment to the next move due to `monsterTurnIdx` already incrementing. Keep the active move until its queued effects drain or input interrupts; close it before advancing to another move. `effects_same_slot_spawn` calls `construct` or `initSpawnedMonster` for the same species/slot and asserts one new lifetime per construction. `effects_foreign_copy_and_selection` asserts that executing a copied battle emits no events, an input-selection pause restores the observer, and a later resumed selection has no stale observer. Instrument only enabled observation paths; do not reorder `executeActions`.
- [ ] **Step 5 — Verify green and commit.** All effects tests pass; `sizeof(BattleContext)` and queued action storage are unchanged. Commit `feat: record resolved combat effects and enemy moves`.

## Task 5: Connect both agents to the streaming recorder

**Files:** Create `include/sim/report/RunRecorder.h`, `src/sim/report/RunRecorder.cpp`, `tests/report/RecorderTests.cpp`; modify both agent headers and `src/sim/search/{SimpleAgent,ScumSearchAgent2}.cpp`.

**Interfaces:** Produce `RunRecorder(TraceWriter&)`, `void startRun(const GameContext&)`, `void enterBattle(const GameContext&, BattleContext&)`, `void recordGameAction(GameContext&, search::GameAction)`, `void recordBattleAction(BattleContext&, search::Action)`, `void finishBattle(const BattleContext&)`, `void leaveBattle(const GameContext&)`, and `void finishRun(const std::string& status, const GameContext&)`. Each `record*Action` executes that one action once. Each agent gets a default-null `report::RunRecorder* recorder` borrowed only during playout. Cache value-only run context for combat snapshots; do not store a dangling `GameContext*` in the recorder.

- [ ] **Step 1 — Write failing agent/lifecycle cases.** `recorder_one_execution` compares action bits and HP/energy before/after a known Bash and asserts exactly one action frame. `recorder_both_agents` attaches the same recording API to both agents and validates their traces. `recorder_combat_lifecycle` checks opening hand, lethal final action, before-exit state, after-exit healing/rewards, and final run status. `recorder_selection_continuation` verifies a card-selection action references its parent card frame and retains that card in its presentation. `recorder_paused` checks `pauseOnCardReward` produces footer `paused`, not victory/loss.
- [ ] **Step 2 — Verify red.** Run CTest filter `'^report.recorder_'`; expect no recorder API/integration.
- [ ] **Step 3 — Implement guarded integration.** Preserve existing logging/history code and replace only the actual execution line with a recorder call when attached. Capture action metadata before execution, activate effects only around committed battle execution, then serialize settled snapshots and catalog deltas. Keep lifecycle recording around existing battle init/exit and playout termination. Suppress duplicate terminal boundaries when the last action already contains the final state; retain explicitly named transitions when inventory changes afterward.
- [ ] **Step 4 — Verify search isolation.** `recorder_search_isolation` runs search with budgets `1` and `100` on a copied battle before a known committed action and asserts zero search-generated frames in both runs. Do not assert different search budgets choose identical gameplay. `recorder_null_path` asserts no snapshot/trace allocation path executes with `recorder == nullptr`. Run the full C++ report suite.
- [ ] **Step 5 — Commit.** `feat: record complete simple and search agent runs`.

## Task 6: Safe offline exporter and explicit real-art preparation

**Files:** Create `include/sim/report/{AssetCatalog,HtmlExporter}.h`, `report/{AssetCatalog,HtmlExporter}.cpp`, `report/assets/sources.json`, `report/viewer/{viewer.html,viewer.css,model.js,dom.js,combat.js,screens.js,viewer.js}` resource skeletons, `tools/{embed_report_resources,prepare_report_assets}.py`, `tests/report/ExporterTests.cpp`, and `tests/report/test_asset_tools.py`; modify `CMakeLists.txt` and `.gitignore` for new generated/test artifacts only.

**Interfaces:** Produce `Json loadAssetCatalog(const std::filesystem::path&)`, `Json embedReferencedAssets(const Json& catalog, const std::set<std::string>& keys)`, `std::string jsonForHtml(const Json&)`, and `void exportReport(std::istream& trace, const std::filesystem::path& assetManifest, std::ostream& html)`. Resource generator consumes the seven viewer source files and outputs a C++17 build header `ReportResources.h`; only static target `sts-report-support` includes it. This target links `sts-report-engine` and is linked by `report-tests`, `report-fixtures`, and `sts-report` when those targets exist. Exporter template markers are `@@REPORT_JSON@@`, `@@VIEWER_CSS@@`, `@@VIEWER_JS@@` and must each occur once before substitution.

- [ ] **Step 1 — Write failing exporter tests.** `export_self_contained` embeds a generated PNG and asserts one data URL despite two referencing keys, inert JSON with schema `1`, inline CSS/scripts, and no external resource URLs. `export_escape` includes `</script><script>globalThis.pwned=1</script>`, `<img onerror=...>`, ampersands, and Unicode separators in text; assert serialized text round-trips and cannot close the data script. `export_missing_art` asserts labeled metadata remains and missing asset keys do not create broken image URLs. `export_asset_validation` rejects malformed catalogs, path escape outside the manifest directory, and active SVG content; an empty manifest is valid. `export_stream_failure` throws and preserves the input trace.
- [ ] **Step 2 — Verify red.** Run CTest filter `'^report.export_'` and `python3 -m unittest discover -s tests/report -p 'test_asset_tools.py'`; expect absent exporter/tool behavior.
- [ ] **Step 3 — Implement packaging.** Parse/validate Task 1's trace, embed only referenced local PNG/JPEG/WebP files, deduplicate by exact bytes, preserve ownership/source notices, and escape `<`, `>`, `&`, U+2028/U+2029 in JSON embedded in HTML. Keep all trace text inert in the renderer. Do not allow network URLs as runtime image sources. Missing files are warnings and textual fallbacks; malformed/unsafe manifests fail before running a simulation.
- [ ] **Step 4 — Implement explicit acquisition and build resources.** `python3 tools/prepare_report_assets.py --sources report/assets/sources.json --output files/report-assets` downloads only named STS1 assets, checks signatures, and writes a manifest mapping stable asset keys to local paths with source URLs and Mega Crit attribution. Support `--local-dir DIR` to prepare an equivalent manifest from supplied files. Tests use mocked HTTP responses and temporary directories; source manifest initially covers the approved mockup's 23 images and can extend without code changes. Use Python standard library only; no downloads during build, simulation, export, or viewing. Resource skeletons already render a clear loading/version-error shell; later tasks implement views.
- [ ] **Step 5 — Verify green and commit.** All exporter/tool tests pass, and normal `main`, `test`, and bindings do not need generated viewer resources. Commit `feat: export standalone reports with original game assets`.

## Task 7: Read-only viewer model, navigation, and safe DOM foundation

**Files:** Implement `report/viewer/{model.js,dom.js,viewer.js,viewer.html,viewer.css}`; create `tests/report/ReportFixtures.cpp`, `tests/report/browser/{package.json,package-lock.json,playwright.config.mjs,navigation.spec.mjs}`, and non-art JSON fixtures under `tests/report/fixtures/`; extend `CMakeLists.txt`.

**Interfaces:** `globalThis.StsReportModel` provides `validateReport(report)`, `frameAt(report,index)`, `groupFrames(report)`, and `healthDelta(beforeHp,afterHp,maxHp)` returning `{remaining,lost,gained,maxHp}`. `globalThis.StsReportDom` provides `text(tag,value,className)`, `asset(report,key,alt,className)`, and `replaceChildren(parent,...nodes)`. `globalThis.StsReportViewer.mount(document,report)` owns navigation and calls renderers from Tasks 8–9. `report-fixtures --output-dir DIR` creates offline HTML fixtures through the real exporter; browser tests use `REPORT_FIXTURE_DIR`.

- [ ] **Step 1 — Write failing browser tests.** `navigation.spec.mjs` opens a generated `file://` report, asserts first/last button boundaries, keyboard/scrubber movement, act/floor/combat/turn jumps, expanded pile inspection, and an unsupported-schema error. Unknown frame/effect types must display fallback text, not a blank page. Text containing HTML must appear literally with no executable DOM. Empty/interrupted reports must show status, not throw. Assertions operate on accessible roles/labels and stable `data-frame-id` attributes.
- [ ] **Step 2 — Verify red.** Install development-only browser tooling in `tests/report/browser` using `npm install --save-dev --save-exact @playwright/test`, commit its generated exact lockfile, and install Chromium with that package's Playwright CLI. Build `report-fixtures`, generate fixtures to `cmake-build-report/report-fixtures`, then run `REPORT_FIXTURE_DIR="$PWD/cmake-build-report/report-fixtures" npm --prefix tests/report/browser test -- navigation.spec.mjs`; expect absent UI behavior. No npm installation is needed for end users or C++ report generation.
- [ ] **Step 3 — Implement navigation and text-safe rendering.** Concatenate plain scripts in dependency order into the generated resource; use no module fetches, external fonts, browser storage, or network API. Build dynamic data with DOM/textContent, not HTML strings. `asset()` returns labeled text when no blob exists. Preserve keyboard focus while changing frames and avoid hijacking arrow keys inside input/selection controls. Label all boundaries and completion states. Omit optional autoplay in the first release; it is not required by the spec.
- [ ] **Step 4 — Verify green and offline behavior.** Set Playwright context `serviceWorkers: 'block'` and `offline: true`, register HTTP(S) request detection, then navigate directly to file URLs. Assert zero external requests and zero page/console errors. Check 1440px and 390px viewports with no page-level horizontal overflow.
- [ ] **Step 5 — Commit.** `feat: add offline report navigation and safe rendering`.

## Task 8: Approved tactical combat layout and accurate effect presentation

**Files:** Implement `report/viewer/combat.js`; extend `dom.js`, `viewer.css`, `ReportFixtures.cpp`; create `tests/report/browser/combat.spec.mjs`.

**Interfaces:** Produce `globalThis.StsReportCombat.render(report, frame, previousFrame) -> HTMLElement`. It consumes only Task 1's JSON contract and Task 7's model/DOM helpers. It exposes no execution API and stores no mutable game state.

- [ ] **Step 1 — Write failing combat tests.** `played_card_persists` selects a card frame, asserts the full played card stays centered despite leaving the hand, and verifies source/target refs. `two_enemies_and_resolved_turn` asserts two enemies, ordered resolved moves, current-next intent labels, and the last card during End Turn. `health_semantics` checks `10 → 3` yields lost `7`, `10 → 7` after loss `7` plus heal `4` yields bar loss `3` and gross effects `7`/`4`, a fully blocked attack has no loss segment, and overkill `7` against `2` HP shows only `2` removed. `current_card_values` checks an X-cost/free card, temporary zero cost, and Searing Blow `+3` remain explicit despite base artwork.
- [ ] **Step 2 — Verify red.** Run the browser test command from Task 7 with `combat.spec.mjs`; expect missing combat presentation.
- [ ] **Step 3 — Implement the approved layout.** Port layout structure, not synthetic data or custom illustration code, from the approved mockup. Relics/potions stay at top; player left, played card/action middle, enemy roster right, hand/energy/piles below, effects right. Use supplied game images; stable entity refs preserve dead targets and distinguish same-slot replacements. Display current costs/modifiers and resolved amounts separately from printed artwork. Use hatched static HP-loss segments, separate blue Block, positive healing labels, and text/numeric equivalents. Do not implement browser-side card rules.
- [ ] **Step 4 — Add and pass `combat_selection_and_unknowns`.** A pending selection retains its parent card and candidate cards without playable controls; unavailable card/monster art uses labels; unknown non-attack intents show their move name and unsupported values rather than zero damage. All combat tests pass at desktop and narrow widths; inspect actual-art multi-enemy and enemy-phase screenshots locally.
- [ ] **Step 5 — Commit.** `feat: render readable combat action slides`.

## Task 9: Full-run screens and terminal summaries

**Files:** Implement `report/viewer/screens.js`; extend `viewer.js`, `viewer.css`, `ReportFixtures.cpp`; create `tests/report/browser/screens.spec.mjs`.

**Interfaces:** Produce `globalThis.StsReportScreens.render(report, frame, previousFrame) -> HTMLElement`, dispatched by `state.screen.kind`. Reuse persistent inventory, selected-action effects, navigation, and Task 7's safe DOM helpers.

- [ ] **Step 1 — Write failing screen tests.** Fixtures cover all Task 3 screen kinds. Assert route/chosen map node, event choice/cost/reward, purchased item and original shop price, taken/skipped normal and boss rewards, rest healing/upgrades, and remove/transform/multi-select choices. A shop-to-map transition must still show its pre-action offer. Victory, loss, and paused/interrupted summaries must have distinct labels and final inventories.
- [ ] **Step 2 — Verify red.** Run browser test filter `screens.spec.mjs`; expect fallback-only views or missing structured fields.
- [ ] **Step 3 — Implement views.** Render recorded option lists as historical content, never buttons for taking options. Keep current floor/run state and top inventory visible. Show observed inventory/HP/gold/deck changes, not narrative claims unsupported by the trace. Render the recorded map's routes and node labels without custom illustration assets.
- [ ] **Step 4 — Verify green.** All screen tests pass offline, including HTML-like event text and missing artwork. Check that all interactive elements are navigation or inspection controls and no view exposes a simulator endpoint.
- [ ] **Step 5 — Commit.** `feat: render complete run history outside combat`.

## Task 10: User-facing report command, recovery behavior, and final regressions

**Files:** Create `include/sim/report/ReportOptions.h`, `report/ReportOptions.cpp`, `apps/report.cpp`, `tests/report/{ReportCommandTests.cpp,DeterminismTests.cpp,BenchmarkReport.cpp}`, `tests/report/test_report_command.py`, `tests/report/browser/generated-run.spec.mjs`, and `docs/run-viewer.md`; update `README.md` and `CMakeLists.txt`.

**Interfaces:** Produce `ReportOptions parseReportOptions(const std::vector<std::string>& args)` with fields `seed` (`uint64_t`), `ascension`, `agent`, `simulationCount`, `output`, and optional `assets`. New executable is `sts-report`; parameters are `--seed <game-seed> --ascension <0..20> --agent <simple|search> --output <file.html> [--simulations <positive-int>] [--assets <manifest.json>]`. Default ascension is `0`, agent is `search`, and search simulation count preserves the current agent default `50000`. Seed and output are required; reject search-only options for the simple agent. No new Python-binding API is required.

- [ ] **Step 1 — Write failing CLI/error tests.** Reject missing seed/output, unknown/duplicate options, invalid seed/ascension, nonpositive simulation count, malformed asset manifests, output/trace path collisions, and existing destinations. Use report `<output>.trace.jsonl` beside the output and a same-directory temporary HTML during final packaging; do not overwrite any existing report or trace. Assert invalid destinations fail before the agent starts. Inject a handled recorder/export failure and assert nonzero exit status, preserved complete JSONL lines, no false success message, and explicitly interrupted HTML only if export succeeds. A fatal child-process termination may leave only the trace; never require a finished HTML in that case.
- [ ] **Step 2 — Verify red.** Run CTest filter `'^report.command_'` and Python `test_report_command.py`; expect no command implementation.
- [ ] **Step 3 — Implement CLI integration.** Preflight paths and asset catalog, value-initialize the chosen agent, configure its existing RNG/policy fields consistently with current runners, attach the recorder, run one game, and export atomically without clobbering another file. Initialize reporting-only counters/configuration explicitly; do not repair unrelated agent behavior. On handled interruption preserve trace/footer warnings and attempt an explicitly partial report. Report actual final success/failure only after streams close successfully.
- [ ] **Step 4 — Add deterministic end-to-end tests.** `determinism_both_agents` uses seed values `1`, `2`, `3` and identical settings for recorded/unrecorded runs; compare selected action bits, final display state, all final game RNG `{seed0,seed1,counter}` triples, and search-agent RNG state. SimpleAgent already retains action history. For ScumSearchAgent2, enable `printActions` in both comparison runs and capture/compare stdout as well as history; separately compare default-logging-disabled runs' final states so the audit setting does not conceal an ordinary-path regression. `determinism_battle_boundaries` executes a fixed legal action sequence on two initialized battle fixtures with and without the recorder and compares all six battle RNG triples after each committed action. Use search budget `10` for the full-run checks; do not infer win rates. Add controlled engine fixtures for supported screens and victory/loss/paused states not reached by those three seeds; retain real full-run traces as generated artifacts rather than claiming one seed necessarily visits every event type.
- [ ] **Step 5 — Verify the actual user workflow.** Generate one simple-agent and one search-agent report with the commands below, then run `generated-run.spec.mjs` against those real files using `file://` and offline contexts. Assert navigable combat plus non-combat frames, decoded embedded assets when supplied, final status, no external requests, and no console errors. Run all C++/Python/browser report suites and rebuild existing targets/bindings. If any baseline engine failure prevents this, report the blocker rather than substituting synthetic fixtures for real-run verification.
- [ ] **Step 6 — Measure disabled overhead.** Build the same benchmark workload against baseline revision `5728ec3` (before engine instrumentation) in a separate temporary build and compare seven alternating single-thread identical-workload runs to the recording-disabled new build. Compile the new `BenchmarkReport.cpp` against each revision's engine sources without modifying the baseline checkout. `report-benchmark` measures fixed combat-search operations with checksums and reports medians, throughput, allocation counters, and state equality. Investigate a median slowdown greater than 5%; do not claim a clean performance result from one noisy sample. Do not add a flaky timing assertion to normal CTest.
- [ ] **Step 7 — Document and commit.** Document build/submodule preparation, explicit asset download/local import, commands for both agents, absent-art fallback, offline opening, incomplete traces, output-file policy, and test commands. Record actual verification/performance evidence and commit `feat: ship standalone simulation run reports`.

Example commands after implementation (run from the repository root):

```bash
cmake -S . -B cmake-build-report -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-report --target sts-report report-tests report-fixtures -j2
python3 tools/prepare_report_assets.py --sources report/assets/sources.json --output files/report-assets
./cmake-build-report/sts-report --seed 4J8K2 --ascension 0 --agent simple --output files/simple-run.html --assets files/report-assets/manifest.json
./cmake-build-report/sts-report --seed 4J8K2 --ascension 0 --agent search --simulations 100 --output files/search-run.html --assets files/report-assets/manifest.json
ctest --test-dir cmake-build-report -R '^report\.' --output-on-failure
python3 -m unittest discover -s tests/report -p 'test_*.py'
./cmake-build-report/report-fixtures --output-dir cmake-build-report/report-fixtures
REPORT_FIXTURE_DIR="$PWD/cmake-build-report/report-fixtures" npm --prefix tests/report/browser test
```

## Plan self-review and execution gate

- [x] Spec coverage: Tasks 1–5 cover trace/state/effects/integration; Task 6 covers resources/art/security; Tasks 7–9 cover every approved view and navigation; Task 10 covers user commands, failure recovery, real runs, compatibility, and disabled overhead.
- [x] Step scan: every task has explicit red/green commands, concrete interfaces/assertions, and an independently reviewable deliverable. Preparation, resources, and docs are attached to their deliverables.
- [x] Type consistency: shared JSON field names and C++/browser method names above are the contracts used by dependent tasks. Unavailable engine orb content remains explicit rather than invented.
- [x] Review focus: all five listed failure modes have named owner-task tests.
- [x] Proportion: test assertions and signatures make decisions without embedding implementation bodies. No product code, dependency installation, or execution has begun.
- [ ] User reviews this plan and chooses Native or Subagent-driven execution before implementation.
