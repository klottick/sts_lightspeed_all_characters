(function (root) {
  'use strict';
  const D = () => root.StsReportDom;
  const el = (tag, value, cls) => D().text(tag, value, cls);
  const list = (parent, values, render) => { (values || []).forEach((value, index) => parent.append(render(value, index))); };
  function health(actor, previous, effects, label) {
    const hp = actor?.hp, max = actor?.maxHp;
    if (!Number.isFinite(hp) || !Number.isFinite(max)) return el('p', `${label}: HP unavailable`, 'muted');
    const change = root.StsReportModel.healthDelta(previous?.hp ?? hp, hp, max);
    const section = el('section', null, 'health'); section.setAttribute('aria-label', `${label}: ${hp} of ${max} HP`);
    section.append(el('div', `HP ${hp} / ${max}${change.lost ? ` · ${change.lost} lost` : ''}${change.gained ? ` · ${change.gained} healed` : ''}`, 'health-label'));
    const track = el('div', null, 'health-track'); track.setAttribute('role', 'img'); track.setAttribute('aria-label', `${label} health ${hp} of ${max}`);
    const fill = el('span', null, 'health-fill'); fill.style.width = `${max ? change.remaining / max * 100 : 0}%`; track.append(fill);
    if (change.lost) { const loss = el('span', null, 'health-loss'); loss.style.left = `${Math.max(0, hp) / max * 100}%`; loss.style.width = `${Math.min(change.lost, max) / max * 100}%`; track.append(loss); }
    section.append(track);
    const relevant = (effects || []).filter(effect => effect.target === actor?.ref);
    for (const effect of relevant) {
      if (effect.kind === 'damage') section.append(el('small', `Damage ${known(effect.incoming)} · Block absorbed ${known(effect.absorbedBlock)} · HP lost ${known(effect.hpLost)} · Overkill ${known(effect.overkill)}`, 'effect-math'));
      if (effect.kind === 'heal') section.append(el('small', `Healed ${known(effect.amount)} HP`, 'heal-label'));
    }
    return section;
  }
  function known(value) { return Number.isFinite(value) ? String(value) : 'unknown'; }
  function statuses(parent, entries) { const row = el('div', null, 'status-list'); list(row, entries, status => el('span', `${status.name || status.id || 'Unknown status'}${status.amount == null ? '' : ` ${status.amount}`}`, 'chip')); parent.append(row); }
  function characterName(id) {
    return typeof id === 'string' ? id.toLowerCase().split('_').map(part => part ? part[0].toUpperCase() + part.slice(1) : '').join(' ') : 'Player';
  }
  function actorName(frame, ref) {
    if (ref == null || ref === '') return null;
    const state = frame.state || {}, combat = state.combat || {};
    if (state.player?.ref === ref || ref === 'player') return characterName(state.character || 'Player');
    const enemy = (combat.enemies || []).find(item => item.ref === ref);
    if (enemy) return enemy.name || enemy.monsterId || null;
    if (typeof ref === 'string' && ref.startsWith('potion/')) {
      const slot = Number(ref.slice('potion/'.length));
      const potion = (state.inventory?.potions || []).find(item => item.slot === slot);
      if (potion && !potion.empty) return potion.name || potion.id || null;
    }
    const cards = [frame.action?.card, combat.lastPlayedCard, ...(combat.hand || []), ...(combat.piles?.draw || []), ...(combat.piles?.discard || []), ...(combat.piles?.exhaust || []), ...(state.inventory?.deck || [])];
    const card = cards.find(item => item && item.ref === ref);
    return card?.name || card?.cardId || null;
  }
  function enemyCard(report, frame, enemy, previous, effects, action) {
    const article = el('article', null, `enemy${enemy.lifeState === 'defeated' ? ' defeated' : ''}`); article.dataset.entityRef = enemy.ref || '';
    article.setAttribute('aria-label', enemy.name || enemy.monsterId || 'Unknown enemy');
    const heading = el('div', null, 'enemy-heading'); heading.append(D().asset(report, enemy.assetKey || `monster/${enemy.monsterId || ''}`, enemy.name || 'Unknown enemy', 'enemy-art'));
    const name = el('div', null); name.append(el('h3', enemy.name || enemy.monsterId || 'Unknown enemy')); heading.append(name); article.append(heading);
    const intent = enemy.intent || {};
    const defeated = enemy.lifeState === 'defeated' || enemy.lifeState === 'escaped' || enemy.lifeState === 'half_dead';
    article.append(el('p', defeated ? `${enemy.lifeState === 'escaped' ? 'Escaped' : enemy.lifeState === 'half_dead' ? 'Half-dead' : 'Defeated'} · no further actions` : `Next intent · ${intent.name || intent.moveId || 'Unknown intent'}${intent.kind === 'attack' ? ` · ${known(intent.damagePerHit)} × ${known(intent.hitCount)}` : intent.kind === 'non_attack' ? ' · non-attack' : ' · unsupported intent'}`, 'intent'));
    article.append(health(enemy, previous?.combat?.enemies?.find(item => item.ref === enemy.ref), effects, enemy.name || 'Enemy'));
    article.append(el('p', `Block ${known(enemy.block)} · ${enemy.lifeState || 'life state unavailable'}`, 'statline')); statuses(article, enemy.statuses);
    const targeted = (action?.targets || []).includes(enemy.ref);
    if (targeted) article.append(el('p', 'Target of recorded action', 'target-tag'));
    return article;
  }
  function cardView(report, card, full = true) {
    const article = el('article', null, `card${full ? ' played-card' : ' hand-card'}`);
    article.setAttribute('aria-label', `${card.name || card.cardId || 'Unknown card'}${card.currentCost === null ? '' : `, ${card.freeToPlay ? 'free' : `${known(card.currentCost)} energy`}`}`);
    article.dataset.cardRef = card.ref == null ? '' : card.ref;
    article.append(D().asset(report, card.assetKey || `card/${card.cardId || ''}`, card.name || card.cardId || 'Unknown card', 'card-art'));
    const details = el('div', null, 'card-details'); details.append(el('strong', card.name || card.cardId || 'Unknown card'));
    const cost = card.freeToPlay ? 'Free to play' : card.currentCost == null ? 'Cost unavailable' : `Current cost ${card.currentCost} energy`;
    details.append(el('span', `${cost}${card.baseCost == null || card.baseCost === card.currentCost ? '' : ` · base ${card.baseCost}`}`, 'card-cost'));
    if (card.upgrades) details.append(el('span', `+${card.upgrades} upgrade${card.upgrades === 1 ? '' : 's'}${card.specialData ? ` · special value ${card.specialData}` : ''}`, 'card-upgrade'));
    article.append(details); return article;
  }
  function effectView(frame, effect, index) {
    const item = el('li', null, 'effect-item'); item.dataset.effect = effect.kind || 'unknown';
    if (Number.isFinite(effect.hpLost)) item.dataset.hpLost = String(effect.hpLost);
    let description = effect.description || 'Recorded effect';
    if (effect.kind === 'enemy_move') {
      const source = actorName(frame, effect.source), target = actorName(frame, effect.target);
      description = `${effect.resolvedMove?.name || 'Unknown move'}${source ? ` by ${source}` : ''}${target ? ` → ${target}` : ''} resolved`;
    } else if (effect.kind === 'damage') {
      const source = actorName(frame, effect.source), target = actorName(frame, effect.target);
      description = `Damage${source ? ` from ${source}` : ''}${target ? ` to ${target}` : ''}: ${known(effect.incoming)} incoming, ${known(effect.absorbedBlock)} absorbed by Block, ${known(effect.hpLost)} HP lost${effect.overkill ? `, ${effect.overkill} overkill` : ''}`;
    } else if (effect.kind === 'heal') {
      const target = actorName(frame, effect.target);
      description = `Healed ${known(effect.amount)} HP${target ? ` · ${target}` : ''}`;
    } else if (effect.kind === 'state_change' || effect.kind === 'hp_change') {
      const target = actorName(frame, effect.target);
      description = `${effect.description || (effect.kind === 'hp_change' ? 'HP changed' : `${effect.field || 'State'} changed`)}${target ? ` · ${target}` : ''}`;
    } else if (effect.kind === 'spawn') {
      const target = actorName(frame, effect.target);
      description = `${effect.description || 'Enemy spawned'}${target ? ` · ${target}` : ''}`;
    }
    else if (!['state_change', 'hp_change', 'spawn'].includes(effect.kind)) description = `${effect.kind || 'unknown_effect'} · ${description}`;
    item.append(el('span', String(index + 1).padStart(2, '0'), 'effect-index'), el('span', description));
    if (effect.kind === 'state_change' && effect.field) item.append(el('small', `${effect.field}: ${JSON.stringify(effect.before)} → ${JSON.stringify(effect.after)}`, 'effect-detail'));
    return item;
  }
  function render(report, frame, previousFrame) {
    const state = frame.state || {}, combat = state.combat || {}, previous = previousFrame?.state || {};
    const main = el('section', null, 'combat-view'); main.setAttribute('aria-label', 'Combat state');
    const top = el('header', null, 'combat-heading'); top.append(el('div', `${state.character || report.run.character || 'Character'} · Act ${frame.act ?? state.act ?? '?'}, Floor ${frame.floor ?? state.floor ?? '?'}, Turn ${frame.turn ?? combat.turn ?? 'unknown'}`, 'eyebrow'), el('h2', combat.inputState === 'EXECUTING_ACTIONS' ? 'Enemy actions resolved' : frame.action?.description || combat.inputState || 'Combat state'));
    const inventories = el('div', null, 'inventory-row');
    const relics = el('section', null, 'inventory-group'); relics.append(el('h3', 'Relics'));
    list(relics, state.inventory?.relics, item => { const row = el('div', null, 'inventory-item'); row.append(D().asset(report, item.assetKey, item.name, 'small-art'), el('span', item.name || item.id || 'Unknown relic')); return row; });
    const potions = el('section', null, 'inventory-group'); potions.append(el('h3', 'Potions'));
    list(potions, state.inventory?.potions, item => { const row = el('div', null, 'inventory-item'); row.append(D().asset(report, item.assetKey, item.empty ? 'Empty potion slot' : item.name, 'small-art'), el('span', item.empty ? `Empty · slot ${item.slot ?? '?'}` : item.name || 'Unknown potion')); return row; });
    inventories.append(relics, potions); main.append(top, inventories);
    const grid = el('div', null, 'combat-grid');
    const player = el('section', null, 'player-board'); player.append(el('h3', 'Player'));
    player.append(D().asset(report, state.characterAssetKey, characterName(state.character || report.run.character), 'player-art'));
    player.append(health(state.player, previous.player, frame.effects, 'Player'), el('p', `Block ${known(state.player?.block)} · Energy ${known(state.player?.energy)} · Gold ${known(state.player?.gold)}`, 'statline')); statuses(player, state.player?.statuses);
    const center = el('section', null, 'action-board'); center.append(el('h3', frame.action?.type === 'end_turn' ? 'Enemy phase · resolved' : frame.action ? 'Recorded action' : 'Initial state'));
    if (frame.action?.card) center.append(cardView(report, frame.action.card));
    else if (frame.action) center.append(el('p', frame.action.description || frame.action.type || 'Unknown action'));
    if (frame.action && ('source' in frame.action || Array.isArray(frame.action.targets))) {
      const source = actorName(frame, frame.action.source);
      const targets = (frame.action.targets || []).map(ref => actorName(frame, ref)).filter(Boolean);
      center.append(el('p', `Source ${source || 'unavailable'} · Targets ${targets.length ? targets.join(', ') : 'none recorded'}`, 'action-refs'));
    }
    const lastCard = combat.lastPlayedCard || fallbackLastCard(report, frame);
    if (!frame.action?.card && lastCard) {
      center.append(el('p', 'Last card played', 'last-card-label'));
      center.append(cardView(report, lastCard));
    }
    if (combat.selection) {
      center.append(el('p', `Pending selection · ${combat.selection.task || 'unknown task'} · ${combat.selection.pickCount ?? 'unknown'} pick(s)`, 'selection-heading'));
      const candidates = el('div', null, 'candidate-list'); list(candidates, combat.selection.offered, card => cardView(report, card, false)); center.append(candidates);
    }
    const enemies = el('section', null, 'enemy-roster'); enemies.append(el('h3', `Enemies (${combat.enemies?.length || 0})`));
    list(enemies, combat.enemies, enemy => enemyCard(report, frame, enemy, previous, frame.effects, frame.action));
    grid.append(player, center, enemies); main.append(grid);
    const hand = el('section', null, 'hand-area'); hand.setAttribute('aria-label', 'Hand after resolution'); hand.append(el('h3', `Hand · ${combat.hand?.length || 0} cards · Energy ${known(state.player?.energy)}`));
    const cards = el('div', null, 'hand-cards'); list(cards, combat.hand, card => cardView(report, card, false)); hand.append(cards);
    const pileRow = el('div', null, 'pile-list');
    for (const pileName of ['draw', 'discard', 'exhaust']) {
      const entries = combat.piles?.[pileName] || [];
      const details = el('details', null, 'pile'); details.append(el('summary', `${pileName[0].toUpperCase()}${pileName.slice(1)} pile (${entries.length})`));
      list(details, entries, card => el('div', `${card.name || card.cardId || 'Unknown card'}${card.upgrades ? ` +${card.upgrades}` : ''}`, 'pile-card')); pileRow.append(details);
    }
    hand.append(pileRow); main.append(hand);
    const inspector = el('section', null, 'effect-inspector'); inspector.append(el('h3', 'Action resolution'));
    const effects = el('ol', null, 'effect-list'); list(effects, frame.effects, (effect, index) => effectView(frame, effect, index));
    if (!frame.effects?.length) effects.append(el('li', 'No effects recorded for this frame.', 'muted'));
    inspector.append(effects);
    main.append(inspector);
    return main;
  }
  function fallbackLastCard(report, frame) {
    const frameIndex = (report.frames || []).findIndex(item => item.id === frame.id);
    if (frameIndex < 0) return null;
    for (let i = frameIndex - 1; i >= 0; --i) {
      const prior = report.frames[i];
      if (prior.combatId !== frame.combatId) continue;
      if (prior.action?.card) return prior.action.card;
      if (prior.state?.combat?.lastPlayedCard) return prior.state.combat.lastPlayedCard;
    }
    return null;
  }
  root.StsReportCombat = { render };
}(globalThis));
