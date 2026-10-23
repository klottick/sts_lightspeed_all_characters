import { test, expect } from '@playwright/test';
import { cleanFixture, reportFixture, sampleCombatFrames, writeFixture, auditBrowser } from './fixture-builder.mjs';

let fixture, audit;
const state = (kind, name, options = []) => ({ ...sampleCombatFrames()[0].state, screen: { kind, name, options }, combat: null });
const f = (id, kind, name, options = [], action = null) => ({ id, kind: 'action', previousFrameId: id ? id - 1 : null, act: 1, floor: id + 1,
  combatId: null, turn: null, action, beforeScreen: null, state: state(kind, name, options), effects: [] });

test.beforeEach(async ({ page }) => {
  audit = auditBrowser(page);
  const frames = [
    f(0, 'map', 'Map', [{ key: 'map/1', label: 'Choose Monster', data: { x: 1, y: 2, room: 'MONSTER' } }], { type: 'map_route', optionKey: 'map/1', description: 'Choose Monster node (1, 2)', preActionOffer: { kind: 'map_node', node: { x: 1, y: 2, room: 'MONSTER' } } }),
    f(1, 'event', 'Ancient Event', [{ key: 'event/0', label: '<b>Give 10 gold</b>', data: { cost: 10, reward: 'Relic' } }, { key: 'event/1', label: 'Take 10 damage', data: { cost: 0, reward: 'Relic' } }], { type: 'event_choice', optionKey: 'event/0', description: 'Give 10 gold', preActionOffer: { kind: 'event_choice', label: 'Give 10 gold', cost: 10, reward: 'Relic' } }),
    f(2, 'shop', 'Shop', [{ key: 'shop/card/1', label: 'Buy Bash', data: { kind: 'card', card: { name: 'Bash', assetKey: 'card/BASH' }, price: 48 } }]),
    f(3, 'map', 'Map', [{ key: 'shop/card/1', label: 'Choose Rest site' }], { type: 'shop_purchase', optionKey: 'shop/card/1', description: 'Purchased Bash', preActionOffer: { kind: 'card', card: { name: 'Bash', assetKey: 'card/BASH' }, price: 48 } }),
    f(4, 'rewards', 'Rewards', [{ key: 'reward/taken', label: 'Take Strike', data: { kind: 'card', card: { name: 'Strike', assetKey: 'card/STRIKE_RED' } } }, { key: 'reward/skip', label: 'Skip card choice', data: { kind: 'skip_card' } }], { type: 'reward', optionKey: 'reward/taken', description: 'Take Strike', preActionOffer: { kind: 'card', card: { name: 'Strike' } } }),
    f(5, 'boss_relics', 'Boss relic rewards', [{ key: 'boss/0', label: 'Take Coffee Dripper', data: { kind: 'relic', name: 'Coffee Dripper' } }, { key: 'boss/skip', label: 'Skip reward' }], { type: 'boss_relic', optionKey: 'boss/0', description: 'Take Coffee Dripper', preActionOffer: { kind: 'relic', name: 'Coffee Dripper' } }),
    f(6, 'rest', 'Rest site', [{ key: 'rest/heal', label: 'Rest' }, { key: 'rest/smith', label: 'Smith: upgrade a card' }], { type: 'rest', optionKey: 'rest/heal', description: 'Rest · healed 6 HP' }),
    f(7, 'treasure', 'Treasure room', [{ key: 'treasure/open', label: 'Open chest' }, { key: 'treasure/skip', label: 'Skip reward' }]),
    f(8, 'card_select', 'Card selection', [{ key: 'select/0', label: 'Remove Strike' }, { key: 'select/1', label: 'Transform Defend' }, { key: 'select/2', label: 'Choose multiple cards' }], { type: 'multi_card_select', optionKey: 'select/1', description: 'Remove Strike and Transform Defend' }),
  ];
  frames[3].beforeScreen = { kind: 'shop', name: 'Shop', options: [
    { key: 'shop/card/1', label: 'Buy Bash', data: { kind: 'card', card: { name: 'Bash' }, price: 48 } },
    { key: 'shop/relic/0', label: 'Buy Burning Blood', data: { kind: 'relic', name: 'Burning Blood', price: 99 } },
  ] };
  frames[6].effects = [{ kind: 'heal', description: 'Healed 6 HP', amount: 6 }, { kind: 'state_change', description: 'Upgraded Bash' }];
  frames[6].state.inventory.deck = [{ name: 'Bash', upgrades: 1 }];
  fixture = await writeFixture(reportFixture({ frames }));
});
test.afterEach(async () => { await cleanFixture(fixture); expect(audit).toEqual({ page: [], console: [], external: [] }); });

test('all_screen_kinds_show_recorded_options_without_game_controls', async ({ page }) => {
  await page.goto(fixture.url);
  const checks = [
    ['Map', 'Choose Monster'], ['Ancient Event', 'cost: 10'], ['Shop', 'Original price: 48 gold'],
    ['Map', 'Purchased Bash'], ['Rewards', 'Skip card choice'], ['Boss relic rewards', 'Coffee Dripper'], ['Rest site', 'Smith: upgrade a card'], ['Treasure room', 'Open chest'], ['Card selection', 'Transform Defend'],
  ];
  for (let i = 0; i < checks.length; i++) {
    if (i) await page.getByRole('button', { name: /next/i }).click();
    await expect(page.locator('.frame-content h2')).toContainText(checks[i][0]);
    await expect(page.locator('.frame-content')).toContainText(checks[i][1]);
    await expect(page.locator('.recorded-option button')).toHaveCount(0);
    if (i === 6) {
      await expect(page.locator('.frame-content')).toContainText('Healed 6 HP');
      await expect(page.locator('.frame-content')).toContainText('Upgraded Bash');
      await expect(page.locator('.frame-content')).toContainText('Bash +1');
    }
  }
  await expect(page.locator('.frame-content')).toContainText(/Selected in this recording/);
  await expect(page.locator('.recorded-option').filter({ hasText: 'Remove Strike' })).toContainText('Recorded option');
  await expect(page.locator('.recorded-option').filter({ hasText: 'Transform Defend' })).toContainText('Selected in this recording');
  await expect(page.locator('img[src^="http"]')).toHaveCount(0);
  for (const width of [1440, 390]) {
    await page.setViewportSize({ width, height: 900 });
    expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBeTruthy();
  }
});

test('option_keys_select_only_the_exact_recorded_option_without_guessing', async ({ page }) => {
  const choices = [
    f(0, 'event', 'First Event', [{ key: 'event/0', label: 'Same label' }, { key: 'event/1', label: 'Same label' }], { type: 'event_choice', optionKey: 'event/1', description: 'Same label' }),
    f(1, 'event', 'Second Event', [{ key: 'event/0', label: 'No key recorded', data: { index: 0 } }, { key: 'event/1', label: 'Other option', data: { index: 1 } }], { type: 'event_choice', description: 'No key recorded', preActionOffer: { kind: 'event_choice', index: 0 } }),
    f(2, 'event', 'Legacy Event', [{ id: 'legacy/0', label: 'Legacy explicit ID' }, { id: 'legacy/1', label: 'Not selected' }], { type: 'event_choice', optionId: 'legacy/0' }),
  ];
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames: choices })); await page.goto(fixture.url);
  await expect(page.locator('.recorded-option')).toHaveCount(2);
  await expect(page.locator('.recorded-option').filter({ hasText: 'Same label' }).nth(0)).toContainText('Recorded option');
  await expect(page.locator('.recorded-option').filter({ hasText: 'Same label' }).nth(1)).toContainText('Selected in this recording');
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.recorded-option')).toHaveCount(2);
  await expect(page.locator('.recorded-option').filter({ hasText: 'No key recorded' })).toContainText('Recorded option');
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.recorded-option').filter({ hasText: 'Legacy explicit ID' })).toContainText('Selected in this recording');
  await expect(page.locator('.recorded-option').filter({ hasText: 'Not selected' })).toContainText('Recorded option');
});

test('historical_screen_offers_and_map_route_context_survive_transitions', async ({ page }) => {
  const first = f(0, 'map', 'Map', [{ key: 'route/a', label: 'Choose row one' }], { type: 'map_route', optionKey: 'route/a', preActionOffer: { kind: 'map_node', node: { x: 2, y: 1, room: 'MONSTER' } } });
  first.act = 1; first.floor = 3; first.state.map = { current: { x: 0, y: 0 }, nodes: [
    { x: 0, y: 0, room: 'START', edges: [2] }, { x: 2, y: 1, room: 'MONSTER', edges: [3] }, { x: 3, y: 2, room: 'REST', edges: [] },
  ] };
  const second = f(1, 'map', 'Map', [{ key: 'route/b', label: 'Choose row two' }], { type: 'map_route', optionKey: 'route/b', preActionOffer: { kind: 'map_node', node: { x: 3, y: 2, room: 'REST' } } });
  second.act = 1; second.floor = 3; second.state.map = { current: { x: 2, y: 1 }, nodes: first.state.map.nodes };
  const transitioned = f(2, 'shop', 'New Shop', [{ key: 'event/1', label: 'Unrelated current offer' }], { type: 'event_choice', optionKey: 'event/1', description: 'Changed screen', preActionOffer: { kind: 'event_choice', label: 'Chosen old event choice', index: 1 } });
  transitioned.act = 1; transitioned.floor = 3; transitioned.state.map = { current: { x: 3, y: 2 }, nodes: first.state.map.nodes };
  transitioned.beforeScreen = { kind: 'event', name: 'Old event', options: [
    { key: 'event/0', label: 'Old event choice' }, { key: 'event/1', label: 'Chosen old event choice' },
  ] };
  transitioned.action.optionKey = 'event/1';
  const otherFloor = f(3, 'map', 'Map', [], null); otherFloor.act = 1; otherFloor.floor = 4; otherFloor.state.map = second.state.map;
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames: [first, second, transitioned, otherFloor] })); await page.goto(fixture.url);
  await expect(page.locator('.chosen-route')).toContainText('column 2, row 1');
  await expect(page.locator('.route-history')).toContainText('column 3, row 2');
  await expect(page.locator('.map-edge').filter({ hasText: '(0, 0) → (2, 1)' })).toBeVisible();
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.chosen-route')).toContainText('column 3, row 2');
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.historical-options .recorded-option').filter({ hasText: 'Chosen old event choice' })).toContainText('Selected in this recording');
  await expect(page.locator('.option-view .recorded-option')).not.toContainText('Selected in this recording');
  await expect(page.locator('.historical-offer')).toBeVisible();
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.route-history .chosen-route')).toHaveCount(0);
});

test('terminal_outcomes_and_inventory_are_distinct', async ({ page }) => {
  const terminal = sampleCombatFrames()[3];
  const reports = ['victory', 'loss', 'paused', 'interrupted'];
  for (const status of reports) {
    await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames: [terminal], status }));
    await page.goto(fixture.url);
    const expected = { victory: 'Victory', loss: 'Defeat', paused: 'Run paused', interrupted: 'Recording interrupted' }[status];
    await expect(page.getByRole('region', { name: expected })).toBeVisible();
    await expect(page.getByRole('heading', { name: 'Current inventory' })).toBeVisible();
    if (status === 'paused' || status === 'interrupted') await expect(page.getByText(/no completed outcome/i)).toBeVisible();
  }
});

test('terminal_outcome_is_only_on_final_frame_and_contains_run_settings', async ({ page }) => {
  const frames = [f(0, 'map', 'Earlier floor'), sampleCombatFrames()[3]];
  frames[0].floor = 2; frames[1].floor = 18;
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames, status: 'loss', run: { settings: { simulationCount: 100, policy: 'fixed test' } } })); await page.goto(fixture.url);
  await expect(page.getByRole('status')).toContainText('loss');
  await expect(page.locator('.run-outcome')).toHaveCount(0);
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.run-outcome')).toContainText('Final recorded state · Floor 18');
  await expect(page.locator('.run-settings')).toContainText('simulationCount');
  await expect(page.locator('.run-settings')).toContainText('100');
});

test('inventory_stays_at_the_top_with_relic_counters_and_potion_slots', async ({ page }) => {
  const inventoryFrame = f(0, 'shop', 'Shop', [{ key: 'shop/leave', label: 'Leave shop' }]);
  inventoryFrame.state.inventory.relics = [{ id: 'BURNING_BLOOD', name: 'Burning Blood', counter: 2, assetKey: 'relic/BURNING_BLOOD' }];
  inventoryFrame.state.inventory.potions = [
    { slot: 0, id: 'FIRE_POTION', name: 'Fire Potion', empty: false, assetKey: 'potion/FIRE_POTION' },
    { slot: 1, name: null, empty: true, assetKey: null },
  ];
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames: [inventoryFrame] })); await page.goto(fixture.url);
  await expect(page.locator('.run-inventory')).toContainText('Burning Blood · counter 2');
  await expect(page.locator('.run-inventory')).toContainText('Slot 0: Fire Potion');
  await expect(page.locator('.run-inventory')).toContainText('Slot 1: Empty');
  const inventoryBeforeOptions = await page.locator('.screen-view').evaluate(node => Boolean(node.querySelector('.run-inventory').compareDocumentPosition(node.querySelector('.option-view')) & Node.DOCUMENT_POSITION_FOLLOWING));
  expect(inventoryBeforeOptions).toBeTruthy();
});

test('unknown_screen_and_no_art_use_readable_text_fallbacks', async ({ page }) => {
  const unknown = f(0, 'future_screen', '<img src=x onerror=alert(1)>', [{ id: 'future', label: '<script>not executable</script>' }]);
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames: [unknown] })); await page.goto(fixture.url);
  await expect(page.getByText('This screen type is not supported by this viewer: future_screen.')).toBeVisible();
  await expect(page.getByText('<script>not executable</script>')).toBeVisible();
  await expect(page.locator('img[src="x"]')).toHaveCount(0);
  await expect(page.locator('script:not([type="application/json"])')).toHaveCount(1);
});
