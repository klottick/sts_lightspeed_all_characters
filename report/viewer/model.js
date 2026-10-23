(function (root) {
  'use strict';
  function validateReport(report) {
    if (!report || typeof report !== 'object' || report.schemaVersion !== 1) return 'Unsupported report version.';
    if (!Array.isArray(report.frames) || !report.run || typeof report.run !== 'object') return 'Invalid report data.';
    return null;
  }
  function frameAt(report, index) {
    if (!report || !Array.isArray(report.frames) || !Number.isInteger(index) || index < 0 || index >= report.frames.length) return null;
    return report.frames[index];
  }
  function groupFrames(report) {
    const groups = new Map();
    (report?.frames || []).forEach((frame, index) => {
      const state = frame?.state || {};
      const key = [frame?.act ?? state.act ?? '?', frame?.floor ?? state.floor ?? '?', frame?.combatId ?? 'run', frame?.turn ?? '—'].join(':');
      if (!groups.has(key)) groups.set(key, { key, act: frame?.act ?? state.act ?? null, floor: frame?.floor ?? state.floor ?? null,
        combatId: frame?.combatId ?? null, turn: frame?.turn ?? null, start: index, end: index, indices: [] });
      const group = groups.get(key); group.end = index; group.indices.push(index);
    });
    return [...groups.values()];
  }
  function numeric(value) { return Number.isFinite(value) ? value : 0; }
  function healthDelta(beforeHp, afterHp, maxHp) {
    const before = numeric(beforeHp), after = numeric(afterHp), max = Math.max(0, numeric(maxHp));
    return { remaining: Math.max(0, Math.min(max, after)), lost: Math.max(0, before - after), gained: Math.max(0, after - before), maxHp: max };
  }
  root.StsReportModel = { validateReport, frameAt, groupFrames, healthDelta };
}(globalThis));
