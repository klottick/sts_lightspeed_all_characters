(function (root) {
  'use strict';
  const D = () => root.StsReportDom;
  const el = (tag, value, cls) => D().text(tag, value, cls);
  function render(report, frame, previousFrame) {
    const state = frame.state || {}, screen = state.screen || {}, kind = screen.kind || 'unknown';
    const view = el('section', null, 'screen-view'); view.dataset.screenKind = kind;
    view.setAttribute('aria-label', `${screen.name || kind} recorded screen`);
    view.append(el('header', null, 'screen-heading'));
    const header = view.firstChild; header.append(el('p', `Act ${frame.act ?? state.act ?? '?'} · Floor ${frame.floor ?? state.floor ?? '?'}`, 'eyebrow'), el('h2', screen.name || `Unknown screen (${kind})`));
    renderInventory(report, view, state.inventory);
    const supported = ['map', 'event', 'shop', 'rewards', 'boss_relics', 'rest', 'treasure', 'card_select', 'combat', 'battle'];
    if (screen.unknown || !supported.includes(kind)) view.append(el('p', `This screen type is not supported by this viewer: ${kind}. Recorded data is preserved below.`, 'warning'));
    const player = state.player || {};
    const stats = el('section', null, 'run-stats'); stats.setAttribute('aria-label', 'Recorded player state');
    stats.append(el('span', `HP ${player.hp ?? 'unknown'} / ${player.maxHp ?? 'unknown'}`), el('span', `Gold ${player.gold ?? 'unknown'}`),
      el('span', `Deck ${state.inventory?.deck?.length ?? 'unknown'} cards`)); view.append(stats);
    if (kind === 'map') renderMap(report, view, frame, state.map || screen.map, screen);
    if (kind === 'combat' || kind === 'battle') view.append(el('p', 'Combat details are unavailable in this frame.', 'warning'));
    const beforeScreen = frame.beforeScreen;
    if (frame.action && beforeScreen && Array.isArray(beforeScreen.options)) renderHistoricalOptions(report, view, beforeScreen, frame.action);
    const actionForCurrent = beforeScreen && !sameScreen(beforeScreen, screen) ? null : frame.action;
    if (kind === 'shop' || kind === 'event' || kind === 'rewards' || kind === 'boss_relics' || kind === 'rest' || kind === 'treasure' || kind === 'card_select' || !supported.includes(kind)) renderOptions(report, view, screen, actionForCurrent);
    if (frame.action?.preActionOffer) {
      const offerSection = el('section', null, 'historical-offer');
      offerSection.append(el('h3', 'Recorded pre-action offer'));
      renderOffer(report, offerSection, frame.action.preActionOffer);
      view.append(offerSection);
    }
    if (frame.action) view.append(el('section', `Recorded action · ${frame.action.description || frame.action.type || 'unknown action'}`, 'recorded-action'));
    if (frame.effects?.length) {
      const effects = el('section', null, 'screen-effects'); effects.append(el('h3', 'Recorded changes'));
      const list = el('ul'); for (const effect of frame.effects) list.append(el('li', effect.description || `${effect.kind || 'unknown effect'} recorded`)); view.append(effects); effects.append(list);
    }
    renderOutcome(view, report, frame);
    return view;
  }
  function sameScreen(before, current) {
    if (!before || !current || before.kind !== current.kind) return false;
    if (before.name != null && current.name != null && before.name !== current.name) return false;
    if (before.eventId != null && current.eventId != null && before.eventId !== current.eventId) return false;
    return true;
  }
  function renderMap(report, parent, frame, map, screen) {
    const section = el('section', null, 'map-view'); section.append(el('h3', 'Recorded map and route'));
    if (!map || !Array.isArray(map.nodes)) {
      section.append(el('p', 'Map layout unavailable in this frame.', 'muted'));
      if (screen.options?.length) section.append(el('p', `Recorded next routes: ${screen.options.map(option => option.label || 'Unknown route').join(' · ')}`, 'route-summary'));
      renderRouteHistory(section, report, frame);
      parent.append(section); return;
    }
    const nodes = el('ol', null, 'map-nodes');
    const current = map.current || {};
    for (const node of map.nodes) {
      const isCurrent = node.x === current.x && node.y === current.y;
      const item = el('li', `${node.room || 'Unknown room'} · column ${node.x ?? '?'}, row ${node.y ?? '?'}${isCurrent ? ' · Current location' : ''}`, isCurrent ? 'map-node current' : 'map-node');
      item.dataset.mapX = node.x; item.dataset.mapY = node.y; nodes.append(item);
    }
    section.append(nodes);
    const edges = el('ul', null, 'map-edges');
    for (const node of map.nodes) {
      for (const x of node.edges || []) edges.append(el('li', `(${node.x ?? '?'}, ${node.y ?? '?'}) → (${x}, ${(Number.isFinite(node.y) ? node.y + 1 : '?')})`, 'map-edge'));
    }
    if (edges.childElementCount) section.append(edges);
    if (screen.options?.length) section.append(el('p', `Recorded next routes: ${screen.options.map(option => option.label || 'Unknown route').join(' · ')}`, 'route-summary'));
    renderRouteHistory(section, report, frame);
    parent.append(section);
  }
  function renderRouteHistory(parent, report, frame) {
    if (frame.act == null || frame.floor == null) return;
    const choices = (report.frames || []).filter(item => item.act === frame.act && item.floor === frame.floor && item.action?.type === 'map_route' && item.action.preActionOffer?.node);
    if (!choices.length) return;
    const section = el('section', null, 'route-history'); section.append(el('h4', 'Recorded route choices this floor'));
    for (const choice of choices) {
      const node = choice.action.preActionOffer.node;
      const label = `${node.room || 'Unknown room'} · column ${node.x ?? '?'}, row ${node.y ?? '?'}`;
      const selected = el('p', `${choice.id === frame.id ? 'Chosen route' : 'Traversed route'} · ${label}`, choice.id === frame.id ? 'chosen-route' : 'route-choice');
      selected.dataset.frameId = String(choice.id); section.append(selected);
    }
    parent.append(section);
  }
  function renderOptions(report, parent, screen, action) {
    const section = el('section', null, 'option-view'); section.append(el('h3', `${screen.kind === 'event' ? 'Event choices' : screen.kind === 'shop' ? 'Shop offer' : screen.kind === 'rest' ? 'Rest-site options' : screen.kind === 'card_select' ? 'Recorded selection options' : 'Recorded options'}`));
    const options = Array.isArray(screen.options) ? screen.options : [];
    if (!options.length) section.append(el('p', screen.unknown ? 'No options were recorded for this unknown screen.' : 'No options recorded.', 'muted'));
    for (const option of options) {
      const row = el('article', null, 'recorded-option');
      if (option.key != null) row.dataset.optionKey = String(option.key);
      if (option.id != null) row.dataset.optionId = String(option.id);
      const selected = Boolean(action && (
        (action.optionKey != null && option.key != null && action.optionKey === option.key) ||
        (action.optionId != null && option.id != null && action.optionId === option.id)
      ));
      row.append(el('h4', option.label || 'Unknown option'), el('p', selected ? 'Selected in this recording' : 'Recorded option · not an interactive game control'));
      if (option.data) renderOffer(report, row, option.data);
      section.append(row);
    }
    parent.append(section);
  }
  function renderHistoricalOptions(report, parent, screen, action) {
    const section = el('section', null, 'historical-options');
    section.append(el('h3', `Previous screen offers · ${screen.name || screen.kind || 'recorded screen'}`));
    const options = screen.options || [];
    if (!options.length) section.append(el('p', 'No offers recorded on the previous screen.', 'muted'));
    for (const option of options) {
      const row = el('article', null, 'recorded-option');
      if (option.key != null) row.dataset.optionKey = String(option.key);
      if (option.id != null) row.dataset.optionId = String(option.id);
      const selected = Boolean(action && (
        (action.optionKey != null && option.key != null && action.optionKey === option.key) ||
        (action.optionId != null && option.id != null && action.optionId === option.id)
      ));
      row.append(el('h4', option.label || 'Unknown option'), el('p', selected ? 'Selected in this recording' : 'Recorded option · not an interactive game control'));
      if (option.data) renderOffer(report, row, option.data);
      section.append(row);
    }
    parent.append(section);
  }
  function renderOffer(report, parent, offer) {
    const label = el('section', null, 'offer-detail');
    const kind = offer.kind || 'unknown';
    label.append(el('h4', offer.label || offer.name || `Recorded ${kind.replaceAll('_', ' ')}`));
    if (Number.isFinite(offer.price)) label.append(el('p', `Original price: ${offer.price} gold`));
    if (offer.card) { label.append(el('p', offer.card.name || offer.card.cardId || 'Unknown card')); if (offer.card.assetKey) label.append(D().asset(report, offer.card.assetKey, offer.card.name || 'Card art', 'option-art')); }
    for (const [key, value] of Object.entries(offer)) {
      if (['kind', 'card', 'label', 'name', 'price', 'assetKey'].includes(key) || value == null || typeof value === 'object') continue;
      label.append(el('p', `${key.replaceAll(/([A-Z])/g, ' $1')}: ${String(value)}`));
    }
    parent.append(label);
  }
  function renderInventory(report, parent, inventory) {
    const section = el('section', null, 'run-inventory inventory-strip'); section.append(el('h3', 'Current inventory'));
    const deck = el('details'); deck.append(el('summary', `Deck (${inventory?.deck?.length ?? 0})`));
    for (const card of inventory?.deck || []) deck.append(el('p', `${card.name || card.cardId || 'Unknown card'}${card.upgrades ? ` +${card.upgrades}` : ''}`));
    const relics = el('div', null, 'inventory-row'); relics.append(el('h4', 'Relics'));
    for (const relic of inventory?.relics || []) {
      const row = el('p', null, 'inventory-item');
      row.append(D().asset(report, relic.assetKey, relic.name, 'tiny-art'));
      row.append(el('span', ` ${relic.name || relic.id || 'Unknown relic'}${relic.counter == null ? '' : ` · counter ${relic.counter}`}`));
      relics.append(row);
    }
    const potions = el('div', null, 'inventory-row'); potions.append(el('h4', 'Potions'));
    for (const potion of inventory?.potions || []) {
      const row = el('p', null, 'inventory-item');
      row.append(D().asset(report, potion.assetKey, potion.empty ? `Empty potion slot ${potion.slot ?? '?'}` : potion.name, 'tiny-art'));
      row.append(el('span', `Slot ${potion.slot ?? '?'}: ${potion.empty ? 'Empty' : potion.name || 'Unknown potion'}`));
      potions.append(row);
    }
    section.append(deck, relics, potions); parent.append(section);
  }
  function renderOutcome(parent, report, frame) {
    if (!['victory', 'loss', 'paused', 'interrupted'].includes(report.status)) return;
    const last = report.frames?.at(-1);
    if (!last || (frame.id !== last.id && frame.kind !== 'run_end')) return;
    const labels = { victory: 'Victory', loss: 'Defeat', paused: 'Run paused', interrupted: 'Recording interrupted' };
    const outcome = el('section', null, 'run-outcome'); outcome.setAttribute('aria-label', labels[report.status]); outcome.append(el('h3', labels[report.status]));
    outcome.append(el('p', report.status === 'interrupted' || report.status === 'paused' ? 'This run has no completed outcome.' : `Final recorded state · Floor ${frame.floor ?? frame.state?.floor ?? '?'}`));
    if (report.run?.settings && typeof report.run.settings === 'object') {
      outcome.append(el('h4', 'Run settings'));
      outcome.append(el('pre', JSON.stringify(report.run.settings, null, 2), 'run-settings'));
    }
    parent.append(outcome);
  }
  root.StsReportScreens = { render };
}(globalThis));
