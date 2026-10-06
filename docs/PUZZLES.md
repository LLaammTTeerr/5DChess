# Puzzles

Main menu -> **Puzzles**. Every puzzle is a position where White is to move and has a forced checkmate in one or two turns.
The puzzles are original positions (see `assets/CREDITS.md`), each one **proved by the engine**: `tools/puzzle_check` searches
every legal turn, not the AI's beam, and a ctest runs it over the whole set.

## Playing

* The list has one column per tier with each puzzle's title, its goal (Mate in 1 / Mate in 2) and a check mark once solved.
  Solved puzzles are remembered (below).
* A puzzle opens on the normal multiverse board (the Guide's embedded board). The panel shows the goal ("White to move - mate in 1"),
  and three buttons:
  * **Hint** shows the puzzle's hint text; **Show piece** (the same button, second press) rings the piece to move.
  * **Reset** puts the position back (**Try again** after a wrong answer).
  * **Show solution** plays the stored line through the board animations. It does not count as solving the puzzle.
* Make a turn and press Submit as in a game. **Any turn that checkmates is accepted**, not only the stored solution (the result of the
  submitted turn is the proof). Anything else shows "Not quite - try again" and the puzzle resets itself after a moment.
* **Mate in 2:** after your first turn the engine first checks that *every* defence of Black loses to a mate in one (a first turn that
  does not win by force is "Not quite": Black's refuting defence is played on the board first, so you see why). Then `ai::Search` at Hard
  (node cap 20 000) picks Black's reply, which is played on the board, and you play the mating turn. The proof is spread over frames
  (a few milliseconds each, with a "Checking your idea... n%" status), so the web build keeps running smoothly.
* **Show solution** after Black's reply plays a mating turn of the position on the board (not the stored reply).
* Solving shows a card with **Next puzzle** / **Back to list** and a short flourish that Reduce motion turns off. Puzzle games never touch
  the autosave.

Tiers: **Warm-up** (mates in 1, mostly one board), **Time travel** (mates in 1 that need a move to another board, 2-3 timelines),
**Deep** (mates in 2, and "branching" mates whose mating turn creates a timeline).

## File format

A puzzle is a [position file](POSITIONS.md) with four more header lines (before the first board line). `title:` is the normal
position title. Files live in `assets/puzzles/` and are named `t<tier>-<nn>-<name>.5dp`; the file name without `.5dp` is the
puzzle's id (progress is stored by id, so never rename a published puzzle). The list is ordered by tier, then file name.

```
5dchess-position 1
title: Back rank
size: 8
rules: double-step
to-move: white
goal: mate-in-1
difficulty: 1
hint: Black's king is boxed in by its own pawns. Which of your pieces can reach the back rank?
solution: (L0T1)e1>(L0T1)e8
L0 T1w: 6k1/5ppp/8/8/8/8/5PPP/4R1K1
```

| Line | Meaning |
| --- | --- |
| `goal: mate-in-1` or `mate-in-2` | what the player has to do |
| `difficulty: 1..3` | the tier: 1 Warm-up, 2 Time travel, 3 Deep |
| `hint: ...` | up to 200 characters of ASCII; says what to look for, not the move |
| `solution: ...` | the stored line in [move notation](NOTATION.md). A mate in 1: one turn. A mate in 2: `<White turn> / <Black reply> / <White turn>`. The moves of one turn are separated by spaces |

`to-move` must be `white`. The puzzle lines are read by `puzzles::parse` (`include/puzzles/Puzzle.h`) and handed to the normal
position parser without them; the engine's `parsePosition` itself does not know them. Fonts hold ASCII only, so keep titles and hints
to plain ASCII.

A board is a plain snapshot: the earlier boards of a timeline are there for the player to look at and to jump to (and the king
standing on them counts, see below), they need not come from a real game, but real games make the best puzzles.

## Adding a puzzle

1. Write the position (the puzzle lines may be left out at first).
2. Ask the engine for the solutions:
   * `puzzle_check --mates my.5dp` lists every mating turn of the position (mate in 1);
   * `puzzle_check --mate2 my.5dp` lists every first turn that wins by force in two, with the number of Black's defences and a complete
     `solution:` line to paste.
3. Aim for few mating turns (one is ideal; extra moves on optional boards multiply them) and, for a mate in 2, at least two defences.
4. Add `goal`, `difficulty`, `hint` and `solution`, save as `assets/puzzles/t<tier>-<nn>-<name>.5dp`, and run the validator.
5. Add a screenshot only if the puzzle is used by a UI script (tests/ui/README.md).

No code change is needed: the files are found by directory, listed in `assets/manifest.txt` as a whole directory (so the web build
embeds new ones) and the CMake web target relinks when one changes.

## Validating

```
cmake -S . -B build -DFDCHESS_BUILD_TOOLS=ON          # or -DFDCHESS_BUILD_TESTS=ON: the tool needs no graphics
cmake --build build --target puzzle_check
build/tools/puzzle_check/puzzle_check assets/puzzles   # -v lists every winning first turn
ctest --test-dir build                                 # runs the same over assets/puzzles (test "puzzle_check"; not registered with
                                                       # -DFDCHESS_SANITIZE=ON, where it takes minutes: tests/puzzle_test.cpp proves a few instead)
```

`puzzles::validate` (`include/puzzles/Solver.h`) checks, exhaustively with the official-rules engine (`IGame`, the legal-turn proof of
[SEARCH.md](SEARCH.md), never the AI):

* the position loads, White is to move and has at least one legal turn (so it is not already mate or stalemate);
* **mate in 1:** the stored solution is a legal turn and checkmates; every legal turn is tried and the mating ones are counted;
* **mate in 2:** White has no mate in 1; for the stored first turn *every* legal reply of Black is tried and White has a mating turn
  after each; the stored reply is one of Black's legal turns and the stored last turn mates after it; every other first turn is
  tried the same way and the winning ones are counted;
* tier 2: at least two timelines, and every winning first turn contains a move to another board (a time jump or a move across
  timelines), so the puzzle cannot be solved on one board;
* title, hint and the format of the lines (`tests/puzzle_test.cpp` also checks the shipped set).

A turn is a set of moves, at most one from each board the mover can move on, so the enumeration (`puzzles::TurnEnumerator`, resumable;
`puzzles::forEachTurn` is the loop over it) is complete, not sampled, and a turn is identified by the position it leads to; it discards a partial turn as soon as a king of the mover can be captured (a capture stays possible whatever else is
played, SEARCH.md F2; the unit tests compare with the unpruned enumeration and with the full legal-turn proof). The time of a proof
grows quickly with the number of timelines and pieces, which is why the mates in 2 are single-timeline positions: keep puzzles small.

### Why a "past" king matters

The engine counts a king on an earlier board as capturable by a piece that can jump back to it (a king may also flee into the
past). So a position with history is not the textbook position of its last board: a check or a time jump can be decisive even where the
last board alone looks quiet. Warm-up puzzles therefore have a single board per timeline; the others rely on this on purpose and say so in
their hint. Let the validator, not intuition, decide.

## Progress

Solved puzzles are stored as `solved=<id>,<id>,...` in the settings-file format ([include/puzzles/Progress.h](../include/puzzles/Progress.h)):
`puzzles.txt` next to `settings.txt` on desktop (the same config directory, written to a temporary file and renamed), the browser's
localStorage key `5dchess.puzzles` on the web, memory only under the UI test harness. Unknown ids are kept out (ids are lower-case
letters, digits, `-` and `_`), a corrupt file simply means nothing solved, and a puzzle that is removed from the game stays harmlessly in
the file.

## The shipped set

| id | tier | goal | timelines | mating first turns |
| --- | --- | --- | --- | --- |
| `t1-01-back-rank` Back rank | Warm-up | mate in 1 | 1 | 1 |
| `t1-02-rolling-rooks` Rolling rooks | Warm-up | mate in 1 | 1 | 1 |
| `t1-03-smothered` Smothered | Warm-up | mate in 1 | 1 | 1 |
| `t1-04-battery` Battery | Warm-up | mate in 1 | 1 | 1 |
| `t1-05-promotion` Promotion | Warm-up | mate in 1 | 1 | 3 (queen, rook or knight) |
| `t2-01-across-timelines` Across the timelines | Time travel | mate in 1 | 2 | 1 |
| `t2-02-bishop-in-time` Bishop in time | Time travel | mate in 1 | 2 | 1 |
| `t2-03-two-turns-back` Two turns back | Time travel | mate in 1 | 2 | 1 |
| `t2-04-knight-crossing` Knight crossing | Time travel | mate in 1 | 3 | 1 |
| `t3-01-quiet-queen` Quiet knight | Deep | mate in 2 | 1 | 1 (6 defences) |
| `t3-02-centre-stage` Centre stage | Deep | mate in 2 | 1 | 1 (5 defences) |
| `t3-03-small-step` Small step | Deep | mate in 2 | 1 | 1 (5 defences) |
| `t3-04-branch-point` Branch point | Deep | mate in 1, branching | 3 | 2 (the mating move `(L0T4)c1>(L0T1)c4`, alone or with exactly one extra move, `(L-1T3)a3>d3`) |
| `t3-05-two-branches` Two fronts | Deep | mate in 1, branching | 4 | 1 |
