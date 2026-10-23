import { test, expect } from '@playwright/test';
import { cleanFixture, reportFixture, sampleCombatFrames, writeFixture, auditBrowser } from './fixture-builder.mjs';

let fixture, audit;
test.beforeEach(async ({ page }) => { audit = auditBrowser(page); fixture = await writeFixture(reportFixture({ frames: sampleCombatFrames() })); });
test.afterEach(async () => { await cleanFixture(fixture); expect(audit).toEqual({ page: [], console: [], external: [] }); });

test('played_card_persists_with_source_and_target_refs', async ({ page }) => {
  await page.goto(fixture.url); await page.getByRole('button', { name: /next/i }).click();
  await expect(page.getByRole('article', { name: /Bash.*2 energy/i })).toBeVisible();
  await expect(page.locator('.action-refs')).toContainText('Source Bash');
  await expect(page.locator('.action-refs')).toContainText('Targets Red Louse');
  await expect(page.locator('.hand-cards')).not.toContainText('Bash');
});

test('two_enemies_and_resolved_turn', async ({ page }) => {
  const frames = sampleCombatFrames();
  frames[1].state.combat.enemies[0].lifeState = 'defeated';
  frames[2].state.combat.lastPlayedCard = frames[1].action.card;
  frames[2].state.combat.enemies[0].lifeState = 'defeated';
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames }));
  await page.goto(fixture.url); await page.getByRole('button', { name: /next/i }).click(); await page.getByRole('button', { name: /next/i }).click();
  await expect(page.getByText('Red Louse').first()).toBeVisible(); await expect(page.getByText('Green Louse').first()).toBeVisible();
  await expect(page.getByText(/Bite by Red Louse.*resolved/i)).toBeVisible(); await expect(page.getByText(/Spit Web by Green Louse.*resolved/i)).toBeVisible();
  await expect(page.getByText(/Next intent/i).first()).toBeVisible();
  await expect(page.locator('.enemy[data-entity-ref="enemy-1"] .intent')).toContainText(/Defeated|No further action/i);
  await expect(page.locator('.enemy[data-entity-ref="enemy-1"] .intent')).not.toContainText('Next intent');
  await expect(page.locator('.action-board .played-card')).toContainText('Bash');
  await expect(page.locator('.effect-inspector .played-card')).toHaveCount(0);
  expect(await page.locator('.effect-item .effect-index').allTextContents()).toEqual(['01', '02', '03', '04']);
  await expect(page.locator('.effect-item').first()).toContainText('Red Louse');
});

test('player_character_art_uses_embedded_original_asset', async ({ page }) => {
  const png = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGNQcEj4DwADBAHAgChw2gAAAABJRU5ErkJggg==';
  const assets = { byKey: { 'character/IRONCLAD': 'blob-0' }, blobs: { 'blob-0': `data:image/png;base64,${png}` }, sources: [], attribution: '' };
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames: sampleCombatFrames(), assets }));
  await page.goto(fixture.url);
  const character = page.locator('.player-board img.player-art');
  await expect(character).toBeVisible();
  await expect(character).toHaveAttribute('alt', 'Ironclad');
  await expect.poll(() => character.evaluate(image => image.naturalWidth)).toBe(1);
});

test('null_actor_references_are_not_invented', async ({ page }) => {
  const frames = sampleCombatFrames();
  frames[1].action.source = null;
  frames[1].action.targets = [];
  frames[2].effects[0].source = null;
  frames[2].effects[0].target = null;
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames })); await page.goto(fixture.url);
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.action-refs')).toHaveText('Source unavailable · Targets none recorded');
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.effect-item').first()).not.toContainText(/unknown target|→ player/i);
});

test('combat_phase_cards_persist_only_within_the_recorded_combat_lifecycle', async ({ page }) => {
  const sample = sampleCombatFrames();
  const combatStart = structuredClone(sample[0]);
  combatStart.kind = 'combat_start'; combatStart.combatId = 1; combatStart.turn = 1;
  combatStart.state.combat.lastPlayedCard = null;
  const played = structuredClone(sample[1]);
  played.id = 1; played.previousFrameId = 0; played.state.combat.lastPlayedCard = played.action.card;
  const endTurn = structuredClone(sample[2]);
  endTurn.id = 2; endTurn.previousFrameId = 1; endTurn.state.combat.lastPlayedCard = played.action.card;
  const combatEnd = structuredClone(endTurn);
  combatEnd.id = 3; combatEnd.previousFrameId = 2; combatEnd.kind = 'combat_end'; combatEnd.action = null; combatEnd.state.combat.lastPlayedCard = played.action.card;
  const runState = structuredClone(sample[3]);
  runState.id = 4; runState.previousFrameId = 3; runState.kind = 'run_state';
  const nextCombat = structuredClone(combatStart);
  nextCombat.id = 5; nextCombat.previousFrameId = 4; nextCombat.combatId = 2; nextCombat.state.combat.id = 2;
  const runEnd = structuredClone(nextCombat);
  runEnd.id = 6; runEnd.previousFrameId = 5; runEnd.kind = 'run_end';
  await cleanFixture(fixture);
  fixture = await writeFixture(reportFixture({ frames: [combatStart, played, endTurn, combatEnd, runState, nextCombat, runEnd], status: 'interrupted' }));
  await page.goto(fixture.url);
  await page.getByRole('slider', { name: /recorded slide/i }).fill('3');
  await expect(page.locator('.action-board .played-card')).toContainText('Bash');
  await page.getByRole('slider', { name: /recorded slide/i }).fill('4');
  await expect(page.locator('.screen-view')).toBeVisible();
  await page.getByRole('slider', { name: /recorded slide/i }).fill('5');
  await expect(page.locator('.action-board .played-card')).toHaveCount(0);
  await page.getByRole('slider', { name: /recorded slide/i }).fill('6');
  await expect(page.locator('.action-board .played-card')).toHaveCount(0);
});

test('health_semantics_and_current_card_values', async ({ page }) => {
  const frames = sampleCombatFrames();
  frames[1].effects[0] = { kind: 'damage', target: 'player', incoming: 7, absorbedBlock: 0, hpLost: 7, overkill: 0, before: 10, after: 3 };
  frames[2].effects = [
    { kind: 'damage', target: 'player', incoming: 7, absorbedBlock: 0, hpLost: 7, overkill: 0, before: 10, after: 3 },
    { kind: 'heal', target: 'player', amount: 4, before: 3, after: 7 },
    { kind: 'damage', target: 'enemy-2', incoming: 4, absorbedBlock: 4, hpLost: 0, overkill: 0 },
  ];
  const xCard = { ...frames[1].action.card, name: 'Searing Blow', currentCost: 0, baseCost: -1, freeToPlay: true, upgrades: 3, specialData: 3 };
  frames[1].action.card = xCard;
  frames[1].state.combat.piles.discard[0] = xCard;
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames })); await page.goto(fixture.url);
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.getByRole('article', { name: /Searing Blow/i })).toContainText(/free|0 energy/i);
  await expect(page.getByRole('article', { name: /Searing Blow/i })).toContainText(/\+3|upgrade/i);
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.getByText(/7 HP lost/i)).toBeVisible();
  await expect(page.getByText(/Healed 4 HP/i).first()).toBeVisible();
  await expect(page.locator('.enemy[data-entity-ref="enemy-2"] .health-loss')).toHaveCount(0);
  await expect(page.locator('[data-effect="damage"][data-hp-lost="0"]')).toHaveCount(1);
});

test('combat_selection_and_unknowns', async ({ page }) => {
  const frames = sampleCombatFrames();
  frames[0].state.combat.selection = { task: 'DISCOVERY', pickCount: 1, offered: [{ ...frames[1].action.card, name: '<b>Odd card</b>' }] };
  frames[0].state.combat.enemies[1].intent = { moveId: 'MYSTERY', name: 'Mysterious move', kind: 'unknown', damagePerHit: null };
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames })); await page.goto(fixture.url);
  await expect(page.getByText(/pending selection/i)).toBeVisible();
  await expect(page.getByText('<b>Odd card</b>').first()).toBeVisible();
  await expect(page.getByText(/Mysterious move/i)).toBeVisible();
  await expect(page.getByText(/unknown.*intent|unsupported/i)).toBeVisible();
  await expect(page.locator('.asset-fallback').first()).toBeVisible();
  await expect(page.getByRole('button', { name: /choose.*Odd card/i })).toHaveCount(0);
});
