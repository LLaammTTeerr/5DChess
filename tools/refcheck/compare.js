#!/usr/bin/env node
// Differential test of the 5DChess engine against 5d-chess-js (https://github.com/alexbay218/5d-chess-js).
//
//   node compare.js --ref /path/to/5d-chess-js --bin build/tools/refcheck/refcheck \
//        [--games 200] [--turns 25] [--modes standard,emit_bishop,...] [--seed0 1] [--jobs 8] [--verbose]
//   node compare.js --ref ... --bin ... --vectors          compare the movement vector tables only
//
// `refcheck` plays one random legal game on our engine and prints every position (turn starts and after every
// move) with the full pseudo-legal move list. This script replays the same game in 5d-chess-js, move by move,
// and at every position compares: the tip boards, which timelines are active / on the present, the set of moves,
// whether the position is in check, whether the turn may be submitted, and (at turn starts, when the reference
// answers within its time limit) checkmate / stalemate. It prints the first divergence of each game.
//
// Coordinate mapping (engine -> reference), for 8x8 or NxN boards:
//   timeline ID n  ->  index l:  n > 0: 2n   n < 0: -2n-1   0: 0        (parse.js: even = White's, odd = Black's)
//   half-turn h    ->  t = h  (the reference's turn index of the same board; board 0 = White to move)
//   rank y         ->  r = y  (row 0 = White's back rank in both)
//   file x         ->  f = N-1-x   (the engine's starting position has K on the d-file and Q on the e-file; the
//                                   reference has Q on d and K on e, so the files are mirrored. Rules are symmetric.)
//   piece          ->  code: P 1, B 3, N 5, R 7, Q 9, K 11; White = code+1 (even), Black = code; negative = unmoved.
// Pawn double steps and castling are controlled in the reference by the "unmoved" sign, so in modes where the
// engine disables them the corresponding pieces are mapped as already moved.
'use strict';
const path = require('path');
const { spawn, fork } = require('child_process');
const readline = require('readline');

function parseArgs() {
  const a = { games: 200, turns: 25, modes: ['standard'], seed0: 1, jobs: 1, verbose: false, vectors: false,
              mateTimeout: 3000, searchBudget: 200000, mateMaxTimelines: 3, gameTimeout: 60, worker: false };
  const v = process.argv.slice(2);
  for (let i = 0; i < v.length; i++) {
    switch (v[i]) {
      case '--ref': a.ref = path.resolve(v[++i]); break;
      case '--bin': a.bin = path.resolve(v[++i]); break;
      case '--games': a.games = +v[++i]; break;
      case '--turns': a.turns = +v[++i]; break;
      case '--modes': a.modes = v[++i].split(','); break;
      case '--seed0': a.seed0 = +v[++i]; break;
      case '--jobs': a.jobs = +v[++i]; break;
      case '--mate-timeout': a.mateTimeout = +v[++i]; break;
      case '--search-budget': a.searchBudget = +v[++i]; break;
      case '--game-timeout': a.gameTimeout = +v[++i]; break;
      case '--mate-max-timelines': a.mateMaxTimelines = +v[++i]; break;
      case '--verbose': a.verbose = true; break;
      case '--vectors': a.vectors = true; break;
      case '--worker': a.worker = true; break;
      case '--tolerate': a.tolerate = v[++i].split(','); break;
      default: throw new Error('unknown argument ' + v[i]);
    }
  }
  if (!a.ref || !a.bin) throw new Error('--ref and --bin are required');
  return a;
}
const args = parseArgs();

// ---------------------------------------------------------------------------------------------------------------
// Load the reference (it uses module-alias, which reads package.json from the working directory).
process.chdir(args.ref);
const Chess = require(path.join(args.ref, 'src/index.js'));
const boardFuncs = require(path.join(args.ref, 'src/board.js'));
const pieceFuncs = require(path.join(args.ref, 'src/piece.js'));
const mateFuncs = require(path.join(args.ref, 'src/mate.js'));

const tlToIdx = (n) => (n > 0 ? 2 * n : n < 0 ? -2 * n - 1 : 0);
const idxToTl = (l) => (l === 0 ? 0 : l % 2 === 0 ? l / 2 : -(l + 1) / 2);
const PROMO = { 0: [10, 8, 4, 6], 1: [9, 7, 3, 5] }; // by colour (0 = White): Q R B N
const NAMES = { 1: 'p', 3: 'b', 5: 'n', 7: 'r', 9: 'q', 11: 'k' };
const sq = (n, r, f) => String.fromCharCode(97 + (n - 1 - f)) + (r + 1); // engine file letters: x = n-1-f

// ---------------------------------------------------------------------------------------------------------------
// Vector tables.
function compareVectors() {
  const { execFileSync } = require('child_process');
  const ours = JSON.parse(execFileSync(args.bin, ['--vectors']).toString());
  const toSet = (vs) => new Set(vs.map((v) => v.join(',')));
  // reference [l, t, r, f]  ->  engine (dx, dy, dz, dw) = (f, r, t, l)
  const ref = (list) => toSet(list.map(([l, t, r, f]) => [f, r, t, l]));
  const table = { king: ref(pieceFuncs.movePos(12)), knight: ref(pieceFuncs.movePos(6)), rook: ref(pieceFuncs.moveVecs(8)),
                  bishop: ref(pieceFuncs.moveVecs(4)), queen: ref(pieceFuncs.moveVecs(10)) };
  let ok = true;
  for (const name of Object.keys(table)) {
    const mine = toSet(ours[name]);
    const missing = [...table[name]].filter((x) => !mine.has(x));
    const extra = [...mine].filter((x) => !table[name].has(x));
    console.log(`${name.padEnd(7)} engine ${mine.size} vectors, reference ${table[name].size}` +
                (missing.length || extra.length ? `  MISSING ${JSON.stringify(missing)} EXTRA ${JSON.stringify(extra)}` : '  identical'));
    if (missing.length || extra.length) ok = false;
  }
  console.log('pawn    engine has no vectors (special-cased); reference moves() special-cases pawns as well');
  process.exit(ok ? 0 : 1);
}
if (args.vectors) compareVectors();

// ---------------------------------------------------------------------------------------------------------------
function buildReference(init) {
  const n = init.n;
  const g = new Chess();
  const raw = [];
  for (const [tl, h, cells] of init.tips) {
    const rows = [];
    for (let r = 0; r < n; r++) {
      const row = [];
      for (let f = 0; f < n; f++) row.push(normalize(init, cells[r * n + (n - 1 - f)]));
      rows.push(row);
    }
    boardFuncs.setTurn(raw, tlToIdx(tl), h, rows);
  }
  g.rawBoard = raw;
  g.rawAction = 0;
  g.rawStartingAction = 0;
  g.rawBoardHistory = [boardFuncs.copy(raw)];
  g.rawActionHistory = [];
  g.rawMoveBuffer = [];
  g.rawPromotionPieces = [10, 8, 4, 6, 9, 7, 3, 5];
  g.skipDetection = true; // no mate search inside move() / submit(); we call it ourselves
  return g;
}

// The engine keeps an "unmoved" flag on every piece but only uses it where the rules enable it.
function normalize(init, code) {
  const a = Math.abs(code);
  const base = a % 2 === 0 ? a - 1 : a;
  if (code < 0) {
    if (base === 1 && !init.doubleStep) return a;
    if ((base === 11 || base === 7) && !init.castling) return a;
  }
  return code;
}

function fmtTl(l) { return 'L' + idxToTl(l); }
function fmtPos(n, p) { return `${fmtTl(p[0])} t${p[1]} ${sq(n, p[2], p[3])}`; }
function fmtMoveKey(n, k) {
  const [a, b, promo, kind] = k.split('|');
  const [from, to] = a.split('>').map((s) => s.split(',').map(Number));
  return `${fmtPos(n, from)} -> ${fmtPos(n, to)}${promo !== '0' ? ' =' + (NAMES[(+promo + (+promo % 2 === 0 ? -1 : 0))] || promo) : ''}${kind ? ' [' + kind + ']' : ''}`;
}
function fmtBoard(n, rows) {
  const out = [];
  for (let r = n - 1; r >= 0; r--) {
    let s = (r + 1) + ' ';
    for (let f = n - 1; f >= 0; f--) { // print file a (x = 0) on the left: x = n-1-f
      const c = rows[r][f];
      if (!c) { s += '. '; continue; }
      const a = Math.abs(c);
      const base = a % 2 === 0 ? a - 1 : a;
      const ch = NAMES[base] || '?';
      s += (a % 2 === 0 ? ch.toUpperCase() : ch) + (c < 0 ? "'" : ' ');
    }
    out.push(s);
  }
  return out.join('\n');
}

// Move keys ------------------------------------------------------------------------------------------------------
function oursKeys(n, color, moves) {
  const keys = [];
  for (const m of moves) {
    const l = tlToIdx(m[0]), l2 = tlToIdx(m[4]);
    const base = `${l},${m[1]},${m[3]},${n - 1 - m[2]}>${l2},${m[5]},${m[7]},${n - 1 - m[6]}`;
    if (m[8] === 1) for (const p of PROMO[color]) keys.push(`${base}|${p}|`);
    else keys.push(`${base}|0|${m[8] === 2 ? 'castle' : m[8] === 3 ? 'ep' : ''}`);
  }
  return keys;
}
function refKeys(moves) {
  const keys = [];
  for (const m of moves) {
    const f = m[0], t = m[1];
    const promo = t[4] || 0;
    const kind = m.length === 4 ? 'castle' : m.length === 3 ? 'ep' : '';
    keys.push(`${f[0]},${f[1]},${f[2]},${f[3]}>${t[0]},${t[1]},${t[2]},${t[3]}|${promo}|${kind}`);
  }
  return keys;
}
function diffSets(a, b) {
  const sa = new Set(a), sb = new Set(b);
  return { onlyA: [...sa].filter((x) => !sb.has(x)), onlyB: [...sb].filter((x) => !fmtDup(x, sb)) };
}
function fmtDup() { return false; }

// ---------------------------------------------------------------------------------------------------------------
// Known, deliberate differences (see docs/RULES.md "Differences from 5d-chess-js"): filters applied to the two move
// sets before comparing them. They are only used for classes that were investigated; every filtered move is counted.
const tolerated = {};
function applyFilters(n, ctx, ours, ref) {
  const names = args.tolerate || [];
  const note = (name, key) => { tolerated[name] = (tolerated[name] || 0) + 1; };
  const oursOnly = ours.filter((k) => !ref.includes(k));
  const refOnly = ref.filter((k) => !ours.includes(k));
  const dropO = new Set(), dropR = new Set();
  for (const name of names) {
    const f = FILTERS[name];
    if (!f) throw new Error('unknown filter ' + name);
    for (const k of oursOnly) if (!dropO.has(k) && f.ours && f.ours(n, ctx, k)) { dropO.add(k); note(name, k); }
    for (const k of refOnly) if (!dropR.has(k) && f.ref && f.ref(n, ctx, k)) { dropR.add(k); note(name, k); }
  }
  return {
    ours: ours.filter((k) => !dropO.has(k)),
    ref: ref.filter((k) => !dropR.has(k)),
    dropOurs: dropO, dropRef: dropR,
  };
}
const FILTERS = {
  // En passant right after a fork: the engine looks at the previous board of the timeline (half-turn h-1), the
  // reference at t-2, which does not exist on the first boards of a forked timeline. See RULES.md.
  'ep-fork': {
    ours: (n, ctx, k) => {
      if (!k.endsWith('|ep')) return false;
      const [l, t] = k.split('|')[0].split('>')[0].split(',').map(Number);
      const tl = ctx.g.rawBoard[l];
      return !(tl && tl[t - 2]);
    },
  },
};

// ---------------------------------------------------------------------------------------------------------------
function tipsOf(g) {
  const out = {};
  g.rawBoard.forEach((tl, l) => { if (tl) out[l] = [tl.length - 1, tl[tl.length - 1]]; });
  return out;
}

function runGame(mode, seed) {
  return new Promise((resolve) => {
    const child = spawn(args.bin, ['--mode', mode, '--seed', String(seed), '--turns', String(args.turns), '--search-budget', String(args.searchBudget)], { stdio: ['ignore', 'pipe', 'inherit'] });
    const rl = readline.createInterface({ input: child.stdout });
    let g = null, init = null, n = 0;
    const history = []; // played moves (engine terms), for reproduction
    let result = { mode, seed, states: 0, turns: 0, divergence: null, stats: { moves: 0, ep: 0, castle: 0, promo: 0, timelineMoves: 0, checks: 0, mateCompared: 0, mateTimeout: 0, newTimelines: 0 } };
    let pending = null; // the ref move chosen for the previous 'played'
    let done = false;
    let queue = Promise.resolve();
    // Hard wall-clock cap per game: a stuck game is recorded as a timeout instead of blocking the batch.
    const timer = setTimeout(() => finish({ kind: 'timeout', detail: `game exceeded ${args.gameTimeout} s` }), args.gameTimeout * 1000);
    const finish = (div) => {
      if (done) return;
      done = true;
      clearTimeout(timer);
      result.divergence = div;
      child.kill();
      rl.close();
      resolve(result);
    };
    const handle = (line) => {
      if (done) return;
      let rec;
      try { rec = JSON.parse(line); } catch (e) { return finish({ kind: 'bad-json', detail: line.slice(0, 200) }); }
      if (rec.t === 'init') {
        init = rec; n = rec.n;
        if (['invasion', 'battle', 'fragment'].includes(mode)) {
          return finish({ kind: 'unsupported', detail: 'several original timelines do not map to the reference (even/odd timeline indexing)' });
        }
        g = buildReference(rec);
        return;
      }
      if (rec.t === 'end') { return finish(null); }
      if (rec.t !== 'state') return;
      result.states++;
      if (rec.i === 0) result.turns++;
      const color = rec.color;
      const ctx = { rec, g, n, history };
      const fail = (kind, extra) => finish(Object.assign({ kind, turn: rec.turn, i: rec.i, color, mode, seed,
        history: history.map((h) => h.slice()), boards: {} }, extra));

      // 1. boards ------------------------------------------------------------------------------------------------
      const refTips = tipsOf(g);
      const ourL = rec.tips.map((t) => tlToIdx(t[0])).sort((a, b) => a - b);
      const refL = Object.keys(refTips).map(Number).sort((a, b) => a - b);
      if (JSON.stringify(ourL) !== JSON.stringify(refL)) {
        return fail('timeline-set', { ours: ourL.map(idxToTl), ref: refL.map(idxToTl) });
      }
      for (const [tl, h, cells] of rec.tips) {
        const l = tlToIdx(tl);
        const [rt, rboard] = refTips[l];
        if (rt !== h) return fail('tip-time', { timeline: tl, ours: h, ref: rt });
        for (let r = 0; r < n; r++) for (let f = 0; f < n; f++) {
          const o = normalize(init, cells[r * n + (n - 1 - f)]);
          if (o !== rboard[r][f]) {
            const oRows = []; for (let rr = 0; rr < n; rr++) { oRows.push([]); for (let ff = 0; ff < n; ff++) oRows[rr].push(normalize(init, cells[rr * n + (n - 1 - ff)])); }
            return fail('board-mismatch', { timeline: tl, half: h, square: sq(n, r, f), ours: o, ref: rboard[r][f],
              oursBoard: '\n' + fmtBoard(n, oRows), refBoard: '\n' + fmtBoard(n, rboard) });
          }
        }
      }

      // 2. active timelines / present --------------------------------------------------------------------------------
      const refActive = boardFuncs.active(g.rawBoard).map(idxToTl).sort((a, b) => a - b);
      const ourActive = rec.active.slice().sort((a, b) => a - b);
      if (JSON.stringify(refActive) !== JSON.stringify(ourActive)) return fail('active-timelines', { ours: ourActive, ref: refActive });
      const refMand = boardFuncs.present(g.rawBoard, g.rawAction).map(idxToTl).sort((a, b) => a - b);
      const ourMand = rec.mand.slice().sort((a, b) => a - b);
      if (JSON.stringify(refMand) !== JSON.stringify(ourMand)) return fail('present-boards', { ours: ourMand, ref: refMand });

      // 3. moves -------------------------------------------------------------------------------------------------
      const refMoves = boardFuncs.moves(g.rawBoard, g.rawAction, false, false, false, g.rawPromotionPieces);
      const rk = refKeys(refMoves);
      const ok = oursKeys(n, color, rec.moves);
      const filtered = applyFilters(n, ctx, ok, rk);
      const refSet = new Set(filtered.ref), oursSet = new Set(filtered.ours);
      const onlyOurs = [...oursSet].filter((k) => !refSet.has(k));
      const onlyRef = [...refSet].filter((k) => !oursSet.has(k));
      result.stats.moves += ok.length;
      for (const k of ok) {
        if (k.endsWith('|ep')) result.stats.ep++;
        else if (k.endsWith('|castle')) result.stats.castle++;
        else if (!k.includes('|0|')) result.stats.promo++;
        const [a] = k.split('|'); const [f, t] = a.split('>').map((s) => s.split(',').map(Number));
        if (f[0] !== t[0] || f[1] !== t[1]) result.stats.timelineMoves++;
      }
      if (onlyOurs.length || onlyRef.length) {
        const involved = new Set();
        for (const k of [...onlyOurs, ...onlyRef]) { const [a] = k.split('|'); for (const p of a.split('>')) { const q = p.split(',').map(Number); involved.add(q[0] + ':' + q[1]); } }
        const boards = {};
        for (const key of involved) {
          const [l, t] = key.split(':').map(Number);
          if (g.rawBoard[l] && g.rawBoard[l][t]) boards[`${fmtTl(l)} t${t}`] = '\n' + fmtBoard(n, g.rawBoard[l][t]);
        }
        return fail('move-set', {
          onlyInEngine: onlyOurs.slice(0, 12).map((k) => fmtMoveKey(n, k)), onlyInReference: onlyRef.slice(0, 12).map((k) => fmtMoveKey(n, k)),
          onlyInEngineCount: onlyOurs.length, onlyInReferenceCount: onlyRef.length,
          rawOnlyInEngine: onlyOurs.slice(0, 12), rawOnlyInReference: onlyRef.slice(0, 12), boards,
        });
      }

      // 4. check and submittability ----------------------------------------------------------------------------------
      const refCheck = !!mateFuncs.checks(g.rawBoard, g.rawAction, true);
      if (refCheck !== !!rec.inCheck) return fail('in-check', { ours: !!rec.inCheck, ref: refCheck });
      if (refCheck) result.stats.checks++;
      const refSubmit = g.submittable();
      if (refSubmit !== !!rec.canSubmit) return fail('submittable', { ours: !!rec.canSubmit, ref: refSubmit });

      // 5. checkmate / stalemate at turn starts ------------------------------------------------------------------------
      // The reference's checkmate / stalemate search has no way to stop inside a single huge position, so it is only asked
      // about positions with few timelines (--mate-max-timelines).
      if (rec.i === 0 && rec.legal && Object.keys(refTips).length <= args.mateMaxTimelines) {
        const last = g.rawBoardHistory[g.rawBoardHistory.length - 1];
        const check = refCheck;
        let refRes;
        if (check) refRes = mateFuncs.checkmate(last, g.rawAction, args.mateTimeout);
        else refRes = mateFuncs.stalemate(last, g.rawAction, args.mateTimeout);
        if (refRes[1]) result.stats.mateTimeout++;
        else if (rec.legal !== 'unknown') {
          result.stats.mateCompared++;
          const oursNone = rec.legal === 'none';
          if (oursNone !== !!refRes[0]) {
            return fail('mate', { ours: rec.legal, inCheck: check, refSaysNoLegalTurn: !!refRes[0], boards: Object.fromEntries(Object.entries(g.rawBoard).filter(([, tl]) => tl).map(([l, tl]) => [`${fmtTl(+l)} t${tl.length - 1}`, '\n' + fmtBoard(n, tl[tl.length - 1])])) });
          }
        }
      }

      // 6. replay the move the player made --------------------------------------------------------------------------------
      if (rec.played) {
        const pm = rec.played[0];
        const base = oursKeys(n, color, [pm])[0].split('|');
        const want = pm[8] === 1 ? `${base[0]}|${rec.played[1]}|` : oursKeys(n, color, [pm])[0];
        const idx = rk.indexOf(want);
        if (idx < 0 && want.endsWith('|ep') && filtered.dropOurs.has(want)) {
          // The engine played an en passant capture that the reference does not offer (documented difference): the games
          // part here, so stop comparing this game (everything before it agreed).
          result.stats.epForkPlayed = (result.stats.epForkPlayed || 0) + 1;
          return finish(null);
        }
        if (idx < 0) return fail('played-move-missing', { move: fmtMoveKey(n, want) });
        const before = Object.keys(g.rawBoard).length;
        g.rawMoveBuffer.push(refMoves[idx]);
        boardFuncs.move(g.rawBoard, refMoves[idx]);
        if (Object.keys(g.rawBoard).length > before) result.stats.newTimelines++;
        history.push(pm.concat([rec.played[1]]));
      }
      if (rec.submit) {
        if (!g.submittable()) return fail('submit-refused', {});
        g.submit();
      }
    };
    rl.on('line', (line) => { queue = queue.then(() => handle(line)); });
    rl.on('close', () => { queue = queue.then(() => finish(null)); });
  });
}

// ---------------------------------------------------------------------------------------------------------------
function describe(d) {
  const lines = [`  ${d.kind}${d.turn !== undefined ? ` at turn ${d.turn} state ${d.i} (${d.color ? 'Black' : 'White'} to move)` : ''}`];
  const skip = new Set(['kind', 'turn', 'i', 'color', 'mode', 'seed', 'history', 'boards', 'rawOnlyInEngine', 'rawOnlyInReference']);
  for (const [k, v] of Object.entries(d)) if (!skip.has(k)) lines.push(`    ${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
  if (d.boards) for (const [k, v] of Object.entries(d.boards)) lines.push(`    board ${k}:${v.split('\n').map((s) => '      ' + s).join('\n')}`);
  if (d.history) lines.push(`    moves so far (engine terms: tl,h,x,y -> tl,h,x,y,tag,promo): ${JSON.stringify(d.history)}`);
  return lines.join('\n');
}

async function main() {
  if (args.worker) {
    const jobs = JSON.parse(process.env.REFCHECK_JOBS);
    const results = [];
    for (const [mode, seed] of jobs) results.push(await runGame(mode, seed));
    process.send({ results, tolerated });
    return;
  }
  const jobs = [];
  const per = Math.ceil(args.games / args.modes.length);
  for (const mode of args.modes) for (let i = 0; i < per; i++) jobs.push([mode, args.seed0 + i]);
  let results = [];
  if (args.jobs > 1) {
    const chunks = Array.from({ length: args.jobs }, () => []);
    jobs.forEach((j, i) => chunks[i % args.jobs].push(j));
    const forwarded = ['--ref', args.ref, '--bin', args.bin, '--turns', String(args.turns), '--mate-timeout', String(args.mateTimeout), '--search-budget', String(args.searchBudget),
      '--mate-max-timelines', String(args.mateMaxTimelines), '--game-timeout', String(args.gameTimeout)]
      .concat(args.tolerate ? ['--tolerate', args.tolerate.join(',')] : []);
    await Promise.all(chunks.filter((c) => c.length).map((chunk) => new Promise((res) => {
      const w = fork(__filename, [...forwarded, '--worker'], { env: Object.assign({}, process.env, { REFCHECK_JOBS: JSON.stringify(chunk) }), cwd: args.ref });
      w.on('message', (r) => {
        results = results.concat(r.results);
        for (const [k, v] of Object.entries(r.tolerated)) tolerated[k] = (tolerated[k] || 0) + v;
      });
      w.on('exit', () => res());
    })));
  } else {
    for (const [mode, seed] of jobs) results.push(await runGame(mode, seed));
  }

  // Summary ------------------------------------------------------------------------------------------------------------
  const byMode = {};
  for (const r of results) {
    const m = (byMode[r.mode] = byMode[r.mode] || { games: 0, clean: 0, turns: 0, states: 0, unsupported: 0, kinds: {}, stats: {}, examples: {} });
    m.games++;
    m.turns += r.turns; m.states += r.states;
    for (const [k, v] of Object.entries(r.stats)) m.stats[k] = (m.stats[k] || 0) + v;
    if (!r.divergence) m.clean++;
    else if (r.divergence.kind === 'unsupported') m.unsupported++;
    else { m.kinds[r.divergence.kind] = (m.kinds[r.divergence.kind] || 0) + 1; (m.examples[r.divergence.kind] = m.examples[r.divergence.kind] || []).push(r); }
  }
  for (const [mode, m] of Object.entries(byMode)) {
    console.log(`\n== ${mode}: ${m.games} games, ${m.turns} turn starts, ${m.states} positions compared, ${m.clean} games without divergence` +
                (m.unsupported ? `, ${m.unsupported} unsupported` : ''));
    console.log('   coverage: ' + JSON.stringify(m.stats));
    for (const [kind, c] of Object.entries(m.kinds)) console.log(`   divergences "${kind}": ${c} game(s)`);
    const shown = args.verbose ? Infinity : 3;
    for (const [kind, list] of Object.entries(m.examples)) {
      for (const r of list.slice(0, shown)) console.log(`  [${mode} seed ${r.seed}]\n${describe(r.divergence)}`);
    }
  }
  if (Object.keys(tolerated).length) console.log('\ntolerated (filtered) moves: ' + JSON.stringify(tolerated));
  const anyDiv = results.some((r) => r.divergence && r.divergence.kind !== 'unsupported');
  process.exit(anyDiv ? 1 : 0);
}
main();
