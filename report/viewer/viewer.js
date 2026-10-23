(function (root) {
  'use strict';
  const D = () => root.StsReportDom;
  const el = (tag, value, cls) => D().text(tag, value, cls);
  function mount(doc, report) {
    const shell = doc.getElementById('report-shell');
    if (!shell) return null;
    const error = root.StsReportModel.validateReport(report);
    if (error) {
      shell.setAttribute('role', 'alert'); shell.dataset.state = 'error';
      D().replaceChildren(shell, el('h1', 'Simulation report'), el('p', error)); return shell;
    }
    let index = 0;
    const heading = el('header', null, 'page-heading');
    const title = el('div'); title.append(el('p', 'Read-only simulation recording', 'eyebrow'), el('h1', 'Slay the Spire · Run report'));
    const runInfo = report.run || {};
    const runMeta = el('div', null, 'run-meta');
    runMeta.append(el('span', `Seed ${runInfo.seedDisplay || runInfo.seed || 'unknown'}`), el('span', `A${runInfo.ascension ?? 'unknown'}`),
      el('span', runInfo.character || 'Character unavailable'), el('span', runInfo.agent || 'Agent unavailable'));
    heading.append(title, runMeta);
    const status = el('p', `Run status: ${report.status || 'unknown'}`, 'status-banner'); status.setAttribute('role', 'status');
    const warningBox = el('aside', null, 'warning-list');
    const warnings = Array.isArray(report.warnings) ? report.warnings : [];
    if (warnings.length) {
      const details = el('details', null, 'warning-details');
      details.append(el('summary', `${warnings.length} warning${warnings.length === 1 ? '' : 's'}`, 'warning-summary'));
      const list = el('ul', null, 'warning-items');
      for (const warning of warnings) list.append(el('li', warning, 'warning'));
      details.append(list); warningBox.append(details);
    }
    const timeline = el('nav', null, 'timeline'); timeline.setAttribute('aria-label', 'Recorded frames');
    const jumpNav = el('nav', null, 'jump-nav'); jumpNav.setAttribute('aria-label', 'Recorded milestones');
    const content = el('main', null, 'frame-content');
    const workspace = el('div', null, 'viewer-workspace');
    const resolution = el('aside', null, 'resolution-panel'); resolution.setAttribute('aria-label', 'Action resolution');
    const navigation = el('nav', null, 'navigation'); navigation.setAttribute('aria-label', 'Recording navigation');
    const previous = el('button', '← Previous', 'nav-button'); previous.type = 'button'; previous.setAttribute('aria-label', 'Previous recorded frame');
    const next = el('button', 'Next →', 'nav-button'); next.type = 'button'; next.setAttribute('aria-label', 'Next recorded frame');
    const scrubWrap = el('label', null, 'scrubber-wrap'); scrubWrap.append(el('span', 'Recorded slide'), Object.assign(doc.createElement('input'), { type: 'range', min: '0', max: String(Math.max(0, report.frames.length - 1)), value: '0', 'aria-label': 'Recorded slide' }));
    const counter = el('span', '', 'frame-counter'); counter.setAttribute('aria-live', 'polite');
    navigation.append(previous, scrubWrap, counter, next);
    const noFrames = el('p', 'No recorded actions or frames are available.', 'empty-state');
    workspace.append(timeline, content, resolution);
    D().replaceChildren(shell, heading, status, warningBox, jumpNav, workspace, navigation);
    const scrubber = scrubWrap.querySelector('input');

    function setIndex(value) {
      if (!report.frames.length) return;
      index = Math.max(0, Math.min(report.frames.length - 1, Number(value) || 0));
      renderCurrent();
    }
    function renderCurrent() {
      const frame = root.StsReportModel.frameAt(report, index);
      const previousFrame = root.StsReportModel.frameAt(report, index - 1);
      const view = frame.state?.combat && frame.state.screen?.kind === 'battle'
        ? root.StsReportCombat.render(report, frame, previousFrame)
        : root.StsReportScreens.render(report, frame, previousFrame);
      view.dataset.frameId = String(frame.id);
      const inspector = view.querySelector('.effect-inspector');
      if (inspector) D().replaceChildren(resolution, inspector);
      else D().replaceChildren(resolution, el('h2', 'Action resolution'), el('p', 'This recorded screen has no combat effects.', 'muted'));
      D().replaceChildren(content, view);
      previous.disabled = index === 0; next.disabled = index === report.frames.length - 1;
      scrubber.value = String(index); scrubber.max = String(Math.max(0, report.frames.length - 1));
      counter.textContent = report.frames.length ? `${index + 1} / ${report.frames.length}` : '0 / 0';
      for (const button of timeline.querySelectorAll('[data-frame-index]')) {
        const active = Number(button.dataset.frameIndex) === index;
        button.classList.toggle('current', active);
        if (active) button.setAttribute('aria-current', 'step'); else button.removeAttribute('aria-current');
      }
    }
    function jumpButton(label, target, aria) {
      const button = el('button', label, 'jump-button'); button.type = 'button'; button.setAttribute('aria-label', aria);
      button.addEventListener('click', () => setIndex(target)); return button;
    }
    report.frames.forEach((frame, frameIndex) => {
      const button = jumpButton(`${String(frame.id).padStart(2, '0')} · ${frame.action?.description || frame.state?.screen?.name || frame.kind || 'Recorded frame'}`, frameIndex, `Jump to frame ${frame.id}: ${frame.action?.description || frame.state?.screen?.name || frame.kind || 'recorded frame'}`);
      button.dataset.frameIndex = String(frameIndex); button.dataset.frameId = String(frame.id); timeline.append(button);
    });
    const milestones = new Map();
    const groups = root.StsReportModel.groupFrames(report);
    for (const field of ['act', 'floor', 'combatId']) {
      for (const group of groups) {
        const value = group[field];
        if (value == null) continue;
        const label = `${field === 'combatId' ? 'Combat' : field[0].toUpperCase() + field.slice(1)} ${value}`;
        if (!milestones.has(label)) milestones.set(label, group.start);
      }
    }
    for (const group of groups) {
      if (group.turn == null) continue;
      const context = [`Act ${group.act ?? '?'}`, `Floor ${group.floor ?? '?'}`];
      if (group.combatId != null) context.push(`Combat ${group.combatId}`);
      const label = `Turn ${group.turn} · ${context.join(' · ')}`;
      if (!milestones.has(label)) milestones.set(label, group.start);
    }
    for (const [description, target] of milestones) jumpNav.append(jumpButton(`Jump to ${description}`, target, `Jump to ${description}`));
    previous.addEventListener('click', () => setIndex(index - 1));
    next.addEventListener('click', () => setIndex(index + 1));
    scrubber.addEventListener('input', () => setIndex(scrubber.value));
    doc.addEventListener('keydown', event => {
      if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) return;
      const target = event.target;
      if (target instanceof HTMLElement && (target.isContentEditable || /^(INPUT|TEXTAREA|SELECT|SUMMARY)$/.test(target.tagName))) return;
      if (event.key === 'ArrowLeft') { event.preventDefault(); setIndex(index - 1); }
      if (event.key === 'ArrowRight') { event.preventDefault(); setIndex(index + 1); }
    });
    if (!report.frames.length) {
      timeline.append(noFrames); content.replaceChildren(noFrames.cloneNode(true)); previous.disabled = true; next.disabled = true;
      scrubber.disabled = true; counter.textContent = '0 / 0';
    } else renderCurrent();
    return shell;
  }
  root.StsReportViewer = { mount };

  const status = document.getElementById('report-status');
  try {
    const node = document.getElementById('report-data');
    const report = JSON.parse(node ? node.textContent : 'null');
    mount(document, report);
  } catch (_error) {
    if (status) { status.dataset.state = 'error'; status.textContent = 'Unable to read the embedded report data.'; }
    else { const shell = document.getElementById('report-shell'); if (shell) { shell.setAttribute('role', 'alert'); shell.textContent = 'Unable to read the embedded report data.'; } }
  }
}(globalThis));
