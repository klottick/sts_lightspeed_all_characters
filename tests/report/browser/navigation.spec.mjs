import { test, expect } from '@playwright/test';
import { cleanFixture, reportFixture, sampleCombatFrames, writeFixture, auditBrowser } from './fixture-builder.mjs';

let fixture, audit;
test.beforeEach(async ({ page }) => { audit = auditBrowser(page); fixture = await writeFixture(reportFixture({ frames: sampleCombatFrames() })); });
test.afterEach(async () => { await cleanFixture(fixture); expect(audit).toEqual({ page: [], console: [], external: [] }); });

test('navigation_boundaries_keyboard_and_scrubber', async ({ page }) => {
  await page.goto(fixture.url);
  const previous = page.getByRole('button', { name: /previous/i });
  const next = page.getByRole('button', { name: /next/i });
  const scrubber = page.getByRole('slider', { name: /recorded slide/i });
  await expect(page.getByRole('button', { name: /jump to act 1/i })).toBeVisible();
  await expect(page.getByRole('button', { name: /jump to floor 1/i })).toBeVisible();
  await expect(page.getByRole('button', { name: /jump to combat 1/i })).toBeVisible();
  await expect(page.getByRole('button', { name: /jump to turn 1/i }).first()).toBeVisible();
  await expect(previous).toBeDisabled();
  await expect(page.locator('.frame-content > [data-frame-id="0"]')).toBeVisible();
  await next.click();
  await expect(page.locator('.frame-content > [data-frame-id="1"]')).toBeVisible();
  await page.keyboard.press('ArrowRight');
  await expect(page.locator('.frame-content > [data-frame-id="2"]')).toBeVisible();
  await scrubber.fill('3');
  await expect(page.locator('.frame-content > [data-frame-id="3"]')).toBeVisible();
  await expect(next).toBeDisabled();
  await expect(page.getByText(/victory/i).first()).toBeVisible();
});

test('navigation_group_jumps_pile_inspection_and_safe_unknown_content', async ({ page }) => {
  const frames = sampleCombatFrames();
  frames[1].floor = 7;
  frames[2].effects.push({ kind: 'unknown_effect', description: '<img src=x onerror=alert(1)>' });
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames, unsupportedEffect: false }));
  await page.goto(fixture.url);
  await page.getByLabel(/jump to floor 7/i).click();
  await expect(page.locator('.frame-content > [data-frame-id="1"]')).toBeVisible();
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.getByText(/unknown_effect|unsupported effect/i).first()).toBeVisible();
  await expect(page.locator('img[src="x"]')).toHaveCount(0);
  const html = await page.locator('body').innerText();
  expect(html).toContain('<img src=x onerror=alert(1)>');
  const discard = page.locator('details.pile').filter({ hasText: /Discard pile/ });
  await expect(discard).toBeVisible();
  await discard.locator('summary').click();
  await expect(discard.locator('.pile-card')).toContainText('Bash');
});

test('unsupported_schema_and_empty_interrupted_reports_are_readable', async ({ page }) => {
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ schemaVersion: 9, frames: [] }));
  await page.goto(fixture.url);
  await expect(page.getByRole('alert')).toContainText(/unsupported.*version/i);
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames: [], status: 'interrupted', warnings: ['partial trace'] }));
  await page.goto(fixture.url);
  await expect(page.getByText(/interrupted/i).first()).toBeVisible();
  await expect(page.locator('.frame-content .empty-state')).toBeVisible();
});

test('desktop_workspace_has_left_timeline_and_right_resolution_inspector', async ({ page }) => {
  await page.setViewportSize({ width: 1440, height: 900 });
  await page.goto(fixture.url);
  await expect(page.locator('.viewer-workspace > .timeline')).toBeVisible();
  await expect(page.locator('.viewer-workspace > .frame-content')).toBeVisible();
  await expect(page.locator('.viewer-workspace > .resolution-panel > .effect-inspector')).toBeVisible();
  const columns = await page.locator('.viewer-workspace').evaluate(node => getComputedStyle(node).gridTemplateColumns);
  expect(columns.trim().split(/\s+/)).toHaveLength(3);
});

test('turn_milestones_disambiguate_combat_and_floor', async ({ page }) => {
  const frames = sampleCombatFrames();
  frames[1].floor = 4; frames[2].floor = 4;
  const secondCombatStart = structuredClone(frames[0]);
  secondCombatStart.id = 4; secondCombatStart.previousFrameId = 3; secondCombatStart.floor = 8;
  secondCombatStart.combatId = 2; secondCombatStart.turn = 1; secondCombatStart.state.combat.id = 2;
  const secondCombatTurn = structuredClone(frames[2]);
  secondCombatTurn.id = 5; secondCombatTurn.previousFrameId = 4; secondCombatTurn.floor = 8;
  secondCombatTurn.combatId = 2; secondCombatTurn.turn = 1; secondCombatTurn.state.combat.id = 2;
  await cleanFixture(fixture);
  fixture = await writeFixture(reportFixture({ frames: [...frames.slice(0, 3), secondCombatStart, secondCombatTurn] }));
  await page.goto(fixture.url);
  const turns = page.getByRole('button', { name: /jump to turn 1/i });
  await expect(turns).toHaveCount(2);
  await expect(turns.nth(0)).toContainText(/Floor 4.*Combat 1/i);
  await expect(turns.nth(1)).toContainText(/Floor 8.*Combat 2/i);
});

test('many_asset_warnings_are_collapsed_but_run_status_stays_visible', async ({ page }) => {
  const warnings = Array.from({ length: 30 }, (_, index) => `Missing image ${index + 1}`);
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames: sampleCombatFrames(), status: 'victory', warnings }));
  await page.goto(fixture.url);
  await expect(page.getByRole('status')).toContainText('victory');
  const summary = page.locator('.warning-summary');
  await expect(summary).toContainText('30 warnings');
  await expect(page.locator('.warning-list details')).not.toHaveAttribute('open', '');
  const firstWarning = page.locator('.warning-items > li').filter({ hasText: /^Missing image 1$/ });
  await expect(firstWarning).toBeHidden();
  await summary.click();
  await expect(firstWarning).toBeVisible();
  await page.getByRole('button', { name: /next/i }).click();
  await page.getByRole('button', { name: /next/i }).click();
  await page.getByRole('button', { name: /next/i }).click();
  await expect(page.locator('.run-outcome .warning')).toHaveCount(0);
});

test('responsive_viewports_and_no_page_errors', async ({ page }) => {
  const errors = []; page.on('pageerror', error => errors.push(error.message));
  await page.goto(fixture.url);
  for (const width of [1440, 390]) {
    await page.setViewportSize({ width, height: 900 });
    expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBeTruthy();
  }
  expect(errors).toEqual([]);
});

test('long_run_milestones_do_not_push_the_combat_board_offscreen', async ({ page }) => {
  const frames = Array.from({ length: 120 }, (_, index) => {
    const frame = structuredClone(sampleCombatFrames()[0]);
    frame.id = index; frame.previousFrameId = index ? index - 1 : null;
    frame.floor = index; frame.combatId = index + 1; frame.turn = index;
    return frame;
  });
  await cleanFixture(fixture); fixture = await writeFixture(reportFixture({ frames }));
  await page.setViewportSize({ width: 1440, height: 900 });
  await page.goto(fixture.url);
  const bounds = await page.locator('.jump-nav').boundingBox();
  expect(bounds.height).toBeLessThanOrEqual(100);
  expect(await page.locator('.jump-nav').evaluate(node => node.scrollHeight > node.clientHeight)).toBeTruthy();
});
