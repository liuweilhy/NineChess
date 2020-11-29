/*
  FlutterMill, a mill game playing frontend derived from ChessRoad
  Copyright (C) 2019 He Zhaoyun (ChessRoad author)
  Copyright (C) 2019-2020 Calcitem <calcitem@outlook.com>

  FlutterMill is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FlutterMill is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import 'package:flutter/material.dart';
import 'package:sanmill/engine/analyze.dart';
import 'package:sanmill/engine/engine.dart';
import 'package:sanmill/engine/native_engine.dart';
import 'package:sanmill/generated/l10n.dart';
import 'package:sanmill/main.dart';
import 'package:sanmill/mill/game.dart';
import 'package:sanmill/mill/mill.dart';
import 'package:sanmill/mill/types.dart';
import 'package:sanmill/services/player.dart';
import 'package:sanmill/style/colors.dart';
import 'package:sanmill/style/toast.dart';

import 'board.dart';
import 'settings_page.dart';

class GamePage extends StatefulWidget {
  //
  static double boardMargin = 10.0, screenPaddingH = 10.0;

  final EngineType engineType;
  final AiEngine engine;

  GamePage(this.engineType) : engine = NativeEngine();

  @override
  _GamePageState createState() => _GamePageState();
}

class _GamePageState extends State<GamePage> {
  //
  String _status = '';
  bool _searching = false;

  @override
  void initState() {
    super.initState();
    Game.shared.init();
    widget.engine.startup();
  }

  changeStatus(String status) => setState(() => _status = status);

  void showTips() {
    final winner = Game.shared.position.winner;

    switch (winner) {
      case Color.nobody:
        if (Game.shared.position.phase == Phase.placing) {
          changeStatus(S.of(context).tipPlace);
        } else if (Game.shared.position.phase == Phase.moving) {
          changeStatus(S.of(context).tipMove);
        }
        break;
      case Color.black:
        changeStatus(S.of(context).blackWin);
        showGameResult(GameResult.win);
        break;
      case Color.white:
        changeStatus(S.of(context).whiteWin);
        showGameResult(GameResult.lose);
        break;
      case Color.draw:
        changeStatus(S.of(context).draw);
        showGameResult(GameResult.draw);
        break;
    }
  }

  onBoardTap(BuildContext context, int index) {
    final position = Game.shared.position;

    int sq = indexToSquare[index];

    if (sq == null) {
      print("putPiece skip index: $index");
      return;
    }

    if (Game.shared.isAiToMove() || Game.shared.aiIsSearching()) {
      return false;
    }

    if (position.phase == Phase.ready) {
      Game.shared.start();
    }

    bool ret = false;

    switch (position.action) {
      case Act.place:
        if (position.putPiece(sq)) {
          if (position.action == Act.remove) {
            //Audios.playTone('mill.mp3');
            changeStatus(S.of(context).tipRemove);
          } else {
            //Audios.playTone('put.mp3');
            changeStatus(S.of(context).tipPlaced);
          }
          ret = true;
          print("putPiece: [$sq]");
          break;
        } else {
          print("putPiece: skip [$sq]");
          changeStatus(S.of(context).tipBanPlace);
        }

        // If cannot move, retry select, do not break
        //[[fallthrough]];
        continue select;
      select:
      case Act.select:
        if (position.selectPiece(sq)) {
          //Audios.playTone('select.mp3');
          Game.shared.select(index);
          ret = true;
          print("selectPiece: [$sq]");
          changeStatus(S.of(context).tipPlace);
        } else {
          //Audios.playTone('banned.mp3');
          print("selectPiece: skip [$sq]");
          changeStatus(S.of(context).tipSelectWrong);
        }
        break;

      case Act.remove:
        if (position.removePiece(sq)) {
          //Audios.playTone('remove.mp3');
          ret = true;
          print("removePiece: [$sq]");
          changeStatus(S.of(context).tipRemoved);
        } else {
          //Audios.playTone('banned.mp3');
          print("removePiece: skip [$sq]");
          changeStatus(S.of(context).tipBanRemove);
        }
        break;

      default:
        break;
    }

    if (ret) {
      Game.shared.sideToMove = position.sideToMove();
      Game.shared.moveHistory.add(position.cmdline);

      // TODO: Need Others?
      // Increment ply counters. In particular, rule50 will be reset to zero later on
      // in case of a capture.
      ++position.gamePly;
      ++position.rule50;
      ++position.pliesFromNull;

      //position.move = m;

      Move m = Move(position.cmdline);
      position.recorder.moveIn(m, position);

      setState(() {});

      if (position.winner == Color.nobody) {
        engineToGo();
      } else {
        showTips();
      }
    }

    Game.shared.sideToMove = position.sideToMove();

    setState(() {});

    return ret;
  }

  engineToGo() async {
    // TODO
    while (Game.shared.position.winner == Color.nobody &&
        Game.shared.isAiToMove()) {
      changeStatus(S.of(context).thinking);

      final response = await widget.engine.search(Game.shared.position);

      if (response.type == 'move') {
        Move mv = response.value;
        final Move move = new Move(mv.move);

        //Battle.shared.move = move;
        Game.shared.doMove(move.move);
        showTips();
      } else {
        changeStatus('Error: ${response.type}');
      }
    }
  }

  newGame() {
    confirm() {
      Navigator.of(context).pop();
      Game.shared.newGame();
      changeStatus(S.of(context).gameStarted);

      if (Game.shared.isAiToMove()) {
        print("New Game: AI's turn.");
        engineToGo();
      }
    }

    cancel() => Navigator.of(context).pop();

    showDialog(
      context: context,
      builder: (BuildContext context) {
        return AlertDialog(
          title: Text(S.of(context).newGame,
              style: TextStyle(color: UIColors.primaryColor)),
          content:
              SingleChildScrollView(child: Text(S.of(context).restartGame)),
          actions: <Widget>[
            FlatButton(child: Text(S.of(context).ok), onPressed: confirm),
            FlatButton(child: Text(S.of(context).cancel), onPressed: cancel),
          ],
        );
      },
    );
  }

  analyzePosition() async {
    //
    Toast.toast(context,
        msg: S.of(context).analyzing, position: ToastPostion.bottom);

    setState(() => _searching = true);

    try {} catch (e) {
      Toast.toast(context,
          msg: S.of(context).error + ": $e", position: ToastPostion.bottom);
    } finally {
      setState(() => _searching = false);
    }
  }

  showAnalyzeItems(
    BuildContext context, {
    String title,
    List<AnalyzeItem> items,
    Function(AnalyzeItem item) callback,
  }) {
    final List<Widget> children = [];

    for (var item in items) {
      children.add(
        ListTile(
          title: Text(item.moveName, style: TextStyle(fontSize: 18)),
          subtitle: Text(S.of(context).winRate + ": ${item.winRate}%"),
          trailing: Text(S.of(context).score + ": ${item.score}'"),
          onTap: () => callback(item),
        ),
      );
      children.add(Divider());
    }

    children.insert(0, SizedBox(height: 10));
    children.add(SizedBox(height: 56));

    showModalBottomSheet(
      context: context,
      builder: (BuildContext context) => SingleChildScrollView(
        child: Column(mainAxisSize: MainAxisSize.min, children: children),
      ),
    );
  }

  String getGameOverReasonString(GameOverReason reason, String winner) {
    String loseReasonStr;
    String winnerStr =
        winner == Color.black ? S.of(context).black : S.of(context).white;
    String loserStr =
        winner == Color.black ? S.of(context).white : S.of(context).black;

    switch (Game.shared.position.gameOverReason) {
      case GameOverReason.loseReasonlessThanThree:
        loseReasonStr = loserStr + S.of(context).loseReasonlessThanThree;
        break;
      case GameOverReason.loseReasonResign:
        loseReasonStr = loserStr + S.of(context).loseReasonResign;
        break;
      case GameOverReason.loseReasonNoWay:
        loseReasonStr = loserStr + S.of(context).loseReasonNoWay;
        break;
      case GameOverReason.loseReasonBoardIsFull:
        loseReasonStr = loserStr + S.of(context).loseReasonBoardIsFull;
        break;
      case GameOverReason.loseReasonTimeOver:
        loseReasonStr = loserStr + S.of(context).loseReasonTimeOver;
        break;
      case GameOverReason.drawReasonRule50:
        loseReasonStr = S.of(context).drawReasonRule50;
        break;
      case GameOverReason.drawReasonBoardIsFull:
        loseReasonStr = S.of(context).drawReasonBoardIsFull;
        break;
      case GameOverReason.drawReasonThreefoldRepetition:
        loseReasonStr = S.of(context).drawReasonThreefoldRepetition;
        break;
      default:
        loseReasonStr = S.of(context).gameOverUnknownReason;
        break;
    }

    return loseReasonStr;
  }

  void showGameResult(GameResult result) {
    //
    Game.shared.position.result = result;

    switch (result) {
      case GameResult.win:
        //Audios.playTone('win.mp3');
        break;
      case GameResult.lose:
        //Audios.playTone('lose.mp3');
        break;
      case GameResult.draw:
        break;
      default:
        break;
    }

    Map<GameResult, String> retMap = {
      GameResult.win: S.of(context).youWin,
      GameResult.lose: S.of(context).youLose,
      GameResult.draw: S.of(context).draw
    };

    var dialogTitle = retMap[result];

    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (BuildContext context) {
        return AlertDialog(
          title:
              Text(dialogTitle, style: TextStyle(color: UIColors.primaryColor)),
          content: Text(getGameOverReasonString(
              Game.shared.position.gameOverReason,
              Game.shared.position.winner)),
          actions: <Widget>[
            FlatButton(
                child: Text(S.of(context).ok),
                onPressed: () => Navigator.of(context).pop()),
          ],
        );
      },
    );

    if (result == GameResult.win) {
      if (widget.engineType == EngineType.humanVsCloud)
        Player.shared.increaseWinCloudEngine();
      else
        Player.shared.increaseWinAi();
    }
  }

  void calcScreenPaddingH() {
    //
    // when screen's height/width rate is less than 16/9, limit witdh of board
    final windowSize = MediaQuery.of(context).size;
    double height = windowSize.height, width = windowSize.width;

    if (height / width < 16.0 / 9.0) {
      width = height * 9 / 16;
      GamePage.screenPaddingH =
          (windowSize.width - width) / 2 - GamePage.boardMargin;
    }
  }

  Widget createPageHeader() {
    Map<EngineType, String> engineTypeToString = {
      EngineType.humanVsAi: S.of(context).humanVsAi,
      EngineType.humanVsHuman: S.of(context).humanVsHuman,
      EngineType.aiVsAi: S.of(context).aiVsAi,
      EngineType.humanVsCloud: S.of(context).humanVsCloud,
    };

    final titleStyle =
        TextStyle(fontSize: 28, color: UIColors.darkTextPrimaryColor);
    final subTitleStyle =
        TextStyle(fontSize: 16, color: UIColors.darkTextSecondaryColor);

    return Container(
      margin: EdgeInsets.only(top: SanmillApp.StatusBarHeight),
      child: Column(
        children: <Widget>[
          Row(
            children: <Widget>[
              IconButton(
                icon: Icon(Icons.arrow_back,
                    color: UIColors.darkTextPrimaryColor),
                onPressed: () => Navigator.of(context).pop(),
              ),
              Expanded(child: SizedBox()),
              Hero(tag: 'logo', child: Image.asset('images/logo-mini.png')),
              SizedBox(width: 10),
              Text(engineTypeToString[widget.engineType], style: titleStyle),
              Expanded(child: SizedBox()),
              IconButton(
                icon:
                    Icon(Icons.settings, color: UIColors.darkTextPrimaryColor),
                onPressed: () => Navigator.of(context).push(
                  MaterialPageRoute(builder: (context) => SettingsPage()),
                ),
              ),
            ],
          ),
          Container(
            height: 4,
            width: 180,
            margin: EdgeInsets.only(bottom: 10),
            decoration: BoxDecoration(
              color: UIColors.boardBackgroundColor,
              borderRadius: BorderRadius.circular(2),
            ),
          ),
          Container(
            padding: EdgeInsets.symmetric(horizontal: 16),
            child: Text(_status, maxLines: 1, style: subTitleStyle),
          ),
        ],
      ),
    );
  }

  Widget createBoard() {
    //
    return Container(
      margin: EdgeInsets.symmetric(
        horizontal: GamePage.screenPaddingH,
        vertical: GamePage.boardMargin,
      ),
      child: Board(
        width: MediaQuery.of(context).size.width - GamePage.screenPaddingH * 2,
        onBoardTap: onBoardTap,
      ),
    );
  }

  Widget createOperatorBar() {
    //
    final buttonStyle = TextStyle(color: UIColors.primaryColor, fontSize: 20);

    return Container(
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(5),
        color: UIColors.boardBackgroundColor,
      ),
      margin: EdgeInsets.symmetric(horizontal: GamePage.screenPaddingH),
      padding: EdgeInsets.symmetric(vertical: 2),
      child: Row(children: <Widget>[
        Expanded(child: SizedBox()),
        FlatButton(
            child: Text(S.of(context).newGame, style: buttonStyle),
            onPressed: newGame),
        Expanded(child: SizedBox()),
        FlatButton(
          child: Text(S.of(context).regret, style: buttonStyle),
          onPressed: () {
            Game.shared.regret(steps: 2);
            setState(() {});
          },
        ),
        Expanded(child: SizedBox()),
        FlatButton(
          child: Text(S.of(context).analyze, style: buttonStyle),
          onPressed: _searching ? null : analyzePosition,
        ),
        Expanded(child: SizedBox()),
      ]),
    );
  }

  Widget buildFooter() {
    //
    final size = MediaQuery.of(context).size;

    final manualText = Game.shared.position.manualText;

    if (size.height / size.width > 16 / 9) {
      return buildManualPanel(manualText);
    } else {
      return buildExpandableRecordPanel(manualText);
    }
  }

  Widget buildManualPanel(String text) {
    //
    final manualStyle = TextStyle(
      fontSize: 18,
      color: UIColors.darkTextSecondaryColor,
      height: 1.5,
    );

    return Expanded(
      child: Container(
        margin: EdgeInsets.symmetric(vertical: 16),
        child: SingleChildScrollView(child: Text(text, style: manualStyle)),
      ),
    );
  }

  Widget buildExpandableRecordPanel(String text) {
    //
    final manualStyle = TextStyle(fontSize: 18, height: 1.5);

    return Expanded(
      child: IconButton(
        icon: Icon(Icons.expand_less, color: UIColors.darkTextPrimaryColor),
        onPressed: () => showDialog(
          context: context,
          barrierDismissible: false,
          builder: (BuildContext context) {
            return AlertDialog(
              title: Text(S.of(context).gameRecord,
                  style: TextStyle(color: UIColors.primaryColor)),
              content:
                  SingleChildScrollView(child: Text(text, style: manualStyle)),
              actions: <Widget>[
                FlatButton(
                  child: Text(S.of(context).ok),
                  onPressed: () => Navigator.of(context).pop(),
                ),
              ],
            );
          },
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    //
    calcScreenPaddingH();

    final header = createPageHeader();
    final board = createBoard();
    final operatorBar = createOperatorBar();
    final footer = buildFooter();

    return Scaffold(
      backgroundColor: UIColors.darkBackgroundColor,
      body: Column(children: <Widget>[header, board, operatorBar, footer]),
    );
  }

  @override
  void dispose() {
    widget.engine.shutdown();
    super.dispose();
  }
}
