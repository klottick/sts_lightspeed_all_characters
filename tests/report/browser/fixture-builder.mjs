import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../');
const viewer = path.join(root, 'report/viewer');
const scripts = ['model.js', 'dom.js', 'combat.js', 'screens.js', 'viewer.js'];

const card = (name, ref, cost = 1) => ({ ref, cardId: name.toUpperCase().replaceAll(' ', '_'), name, upgrades: 0,
  baseCost: cost, currentCost: cost, freeToPlay: false, specialData: 0, assetKey: `card/${name.toUpperCase().replaceAll(' ', '_')}` });
const player = (hp = 10) => ({ ref: 'player', hp, maxHp: 10, gold: 50, block: 0, energy: 3, energyPerTurn: 3, statuses: [], stance: 'NEUTRAL' });
const frame = (id, kind, state, extras = {}) => ({ id, kind, previousFrameId: id ? id - 1 : null, act: 1, floor: id + 1,
  combatId: null, turn: null, action: null, beforeScreen: null, state, effects: [], ...extras });
const baseState = (screen = { kind: 'map', name: 'Map', options: [] }) => ({ screen, act: 1, floor: 1, ascension: 0,
  character: 'IRONCLAD', characterAssetKey: 'character/IRONCLAD', player: player(), inventory: { deck: [], relics: [], potions: [] }, map: null, combat: null });

export function reportFixture({ frames, status = 'victory', schemaVersion = 1, warnings = [], run = {}, catalog = {}, assets,
  unsupportedEffect = false } = {}) {
  const report = { schemaVersion, run: { seed: '123', seedDisplay: '123', character: 'IRONCLAD', ascension: 0, agent: 'simple', settings: {}, ...run },
    catalog, frames: frames ?? [], status, warnings, assets: assets ?? { byKey: {}, blobs: {}, sources: [], attribution: '' } };
  if (unsupportedEffect && report.frames[0]) report.frames[0].effects.push({ kind: 'future_effect', description: '<img src=x onerror=alert(1)>' });
  return report;
}

export function sampleCombatFrames() {
  const bash = card('Bash', 'card-1', 2);
  const defend = card('Defend', 'card-2');
  const start = baseState({ kind: 'battle', name: 'Combat', options: [] });
  start.combat = { id: 1, turn: 1, inputState: 'PLAYER_NORMAL', enemies: [
    { ref: 'enemy-1', slot: 0, monsterId: 'RED_LOUSE', name: 'Red Louse', hp: 2, maxHp: 15, block: 0, lifeState: 'alive', statuses: [], assetKey: 'monster/RED_LOUSE', intent: { moveId: 'BITE', name: 'Bite', kind: 'attack', damagePerHit: 7, hitCount: 1 } },
    { ref: 'enemy-2', slot: 1, monsterId: 'GREEN_LOUSE', name: 'Green Louse', hp: 13, maxHp: 13, block: 0, lifeState: 'alive', statuses: [], assetKey: 'monster/GREEN_LOUSE', intent: { moveId: 'SPIT_WEB', name: 'Spit Web', kind: 'non_attack', damagePerHit: null, hitCount: null } },
  ], hand: [bash, defend], piles: { draw: [], discard: [], exhaust: [] }, selection: null };
  const afterBash = structuredClone(start); afterBash.player.hp = 3; afterBash.combat.enemies[0].hp = 0; afterBash.combat.hand = [defend]; afterBash.combat.piles.discard = [bash];
  const afterTurn = structuredClone(afterBash); afterTurn.combat.turn = 2; afterTurn.player.hp = 7; afterTurn.player.statuses = [{ id: 'WEAK', name: 'Weak', amount: 2 }];
  const terminal = baseState({ kind: 'rewards', name: 'Rewards', options: [{ id: 'reward/1', label: 'Take card', data: { kind: 'card', card: card('Shrug It Off', null) } }] });
  terminal.player.hp = 7;
  return [
    frame(0, 'run_start', start),
    frame(1, 'action', afterBash, { turn: 1, combatId: 1, action: { type: 'card_play', source: 'card-1', targets: ['enemy-1'], card: bash, description: 'Play Bash' }, effects: [{ kind: 'damage', source: 'card-1', target: 'enemy-1', incoming: 7, absorbedBlock: 0, hpLost: 2, overkill: 5, before: 2, after: 0, description: '2 HP lost' }] }),
    frame(2, 'action', afterTurn, { turn: 1, combatId: 1, action: { type: 'end_turn', description: 'End Turn' }, effects: [
      { kind: 'enemy_move', source: 'enemy-1', target: 'player', resolvedMove: { name: 'Bite', kind: 'attack', damagePerHit: 7, hitCount: 1 }, description: 'Bite resolved' },
      { kind: 'damage', source: 'enemy-1', target: 'player', incoming: 7, absorbedBlock: 0, hpLost: 0, overkill: 0, description: 'Hit prevented HP loss' },
      { kind: 'enemy_move', source: 'enemy-2', target: 'player', resolvedMove: { name: 'Spit Web', kind: 'non_attack' }, description: 'Spit Web resolved' },
      { kind: 'state_change', source: 'enemy-2', target: 'player', field: 'statuses', before: [], after: [{ id: 'WEAK', name: 'Weak', amount: 2 }], description: 'Weak applied' },
    ] }),
    frame(3, 'transition', terminal),
  ];
}

export async function writeFixture(report) {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'sts-report-browser-'));
  let html = await fs.readFile(path.join(viewer, 'viewer.html'), 'utf8');
  const css = await fs.readFile(path.join(viewer, 'viewer.css'), 'utf8');
  const js = (await Promise.all(scripts.map((name) => fs.readFile(path.join(viewer, name), 'utf8')))).join('\n');
  const json = JSON.stringify(report).replaceAll('<', '\\u003c').replaceAll('>', '\\u003e').replaceAll('&', '\\u0026');
  html = html.replace('@@REPORT_JSON@@', json).replace('@@VIEWER_CSS@@', css).replace('@@VIEWER_JS@@', js);
  const file = path.join(dir, 'report.html');
  await fs.writeFile(file, html);
  return { dir, file, url: `file://${file}` };
}

export async function cleanFixture(fixture) {
  await fs.rm(fixture.dir, { recursive: true, force: true });
}

export function auditBrowser(page) {
  const failures = { page: [], console: [], external: [] };
  page.on('pageerror', error => failures.page.push(error.message));
  page.on('console', message => { if (message.type() === 'error') failures.console.push(message.text()); });
  page.on('request', request => { if (/^https?:/i.test(request.url())) failures.external.push(request.url()); });
  return failures;
}
