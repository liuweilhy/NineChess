# NineChess

**English** | [简体中文](./Readme_cn.md)

## Language
The program supports Simplified Chinese, Traditional Chinese, English, Japanese, Korean, German, French, Russian, Spanish and Portuguese; it follows your system locale on first launch and can be switched at any time under "Options → Settings → Language".

## Project
`NineChess` is a Nine Men's Morris family game written in Qt. This repository contains both the GUI application `NineChess` and the command-line test harness `NineChessConsole`.
The core code is now organised as "pure game model + controller-level match management + bitboard Alpha-Beta AI", which makes it easier to extend rules, debug the AI and run regression tests.
The AI engine supports multi-threaded search (Lazy SMP), time budgets with mid-search interruption, randomised root selection and quiet search, and ships with a per-rule statistical opening book. Every AI behaviour can be reproduced and verified from `NineChessConsole`.

## Supported Rules

### Nine Men's Morris
![Nine Men's Morris](./screenshot/莫里斯九子棋.PNG "Optional title")

Nine Men's Morris is one of the most classic variants of the game:

- The board has 24 points; each side has 9 pieces, placed alternately at the start.
- Forming a "three-in-a-row" lets you remove one of the opponent's pieces.
- In the middle game pieces normally move only along a line to an adjacent point.
- A side reduced to 3 pieces may "fly" a piece to any empty point.
- A side loses when it has fewer than 3 pieces, or when it can no longer satisfy the rules.

### Cheng San Qi

- Close to Nine Men's Morris, but a side with 3 pieces left may not fly.
- In the moving stage, being blocked (unable to move) loses the game.

### Da San Qi (12 pieces)
![12 pieces](./screenshot/12子棋.PNG "Optional title")

This variant uses a board with diagonal lines; each side has 12 pieces:

1. A point where a piece was removed during placement stays forbidden until the moving stage.
2. If the board is completely filled during placement, the first player loses.
3. After placement, the player who placed second moves first.
4. Several "three-in-a-rows" formed by one move allow several consecutive removals.
5. The remaining basic rules are close to Cheng San Qi.

### NineChess
![NineChess](./screenshot/九连棋.PNG "Optional title")

NineChess is the default rule set of this project; its distinguishing feature is that the pieces are numbered:

1. The basic flow is close to Cheng San Qi.
2. A "three-in-a-row" formed by the same numbers at the same positions cannot remove a piece again.
3. If a side is blocked in the moving stage, the opponent simply continues instead of winning immediately.
4. As many valid "three-in-a-rows" as one move creates, that many pieces may be removed.

## Current Status

### GUI
![GUI](./screenshot/GUI.PNG "Optional title")

The GUI project in this repository is back to a compilable state and keeps the original desktop look. The boundary between legacy code and the new structure is still being tidied up; the conventions that matter most today are:

- `NineChess` owns pure rules, position state, command parsing, position transforms and hashing only.
- `GameController` owns match logic such as time limits, step limits, timeout adjudication and forcing an AI move on timeout.
- External adjudication always goes through `adjudicateWin()` / `adjudicateDraw()`; `giveup()` means resignation only.
- Commands and text game records use `(c,p)`, `(c1,p1)->(c2,p2)`, `-(c,p)`, `-0`, `-1` and `==`.

### Game Record Format

A game record is a plain command stream (UTF-8 without BOM). The first line is the game-setup command `r<rule>s<steps>t<minutes>` (for example `r2s100t10`; segments are optional and may appear in any order, and `0` means unlimited), followed by one move command per line:

```text
r2s100t10
(1,0)
(0,0)
...
-1        ← appended only on a timeout loss (second player loses; -0 if the first player loses)
```

- Opening a game record switches the rule and restores the step/time limits according to the setup command; the setup command is recognised by the controller and the console, while the model itself is unaware of it.
- Draws (adjudicated or by step limit) and resignations are already stored as `==`, `-0` and `-1` commands, so replay restores them naturally; a timeout loss has no command record, so the loser's `-0`/`-1` is appended on save.
- Older records without a setup line (pure move commands) stay compatible and are replayed under the current rule.
- The setup command is parsed by `parseSetupCommand()` (`ninechess.h`), shared by the controller and the console.

### Main Features

1. Four built-in rule sets: Cheng San Qi, Da San Qi, NineChess and Nine Men's Morris.
2. Graphical board, piece animation, sound effects and status-bar hints.
3. Game record display, command replay and history browsing; records are plain command streams whose first line stores the rule and the limits and are restored automatically on open, with externally adjudicated endings (such as a timeout loss) appended as result commands.
4. Position mirroring, flipping, discrete-angle rotation and colour swapping.
5. Human-versus-AI play and the basics of AI-versus-AI self-play.
6. A separate command-line test program for debugging rules, commands and position text output.
7. Bitboard Alpha-Beta search engine: iterative deepening, bucketed transposition table (16-byte slots × 1M entries), TT move/killer/history ordering, quiet search, aspiration windows, repetition penalties (including the root position and the real game history), point-structure evaluation, PVS zero-width probes, forced-capture extension and LMR late-move reductions.
8. Lazy SMP multi-threaded search (the thread count is derived from the CPU core count by default) with time budgets and mid-search interruption; dynamic depth (2 extra plies for middle-game root positions).
9. Randomised root selection (exact scores + score-gap threshold + weighted randomness, reproducible with a seed).
10. Statistical opening book trained by self-play: independent per rule, compressed with the 16 symmetries, with scores accumulated over multiple training rounds.
11. Step-limit urgency score: the controller passes the remaining-step hint (the model is unaware of it), which pushes the materially leading side to convert its advantage as the steps run out and eases drawish endings.

## AI Notes

`NineChess_AI_AB` has been rewritten around a bitboard representation. The engine's **complete algorithm logic and full parameter manual** (classification / type / default / range / scope / hardcoded constants) as well as the **sentinel-semantics warnings and A/B methodology** are in [AI_ALPHABETA.md](./AI_ALPHABETA.md); the repository-wide overview (model / state machine / rules / test system) is in [AI_SUMMARY.md](./AI_SUMMARY.md). Highlights:

- Alpha-Beta pruning + iterative deepening + quiet search, with PVS zero-width probes, aspiration windows, forced-capture extension, LMR late-move reductions, repetition penalties (including the root position and the real game history) and TT move/killer/history ordering.
- The transposition table is **private to each AI instance** (one table each for the first and second player, never shared, so the two A/B arms and the two self-play sides no longer exchange knowledge through the TT; changed in 2026-09 from "globally shared per rule" to instance-owned), with 256 shard locks (multi-threading inside one instance does not block itself) and a fixed-length bucket array of 1M entries (16-byte slots, 32-byte aligned cache lines); symmetric canonicalisation by default applies to "canonical at opening nodes only" (smallest key over the 16 views); the hard hash for numbered rules is maintained incrementally and verified node by node by the `selfcheck` command.
- Lazy SMP multi-threading (the thread count is derived from the CPU core count by default) with time budgets and mid-search interruption; dynamic depth (2 extra plies for middle-game root positions).
- Default GUI AI configuration: search depth 10, 5 seconds per move (the UI allows depth 1~20 and 1~60 seconds).
- Evaluation: per-rule `EvalWeights` tables plus overridable terms such as point-structure value, near-win pressure and step-limit urgency; the `tune` command performs per-rule coordinate-descent tuning (with endgame validation against overfitting and a printable weight line).
- Randomised root selection (exact scores + score-gap threshold + weighted randomness, reproducible with a seed, by default only for the first 10 moves with gap 16) and a statistical opening book (independent per rule, compressed with the 16 symmetries); neither the opening book nor an endgame book is fully implemented, and both stay disabled for now because the current strength is sufficient (see [AI_SUMMARY.md](./AI_SUMMARY.md) §15).

### Measured Results (Release x64)

> **Debug versus Release**: every engine figure in this section and in the benchmark comes from Release x64 (`/O2`) builds.
> **Debug builds have no compiler optimisation and a much lower NPS (usually a fraction of Release), so the AI in a Debug GUI searches noticeably slower than the benchmark numbers suggest** — this is a difference in compiler optimisation, not in algorithms; use Release builds when evaluating AI strength or running comparisons.

- Hash modes: empty opening at depth 7, canonical 35,356 nodes / 24 ms versus 150,854 nodes / 55 ms with plain hashing; a real middle-game position at depth 6 runs at about 0.49M NPS in fully symmetric mode versus 1.30M NPS plain (about 4× the per-node cost with identical node counts).
- Multi-threading (16 logical cores): about 1.7M NPS single-threaded on an empty board at depth 10 after the snapshot/hash optimisations; about 7.8M NPS with 14 threads (roughly 12×); an 8-second budget completes depth 11.
- Snapshot/hash optimisations: for the numbered rule (NineChess) the empty board at depth 10 drops from 5.5 s to 3.1 s (2.2 s after PVS), and for ordinary rules from 1.05 s to 0.74 s; node counts, best moves and scores are identical to before the optimisation.
- Dynamic depth: a fixed depth of 10 in the middle game becomes an actual depth of 12, 35 ms → 195 ms, a significant rise in effective strength.
- Opening book (rule 2, trained on 500 games): the empty-board entry has 530 samples and the first move `(2,0)` wins 81%; in a depth-difference 5v7 match over 60 games the first player's win rate rose from 13 to 18 when using the book (180 book moves with zero rejections).
- Observation: under the current evaluation the first-player advantage is obvious (in depth 5~8 self-play the second player almost never wins), most games end in a draw at the step limit, and the engine is conservative with slow endgame conversion — tuning "aggressiveness" in the evaluation is the next focus; `vs` already supports alternating the first move between odd/even games and one-sided parameters (`wp`/`sp`/`ft`/`ms`/`tp`/`sr`) for that A/B work.
- Empirical limits of the near-win pressure term (`wp`): 60 self-play games gave 8:2 against the side with it disabled (proving self-consistency), but 80 games per arm against the outside with wp=150 versus wp=0 differ by only 3 wins, below the ±5-win re-run noise of the same configuration (re-measured 2026-09, see `benchmark/results/evalab_*/CORRECTION.md`) — the external strength effect is unproven, and A/B of evaluation terms must establish a noise baseline before drawing conclusions.

## Project Layout

The project roughly follows MVC:

### Model

- `NineChess/src/ninechess_common.h`
  Core shared constants plus the `Rule`, `ChessData` and bitboard state definitions.
- `NineChess/src/ninechess.h/.cpp`
  The pure game model, owning rules, positions, commands, transforms, hashing and win/loss decisions.
- `NineChess/src/ninechess_ai_ab.h/.cpp`
  The Alpha-Beta AI.
- `NineChess/src/ninechess_symmetry.h/.cpp`
  Shared symmetry utilities for the 16 equivalent views (used by both the AI transposition table and the opening book).
- `NineChess/src/ninechess_book.h/.cpp`
  The statistical opening book (independent per rule, symmetrically compressed, scored by self-play).

### View

- `NineChess/src/ninechesswindow.*`
  The main window.
- `NineChess/src/gamescene.*`
  The game scene.
- `NineChess/src/gameview.*`
  The game view.
- `NineChess/src/boarditem.*`
  The board graphics item.
- `NineChess/src/pieceitem.*`
  The piece graphics item.

### Controller

- `NineChess/src/gamecontroller.*`
  Manages the game flow, time/step limits, UI synchronisation and AI dispatching.
- `NineChess/src/aithread.*`
  The AI thread wrapper.

### Console Test

- `NineChessConsole/ninechessconsole.cpp`
  Reuses the core model directly; good for rule verification, command-line moves and regression testing.

### Resources, Version And Localisation

- `NineChess/src/ninechess_version.h`
  The single source of the version number: change its four numbers and the executable version resource, the window title and the About dialog all follow.
- `NineChess/translations/ninechess_*.ts` / `*.qm`
  UI translation sources and their compiled catalogues, packed into the executable under `/i18n` (see "Language" above).

### AI Benchmark Tools

- `benchmark/benchmark*.cpp` + `benchmark/build_*.bat`
  Same-process match drivers for the 2018, 2026-05 and current engines, plus a self-play driver, used for AI strength comparison; see [benchmark/README.md](./benchmark/README.md) and the reports under `benchmark/results/`.
- `benchmark/may_engine/`, `benchmark/old_engine/`
  Frozen copies of the earlier engines, referenced only as match opponents.

## Build Notes

### Windows / Visual Studio

- Solution file: `ninechess.sln`
- GUI project: `NineChess`
- Console project: `NineChessConsole`
- The current GUI project configuration is verified to build with `Qt 5.15.2 (msvc2019_64) + MSVC v143` (VS2022 toolset, since 2026-09-14; previously v142).

### qmake

- The GUI project also keeps `NineChess/ninechess.pro`.
- `/utf-8` is added explicitly for MSVC so that UTF-8 sources without BOM are not misread as the local code page.
- The project asks for `c++17`; note that qmake only recognises the lowercase `c++NN` spelling.

### Linux / Debian

- The qmake project targets Qt 5 (the Windows build uses Qt 5.15.2), and Debian 11/12/13 ship Qt 5.15, so the distribution packages are enough. Qt 6 does not compile: three Qt 5-only APIs are used (`QDesktopWidget` / `qApp->desktop()`, `QString::SkipEmptyParts`, `QTextStream::setCodec`).
- Dependencies:
  `sudo apt install build-essential qtbase5-dev qtbase5-dev-tools qttools5-dev-tools qtmultimedia5-dev libqt5multimedia5-plugins qttranslations5-l10n fonts-noto-cjk`
  `libqt5multimedia5-plugins` is the backend that actually plays the `QSoundEffect` samples, `qttranslations5-l10n` supplies the `qtbase_<code>.qm` catalogues for Qt's own widgets, and `fonts-noto-cjk` covers the Chinese/Japanese/Korean UI text.
- Build: `cd NineChess && qmake && make -j`
- Interface font: `Microsoft YaHei` on Windows, extended with cross-platform fallback families (`Noto Sans CJK`, `WenQuanYi`, `Malgun Gothic`, `PingFang SC`, ...) — see `uiFontFamilies()` in `NineChess/src/ninechesswindow.cpp`. With no CJK font installed the UI ends up on `sans-serif` and may show empty boxes.
- `NineChessConsole` and the benchmark drivers are built on Linux by `bash benchmark/linux/build.sh` (g++, with the `windows.h` / `direct.h` shims under `benchmark/linux/`).
- `tests/*.ps1` are Windows-only PowerShell scripts.

## Encoding And Text Format

The repository has settled on the following conventions:

- Source, Markdown and project text files use `UTF-8 without BOM`.
- `CRLF` line endings are used on Windows.
- `.editorconfig`, `.gitattributes` and `AGENTS.md` together enforce encoding and line endings.
- GUI, console and core sources must all stay compatible with the `/utf-8` compiler option.

## Command-Line Debugging

`NineChessConsole` is the quick way to verify rules, commands and AI behaviour; the core conventions are:

- All coordinates are 0-based.
- `rule N` switches the rule, with `N` in the range `0..3`.
- `history` shows the command history, `undo` steps back one move and `new` starts a new game.
- The rule number can be given directly at startup, for example:

```text
NineChessConsole.exe 2
NineChessConsole.exe --rule 2
```

AI debugging commands (omitted arguments fall back to their defaults; `help` documents them fully):

- `search [d] [t] [h] [r] [g] [th] [s] [pv] [a] [rep] [q] [dd] [pvs] [ext] [lmr]`
  Single-position search, printing the best move, score, depth, elapsed time, nodes, NPS, TT statistics and root move scores.
- `match [n] [d] [h] [r] [th] [s] [mp] [dd]`
  Self-play of n games with identical settings, reporting wins / moves / nodes / NPS; `mp` passes the step limit into the evaluation urgency term automatically.
- `vs [n] [d1] [h1] [pv1] [d2] [h2] [pv2] [s] [a] [rep] [wp] [sp] [ft] [th] [ext] [lmr] [ms] [tp] [sr]`
  Matches between differently configured engines (depth difference, hash mode, point value and so on) for strength A/B; the first move alternates between odd and even games, and `wp`/`sp`/`ft`/`ext`/`lmr`/`ms`/`tp`/`sr` apply to engine 2 only (`ext`/`lmr`/`ms`/`tp` take -1 to keep the default).
- `booktrain [n] [d] [s]` trains the opening book by self-play; `bookstat` shows entries and win rates; `bookon` / `bookoff` enable or disable the book (effective for `match` / `vs`); `booksave` / `bookload` store and load it.
- `selfcheck [n] [d]` runs random self-play over the 4 rules and verifies the incremental hash node by node (a health check after engine changes).
- `tune [n] [d] [s] [r]` tunes parameters automatically for the current rule (coordinate descent + endgame validation, printing a weight line you can paste back).

Parameter shorthand: `h` hash mode (0 plain / 1 canonical at opening nodes only / 2 canonical everywhere), `r` randomised root, `g` random score gap, `th` thread count (0 = derive from the CPU core count), `s` seed, `pv` point value weight, `a` aspiration window, `rep` repetition penalty, `q` quiet search, `dd` dynamic depth, `pvs` PVS probes, `ext` forced-capture extension (on by default), `lmr` LMR reductions (on by default), `ms` closed-mill protection, `tp` phase interpolation (off by default), `sr` remaining-step hint.

The full semantics of every parameter, the differences between command defaults and engine defaults, and the disable/sentinel rules (`wp`/`pv` have no -1 sentinel, while `sp`/`ft`/`ms` and others treat <0 as "keep the default") are in [AI_ALPHABETA.md](./AI_ALPHABETA.md) §3/§4.

## Tests And Regression

The repository provides two layers of automated tests:

- `tests/RuleHarness.vcxproj` + `tests/rule_harness.cpp`
  Reuses the `NineChess` core for rule-level white-box tests, covering legality checks, capture logic, blocked-loss, three-in-a-row history and the flying rule.
- `tests/Run-ConsoleBlackBoxTests.ps1`
  A black-box test that replays command-line input and checks `NineChessConsole` output, covering the whole chain of `rules` / `rule` / `history` / `undo` / `-0` / `==` and startup arguments.

The test entry points available today are:

- `tests/Test-Rule0-ChengSanQi.ps1`
  Tests rule 0 (Cheng San Qi) on its own.
- `tests/Test-Rule1-DaSanQi.ps1`
  Tests rule 1 (Da San Qi / 12-piece) on its own.
- `tests/Test-Rule2-JiuLianQi.ps1`
  Tests rule 2 (NineChess) on its own.
- `tests/Test-Rule3-Morris.ps1`
  Tests rule 3 (Nine Men's Morris) on its own.
- `tests/Run-All-RuleTests.ps1`
  Runs the 4 rule white-box tests in sequence and prints a summary.
- `tests/Run-ConsoleBlackBoxTests.ps1`
  Runs the console black-box replay tests and prints a summary.
- `tests/Run-All-RegressionTests.ps1`
  Runs both layers (rule white-box + console black-box) in one go; this is the recommended main entry point.
- `tests/Run-MatchBattery.ps1`
  Long-running match verification: batch-runs several `vs` configurations (book on/off, depth difference, point value and so on) and prints a comparison table; `-Games N` controls the number of games per group.

In Windows PowerShell the full regression can be run directly:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\Run-All-RegressionTests.ps1"
```

To run only the rule tests:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\Run-All-RuleTests.ps1"
```

To run only the console black-box tests:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\Run-ConsoleBlackBoxTests.ps1"
```

These tests currently focus on:

- Opening, middle game, captures and win/loss decisions under all 4 rule sets;
- Illegal moves never pollute the position or the command history;
- Da San Qi forbidden-point reuse and multiple removals from double three-in-a-rows;
- NineChess numbered three-in-a-row history and the continue-after-blocked rule;
- The 3-piece flying rule in Nine Men's Morris;
- `NineChessConsole` rule switching, history, undo, resignation, draw adjudication and startup arguments;
- AI match verification: batch `vs` matches via `Run-MatchBattery.ps1` (book on/off, depth difference, evaluation configuration), plus `match` / `vs` self-play and configuration comparison.

## History, Licence And Author

- Update history: [History.txt](./History.txt)
- Licence: [Licence.txt](./Licence.txt)
- Original author: `liuweilhy`
- Contact: `liuweilhy@163.com`

The earliest core model code dates back to 2013 and the Qt GUI took shape over the following years; this repository keeps the original direction while continuing to tidy up the rule layer, the controller layer and the AI.

## Links And Downloads

- Source (Gitee): [https://gitee.com/liuweilhy/NineChess](https://gitee.com/liuweilhy/NineChess)
- Releases (Gitee): [https://gitee.com/liuweilhy/NineChess/releases](https://gitee.com/liuweilhy/NineChess/releases)
- CSDN resource page: [https://download.csdn.net/download/liuweilhy/10871298](https://download.csdn.net/download/liuweilhy/10871298)
- Baidu Netdisk: [https://pan.baidu.com/s/1NZnmAUozbPt9K04fTouxMA](https://pan.baidu.com/s/1NZnmAUozbPt9K04fTouxMA)

## Donate
![GUI](./screenshot/donate.png "donate")
