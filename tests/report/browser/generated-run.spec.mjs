import fs from 'node:fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { test, expect } from '@playwright/test';

const generatedDir = process.env.REPORT_GENERATED_DIR;
test.skip(!generatedDir, 'set REPORT_GENERATED_DIR to test actual sts-report outputs');

for (const name of ['simple-run.html', 'search-run.html']) {
  test(`real_${name.replaceAll('-', '_')}_opens_offline_and_matches_trace`, async ({ page }) => {
    const pageErrors = [], consoleErrors = [], external = [];
    page.on('pageerror', error => pageErrors.push(error.message));
    page.on('console', message => { if (message.type() === 'error') consoleErrors.push(message.text()); });
    page.on('request', request => { if (/^https?:/i.test(request.url())) external.push(request.url()); });

    const htmlPath = path.resolve(generatedDir, name);
    const tracePath = `${htmlPath}.trace.jsonl`;
    const traceLines = (await fs.readFile(tracePath, 'utf8')).trim().split(/\r?\n/).map(line => JSON.parse(line));
    const traceRun = traceLines[0];
    const traceFooter = traceLines.at(-1);
    const traceFrames = traceLines.filter(line => line.type === 'frame').map(line => line.frame);
    await page.goto(pathToFileURL(htmlPath).href);

    const report = await page.locator('#report-data').evaluate(node => JSON.parse(node.textContent));
    expect(report.status).toBe(traceFooter.status);
    expect(await page.getByRole('status').innerText()).toContain(traceFooter.status);
    expect(report.frames.length).toBeGreaterThan(1);
    expect(report.frames.map(frame => frame.id)).toEqual(traceFrames.map(frame => frame.id));
    expect(report.run.seed).toBe(traceRun.run.seed);
    expect(report.frames.some(frame => frame.state?.combat)).toBeTruthy();
    expect(report.frames.some(frame => frame.state?.screen?.kind !== 'battle')).toBeTruthy();

    const slider = page.getByRole('slider', { name: /recorded slide/i });
    await slider.fill('0');
    await expect(page.getByRole('button', { name: /previous recorded frame/i })).toBeDisabled();
    await slider.fill(String(report.frames.length - 1));
    await expect(page.getByRole('button', { name: /next recorded frame/i })).toBeDisabled();
    await slider.fill('0');
    const endTurn = report.frames.findIndex(frame => frame.action?.type === 'end_turn' && frame.state?.combat?.lastPlayedCard);
    if (endTurn >= 0) {
      await slider.fill(String(endTurn));
      await expect(page.locator('.action-board .played-card')).toBeVisible();
    }

    if (Object.keys(report.assets?.blobs || {}).length) {
      await expect.poll(() => page.locator('img').evaluateAll(images => images.filter(image => image.src.startsWith('data:image/') && image.naturalWidth > 0).length)).toBeGreaterThan(0);
    }
    expect(pageErrors).toEqual([]);
    expect(consoleErrors).toEqual([]);
    expect(external).toEqual([]);
  });
}
