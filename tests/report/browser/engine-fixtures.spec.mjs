import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { test, expect } from '@playwright/test';
import { auditBrowser } from './fixture-builder.mjs';

const directory = process.env.REPORT_FIXTURE_DIR;
test.skip(!directory, 'set REPORT_FIXTURE_DIR to exercise C++ exported fixtures');

for (const file of ['controlled-combat.html', 'screen-showcase.html']) {
  test(`engine_export_${file}_every_frame_is_readable_offline`, async ({ page }) => {
    const audit = auditBrowser(page);
    await page.goto(pathToFileURL(path.resolve(directory, file)).href);
    const frames = await page.locator('#report-data').evaluate(node => JSON.parse(node.textContent).frames);
    const slider = page.getByRole('slider', { name: /recorded slide/i });
    for (let index = 0; index < frames.length; index++) {
      await slider.fill(String(index));
      await expect(page.locator('.frame-content > [data-frame-id]')).toHaveAttribute('data-frame-id', String(frames[index].id));
      await expect(page.locator('.frame-content')).not.toBeEmpty();
      if (frames[index].state.combat && frames[index].state.screen.kind === 'battle') {
        await expect(page.locator('.enemy')).toHaveCount(frames[index].state.combat.enemies.length);
      }
    }
    expect(audit).toEqual({ page: [], console: [], external: [] });
  });
}
