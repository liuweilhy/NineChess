# Repository Instructions

## Project Overview

- This repository contains two Visual Studio projects in `D:\My program\QT\NineChess\ninechess.sln`.
- `NineChess` is the Qt GUI application.
- `NineChessConsole` is the console harness for rule, command, and model regression testing.

## Architecture Boundaries

- `D:\My program\QT\NineChess\NineChess\src\ninechess.*` is the pure game core.
- `NineChess` owns rules, board state, command parsing, geometric transforms, hashing, and win/draw state.
- Do not put time limits or step limits back into `Rule`, `ChessData`, or other pure model structures.
- `D:\My program\QT\NineChess\NineChess\src\gamecontroller.*` owns match and session concerns: clocks, step limits, timeout adjudication, forced AI move on timeout, and UI coordination.
- External adjudication must go through `adjudicateWin()` or `adjudicateDraw()`.
- `giveup()` means resignation only.

## Rules And Commands

- Built-in rules are `0 ChengSanQi`, `1 DaSanQi (12-piece)`, `2 JiuLianQi` (default), and `3 Nine Men's Morris`.
- Core and console coordinates are 0-based: `c=0..2`, `p=0..7`.
- Command text conventions are `(c,p)`, `(c1,p1)->(c2,p2)`, `-(c,p)`, `-0`, `-1`, and `==`.

## AI Notes

- `D:\My program\QT\NineChess\NineChess\src\ninechess_ai_ab.*` is the Alpha-Beta AI.
- The current AI is bitboard-based and should prefer the fast no-validation APIs `chooseFast()`, `placeFast()`, and `captureFast()`.
- Symmetry-aware transposition lookup is intentional. Equivalent positions under inner/outer turn, mirror, vertical flip, and discrete rotation may share cached scores.
- When hashing `ACTION_PLACE` states, remember that `selectedPos` is part of the effective search state.

## Naming And Compatibility

- Prefer the new field names from `ninechess_common.h`.
- Do not reintroduce old union aliases or legacy duplicated names.
- Keep names such as `piecesPerSide`, `minPiecesToSurvive`, `hasDiagonalLines`, `hasForbiddenPoints`, and `allowFlying`.

## Encoding And Formatting

- Save source, headers, markdown, and project text files as UTF-8 without BOM.
- Use CRLF line endings for edited text files on Windows.
- Do not add BOM to files that are already UTF-8 without BOM.
- The project is configured to compile with `/utf-8`; keep edited source compatible with that setting.
- `.editorconfig` and `.gitattributes` are part of the contract and should stay aligned with the source tree.

## Build Notes

- The GUI project currently uses Qt 5.15.2 (`msvc2019_64`) with MSVC toolset `v142`.
- `D:\My program\QT\NineChess\NineChess\ninechess.pro` is kept in sync with the MSBuild project for source lists and `/utf-8`.
- For quick model sanity checks, prefer `NineChessConsole` before changing GUI behavior.
