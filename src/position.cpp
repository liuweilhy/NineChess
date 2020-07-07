﻿/*
  Sanmill, a mill game playing engine derived from NineChess 1.5
  Copyright (C) 2015-2018 liuweilhy (NineChess author)
  Copyright (C) 2019-2020 Calcitem <calcitem@outlook.com>

  Sanmill is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Sanmill is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <climits>

#include "search.h"
#include "movegen.h"
#include "option.h"
#include "zobrist.h"
#include "bitboard.h"
#include "prefetch.h"

string tips;

Position::Position()
{
    construct_key();

    set_position(&RULES[DEFAULT_RULE_NUMBER]);

    score[BLACK] = score[WHITE] = score_draw = nPlayed = 0;

    //tips.reserve(1024);
    cmdlist.reserve(256);

#ifdef PREFETCH_SUPPORT
    prefetch_range(millTable, sizeof(millTable));
#endif
}

Position::~Position()
{
    cmdlist.clear();
}

int Position::pieces_on_board_count()
{
    nPiecesOnBoard[BLACK] = nPiecesOnBoard[WHITE] = 0;

    for (int r = 1; r < FILE_NB + 2; r++) {
        for (int s = 0; s < RANK_NB; s++) {
            Square square = static_cast<Square>(r * RANK_NB + s);
            if (board[square] & B_STONE) {
                nPiecesOnBoard[BLACK]++;
            } else if (board[square] & W_STONE) {
                nPiecesOnBoard[WHITE]++;
            }
#if 0
            else if (board[square] & BAN_STONE) {
            }
#endif
        }
    }

    if (nPiecesOnBoard[BLACK] > rule.nTotalPiecesEachSide ||
        nPiecesOnBoard[WHITE] > rule.nTotalPiecesEachSide) {
        return -1;
    }

    return nPiecesOnBoard[BLACK] + nPiecesOnBoard[WHITE];
}

int Position::pieces_in_hand_count()
{
    nPiecesInHand[BLACK] = rule.nTotalPiecesEachSide - nPiecesOnBoard[BLACK];
    nPiecesInHand[WHITE] = rule.nTotalPiecesEachSide - nPiecesOnBoard[WHITE];

    return nPiecesInHand[BLACK] + nPiecesInHand[WHITE];
}

bool Position::set_position(const struct Rule *newRule)
{
    rule = *newRule;

    this->currentStep = 0;
    this->moveStep = 0;

    phase = PHASE_READY;
    set_side_to_move(BLACK);
    action = ACTION_PLACE;

    memset(board, 0, sizeof(board));
    st.key = 0;
    memset(byTypeBB, 0, sizeof(byTypeBB));

    if (pieces_on_board_count() == -1) {
        return false;
    }

    pieces_in_hand_count();
    nPiecesNeedRemove = 0;
    millListSize = 0;
    winner = NOBODY;
    MoveList::create();
    create_mill_table();
    currentSquare = SQ_0;
    elapsedSeconds[BLACK] = elapsedSeconds[WHITE] = 0;
    set_tips();
    cmdlist.clear();

    int r;
    for (r = 0; r < N_RULES; r++) {
        if (strcmp(rule.name, RULES[r].name) == 0)
            break;
    }

    if (sprintf(cmdline, "r%1u s%03u t%02u", r + 1, rule.maxStepsLedToDraw, rule.maxTimeLedToLose) > 0) {
        cmdlist.emplace_back(string(cmdline));
        return true;
    }

    cmdline[0] = '\0';
    return false;
}

bool Position::reset()
{
    if (phase == PHASE_READY &&
        elapsedSeconds[BLACK] == elapsedSeconds[WHITE] == 0) {
        return true;
    }

    currentStep = 0;
    moveStep = 0;

    phase = PHASE_READY;
    set_side_to_move(BLACK);
    action = ACTION_PLACE;

    winner = NOBODY;

    memset(board, 0, sizeof(board));
    st.key = 0;
    memset(byTypeBB, 0, sizeof(byTypeBB));

    nPiecesOnBoard[BLACK] = nPiecesOnBoard[WHITE] = 0;
    nPiecesInHand[BLACK] = nPiecesInHand[WHITE] = rule.nTotalPiecesEachSide;
    nPiecesNeedRemove = 0;
    millListSize = 0;
    currentSquare = SQ_0;
    elapsedSeconds[BLACK] = elapsedSeconds[WHITE] = 0;
    set_tips();
    cmdlist.clear();

#ifdef ENDGAME_LEARNING
    if (gameOptions.getLearnEndgameEnabled() && nPlayed != 0 && nPlayed % 256 == 0) {
        AIAlgorithm::recordEndgameHashMapToFile();
    }
#endif /* ENDGAME_LEARNING */

    int i;

    for (i = 0; i < N_RULES; i++) {
        if (strcmp(rule.name, RULES[i].name) == 0)
            break;
    }

    if (sprintf(cmdline, "r%1u s%03u t%02u",
                i + 1, rule.maxStepsLedToDraw, rule.maxTimeLedToLose) > 0) {
        cmdlist.emplace_back(string(cmdline));
        return true;
    }

    cmdline[0] = '\0';

    return false;
}

bool Position::start()
{
    switch (phase) {
    case PHASE_PLACING:
    case PHASE_MOVING:
        return false;
    case PHASE_GAMEOVER:
        reset();
        [[fallthrough]];
    case PHASE_READY:
        startTime = time(nullptr);
        phase = PHASE_PLACING;
        return true;
    default:
        return false;
    }
}

bool Position::place_piece(Square square, bool updateCmdlist)
{
    File file;
    Rank rank;
    int i;
    int seconds = -1;

    Piece piece = NO_PIECE;
    int n = 0;

    int us = sideToMove;

    Bitboard fromTo;

    if (phase == PHASE_GAMEOVER)
        return false;

    if (phase == PHASE_READY)
        start();

    if (action != ACTION_PLACE)
        return false;

    if (!onBoard[square] || board[square])
        return false;

    Position::square_to_polar(square, file, rank);

    if (phase == PHASE_PLACING) {
        piece = (Piece)((0x01 | (sideToMove << PLAYER_SHIFT)) + rule.nTotalPiecesEachSide - nPiecesInHand[us]);
        nPiecesInHand[us]--;
        nPiecesOnBoard[us]++;

        board[square] = piece;

        update_key(square);

        byTypeBB[ALL_PIECES] |= square;
        byTypeBB[us] |= square;

        move = static_cast<Move>(square);

        if (updateCmdlist) {
            seconds = update();
            sprintf(cmdline, "(%1u,%1u) %02u:%02u",
                    file, rank, seconds / 60, seconds % 60);
            cmdlist.emplace_back(string(cmdline));
            currentStep++;
        }

        currentSquare = square;

        n = add_mills(currentSquare);

        if (n == 0) {
            assert(nPiecesInHand[BLACK] >= 0 && nPiecesInHand[WHITE] >= 0);     

            if (nPiecesInHand[BLACK] == 0 && nPiecesInHand[WHITE] == 0) {
                if (check_gameover_condition(updateCmdlist)) {
                    goto out;
                }

                phase = PHASE_MOVING;
                action = ACTION_SELECT;
                clean_banned();

                if (rule.isDefenderMoveFirst) {
                    set_side_to_move(WHITE);
                } else {
                    set_side_to_move(BLACK);
                }

                if (check_gameover_condition(updateCmdlist)) {
                    goto out;
                }
            } else {
                change_side_to_move();
            }
        } else {
            nPiecesNeedRemove = rule.allowRemoveMultiPiecesWhenCloseMultiMill ? n : 1;
            action = ACTION_REMOVE;
        }

        goto out;
    }

    if (check_gameover_condition(updateCmdlist)) {
        goto out;
    }

    // When hase == GAME_MOVING

    // if illegal
    if (nPiecesOnBoard[sideToMove] > rule.nPiecesAtLeast ||
        !rule.allowFlyWhenRemainThreePieces) {
        for (i = 0; i < 4; i++) {
            if (square == MoveList::moveTable[currentSquare][i])
                break;
        }

        // not in moveTable
        if (i == 4) {
            return false;
        }
    }

    move = make_move(currentSquare, square);

    if (updateCmdlist) {
        seconds = update();
        sprintf(cmdline, "(%1u,%1u)->(%1u,%1u) %02u:%02u", currentSquare / RANK_NB, currentSquare % RANK_NB + 1,
                file, rank, seconds / 60, seconds % 60);
        cmdlist.emplace_back(string(cmdline));
        currentStep++;
        moveStep++;
    }

    fromTo = square_bb(currentSquare) | square_bb(square);
    byTypeBB[ALL_PIECES] ^= fromTo;
    byTypeBB[us] ^= fromTo;

    board[square] = board[currentSquare];

    update_key(square);
    revert_key(currentSquare);

    board[currentSquare] = NO_PIECE;

    currentSquare = square;
    n = add_mills(currentSquare);

    // midgame
    if (n == 0) {
        action = ACTION_SELECT;
        change_side_to_move();

        if (check_gameover_condition(updateCmdlist)) {
            goto out;
        }
    } else {
        nPiecesNeedRemove = rule.allowRemoveMultiPiecesWhenCloseMultiMill ? n : 1;
        action = ACTION_REMOVE;
    }

out:
    if (updateCmdlist) {
        set_tips();
    }

    return true;
}

bool Position::_placePiece(File file, Rank rank)
{
    Square square = Position::polar_to_square(file, rank);

    return place_piece(square, true);
}

bool Position::_removePiece(File file, Rank rank)
{
    Square square = Position::polar_to_square(file, rank);

    return remove_piece(square, 1);
}

bool Position::remove_piece(Square square, bool updateCmdlist)
{
    if (phase & PHASE_NOTPLAYING)
        return false;

    if (action != ACTION_REMOVE)
        return false;

    if (nPiecesNeedRemove <= 0)
        return false;

    File file;
    Rank rank;
    Position::square_to_polar(square, file, rank);

    int seconds = -1;

    int oppId = them;

    // if piece is not their
    if (!((them << PLAYER_SHIFT) & board[square]))
        return false;

    if (!rule.allowRemovePieceInMill &&
        in_how_many_mills(square, NOBODY) &&
        !is_all_in_mills(~sideToMove)) {
        return false;
    }

    if (rule.hasBannedLocations && phase == PHASE_PLACING) {
        revert_key(square);
        board[square] = BAN_STONE;
        update_key(square);

        byTypeBB[oppId] ^= square;
        byTypeBB[BAN] |= square;
    } else { // Remove
        revert_key(square);
        board[square] = NO_PIECE;

        byTypeBB[ALL_PIECES] ^= square;
        byTypeBB[them] ^= square;
    }

    nPiecesOnBoard[them]--;

    move = static_cast<Move>(-square);

    if (updateCmdlist) {
        seconds = update();
        sprintf(cmdline, "-(%1u,%1u)  %02u:%02u", file, rank, seconds / 60, seconds % 60);
        cmdlist.emplace_back(string(cmdline));
        currentStep++;
        moveStep = 0;
    }

    currentSquare = SQ_0;
    nPiecesNeedRemove--;

    // Remove piece completed

    if (check_gameover_condition(updateCmdlist)) {
        goto out;
    }

    if (nPiecesNeedRemove > 0) {
        return true;
    }

    if (phase == PHASE_PLACING) {
        if (nPiecesInHand[BLACK] == 0 && nPiecesInHand[WHITE] == 0) {

            phase = PHASE_MOVING;
            action = ACTION_SELECT;
            clean_banned();

            if (rule.isDefenderMoveFirst) {
                set_side_to_move(WHITE);
            } else {
                set_side_to_move(BLACK);
            }

            if (check_gameover_condition(updateCmdlist)) {
                goto out;
            }
        } else {
            action = ACTION_PLACE;
            change_side_to_move();

            if (check_gameover_condition(updateCmdlist)) {
                goto out;
            }
        }
    } else {
        action = ACTION_SELECT;
        change_side_to_move();

        if (check_gameover_condition(updateCmdlist)) {
            goto out;
        }
    }

out:
    if (updateCmdlist) {
        set_tips();
    }

    return true;
}

bool Position::select_piece(Square square)
{
    if (phase != PHASE_MOVING)
        return false;

    if (action != ACTION_SELECT && action != ACTION_PLACE)
        return false;

    if (board[square] & (sideToMove << PLAYER_SHIFT)) {
        currentSquare = square;
        action = ACTION_PLACE;

        return true;
    }

    return false;
}

bool Position::_selectPiece(File file, Rank rank)
{
    return select_piece(Position::polar_to_square(file, rank));
}

bool Position::giveup(Color loser)
{
    if (phase & PHASE_NOTPLAYING ||
        phase == PHASE_NONE) {
        return false;
    }

    phase = PHASE_GAMEOVER;

    Color loserColor = loser;
    char loserCh = color_to_char(loserColor);
    string loserStr = char_to_string(loserCh);

    winner = ~loser;
    tips = "玩家" + loserStr + "投子认负";
    sprintf(cmdline, "Player%d give up!", loserColor);
    score[winner]++;

    cmdlist.emplace_back(string(cmdline));

    return true;
}

bool Position::command(const char *cmd)
{
    int ruleIndex;
    unsigned t;
    Step step;
    File file1, file2;
    Rank rank1, rank2;
    int args = 0;
    int mm = 0, ss = 0;

    if (sscanf(cmd, "r%1u s%3hd t%2u", &ruleIndex, &step, &t) == 3) {
        if (ruleIndex <= 0 || ruleIndex > N_RULES) {
            return false;
        }

        return set_position(&RULES[ruleIndex - 1]);
    }

    args = sscanf(cmd, "(%1u,%1u)->(%1u,%1u) %2u:%2u", &file1, &rank1, &file2, &rank2, &mm, &ss);

    if (args >= 4) {
        if (args == 7) {
            if (mm >= 0 && ss >= 0)
                tm = mm * 60 + ss;
        }

        if (_selectPiece(file1, rank1)) {
            return _placePiece(file2, rank2);
        }

        return false;
    }

    args = sscanf(cmd, "-(%1u,%1u) %2u:%2u", &file1, &rank1, &mm, &ss);
    if (args >= 2) {
        if (args == 5) {
            if (mm >= 0 && ss >= 0)
                tm = mm * 60 + ss;
        }
        return _removePiece(file1, rank1);
    }

    args = sscanf(cmd, "(%1u,%1u) %2u:%2u", &file1, &rank1, &mm, &ss);
    if (args >= 2) {
        if (args == 5) {
            if (mm >= 0 && ss >= 0)
                tm = mm * 60 + ss;
        }
        return _placePiece(file1, rank1);
    }

    args = sscanf(cmd, "Player%1u give up!", &t);

    if (args == 1) {
        return giveup((Color)t);
    }

#ifdef THREEFOLD_REPETITION
    if (!strcmp(cmd, "Threefold Repetition. Draw!")) {
        return true;
    }

    if (!strcmp(cmd, "draw")) {
        phase = PHASE_GAMEOVER;
        winner = DRAW;
        score_draw++;
        tips = "三次重复局面判和。";
        sprintf(cmdline, "Threefold Repetition. Draw!");
        cmdlist.emplace_back(string(cmdline));
        return true;
    }
#endif /* THREEFOLD_REPETITION */

    return false;
}

bool Position::do_move(Move m)
{
    MoveType mt = type_of(m);

    switch (mt) {
    case MOVETYPE_REMOVE:
        return remove_piece(static_cast<Square>(-m));
    case MOVETYPE_MOVE:
        if (select_piece(from_sq(m))) {
            return place_piece(to_sq(m));
        }
        break;
    case MOVETYPE_PLACE:
        return place_piece(to_sq(m));
    default:
        break;
    }

    return false;
}

Color Position::get_winner() const
{
    return winner;
}

int Position::update()
{
    int ret = -1;
    int timePoint = -1;
    time_t *ourSeconds = &elapsedSeconds[sideToMove];
    time_t theirSeconds = elapsedSeconds[them];

    if (!(phase & PHASE_PLAYING)) {
        return -1;
    }

    currentTime = time(NULL);

    if (timePoint >= *ourSeconds) {
        *ourSeconds = ret = timePoint;
        startTime = currentTime - (elapsedSeconds[BLACK] + elapsedSeconds[WHITE]);
    } else {
        *ourSeconds = ret = currentTime - startTime - theirSeconds;
    }

    if (rule.maxTimeLedToLose > 0) {
        check_gameover_condition();
    }

    return ret;
}

bool Position::check_gameover_condition(int8_t updateCmdlist)
{
    if (phase & PHASE_NOTPLAYING) {
        return true;
    }

    if (rule.maxTimeLedToLose > 0) {
        phase = PHASE_GAMEOVER;

        if (updateCmdlist) {
            for (int i = 1; i <= 2; i++) {
                if (elapsedSeconds[i] > rule.maxTimeLedToLose * 60) {
                    elapsedSeconds[i] = rule.maxTimeLedToLose * 60;
                    winner = ~Color(i);
                    tips = "玩家" + char_to_string(color_to_char(Color(i))) + "超时判负。";
                    sprintf(cmdline, "Time over. Player%d win!", ~Color(i));
                }
            }

            cmdlist.emplace_back(string(cmdline));
        }

        return true;
    }

    if (rule.maxStepsLedToDraw > 0 &&
        moveStep > rule.maxStepsLedToDraw) {
        winner = DRAW;
        phase = PHASE_GAMEOVER;
        if (updateCmdlist) {
            sprintf(cmdline, "Steps over. In draw!");
            cmdlist.emplace_back(string(cmdline));
        }

        return true;
    }

    for (int i = 1; i <= 2; i++)
    {
        if (nPiecesOnBoard[i] + nPiecesInHand[i] < rule.nPiecesAtLeast) {
            winner = ~Color(i);
            phase = PHASE_GAMEOVER;

            if (updateCmdlist) {
                sprintf(cmdline, "Player%d win!", winner);
                cmdlist.emplace_back(string(cmdline));
            }

            return true;
        }
    }

#ifdef MCTS_AI
#if 0
    int diff = nPiecesOnBoard[BLACK] - nPiecesOnBoard[WHITE];
    if (diff > 4) {
        winner = BLACK;
        phase = PHASE_GAMEOVER;
        sprintf(cmdline, "Player1 win!");
        cmdlist.emplace_back(string(cmdline));

        return true;
    }

    if (diff < -4) {
        winner = WHITE;
        phase = PHASE_GAMEOVER;
        sprintf(cmdline, "Player2 win!");
        cmdlist.emplace_back(string(cmdline));

        return true;
    }
#endif
#endif

    if (nPiecesOnBoard[BLACK] + nPiecesOnBoard[WHITE] >= RANK_NB * FILE_NB) {
        phase = PHASE_GAMEOVER;

        if (rule.isBlackLosebutNotDrawWhenBoardFull) {
            winner = WHITE;
            if (updateCmdlist) {
                sprintf(cmdline, "Player2 win!");
            }
        } else {
            winner = DRAW; 
            if (updateCmdlist) {
                sprintf(cmdline, "Full. In draw!");
            }
        }

        if (updateCmdlist) {
            cmdlist.emplace_back(string(cmdline));
        }

        return true;
    }

    if (phase == PHASE_MOVING && action == ACTION_SELECT && is_all_surrounded()) {
        // TODO: move to next branch
        phase = PHASE_GAMEOVER;

        if (rule.isLoseButNotChangeTurnWhenNoWay) {
            if (updateCmdlist) {
                tips = "玩家" + char_to_string(color_to_char(sideToMove)) + "无子可走被闷";
                winner = ~sideToMove;
                sprintf(cmdline, "Player%d no way to go. Player%d win!", sideToMove, winner);
                cmdlist.emplace_back(string(cmdline));  // TODO: memleak
            }

            return true;
        }

        change_side_to_move();

        return false;
    }

    return false;
}

int Position::get_mobility_diff(bool includeFobidden)
{
    // TODO: Deal with rule is no ban location
    int mobilityBlack = 0;
    int mobilityWhite = 0;
    int diff = 0;
    int n = 0;

    for (Square i = SQ_BEGIN; i < SQ_END; i = static_cast<Square>(i + 1)) {
        n = surrounded_empty_squres_count(i, includeFobidden);

        if (board[i] & B_STONE) {
            mobilityBlack += n;
        } else if (board[i] & W_STONE) {
            mobilityWhite += n;
        }
    }

    diff = mobilityBlack - mobilityWhite;

    return diff;
}

void Position::clean_banned()
{
    if (!rule.hasBannedLocations) {
        return;
    }

    Square square = SQ_0;

    for (int r = 1; r <= FILE_NB; r++) {
        for (int s = 0; s < RANK_NB; s++) {
            square = static_cast<Square>(r * RANK_NB + s);

            if (board[square] == BAN_STONE) {
                revert_key(square);
                board[square] = NO_PIECE;
                byTypeBB[ALL_PIECES] ^= square;   // Need to remove?
            }
        }
    }
}

void Position::set_side_to_move(Color c)
{
    sideToMove = c;
    them = ~sideToMove;
}

void Position::change_side_to_move()
{
    set_side_to_move(~sideToMove);
}

bool Position::do_null_move()
{
    change_side_to_move();
    return true;
}

bool Position::undo_null_move()
{
    change_side_to_move();
    return true;
}

void Position::set_tips()
{
    string winnerStr, t;
    string turnStr = char_to_string(color_to_char(sideToMove));

    switch (phase) {
    case PHASE_READY:
        tips = "轮到玩家1落子，剩余" + std::to_string(nPiecesInHand[BLACK]) + "子" +
            "  比分 " + to_string(score[BLACK]) + ":" + to_string(score[WHITE]) + ", 和棋 " + to_string(score_draw);
        break;

    case PHASE_PLACING:
        if (action == ACTION_PLACE) {
            tips = "轮到玩家" + turnStr + "落子，剩余" + std::to_string(nPiecesInHand[sideToMove]) + "子";
        } else if (action == ACTION_REMOVE) {
            tips = "成三！轮到玩家" + turnStr + "去子，需去" + std::to_string(nPiecesNeedRemove) + "子";
        }
        break;

    case PHASE_MOVING:
        if (action == ACTION_PLACE || action == ACTION_SELECT) {
            tips = "轮到玩家" + turnStr + "选子移动";
        } else if (action == ACTION_REMOVE) {
            tips = "成三！轮到玩家" + turnStr + "去子，需去" + std::to_string(nPiecesNeedRemove) + "子";
        }
        break;

    case PHASE_GAMEOVER:  
        if (winner == DRAW) {
            score_draw++;
            tips = "双方平局！比分 " + to_string(score[BLACK]) + ":" + to_string(score[WHITE]) + ", 和棋 " + to_string(score_draw);
            break;
        }

        winnerStr = char_to_string(color_to_char(winner));

        score[winner]++;

        t = "玩家" + winnerStr + "获胜！比分 " + to_string(score[BLACK]) + ":" + to_string(score[WHITE]) + ", 和棋 " + to_string(score_draw);

        if (tips.find("无子可走") != string::npos) {
            tips += t;
        } else {
            tips = t;
        }

        break;

    default:
        break;
    }
}

time_t Position::get_elapsed_time(int us)
{
    return elapsedSeconds[us];
}

void Position::construct_key()
{
    st.key = 0;
}

Key Position::key()
{
    // TODO: Move to suitable function
    return update_key_misc();
}

Key Position::update_key(Square square)
{
    // PieceType is board[square] 

    // 0b00 - no piece，0b01 = 1 black，0b10 = 2 white，0b11 = 3 ban
    int pieceType = color_on(square);
    // TODO: this is std, but current code can work
    //Location loc = board[square];
    //int pieceType = loc == 0x0f? 3 : loc >> PLAYER_SHIFT;

    st.key ^= zobrist[square][pieceType];

    return st.key;
}

Key Position::revert_key(Square square)
{
    return update_key(square);
}

Key Position::update_key_misc()
{
    const int KEY_MISC_BIT = 8;

    st.key = st.key << KEY_MISC_BIT >> KEY_MISC_BIT;
    Key hi = 0;

    if (sideToMove == WHITE) {
        hi |= 1U;
    }

    if (action == ACTION_REMOVE) {
        hi |= 1U << 1;
    }

    hi |= static_cast<Key>(nPiecesNeedRemove) << 2;
    hi |= static_cast<Key>(nPiecesInHand[BLACK]) << 4;     // TODO: may use phase is also OK?

    st.key = st.key | (hi << (CHAR_BIT * sizeof(Key) - KEY_MISC_BIT));

    return st.key;
}

Key Position::next_primary_key(Move m)
{
    Key npKey = st.key /* << 8 >> 8 */;
    Square sq = static_cast<Square>(to_sq(m));;
    MoveType mt = type_of(m);

    if (mt == MOVETYPE_REMOVE) {
        int pieceType = ~sideToMove;
        npKey ^= zobrist[sq][pieceType];

        if (rule.hasBannedLocations && phase == PHASE_PLACING) {
            npKey ^= zobrist[sq][BAN];
        }

        return npKey;
    }
    
    int pieceType = sideToMove;
    npKey ^= zobrist[sq][pieceType];

    if (mt == MOVETYPE_MOVE) {
        npKey ^= zobrist[from_sq(m)][pieceType];
    }

    return npKey;
}

///////////////////////////////////////////////////////////////////////////////

/*
  Sanmill, a mill game playing engine derived from NineChess 1.5
  Copyright (C) 2015-2018 liuweilhy (NineChess author)
  Copyright (C) 2019-2020 Calcitem <calcitem@outlook.com>

  Sanmill is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Sanmill is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "movegen.h"
#include "prefetch.h"

const int Position::onBoard[SQUARE_NB] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int Position::millTable[SQUARE_NB][LD_NB][FILE_NB - 1] = { {{0}} };

#if 0
Position &Position::operator= (const Position &other)
{
    if (this == &other)
        return *this;

    memcpy(this->board, other.board, sizeof(this->board));
    memcpy(this->byTypeBB, other.byTypeBB, sizeof(this->byTypeBB));

    memcpy(&millList, &other.millList, sizeof(millList));
    millListSize = other.millListSize;

    return *this;
}
#endif

void Position::create_mill_table()
{
    const int millTable_noObliqueLine[SQUARE_NB][LD_NB][2] = {
        /* 0 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 1 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 2 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 3 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 4 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 5 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 6 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 7 */ {{0, 0}, {0, 0}, {0, 0}},

        /* 8 */ {{16, 24}, {9, 15}, {0, 0}},
        /* 9 */ {{0, 0}, {15, 8}, {10, 11}},
        /* 10 */ {{18, 26}, {11, 9}, {0, 0}},
        /* 11 */ {{0, 0}, {9, 10}, {12, 13}},
        /* 12 */ {{20, 28}, {13, 11}, {0, 0}},
        /* 13 */ {{0, 0}, {11, 12}, {14, 15}},
        /* 14 */ {{22, 30}, {15, 13}, {0, 0}},
        /* 15 */ {{0, 0}, {13, 14}, {8, 9}},

        /* 16 */ {{8, 24}, {17, 23}, {0, 0}},
        /* 17 */ {{0, 0}, {23, 16}, {18, 19}},
        /* 18 */ {{10, 26}, {19, 17}, {0, 0}},
        /* 19 */ {{0, 0}, {17, 18}, {20, 21}},
        /* 20 */ {{12, 28}, {21, 19}, {0, 0}},
        /* 21 */ {{0, 0}, {19, 20}, {22, 23}},
        /* 22 */ {{14, 30}, {23, 21}, {0, 0}},
        /* 23 */ {{0, 0}, {21, 22}, {16, 17}},

        /* 24 */ {{8, 16}, {25, 31}, {0, 0}},
        /* 25 */ {{0, 0}, {31, 24}, {26, 27}},
        /* 26 */ {{10, 18}, {27, 25}, {0, 0}},
        /* 27 */ {{0, 0}, {25, 26}, {28, 29}},
        /* 28 */ {{12, 20}, {29, 27}, {0, 0}},
        /* 29 */ {{0, 0}, {27, 28}, {30, 31}},
        /* 30 */ {{14, 22}, {31, 29}, {0, 0}},
        /* 31 */ {{0, 0}, {29, 30}, {24, 25}},

        /* 32 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 33 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 34 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 35 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 36 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 37 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 38 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 39 */ {{0, 0}, {0, 0}, {0, 0}}
    };

    const int millTable_hasObliqueLines[SQUARE_NB][LD_NB][2] = {
        /*  0 */ {{0, 0}, {0, 0}, {0, 0}},
        /*  1 */ {{0, 0}, {0, 0}, {0, 0}},
        /*  2 */ {{0, 0}, {0, 0}, {0, 0}},
        /*  3 */ {{0, 0}, {0, 0}, {0, 0}},
        /*  4 */ {{0, 0}, {0, 0}, {0, 0}},
        /*  5 */ {{0, 0}, {0, 0}, {0, 0}},
        /*  6 */ {{0, 0}, {0, 0}, {0, 0}},
        /*  7 */ {{0, 0}, {0, 0}, {0, 0}},

        /*  8 */ {{16, 24}, {9, 15}, {0, 0}},
        /*  9 */ {{17, 25}, {15, 8}, {10, 11}},
        /* 10 */ {{18, 26}, {11, 9}, {0, 0}},
        /* 11 */ {{19, 27}, {9, 10}, {12, 13}},
        /* 12 */ {{20, 28}, {13, 11}, {0, 0}},
        /* 13 */ {{21, 29}, {11, 12}, {14, 15}},
        /* 14 */ {{22, 30}, {15, 13}, {0, 0}},
        /* 15 */ {{23, 31}, {13, 14}, {8, 9}},

        /* 16 */ {{8, 24}, {17, 23}, {0, 0}},
        /* 17 */ {{9, 25}, {23, 16}, {18, 19}},
        /* 18 */ {{10, 26}, {19, 17}, {0, 0}},
        /* 19 */ {{11, 27}, {17, 18}, {20, 21}},
        /* 20 */ {{12, 28}, {21, 19}, {0, 0}},
        /* 21 */ {{13, 29}, {19, 20}, {22, 23}},
        /* 22 */ {{14, 30}, {23, 21}, {0, 0}},
        /* 23 */ {{15, 31}, {21, 22}, {16, 17}},

        /* 24 */ {{8, 16}, {25, 31}, {0, 0}},
        /* 25 */ {{9, 17}, {31, 24}, {26, 27}},
        /* 26 */ {{10, 18}, {27, 25}, {0, 0}},
        /* 27 */ {{11, 19}, {25, 26}, {28, 29}},
        /* 28 */ {{12, 20}, {29, 27}, {0, 0}},
        /* 29 */ {{13, 21}, {27, 28}, {30, 31}},
        /* 30 */ {{14, 22}, {31, 29}, {0, 0}},
        /* 31 */ {{15, 23}, {29, 30}, {24, 25}},

        /* 32 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 33 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 34 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 35 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 36 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 37 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 38 */ {{0, 0}, {0, 0}, {0, 0}},
        /* 39 */ {{0, 0}, {0, 0}, {0, 0}}
    };

    if (rule.hasObliqueLines) {
        memcpy(millTable, millTable_hasObliqueLines, sizeof(millTable));
    } else {
        memcpy(millTable, millTable_noObliqueLine, sizeof(millTable));
    }

#ifdef DEBUG_MODE
    for (int i = 0; i < SQUARE_NB; i++) {
        loggerDebug("/* %d */ {", i);
        for (int j = 0; j < MD_NB; j++) {
            loggerDebug("{");
            for (int k = 0; k < 2; k++) {
                if (k == 0) {
                    loggerDebug("%d, ", millTable[i][j][k]);
                } else {
                    loggerDebug("%d", millTable[i][j][k]);
                }

            }
            if (j == 2)
                loggerDebug("}");
            else
                loggerDebug("}, ");
        }
        loggerDebug("},\n");
    }

    loggerDebug("======== millTable End =========\n");
#endif /* DEBUG_MODE */
}

void Position::square_to_polar(const Square square, File &file, Rank &rank)
{
    //r = square / RANK_NB;
    //s = square % RANK_NB + 1;
    file = File(square >> 3);
    rank = Rank((square & 0x07) + 1);
}

Square Position::polar_to_square(File file, Rank rank)
{
    assert(!(file < 1 || file > FILE_NB || rank < 1 || rank > RANK_NB));

    return static_cast<Square>(file * RANK_NB + rank - 1);
}

Color Position::color_on(Square s)
{
    return Color((board[s] & 0x30) >> PLAYER_SHIFT);
}

int Position::in_how_many_mills(Square square, Color c, Square squareSelected)
{
    int n = 0;
    Piece locbak = NO_PIECE;

    if (c == NOBODY) {
        c = Color(color_on(square) >> PLAYER_SHIFT);
    }

    if (squareSelected != SQ_0) {
        locbak = board[squareSelected];
        board[squareSelected] = NO_PIECE;
    }

    for (int l = 0; l < LD_NB; l++) {
        if ((c << PLAYER_SHIFT) &
            board[millTable[square][l][0]] &
            board[millTable[square][l][1]]) {
            n++;
        }
    }

    if (squareSelected != SQ_0) {
        board[squareSelected] = locbak;
    }

    return n;
}

int Position::add_mills(Square square)
{
    uint64_t mill = 0;
    int n = 0;
    int idx[3], min, temp;
    Color m = color_on(square);

    for (int i = 0; i < 3; i++) {
        idx[0] = square;
        idx[1] = millTable[square][i][0];
        idx[2] = millTable[square][i][1];

        // no mill
        if (!((m << PLAYER_SHIFT) & board[idx[1]] & board[idx[2]])) {
            continue;
        }

        // close mill

        // sort
        for (int j = 0; j < 2; j++) {
            min = j;

            for (int k = j + 1; k < 3; k++) {
                if (idx[min] > idx[k])
                    min = k;
            }

            if (min == j) {
                continue;
            }

            temp = idx[min];
            idx[min] = idx[j];
            idx[j] = temp;
        }

        mill = (static_cast<uint64_t>(board[idx[0]]) << 40)
            + (static_cast<uint64_t>(idx[0]) << 32)
            + (static_cast<uint64_t>(board[idx[1]]) << 24)
            + (static_cast<uint64_t>(idx[1]) << 16)
            + (static_cast<uint64_t>(board[idx[2]]) << 8)
            + static_cast<uint64_t>(idx[2]);

        if (rule.allowRemovePiecesRepeatedlyWhenCloseSameMill) {
            n++;
            continue;
        }

        int im = 0;
        for (im = 0; im < millListSize; im++) {
            if (mill == millList[im]) {
                break;
            }
        }

        if (im == millListSize) {
            n++;
            millList[i] = mill;
            millListSize++;
        }
    }

    return n;
}

bool Position::is_all_in_mills(Color c)
{
    for (Square i = SQ_BEGIN; i < SQ_END; i = static_cast<Square>(i + 1)) {
        if (board[i] & ((uint8_t)(c << PLAYER_SHIFT))) {
            if (!in_how_many_mills(i, NOBODY)) {
                return false;
            }
        }
    }

    return true;
}

// Stat include ban
int Position::surrounded_empty_squres_count(Square square, bool includeFobidden)
{
    //assert(rule.hasBannedLocations == includeFobidden);

    int count = 0;

    if (nPiecesOnBoard[sideToMove] > rule.nPiecesAtLeast ||
        !rule.allowFlyWhenRemainThreePieces) {
        Square moveSquare;
        for (MoveDirection d = MD_BEGIN; d < MD_NB; d = (MoveDirection)(d + 1)) {
            moveSquare = static_cast<Square>(MoveList::moveTable[square][d]);
            if (moveSquare) {
                if (board[moveSquare] == 0x00 ||
                    (includeFobidden && board[moveSquare] == BAN_STONE)) {
                    count++;
                }
            }
        }
    }

    return count;
}

void Position::surrounded_pieces_count(Square square, int &nOurPieces, int &nTheirPieces, int &nBanned, int &nEmpty)
{
    Square moveSquare;

    for (MoveDirection d = MD_BEGIN; d < MD_NB; d = (MoveDirection)(d + 1)) {
        moveSquare = static_cast<Square>(MoveList::moveTable[square][d]);

        if (!moveSquare) {
            continue;
        }

        enum Piece pieceType = static_cast<Piece>(board[moveSquare]);

        switch (pieceType) {
        case NO_PIECE:
            nEmpty++;
            break;
        case BAN_STONE:
            nBanned++;
            break;
        default:
            if (sideToMove == pieceType >> PLAYER_SHIFT) {
                nOurPieces++;
            } else {
                nTheirPieces++;
            }
            break;
        }
    }
}

bool Position::is_all_surrounded()
{
    // Full
    if (nPiecesOnBoard[BLACK] + nPiecesOnBoard[WHITE] >= RANK_NB * FILE_NB)
        return true;

    // Can fly
    if (nPiecesOnBoard[sideToMove] <= rule.nPiecesAtLeast &&
        rule.allowFlyWhenRemainThreePieces) {
        return false;
    }

    Square moveSquare;

    for (Square sq = SQ_BEGIN; sq < SQ_END; sq = (Square)(sq + 1)) {
        if (!(sideToMove & color_on(sq))) {
            continue;
        }

        for (MoveDirection d = MD_BEGIN; d < MD_NB; d = (MoveDirection)(d + 1)) {
            moveSquare = static_cast<Square>(MoveList::moveTable[sq][d]);
            if (moveSquare && !board[moveSquare]) {
                return false;
            }
        }
    }

    return true;
}

bool Position::is_star_square(Square square)
{
    if (rule.nTotalPiecesEachSide == 12) {
        return (square == 17 ||
                square == 19 ||
                square == 21 ||
                square == 23);
    }

    return (square == 16 ||
            square == 18 ||
            square == 20 ||
            square == 22);
}

void Position::mirror(int32_t move_, Square square, bool cmdChange /*= true*/)
{
    Piece ch;
    int r, s;
    int i;

    for (r = 1; r <= FILE_NB; r++) {
        for (s = 1; s < RANK_NB / 2; s++) {
            ch = board[r * RANK_NB + s];
            board[r * RANK_NB + s] = board[(r + 1) * RANK_NB - s];
            board[(r + 1) * RANK_NB - s] = ch;
        }
    }

    uint64_t llp[3] = { 0 };

    if (move_ < 0) {
        r = (-move_) / RANK_NB;
        s = (-move_) % RANK_NB;
        s = (RANK_NB - s) % RANK_NB;
        move_ = -(r * RANK_NB + s);
    } else {
        llp[0] = static_cast<uint64_t>(from_sq((Move)move_));
        llp[1] = to_sq((Move)move_);

        for (i = 0; i < 2; i++) {
            r = static_cast<int>(llp[i]) / RANK_NB;
            s = static_cast<int>(llp[i]) % RANK_NB;
            s = (RANK_NB - s) % RANK_NB;
            llp[i] = (static_cast<uint64_t>(r) * RANK_NB + s);
        }

        move_ = static_cast<int16_t>(((llp[0] << 8) | llp[1]));
    }

    if (square != 0) {
        r = square / RANK_NB;
        s = square % RANK_NB;
        s = (RANK_NB - s) % RANK_NB;
        square = static_cast<Square>(r * RANK_NB + s);
    }

    if (rule.allowRemovePiecesRepeatedlyWhenCloseSameMill) {
        for (auto &mill : millList) {
            llp[0] = (mill & 0x000000ff00000000) >> 32;
            llp[1] = (mill & 0x0000000000ff0000) >> 16;
            llp[2] = (mill & 0x00000000000000ff);

            for (i = 0; i < 3; i++) {
                r = static_cast<int>(llp[i]) / RANK_NB;
                s = static_cast<int>(llp[i]) % RANK_NB;
                s = (RANK_NB - s) % RANK_NB;
                llp[i] = static_cast<uint64_t>(r * RANK_NB + s);
            }

            mill &= 0xffffff00ff00ff00;
            mill |= (llp[0] << 32) | (llp[1] << 16) | llp[2];
        }
    }

    if (cmdChange) {
        int r1, s1, r2, s2;
        int args = 0;
        int mm = 0, ss = 0;

        args = sscanf(cmdline, "(%1u,%1u)->(%1u,%1u) %2u:%2u", &r1, &s1, &r2, &s2, &mm, &ss);
        if (args >= 4) {
            s1 = (RANK_NB - s1 + 1) % RANK_NB;
            s2 = (RANK_NB - s2 + 1) % RANK_NB;
            cmdline[3] = '1' + static_cast<char>(s1);
            cmdline[10] = '1' + static_cast<char>(s2);
        } else {
            args = sscanf(cmdline, "-(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
            if (args >= 2) {
                s1 = (RANK_NB - s1 + 1) % RANK_NB;
                cmdline[4] = '1' + static_cast<char>(s1);
            } else {
                args = sscanf(cmdline, "(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
                if (args >= 2) {
                    s1 = (RANK_NB - s1 + 1) % RANK_NB;
                    cmdline[3] = '1' + static_cast<char>(s1);
                }
            }
        }

        for (auto &iter : cmdlist) {
            args = sscanf(iter.c_str(), "(%1u,%1u)->(%1u,%1u) %2u:%2u", &r1, &s1, &r2, &s2, &mm, &ss);
            if (args >= 4) {
                s1 = (RANK_NB - s1 + 1) % RANK_NB;
                s2 = (RANK_NB - s2 + 1) % RANK_NB;
                iter[3] = '1' + static_cast<char>(s1);
                iter[10] = '1' + static_cast<char>(s2);
            } else {
                args = sscanf(iter.c_str(), "-(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
                if (args >= 2) {
                    s1 = (RANK_NB - s1 + 1) % RANK_NB;
                    iter[4] = '1' + static_cast<char>(s1);
                } else {
                    args = sscanf(iter.c_str(), "(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
                    if (args >= 2) {
                        s1 = (RANK_NB - s1 + 1) % RANK_NB;
                        iter[3] = '1' + static_cast<char>(s1);
                    }
                }
            }
        }
    }
}

void Position::turn(int32_t move_, Square square, bool cmdChange /*= true*/)
{
    Piece ch;
    int r, s;
    int i;

    for (s = 0; s < RANK_NB; s++) {
        ch = board[RANK_NB + s];
        board[RANK_NB + s] = board[RANK_NB * FILE_NB + s];
        board[RANK_NB * FILE_NB + s] = ch;
    }

    uint64_t llp[3] = { 0 };

    if (move_ < 0) {
        r = (-move_) / RANK_NB;
        s = (-move_) % RANK_NB;

        if (r == 1)
            r = FILE_NB;
        else if (r == FILE_NB)
            r = 1;

        move_ = -(r * RANK_NB + s);
    } else {
        llp[0] = static_cast<uint64_t>(from_sq((Move)move_));
        llp[1] = to_sq((Move)move_);

        for (i = 0; i < 2; i++) {
            r = static_cast<int>(llp[i]) / RANK_NB;
            s = static_cast<int>(llp[i]) % RANK_NB;

            if (r == 1)
                r = FILE_NB;
            else if (r == FILE_NB)
                r = 1;

            llp[i] = static_cast<uint64_t>(r * RANK_NB + s);
        }

        move_ = static_cast<int16_t>(((llp[0] << 8) | llp[1]));
    }

    if (square != 0) {
        r = square / RANK_NB;
        s = square % RANK_NB;

        if (r == 1)
            r = FILE_NB;
        else if (r == FILE_NB)
            r = 1;

        square = static_cast<Square>(r * RANK_NB + s);
    }

    if (rule.allowRemovePiecesRepeatedlyWhenCloseSameMill) {
        for (auto &mill : millList) {
            llp[0] = (mill & 0x000000ff00000000) >> 32;
            llp[1] = (mill & 0x0000000000ff0000) >> 16;
            llp[2] = (mill & 0x00000000000000ff);

            for (i = 0; i < 3; i++) {
                r = static_cast<int>(llp[i]) / RANK_NB;
                s = static_cast<int>(llp[i]) % RANK_NB;

                if (r == 1)
                    r = FILE_NB;
                else if (r == FILE_NB)
                    r = 1;

                llp[i] = static_cast<uint64_t>(r * RANK_NB + s);
            }

            mill &= 0xffffff00ff00ff00;
            mill |= (llp[0] << 32) | (llp[1] << 16) | llp[2];
        }
    }

    // 命令行解析
    if (cmdChange) {
        int r1, s1, r2, s2;
        int args = 0;
        int mm = 0, ss = 0;

        args = sscanf(cmdline, "(%1u,%1u)->(%1u,%1u) %2u:%2u",
                      &r1, &s1, &r2, &s2, &mm, &ss);

        if (args >= 4) {
            if (r1 == 1)
                r1 = FILE_NB;
            else if (r1 == FILE_NB)
                r1 = 1;

            if (r2 == 1)
                r2 = FILE_NB;
            else if (r2 == FILE_NB)
                r2 = 1;

            cmdline[1] = '0' + static_cast<char>(r1);
            cmdline[8] = '0' + static_cast<char>(r2);
        } else {
            args = sscanf(cmdline, "-(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
            if (args >= 2) {
                if (r1 == 1)
                    r1 = FILE_NB;
                else if (r1 == FILE_NB)
                    r1 = 1;
                cmdline[2] = '0' + static_cast<char>(r1);
            } else {
                args = sscanf(cmdline, "(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
                if (args >= 2) {
                    if (r1 == 1)
                        r1 = FILE_NB;
                    else if (r1 == FILE_NB)
                        r1 = 1;
                    cmdline[1] = '0' + static_cast<char>(r1);
                }
            }
        }

        for (auto &iter : cmdlist) {
            args = sscanf(iter.c_str(),
                          "(%1u,%1u)->(%1u,%1u) %2u:%2u",
                          &r1, &s1, &r2, &s2, &mm, &ss);

            if (args >= 4) {
                if (r1 == 1)
                    r1 = FILE_NB;
                else if (r1 == FILE_NB)
                    r1 = 1;

                if (r2 == 1)
                    r2 = FILE_NB;
                else if (r2 == FILE_NB)
                    r2 = 1;

                iter[1] = '0' + static_cast<char>(r1);
                iter[8] = '0' + static_cast<char>(r2);
            } else {
                args = sscanf(iter.c_str(), "-(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
                if (args >= 2) {
                    if (r1 == 1)
                        r1 = FILE_NB;
                    else if (r1 == FILE_NB)
                        r1 = 1;

                    iter[2] = '0' + static_cast<char>(r1);
                } else {
                    args = sscanf(iter.c_str(), "(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
                    if (args >= 2) {
                        if (r1 == 1)
                            r1 = FILE_NB;
                        else if (r1 == FILE_NB)
                            r1 = 1;

                        iter[1] = '0' + static_cast<char>(r1);
                    }
                }
            }
        }
    }
}

void Position::rotate(int degrees, int32_t move_, Square square, bool cmdChange /*= true*/)
{
    degrees = degrees % 360;

    if (degrees < 0)
        degrees += 360;

    if (degrees == 0 || degrees % 90)
        return;

    degrees /= 45;

    Piece ch1, ch2;
    int r, s;
    int i;

    if (degrees == 2) {
        for (r = 1; r <= FILE_NB; r++) {
            ch1 = board[r * RANK_NB];
            ch2 = board[r * RANK_NB + 1];

            for (s = 0; s < RANK_NB - 2; s++) {
                board[r * RANK_NB + s] = board[r * RANK_NB + s + 2];
            }

            board[r * RANK_NB + 6] = ch1;
            board[r * RANK_NB + 7] = ch2;
        }
    } else if (degrees == 6) {
        for (r = 1; r <= FILE_NB; r++) {
            ch1 = board[r * RANK_NB + 7];
            ch2 = board[r * RANK_NB + 6];

            for (s = RANK_NB - 1; s >= 2; s--) {
                board[r * RANK_NB + s] = board[r * RANK_NB + s - 2];
            }

            board[r * RANK_NB + 1] = ch1;
            board[r * RANK_NB] = ch2;
        }
    } else if (degrees == 4) {
        for (r = 1; r <= FILE_NB; r++) {
            for (s = 0; s < RANK_NB / 2; s++) {
                ch1 = board[r * RANK_NB + s];
                board[r * RANK_NB + s] = board[r * RANK_NB + s + 4];
                board[r * RANK_NB + s + 4] = ch1;
            }
        }
    } else {
        return;
    }

    uint64_t llp[3] = { 0 };

    if (move_ < 0) {
        r = (-move_) / RANK_NB;
        s = (-move_) % RANK_NB;
        s = (s + RANK_NB - degrees) % RANK_NB;
        move_ = -(r * RANK_NB + s);
    } else {
        llp[0] = static_cast<uint64_t>(from_sq((Move)move_));
        llp[1] = to_sq((Move)move_);
        r = static_cast<int>(llp[0]) / RANK_NB;
        s = static_cast<int>(llp[0]) % RANK_NB;
        s = (s + RANK_NB - degrees) % RANK_NB;
        llp[0] = static_cast<uint64_t>(r * RANK_NB + s);
        r = static_cast<int>(llp[1]) / RANK_NB;
        s = static_cast<int>(llp[1]) % RANK_NB;
        s = (s + RANK_NB - degrees) % RANK_NB;
        llp[1] = static_cast<uint64_t>(r * RANK_NB + s);
        move_ = static_cast<int16_t>(((llp[0] << 8) | llp[1]));
    }

    if (square != 0) {
        r = square / RANK_NB;
        s = square % RANK_NB;
        s = (s + RANK_NB - degrees) % RANK_NB;
        square = static_cast<Square>(r * RANK_NB + s);
    }

    if (rule.allowRemovePiecesRepeatedlyWhenCloseSameMill) {
        for (auto &mill : millList) {
            llp[0] = (mill & 0x000000ff00000000) >> 32;
            llp[1] = (mill & 0x0000000000ff0000) >> 16;
            llp[2] = (mill & 0x00000000000000ff);

            for (i = 0; i < 3; i++) {
                r = static_cast<int>(llp[i]) / RANK_NB;
                s = static_cast<int>(llp[i]) % RANK_NB;
                s = (s + RANK_NB - degrees) % RANK_NB;
                llp[i] = static_cast<uint64_t>(r * RANK_NB + s);
            }

            mill &= 0xffffff00ff00ff00;
            mill |= (llp[0] << 32) | (llp[1] << 16) | llp[2];
        }
    }

    if (cmdChange) {
        int r1, s1, r2, s2;
        int args = 0;
        int mm = 0, ss = 0;

        args = sscanf(cmdline, "(%1u,%1u)->(%1u,%1u) %2u:%2u", &r1, &s1, &r2, &s2, &mm, &ss);
        if (args >= 4) {
            s1 = (s1 - 1 + RANK_NB - degrees) % RANK_NB;
            s2 = (s2 - 1 + RANK_NB - degrees) % RANK_NB;
            cmdline[3] = '1' + static_cast<char>(s1);
            cmdline[10] = '1' + static_cast<char>(s2);
        } else {
            args = sscanf(cmdline, "-(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);

            if (args >= 2) {
                s1 = (s1 - 1 + RANK_NB - degrees) % RANK_NB;
                cmdline[4] = '1' + static_cast<char>(s1);
            } else {
                args = sscanf(cmdline, "(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);

                if (args >= 2) {
                    s1 = (s1 - 1 + RANK_NB - degrees) % RANK_NB;
                    cmdline[3] = '1' + static_cast<char>(s1);
                }
            }
        }

        for (auto &iter : cmdlist) {
            args = sscanf(iter.c_str(), "(%1u,%1u)->(%1u,%1u) %2u:%2u", &r1, &s1, &r2, &s2, &mm, &ss);

            if (args >= 4) {
                s1 = (s1 - 1 + RANK_NB - degrees) % RANK_NB;
                s2 = (s2 - 1 + RANK_NB - degrees) % RANK_NB;
                iter[3] = '1' + static_cast<char>(s1);
                iter[10] = '1' + static_cast<char>(s2);
            } else {
                args = sscanf(iter.c_str(), "-(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);

                if (args >= 2) {
                    s1 = (s1 - 1 + RANK_NB - degrees) % RANK_NB;
                    iter[4] = '1' + static_cast<char>(s1);
                } else {
                    args = sscanf(iter.c_str(), "(%1u,%1u) %2u:%2u", &r1, &s1, &mm, &ss);
                    if (args >= 2) {
                        s1 = (s1 - 1 + RANK_NB - degrees) % RANK_NB;
                        iter[3] = '1' + static_cast<char>(s1);
                    }
                }
            }
        }
    }
}

void Position::print_board()
{
    if (rule.nTotalPiecesEachSide == 12) {
        loggerDebug("\n"
                    "31 ----- 24 ----- 25\n"
                    "| \\       |      / |\n"
                    "|  23 -- 16 -- 17  |\n"
                    "|  | \\    |   / |  |\n"
                    "|  |  15-08-09  |  |\n"
                    "30-22-14    10-18-26\n"
                    "|  |  13-12-11  |  |\n"
                    "|  | /    |   \\ |  |\n"
                    "|  21 -- 20 -- 19  |\n"
                    "| /       |      \\ |\n"
                    "29 ----- 28 ----- 27\n"
                    "\n");
    } else {
        loggerDebug("\n"
                    "31 ----- 24 ----- 25\n"
                    "|         |        |\n"
                    "|  23 -- 16 -- 17  |\n"
                    "|  |      |     |  |\n"
                    "|  |  15-08-09  |  |\n"
                    "30-22-14    10-18-26\n"
                    "|  |  13-12-11  |  |\n"
                    "|  |      |     |  |\n"
                    "|  21 -- 20 -- 19  |\n"
                    "|         |        |\n"
                    "29 ----- 28 ----- 27\n"
                    "\n");
    }
}