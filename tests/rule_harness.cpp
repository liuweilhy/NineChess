#include "ninechess.h"

#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

uint32_t bitOf(const int pos)
{
    return 1u << pos;
}

std::vector<int> toVector(const std::initializer_list<int> positions)
{
    return std::vector<int>(positions);
}

uint32_t maskOf(const std::vector<int>& positions)
{
    uint32_t mask = 0u;
    for (const int pos : positions) {
        mask |= bitOf(pos);
    }
    return mask;
}

int posOf(const NineChess& chess, const int c, const int p)
{
    return chess.cpToPos(c, p);
}

void clearAuxData(NineChess::ChessData& data)
{
    data.forbiddenBoard = 0u;
    for (int i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        data.numberBoards[i] = 0u;
    }
    data.millHistory.clear();
}

void assignSequentialNumbers(
    NineChess::ChessData& data,
    const std::vector<int>& player1Positions,
    const std::vector<int>& player2Positions)
{
    clearAuxData(data);

    for (size_t i = 0; i < player1Positions.size() && i < NUMBERED_PIECE_COUNT; ++i) {
        data.numberBoards[i] |= bitOf(player1Positions[i]);
    }
    for (size_t i = 0; i < player2Positions.size() && i < NUMBERED_PIECE_COUNT; ++i) {
        data.numberBoards[i] |= bitOf(player2Positions[i]);
    }
}

void setupMidgame(
    NineChess& chess,
    const NineChess::Players turn,
    const std::vector<int>& player1Positions,
    const std::vector<int>& player2Positions,
    const bool withNumbers = false)
{
    chess.reset();
    NineChess::ChessData& data = chess.getData();
    data.player1Board = maskOf(player1Positions);
    data.player2Board = maskOf(player2Positions);
    clearAuxData(data);
    if (withNumbers) {
        assignSequentialNumbers(data, player1Positions, player2Positions);
    }
    data.setPlayer2InHand(0u);
    data.clearPendingCaptures();
    data.setState(NineChess::GAME_MID, NineChess::ACTION_CHOOSE, turn);
    chess.refreshTip();
}

void setupOpeningCapture(
    NineChess& chess,
    const NineChess::Players turn,
    const uint32_t player2InHand,
    const std::vector<int>& player1Positions,
    const std::vector<int>& player2Positions,
    const bool withNumbers = false,
    const uint32_t pendingCaptures = 1u)
{
    chess.reset();
    NineChess::ChessData& data = chess.getData();
    data.player1Board = maskOf(player1Positions);
    data.player2Board = maskOf(player2Positions);
    clearAuxData(data);
    if (withNumbers) {
        assignSequentialNumbers(data, player1Positions, player2Positions);
    }
    data.setPlayer2InHand(player2InHand);
    data.setPendingCaptures(pendingCaptures);
    data.setState(NineChess::GAME_OPENING, NineChess::ACTION_CAPTURE, turn);
    chess.refreshTip();
}

struct CaseContext {
    int checks = 0;
    int failed = 0;

    void expect(const bool condition, const std::string& message)
    {
        ++checks;
        if (condition) {
            std::cout << "    [PASS] " << message << "\n";
        }
        else {
            ++failed;
            std::cout << "    [FAIL] " << message << "\n";
        }
    }

    void expectCommand(
        NineChess& chess,
        const std::string& command,
        const bool expected,
        const std::string& message)
    {
        const size_t historyBefore = chess.getCmdHistory().size();
        const std::string lastBefore = chess.getCmdLine();
        const bool actual = chess.command(command.c_str());

        std::ostringstream out;
        out << message << " [" << command << "]";
        expect(actual == expected, out.str());

        if (!expected) {
            expect(chess.getCmdHistory().size() == historyBefore,
                "invalid command keeps history size");
            expect(std::string(chess.getCmdLine()) == lastBefore,
                "invalid command keeps last command");
        }
    }
};

struct Harness {
    int cases = 0;
    int failedCases = 0;
    int checks = 0;
    int failedChecks = 0;

    void runCase(const std::string& name, const std::function<void(CaseContext&)>& fn)
    {
        std::cout << "\n== " << name << " ==\n";
        CaseContext ctx;
        fn(ctx);

        ++cases;
        checks += ctx.checks;
        failedChecks += ctx.failed;

        if (ctx.failed == 0) {
            std::cout << "  CASE PASS\n";
        }
        else {
            ++failedCases;
            std::cout << "  CASE FAIL (" << ctx.failed << "/" << ctx.checks << " failed)\n";
        }
    }

    int exitCode() const
    {
        return failedCases == 0 ? 0 : 1;
    }
};

void runRule0(Harness& harness)
{
    harness.runCase("rule0_opening_capture_and_reuse_allowed", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(0);
        chess.start();

        t.expectCommand(chess, "(0,0)->(0,1)", false, "opening move command must be rejected");
        t.expectCommand(chess, "-(0,0)", false, "opening capture command must be rejected");

        t.expectCommand(chess, "(0,7)", true, "player1 places first point");
        t.expectCommand(chess, "(0,2)", true, "player2 places first point");
        t.expectCommand(chess, "(0,0)", true, "player1 extends top line");
        t.expectCommand(chess, "(0,3)", true, "player2 places second point");
        t.expectCommand(chess, "(0,1)", true, "player1 completes a mill");

        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "mill switches to capture action");
        t.expect(chess.getPendingCaptures() == 1u, "single mill requires exactly one capture");

        t.expectCommand(chess, "-(0,2)", true, "capture succeeds");
        t.expect(chess.getAction() == NineChess::ACTION_PLACE, "opening resumes with place action");
        t.expect(chess.getTurn() == NineChess::PLAYER2, "turn switches to player2 after capture");
        t.expect(chess.getData().forbiddenBoard == 0u, "captured point is not forbidden in rule0");

        t.expectCommand(chess, "(0,2)", true, "captured point can be reused immediately");
        t.expect(chess.getWhosPiece(0, 2) == NineChess::PLAYER2, "player2 piece is back on the captured point");
    });

    harness.runCase("rule0_double_mill_only_one_capture", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(0);
        chess.start();

        t.expectCommand(chess, "(0,0)", true, "p1 setup 1");
        t.expectCommand(chess, "(0,2)", true, "p2 setup 1");
        t.expectCommand(chess, "(2,0)", true, "p1 setup 2");
        t.expectCommand(chess, "(2,2)", true, "p2 setup 2");
        t.expectCommand(chess, "(1,7)", true, "p1 setup 3");
        t.expectCommand(chess, "(0,4)", true, "p2 setup 3");
        t.expectCommand(chess, "(1,1)", true, "p1 setup 4");
        t.expectCommand(chess, "(2,4)", true, "p2 setup 4");
        t.expectCommand(chess, "(1,0)", true, "p1 creates a double mill by placement");

        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "double mill still enters capture action");
        t.expect(chess.getPendingCaptures() == 1u, "rule0 only allows one capture for a double mill");

        t.expectCommand(chess, "-(0,2)", true, "first capture succeeds");
        t.expect(chess.getAction() == NineChess::ACTION_PLACE, "capture phase ends after one capture");
        t.expect(chess.getTurn() == NineChess::PLAYER2, "turn moves to player2");
        t.expectCommand(chess, "-(2,2)", false, "second capture must be rejected in rule0");
    });

    harness.runCase("rule0_three_pieces_cannot_fly", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(0);
        setupMidgame(chess, NineChess::PLAYER1,
            toVector({ posOf(chess, 0, 0), posOf(chess, 0, 2), posOf(chess, 0, 4) }),
            toVector({ posOf(chess, 0, 7), posOf(chess, 0, 1), posOf(chess, 0, 3) }));

        t.expectCommand(chess, "(0,0)->(1,4)", false, "non-adjacent move must be rejected without flying");
        t.expect(chess.getWhosPiece(0, 0) == NineChess::PLAYER1, "source piece stays in place after rejected move");
        t.expect(chess.getWhosPiece(1, 4) == NineChess::NOBODY, "target point stays empty after rejected move");
        t.expect(chess.getTurn() == NineChess::PLAYER1, "turn does not change after rejected move");
    });

    harness.runCase("rule0_blocked_player_loses", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(0);
        setupMidgame(chess, NineChess::PLAYER1,
            toVector({
                posOf(chess, 0, 0), posOf(chess, 0, 2), posOf(chess, 0, 4),
                posOf(chess, 0, 6), posOf(chess, 1, 0)
            }),
            toVector({ posOf(chess, 0, 1), posOf(chess, 0, 3), posOf(chess, 0, 5) }));

        t.expectCommand(chess, "(1,0)->(1,1)", true, "player1 makes a quiet move");
        t.expect(chess.getPhase() == NineChess::GAME_OVER, "game ends when next player is blocked");
        t.expect(chess.getWinner() == NineChess::PLAYER1, "player1 wins because player2 is blocked");
    });
}

void runRule1(Harness& harness)
{
    harness.runCase("rule1_forbidden_point_blocks_reuse", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(1);
        chess.start();

        t.expectCommand(chess, "(0,7)", true, "player1 places first point");
        t.expectCommand(chess, "(0,2)", true, "player2 places first point");
        t.expectCommand(chess, "(0,0)", true, "player1 extends top line");
        t.expectCommand(chess, "(0,3)", true, "player2 places second point");
        t.expectCommand(chess, "(0,1)", true, "player1 completes a mill");
        t.expectCommand(chess, "-(0,2)", true, "capture succeeds");

        const int forbiddenPos = posOf(chess, 0, 2);
        t.expect((chess.getData().forbiddenBoard & bitOf(forbiddenPos)) != 0u,
            "captured point becomes forbidden during opening");
        t.expectCommand(chess, "(0,2)", false, "forbidden point cannot be reused immediately");
        t.expect(chess.getWhosPiece(0, 2) == NineChess::NOBODY, "forbidden point stays empty");
    });

    harness.runCase("rule1_diagonal_cross_ring_mill_is_valid", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(1);
        chess.start();

        t.expectCommand(chess, "(0,1)", true, "player1 takes inner diagonal point");
        t.expectCommand(chess, "(0,0)", true, "player2 reply");
        t.expectCommand(chess, "(1,1)", true, "player1 takes middle diagonal point");
        t.expectCommand(chess, "(0,2)", true, "player2 reply");
        t.expectCommand(chess, "(2,1)", true, "player1 completes diagonal cross-ring mill");

        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "diagonal mill triggers capture");
        t.expect(chess.getPendingCaptures() == 1u, "diagonal mill needs one capture");
        t.expectCommand(chess, "-(0,0)", true, "capture after diagonal mill succeeds");
    });

    harness.runCase("rule1_double_mill_requires_two_captures", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(1);
        chess.start();

        t.expectCommand(chess, "(0,0)", true, "p1 setup 1");
        t.expectCommand(chess, "(0,2)", true, "p2 setup 1");
        t.expectCommand(chess, "(2,0)", true, "p1 setup 2");
        t.expectCommand(chess, "(2,2)", true, "p2 setup 2");
        t.expectCommand(chess, "(1,7)", true, "p1 setup 3");
        t.expectCommand(chess, "(0,4)", true, "p2 setup 3");
        t.expectCommand(chess, "(1,1)", true, "p1 setup 4");
        t.expectCommand(chess, "(2,4)", true, "p2 setup 4");
        t.expectCommand(chess, "(1,0)", true, "p1 creates a double mill by placement");

        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "double mill enters capture");
        t.expect(chess.getPendingCaptures() == 2u, "rule1 allows two captures for a double mill");

        t.expectCommand(chess, "-(0,2)", true, "first capture succeeds");
        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "capture phase continues");
        t.expect(chess.getPendingCaptures() == 1u, "one capture remains after the first capture");

        t.expectCommand(chess, "-(2,2)", true, "second capture succeeds");
        t.expect(chess.getAction() == NineChess::ACTION_PLACE, "opening returns to placement after two captures");
        t.expect(chess.getTurn() == NineChess::PLAYER2, "turn moves to player2 after both captures");
    });

    harness.runCase("rule1_defender_moves_first_after_opening", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(1);
        setupOpeningCapture(chess, NineChess::PLAYER2, 0u,
            toVector({ posOf(chess, 0, 0), posOf(chess, 0, 2), posOf(chess, 0, 4), posOf(chess, 0, 6) }),
            toVector({ posOf(chess, 0, 1), posOf(chess, 0, 3), posOf(chess, 0, 5), posOf(chess, 0, 7) }));

        t.expectCommand(chess, "-(0,0)", true, "final opening capture succeeds");
        t.expect(chess.getPhase() == NineChess::GAME_MID, "game enters midgame after all pieces are placed");
        t.expect(chess.getAction() == NineChess::ACTION_CHOOSE, "midgame starts in choose action");
        t.expect(chess.getTurn() == NineChess::PLAYER2, "defender moves first in rule1");
        t.expect(chess.getWinner() == NineChess::NOBODY, "game continues without a winner");
    });
}

void runRule2(Harness& harness)
{
    harness.runCase("rule2_same_numbered_mill_cannot_repeat_capture", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(2);

        const std::vector<int> player1 = {
            posOf(chess, 0, 7),
            posOf(chess, 0, 0),
            posOf(chess, 0, 1)
        };
        const std::vector<int> player2 = {
            posOf(chess, 0, 3),
            posOf(chess, 0, 5),
            posOf(chess, 0, 6)
        };
        setupMidgame(chess, NineChess::PLAYER1, player1, player2, true);
        chess.getData().millHistory.push_back(makeMillKey(false, 0u, 0u, 1u, 2u));

        t.expectCommand(chess, "(0,1)->(0,2)", true, "player1 breaks the recorded mill");
        t.expectCommand(chess, "(0,3)->(0,4)", true, "player2 makes a quiet reply");
        t.expectCommand(chess, "(0,2)->(0,1)", true, "player1 reforms the same numbered mill");

        t.expect(chess.getAction() == NineChess::ACTION_CHOOSE, "reforming the same numbered mill does not enter capture");
        t.expect(chess.getPendingCaptures() == 0u, "no capture is granted for the repeated numbered mill");
        t.expect(chess.getTurn() == NineChess::PLAYER2, "turn passes to player2");
        t.expect(chess.getData().millHistory.size() == 1u, "mill history stays unchanged");
    });

    harness.runCase("rule2_blocked_turn_is_skipped", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(2);
        setupMidgame(chess, NineChess::PLAYER1,
            toVector({
                posOf(chess, 0, 0), posOf(chess, 0, 2), posOf(chess, 0, 4),
                posOf(chess, 0, 6), posOf(chess, 1, 0)
            }),
            toVector({ posOf(chess, 0, 1), posOf(chess, 0, 3), posOf(chess, 0, 5) }),
            true);

        t.expectCommand(chess, "(1,0)->(1,1)", true, "player1 makes a quiet move");
        t.expect(chess.getPhase() == NineChess::GAME_MID, "game stays in midgame");
        t.expect(chess.getWinner() == NineChess::NOBODY, "blocked player does not lose in rule2");
        t.expect(chess.getTurn() == NineChess::PLAYER1, "blocked turn is skipped back to player1");
        t.expect(chess.getAction() == NineChess::ACTION_CHOOSE, "game continues in choose action");
    });

    harness.runCase("rule2_double_mill_requires_two_captures", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(2);
        chess.start();

        t.expectCommand(chess, "(0,0)", true, "p1 setup 1");
        t.expectCommand(chess, "(0,2)", true, "p2 setup 1");
        t.expectCommand(chess, "(2,0)", true, "p1 setup 2");
        t.expectCommand(chess, "(2,2)", true, "p2 setup 2");
        t.expectCommand(chess, "(1,7)", true, "p1 setup 3");
        t.expectCommand(chess, "(0,4)", true, "p2 setup 3");
        t.expectCommand(chess, "(1,1)", true, "p1 setup 4");
        t.expectCommand(chess, "(2,4)", true, "p2 setup 4");
        t.expectCommand(chess, "(1,0)", true, "p1 creates a double mill by placement");

        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "double mill enters capture");
        t.expect(chess.getPendingCaptures() == 2u, "rule2 allows two captures for a double mill");

        t.expectCommand(chess, "-(0,2)", true, "first capture succeeds");
        t.expect(chess.getPendingCaptures() == 1u, "one capture remains after the first capture");
        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "capture continues after the first capture");

        t.expectCommand(chess, "-(2,2)", true, "second capture succeeds");
        t.expect(chess.getTurn() == NineChess::PLAYER2, "turn moves to player2 after both captures");
        t.expect(chess.getAction() == NineChess::ACTION_PLACE, "opening resumes with placement");
    });

    harness.runCase("rule2_three_pieces_still_cannot_fly", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(2);
        setupMidgame(chess, NineChess::PLAYER1,
            toVector({ posOf(chess, 0, 0), posOf(chess, 0, 2), posOf(chess, 0, 4) }),
            toVector({ posOf(chess, 0, 7), posOf(chess, 0, 1), posOf(chess, 0, 3) }),
            true);

        t.expectCommand(chess, "(0,0)->(1,4)", false, "non-adjacent move must be rejected in rule2");
        t.expect(chess.getWhosPiece(0, 0) == NineChess::PLAYER1, "source piece stays in place");
        t.expect(chess.getWhosPiece(1, 4) == NineChess::NOBODY, "target point stays empty");
    });

    harness.runCase("rule2_opening_quiet_reply_does_not_end_early", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(2);
        chess.start();

        const std::vector<std::string> commands = {
            "(0,7)", "(0,0)", "(0,1)", "(1,0)", "(1,1)", "(2,0)",
            "-(1,1)",
            "(1,1)", "(2,1)", "(1,2)", "(2,7)",
            "-(0,1)",
            "(1,3)",
            "-(2,0)",
            "(2,0)",
            "-(0,7)",
            "-(1,1)",
            "(1,1)",
            "-(2,0)",
            "(2,0)",
            "-(1,1)",
            "-(1,2)",
            "(1,2)",
            "(2,2)"
        };

        for (size_t i = 0; i < commands.size(); ++i) {
            std::ostringstream label;
            label << "replay command " << (i + 1u) << " succeeds";
            t.expectCommand(chess, commands[i], true, label.str());
        }

        t.expect(chess.getPhase() == NineChess::GAME_OPENING,
            "opening replay stays in opening after the quiet reply");
        t.expect(chess.getWinner() == NineChess::NOBODY,
            "quiet reply does not adjudicate a winner");
        t.expect(chess.getAction() == NineChess::ACTION_PLACE,
            "game returns to placement after the quiet reply");
        t.expect(chess.getTurn() == NineChess::PLAYER1,
            "turn passes back to player1 after player2 places");
        t.expect(chess.getPlayer1InHand() == 1u,
            "player1 still has one piece in hand");
        t.expect(chess.getPlayer2InHand() == 1u,
            "player2 still has one piece in hand");
        t.expect(chess.getPlayer1OnBoardCount() == 2u,
            "player1 still has two pieces on the board");
        t.expect(chess.getPlayer2OnBoardCount() == 6u,
            "player2 has six pieces on the board");
    });
}

void runRule3(Harness& harness)
{
    harness.runCase("rule3_three_pieces_can_fly", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(3);
        setupMidgame(chess, NineChess::PLAYER1,
            toVector({ posOf(chess, 0, 0), posOf(chess, 0, 2), posOf(chess, 0, 4) }),
            toVector({ posOf(chess, 0, 7), posOf(chess, 0, 1), posOf(chess, 0, 3) }));

        t.expectCommand(chess, "(0,0)->(1,4)", true, "player1 can fly to a non-adjacent point");
        t.expect(chess.getWhosPiece(0, 0) == NineChess::NOBODY, "source point becomes empty after flying");
        t.expect(chess.getWhosPiece(1, 4) == NineChess::PLAYER1, "target point receives the flying piece");
        t.expect(chess.getTurn() == NineChess::PLAYER2, "turn passes to player2 after flying");
    });

    harness.runCase("rule3_repeated_mill_can_capture_again", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(3);
        setupMidgame(chess, NineChess::PLAYER1,
            toVector({ posOf(chess, 0, 7), posOf(chess, 0, 0), posOf(chess, 0, 1) }),
            toVector({ posOf(chess, 0, 3), posOf(chess, 0, 5), posOf(chess, 0, 6) }));

        t.expectCommand(chess, "(0,1)->(0,2)", true, "player1 breaks an existing mill");
        t.expectCommand(chess, "(0,3)->(0,4)", true, "player2 makes a quiet reply");
        t.expectCommand(chess, "(0,2)->(0,1)", true, "player1 reforms the same mill");

        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "reforming the same mill still grants capture");
        t.expect(chess.getPendingCaptures() == 1u, "rule3 grants one capture for the repeated mill");

        t.expectCommand(chess, "-(0,4)", true, "capture after repeated mill succeeds");
        t.expect(chess.getPhase() == NineChess::GAME_OVER, "capturing the opponent down to two pieces ends the game");
        t.expect(chess.getWinner() == NineChess::PLAYER1, "player1 wins after reducing player2 to two pieces");
    });

    harness.runCase("rule3_double_mill_only_one_capture", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(3);
        chess.start();

        t.expectCommand(chess, "(0,0)", true, "p1 setup 1");
        t.expectCommand(chess, "(0,2)", true, "p2 setup 1");
        t.expectCommand(chess, "(2,0)", true, "p1 setup 2");
        t.expectCommand(chess, "(2,2)", true, "p2 setup 2");
        t.expectCommand(chess, "(1,7)", true, "p1 setup 3");
        t.expectCommand(chess, "(0,4)", true, "p2 setup 3");
        t.expectCommand(chess, "(1,1)", true, "p1 setup 4");
        t.expectCommand(chess, "(2,4)", true, "p2 setup 4");
        t.expectCommand(chess, "(1,0)", true, "p1 creates a double mill by placement");

        t.expect(chess.getAction() == NineChess::ACTION_CAPTURE, "double mill enters capture");
        t.expect(chess.getPendingCaptures() == 1u, "rule3 only allows one capture for a double mill");

        t.expectCommand(chess, "-(0,2)", true, "first capture succeeds");
        t.expectCommand(chess, "-(2,2)", false, "second capture must be rejected in rule3");
    });

    harness.runCase("rule3_blocked_player_loses", [](CaseContext& t) {
        NineChess chess;
        chess.setRule(3);
        setupMidgame(chess, NineChess::PLAYER1,
            toVector({
                posOf(chess, 0, 0), posOf(chess, 0, 2), posOf(chess, 0, 4),
                posOf(chess, 1, 6), posOf(chess, 2, 2)
            }),
            toVector({
                posOf(chess, 0, 1), posOf(chess, 0, 3),
                posOf(chess, 0, 5), posOf(chess, 0, 7)
            }));

        t.expectCommand(chess, "(1,6)->(0,6)", true, "player1 closes the last escape square");
        t.expect(chess.getPhase() == NineChess::GAME_OVER, "game ends when next player is blocked");
        t.expect(chess.getWinner() == NineChess::PLAYER1, "player1 wins because player2 is blocked");
    });
}

int parseRuleIndex(const char* text)
{
    if (text == nullptr) {
        return -1;
    }

    try {
        size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        if (consumed != std::string(text).size()) {
            return -1;
        }
        return value;
    }
    catch (...) {
        return -1;
    }
}

void printUsage()
{
    std::cout << "Usage: RuleHarness.exe <rule-index>\n"
              << "  0 = ChengSanQi\n"
              << "  1 = DaSanQi\n"
              << "  2 = JiuLianQi\n"
              << "  3 = Morris\n";
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printUsage();
        return 2;
    }

    const int ruleIndex = parseRuleIndex(argv[1]);
    Harness harness;

    switch (ruleIndex)
    {
    case 0:
        runRule0(harness);
        break;
    case 1:
        runRule1(harness);
        break;
    case 2:
        runRule2(harness);
        break;
    case 3:
        runRule3(harness);
        break;
    default:
        printUsage();
        return 2;
    }

    std::cout << "\n==== Summary ====\n";
    std::cout << "Cases:  " << (harness.cases - harness.failedCases) << "/" << harness.cases << " passed\n";
    std::cout << "Checks: " << (harness.checks - harness.failedChecks) << "/" << harness.checks << " passed\n";

    if (harness.failedCases == 0) {
        std::cout << "Overall result: PASS\n";
    }
    else {
        std::cout << "Overall result: FAIL\n";
    }

    return harness.exitCode();
}
