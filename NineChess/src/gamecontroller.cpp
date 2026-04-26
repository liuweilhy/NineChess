/****************************************************************************
** GameController - 游戏控制器实现
**
** 负责游戏流程控制、棋子显示、AI管理、计时和音效
****************************************************************************/

#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QTimer>
#include <QSoundEffect>
#include <QDebug>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QVector>
#include <algorithm>
#include <chrono>
#include "gamecontroller.h"
#include "boarditem.h"

namespace {

constexpr int kPieceInHand = -1;
constexpr int kPieceCaptured = -2;
constexpr int kPieceUnassigned = -3;

QVector<int> collectBoardPositions(uint32_t boardMask)
{
    QVector<int> positions;
    for (int pos = 0; pos < NineChess::BOARD_SIZE; ++pos) {
        if ((boardMask & (1u << pos)) != 0u) {
            positions.append(pos);
        }
    }
    return positions;
}

void ensureDisplaySlots(QVector<int>& displaySlots, int piecesPerSide)
{
    if (displaySlots.size() != piecesPerSide) {
        displaySlots = QVector<int>(piecesPerSide, kPieceInHand);
    }
}

void assignSlotsByPreviousState(QVector<int>& nextSlots,
    const QVector<int>& previousSlots,
    int previousState,
    int newState,
    int& remaining)
{
    if (remaining <= 0) {
        return;
    }

    for (int slot = 0; slot < nextSlots.size() && remaining > 0; ++slot) {
        if (nextSlots[slot] == kPieceUnassigned && previousSlots[slot] == previousState) {
            nextSlots[slot] = newState;
            --remaining;
        }
    }
}

void buildUnnumberedDisplaySlots(QVector<int>& previousSlots,
    uint32_t previousBoard,
    uint32_t currentBoard,
    int inHandCount,
    int piecesPerSide,
    QVector<int>& nextSlots)
{
    ensureDisplaySlots(previousSlots, piecesPerSide);
    nextSlots = QVector<int>(piecesPerSide, kPieceUnassigned);

    const QVector<int> currentBoardPos = collectBoardPositions(currentBoard);
    bool assignedBoardPos[NineChess::BOARD_SIZE] = {};
    const uint32_t removedBoard = previousBoard & ~currentBoard;
    const uint32_t addedBoard = currentBoard & ~previousBoard;

    // 前后局面各只有一个出点和一个入点时，说明只有一颗棋子真的移动了。
    // 这时优先让同一个前台棋子从旧点动画到新点。
    if (removedBoard != 0u
        && addedBoard != 0u
        && (removedBoard & (removedBoard - 1u)) == 0u
        && (addedBoard & (addedBoard - 1u)) == 0u) {
        const int fromPos = CTZ32(removedBoard);
        const int toPos = CTZ32(addedBoard);
        for (int slot = 0; slot < piecesPerSide; ++slot) {
            if (previousSlots[slot] == fromPos) {
                nextSlots[slot] = toPos;
                assignedBoardPos[toPos] = true;
                break;
            }
        }
    }

    // 盘上未变化的棋子保持原槽位不动。
    for (int slot = 0; slot < piecesPerSide; ++slot) {
        const int state = previousSlots[slot];
        if (state >= 0
            && nextSlots[slot] == kPieceUnassigned
            && (currentBoard & (1u << state)) != 0u) {
            nextSlots[slot] = state;
            assignedBoardPos[state] = true;
        }
    }

    QVector<int> pendingBoardPos;
    for (int pos : currentBoardPos) {
        if (!assignedBoardPos[pos]) {
            pendingBoardPos.append(pos);
        }
    }

    // 盘上消失后又需要重新分配的槽位，优先继续承担盘上棋子的动画。
    for (int slot = 0; slot < piecesPerSide && !pendingBoardPos.isEmpty(); ++slot) {
        if (nextSlots[slot] == kPieceUnassigned && previousSlots[slot] >= 0) {
            nextSlots[slot] = pendingBoardPos.takeFirst();
        }
    }

    // 新落子的动画优先从手牌区拿一颗前台棋子出来。
    for (int slot = 0; slot < piecesPerSide && !pendingBoardPos.isEmpty(); ++slot) {
        if (nextSlots[slot] == kPieceUnassigned && previousSlots[slot] == kPieceInHand) {
            nextSlots[slot] = pendingBoardPos.takeFirst();
        }
    }

    // 历史回放等跨步跳转时，剩余的新盘上棋子再从墓地区补位。
    for (int slot = 0; slot < piecesPerSide && !pendingBoardPos.isEmpty(); ++slot) {
        if (nextSlots[slot] == kPieceUnassigned && previousSlots[slot] == kPieceCaptured) {
            nextSlots[slot] = pendingBoardPos.takeFirst();
        }
    }

    for (int slot = 0; slot < piecesPerSide && !pendingBoardPos.isEmpty(); ++slot) {
        if (nextSlots[slot] == kPieceUnassigned) {
            nextSlots[slot] = pendingBoardPos.takeFirst();
        }
    }

    const int boardCount = currentBoardPos.size();
    const int handCount = std::max(0, std::min(inHandCount, piecesPerSide - boardCount));
    const int capturedCount = std::max(0, piecesPerSide - boardCount - handCount);
    int remainingCaptured = capturedCount;
    int remainingHand = handCount;

    // 先尽量保留原本就在墓地/手牌区的前台棋子，减少无意义的跳动。
    assignSlotsByPreviousState(nextSlots, previousSlots, kPieceCaptured, kPieceCaptured, remainingCaptured);
    assignSlotsByPreviousState(nextSlots, previousSlots, kPieceInHand, kPieceInHand, remainingHand);

    // 其余从棋盘下来的槽位，再按当前局面需要分配到墓地或手牌区。
    for (int slot = 0; slot < piecesPerSide; ++slot) {
        if (nextSlots[slot] != kPieceUnassigned) {
            continue;
        }
        if (remainingCaptured > 0) {
            nextSlots[slot] = kPieceCaptured;
            --remainingCaptured;
        }
        else {
            nextSlots[slot] = kPieceInHand;
            if (remainingHand > 0) {
                --remainingHand;
            }
        }
    }
}

} // namespace

GameController::GameController(GameScene &scene, QObject *parent) : QObject(parent),
    scene(scene),
    currentPiece(nullptr),
    player1DisplayBoard(0),
    player2DisplayBoard(0),
    currentRow(-1),
    isEditing(false),
    isInverted(false),
    isEngine1(false),
    isEngine2(false),
    hasAnimation(true),
    durationTime(250),
    hasSound(true),
    timeID(0),
    ruleNo(-1),
    timeLimit(0),
    stepsLimit(120),
    displayTime1MS(0),
    displayTime2MS(0),
    player1ElapsedMS(0),
    player2ElapsedMS(0),
    turnStartTimeMS(0),
    forcedAiTimeoutTurn(NineChess::NOBODY),
    ai1(1),
    ai2(2)
{
    // 已在view的样式表中添加背景，scene中不用添加背景
    // 区别在于，view中的背景不随视图变换而变换，scene中的背景随视图变换而变换
    //scene.setBackgroundBrush(QPixmap(":/image/resources/image/background.png"));

    // 音频缓存
    soundCache["capture"] = new QSoundEffect();
    soundCache["choose"] = new QSoundEffect();
    soundCache["drog"] = new QSoundEffect();
    soundCache["forbidden"] = new QSoundEffect();
    soundCache["loss"] = new QSoundEffect();
    soundCache["move"] = new QSoundEffect();
    soundCache["newgame"] = new QSoundEffect();
    soundCache["remove"] = new QSoundEffect();
    soundCache["warning"] = new QSoundEffect();
    soundCache["win"] = new QSoundEffect();
    soundCache["capture"]->setSource(QUrl("qrc:/sound/resources/sound/capture.wav"));
    soundCache["choose"]->setSource(QUrl("qrc:/sound/resources/sound/choose.wav"));
    soundCache["drog"]->setSource(QUrl("qrc:/sound/resources/sound/drog.wav"));
    soundCache["forbidden"]->setSource(QUrl("qrc:/sound/resources/sound/forbidden.wav"));
    soundCache["loss"]->setSource(QUrl("qrc:/sound/resources/sound/loss.wav"));
    soundCache["move"]->setSource(QUrl("qrc:/sound/resources/sound/move.wav"));
    soundCache["newgame"]->setSource(QUrl("qrc:/sound/resources/sound/newgame.wav"));
    soundCache["remove"]->setSource(QUrl("qrc:/sound/resources/sound/remove.wav"));
    soundCache["warning"]->setSource(QUrl("qrc:/sound/resources/sound/warning.wav"));
    soundCache["win"]->setSource(QUrl("qrc:/sound/resources/sound/win.wav"));

    gameReset();
    
    connect(&ai1, &AiThread::calcStarted, this, &GameController::onAiCalcStarted);
    connect(&ai2, &AiThread::calcStarted, this, &GameController::onAiCalcStarted);

    // 关联AI和控制器的招法命令行
    connect(&ai1, SIGNAL(command(const QString &, bool)),
        this, SLOT(command(const QString &, bool)));
    connect(&ai2, SIGNAL(command(const QString &, bool)),
        this, SLOT(command(const QString &, bool)));

    // 安装事件过滤器监视scene的各个事件，由于我重载了QGraphicsScene，相关事件在重载函数中已设定，不必安装监视器。
    //scene.installEventFilter(this);
}

GameController::~GameController()
{
    // 停止计时器
    if (timeID != 0)
        killTimer(timeID);
    // 停掉线程
    ai1.stop();
    ai2.stop();
    ai1.wait();
    ai2.wait();
}

// ==================== 菜单栏数据 ====================
const QMap<int, QStringList> GameController::getActions()
{
    // 主窗口更新菜单栏
    // 之所以不用信号和槽的模式，是因为发信号的时候槽还来不及关联
    QMap<int, QStringList> actions;
    for (int i = 0; i < NineChess::RULE_COUNT; i++)
    {
        // QMap的key存放int索引值，value存放规则名称和规则提示
        QStringList strlist;
        strlist.append(tr(NineChess::rules[i].name));
        strlist.append(tr(NineChess::rules[i].description));
        actions.insert(i, strlist);
    }
    return actions;
}

int64_t GameController::currentTimeMS()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

const AiThread* GameController::aiSourceFromSender() const
{
    const QObject *source = sender();
    if (source == &ai1) {
        return &ai1;
    }
    if (source == &ai2) {
        return &ai2;
    }
    return nullptr;
}

GameController::AiDispatchState& GameController::aiDispatchState(const AiThread* sourceAi)
{
    return sourceAi == &ai2 ? aiDispatch2 : aiDispatch1;
}

bool GameController::executeCommandInternal(const QString &cmd, bool update, const AiThread *sourceAi)
{
    if (sourceAi == &ai1) {
        if (!isEngine1 || chess.getTurn() != NineChess::PLAYER1) {
            return false;
        }
    }
    else if (sourceAi == &ai2) {
        if (!isEngine2 || chess.getTurn() != NineChess::PLAYER2) {
            return false;
        }
    }

    if (chess.getPhase() == NineChess::GAME_NOTSTARTED) {
        if (update) {
            gameStart();
        }
    }

    const NineChess::Players previousTurn = chess.getTurn();
    const uint16_t previousStatus = chess.getStatus();
    const int32_t previousSelectedPos = chess.getCurrentPos();
    const SoundAction soundAction = soundActionFromCommand(cmd, previousStatus);

    if (!chess.command(cmd.toStdString().c_str())) {
        return false;
    }

    advanceGameStateRevision();

    if (update) {
        if (!applyStepLimit(previousTurn)
            && (chess.whoWin() != NineChess::NOBODY || chess.getTurn() != previousTurn)) {
            finishTurnClock(previousTurn);
        }
    }

    syncManualListFromChess();
    message = QString::fromStdString(chess.getTip());
    emit statusBarChanged(message);

    if (update && (&chess == &(this->chess))) {
        syncAiState();
    }

    if (update) {
        playActionSound(soundAction, true, previousStatus, previousSelectedPos, &chess);
        updateScence();
    }
    return true;
}

#if 0

    // 记录执行前的动作类型
    // 如果未开局则开局
    if (chess.getPhase() == NineChess::GAME_NOTSTARTED) {
        if (update) {
            gameStart();
        }
    }

    const NineChess::Players previousTurn = chess.getTurn();

    if (!chess.command(cmd.toStdString().c_str())) {
        return false;
    }

    advanceGameStateRevision();

    // 当前状态
}

    uint16_t flags = chess.getFlags();

    if (update) {
        if (!applyStepLimit(previousTurn)
            && (chess.whoWin() != NineChess::NOBODY || chess.getTurn() != previousTurn)) {
            finishTurnClock(previousTurn);
        }
    }

    syncManualListFromChess();
    message = QString::fromStdString(chess.getTip());
    emit statusBarChanged(message);

    if (update && (&chess == &(this->chess))) {
        syncAiState();
    }

    if (update) {
        playActionSound(chess.whoWin() != NineChess::NOBODY ? NineChess::GAME_OVER : flags);
        updateScence();
    }
    return true;
}

#endif

void GameController::onAiCalcStarted()
{
    const AiThread *sourceAi = aiSourceFromSender();
    if (sourceAi == nullptr) {
        return;
    }

    AiDispatchState &state = aiDispatchState(sourceAi);
    state.calcStartedMS = currentTimeMS();
    state.calcRevision = gameStateRevision;
    ++state.calcSequence;
    state.hasPendingCommand = false;
}

void GameController::dispatchPendingAiCommand(const AiThread *sourceAi, uint64_t sequence)
{
    if (sourceAi == nullptr) {
        return;
    }

    AiDispatchState &state = aiDispatchState(sourceAi);
    if (!state.hasPendingCommand || state.pendingSequence != sequence) {
        return;
    }

    const QString cmd = state.pendingCommand;
    const bool update = state.pendingUpdate;
    state.hasPendingCommand = false;

    if (state.calcRevision != gameStateRevision) {
        return;
    }

    executeCommandInternal(cmd, update, sourceAi);
}

void GameController::invalidatePendingAiCommands()
{
    aiDispatch1.hasPendingCommand = false;
    aiDispatch2.hasPendingCommand = false;
}

void GameController::advanceGameStateRevision()
{
    ++gameStateRevision;
    invalidatePendingAiCommands();
}

void GameController::resetClockState()
{
    player1ElapsedMS = 0;
    player2ElapsedMS = 0;
    turnStartTimeMS = 0;
    forcedAiTimeoutTurn = NineChess::NOBODY;
}

void GameController::restartTurnClock()
{
    if (chess.getPhase() == NineChess::GAME_OPENING || chess.getPhase() == NineChess::GAME_MID) {
        turnStartTimeMS = currentTimeMS();
    }
    else {
        turnStartTimeMS = 0;
    }
    forcedAiTimeoutTurn = NineChess::NOBODY;
}

void GameController::finishTurnClock(NineChess::Players finishedTurn)
{
    if (finishedTurn != NineChess::PLAYER1 && finishedTurn != NineChess::PLAYER2) {
        restartTurnClock();
        return;
    }

    const int64_t now = currentTimeMS();
    if (turnStartTimeMS > 0) {
        const int64_t delta = std::max<int64_t>(0, now - turnStartTimeMS);
        if (finishedTurn == NineChess::PLAYER1) {
            player1ElapsedMS += delta;
        }
        else {
            player2ElapsedMS += delta;
        }
    }

    if (chess.getPhase() == NineChess::GAME_OPENING || chess.getPhase() == NineChess::GAME_MID) {
        turnStartTimeMS = now;
    }
    else {
        turnStartTimeMS = 0;
    }
    forcedAiTimeoutTurn = NineChess::NOBODY;
}

void GameController::getElapsedTimesMS(int &elapsed1, int &elapsed2) const
{
    int64_t value1 = player1ElapsedMS;
    int64_t value2 = player2ElapsedMS;

    if (turnStartTimeMS > 0
        && (chess.getPhase() == NineChess::GAME_OPENING || chess.getPhase() == NineChess::GAME_MID)) {
        const int64_t delta = std::max<int64_t>(0, currentTimeMS() - turnStartTimeMS);
        if (chess.getTurn() == NineChess::PLAYER1) {
            value1 += delta;
        }
        else if (chess.getTurn() == NineChess::PLAYER2) {
            value2 += delta;
        }
    }

    elapsed1 = static_cast<int>(value1);
    elapsed2 = static_cast<int>(value2);
}

void GameController::refreshTimeDisplays()
{
    int elapsed1 = 0;
    int elapsed2 = 0;
    getElapsedTimesMS(elapsed1, elapsed2);

    if (timeLimit > 0) {
        displayTime1MS = std::max(0, timeLimit * 60000 - elapsed1);
        displayTime2MS = std::max(0, timeLimit * 60000 - elapsed2);
    }
    else {
        displayTime1MS = elapsed1;
        displayTime2MS = elapsed2;
    }

    const QTime qt1 = QTime(0, 0, 0, 0).addMSecs(displayTime1MS);
    const QTime qt2 = QTime(0, 0, 0, 0).addMSecs(displayTime2MS);
    emit time1Changed(qt1.toString("mm:ss.zzz"));
    emit time2Changed(qt2.toString("mm:ss.zzz"));
}

void GameController::syncManualListFromChess()
{
    const std::vector<std::string>* cmdList = chess.getCmdList();
    const int cmdCount = static_cast<int>(cmdList->size());

    // 没有棋谱时，只保留第 0 行显示当前命令文本。
    if (cmdCount <= 0)
    {
        if (manualListModel.rowCount() <= 0) {
            manualListModel.insertRow(0);
        }
        manualListModel.setData(manualListModel.index(0), chess.getCmdLine());
        currentRow = 0;
        return;
    }

    // 第 0 行始终对应第一招；首招落下时只覆盖第 0 行，不整表重建。
    if (manualListModel.rowCount() <= 0)
    {
        manualListModel.insertRow(0);
    }
    manualListModel.setData(manualListModel.index(0), cmdList->front().c_str());

    // 后续只追加新增的棋谱行，避免 removeRows() + 全量插回去。
    for (int row = manualListModel.rowCount(); row < cmdCount; ++row)
    {
        manualListModel.insertRow(row);
        manualListModel.setData(manualListModel.index(row), cmdList->at(row).c_str());
    }

    currentRow = cmdCount - 1;
}

void GameController::syncAiState()
{
    if (chess.whoWin() == NineChess::NOBODY) {
        if (chess.getTurn() == NineChess::PLAYER1) {
            if (isEngine1) {
                ai1.resume();
            }
            if (isEngine2) {
                ai2.pause();
            }
        }
        else if (chess.getTurn() == NineChess::PLAYER2) {
            if (isEngine1) {
                ai1.pause();
            }
            if (isEngine2) {
                ai2.resume();
            }
        }
    }
    else {
        ai1.stop();
        ai2.stop();
    }
}

void GameController::emitPieceCountsChanged(const NineChess* chess)
{
    if (chess == nullptr) {
        chess = &(this->chess);
    }

    emit pieceCountsChanged(
        QStringLiteral(" %1 => %2")
            .arg(chess->getPlayer1InHand())
            .arg(chess->getPlayer1OnBoardCount()),
        QStringLiteral(" %1 => %2")
            .arg(chess->getPlayer2InHand())
            .arg(chess->getPlayer2OnBoardCount()));
}

bool GameController::applyStepLimit(NineChess::Players previousTurn)
{
    if (stepsLimit <= 0 || chess.whoWin() != NineChess::NOBODY) {
        return false;
    }
    if (static_cast<int>(chess.getCmdList()->size()) < stepsLimit) {
        return false;
    }

    if (!chess.command("==")) {
        return false;
    }

    advanceGameStateRevision();
    finishTurnClock(previousTurn);
    return true;
}

void GameController::handleTimeout()
{
    if (timeLimit <= 0 || chess.whoWin() != NineChess::NOBODY) {
        return;
    }

    const NineChess::Players loser = chess.getTurn();
    if (loser != NineChess::PLAYER1 && loser != NineChess::PLAYER2) {
        return;
    }

    int elapsed1 = 0;
    int elapsed2 = 0;
    getElapsedTimesMS(elapsed1, elapsed2);
    const int remaining1 = timeLimit * 60000 - elapsed1;
    const int remaining2 = timeLimit * 60000 - elapsed2;
    const bool timedOut = (loser == NineChess::PLAYER1) ? (remaining1 <= 0) : (remaining2 <= 0);
    if (!timedOut) {
        return;
    }

    const bool isEngine = (loser == NineChess::PLAYER1) ? isEngine1 : isEngine2;
    if (isEngine && forcedAiTimeoutTurn != loser) {
        forcedAiTimeoutTurn = loser;
        if (loser == NineChess::PLAYER1) {
            ai1.act();
        }
        else {
            ai2.act();
        }
        return;
    }

    const NineChess::Players winner = loser == NineChess::PLAYER1
        ? NineChess::PLAYER2 : NineChess::PLAYER1;
    if (!chess.adjudicateWin(winner, loser == NineChess::PLAYER1
        ? "玩家1超时，恭喜玩家2获胜！"
        : "玩家2超时，恭喜玩家1获胜！")) {
        return;
    }

    advanceGameStateRevision();
    finishTurnClock(loser);
    syncManualListFromChess();
    message = QString::fromStdString(chess.getTip());
    emit statusBarChanged(message);
    syncAiState();
    refreshTimeDisplays();
    playActionSound(SoundAction::Warning, true, 0, -1, &chess);
    updateScence();
}

// ==================== 游戏控制 ====================
void GameController::gameStart()
{
    chess.start();
    advanceGameStateRevision();
    chessTemp = chess;
    restartTurnClock();
    // 每隔100毫秒调用一次定时器处理函数
    if (timeID == 0) {
        timeID = startTimer(100);
    }
}

// 游戏重置
void GameController::gameReset()
{
    // 停止计时器
    if (timeID != 0)
        killTimer(timeID);
    // 定时器ID为0
    timeID = 0;
    // 重置游戏
    chess.reset();
    advanceGameStateRevision();
    chessTemp = chess;
    resetClockState();

    // 停掉线程
    ai1.stop();
    ai2.stop();
    isEngine1 = false;
    isEngine2 = false;

    // 清除棋子
    qDeleteAll(pieceList);
    pieceList.clear();
    currentPiece = nullptr;
    // 重新绘制棋盘
    scene.setDiagonal(chess.getRule()->hasDiagonalLines);

    // 绘制所有棋子，放在起始位置
    // 0: 先手第1子； 1：后手第1子
    // 2：先手嫡2子； 3：后手第2子
    // ......
    PieceItem::Models md;
    PieceItem *newP;
    for (int i = 0; i < chess.getRule()->piecesPerSide; i++)
    {
        // 先手的棋子
        md = isInverted ? PieceItem::whitePiece : PieceItem::blackPiece;
        newP = new PieceItem;
        newP->setModel(md);
        newP->setPos(scene.pos_p1);
        newP->setNum(i + 1);
        // 如果重复三连不可用，则显示棋子序号，九连棋专用玩法
        if (!(chess.getRule()->allowRepeatedMills))
            newP->setShowNum(true);
        pieceList.append(newP);
        scene.addItem(newP);

        // 后手的棋子
        md = isInverted ? PieceItem::blackPiece : PieceItem::whitePiece;
        newP = new PieceItem;
        newP->setModel(md);
        newP->setPos(scene.pos_p2);
        newP->setNum(i + 1);
        // 如果重复三连不可用，则显示棋子序号，九连棋专用玩法
        if (!(chess.getRule()->allowRepeatedMills))
            newP->setShowNum(true);
        pieceList.append(newP);
        scene.addItem(newP);
    }

    player1DisplaySlots = QVector<int>(chess.getRule()->piecesPerSide, kPieceInHand);
    player2DisplaySlots = QVector<int>(chess.getRule()->piecesPerSide, kPieceInHand);
    player1DisplayBoard = 0u;
    player2DisplayBoard = 0u;

    // 如果规则不要求计时，则time1和time2表示已用时间
    refreshTimeDisplays();
    // 更新棋谱
    manualListModel.removeRows(0, manualListModel.rowCount());
    manualListModel.insertRow(0);
    manualListModel.setData(manualListModel.index(0), chess.getCmdLine());
    currentRow = 0;
    // 发信号更新状态栏
    message = QString::fromStdString(chess.getTip());
    emit statusBarChanged(message);
    emitPieceCountsChanged();

    playActionSound(SoundAction::NewGame, true, 0, -1, &chess);
}

// ==================== 设置方法 ====================
// 设置编辑棋局状态
void GameController::setEditing(bool arg)
{
    isEditing = arg;
}

// 设置黑白反转状态
void GameController::setInvert(bool arg)
{
    isInverted = arg;
    // 遍历所有棋子
    for (PieceItem * p : pieceList)
    {
        if (p)
        {
            // 黑子变白
            if (p->getModel() == PieceItem::blackPiece)
                p->setModel(PieceItem::whitePiece);
            // 白子变黑
            else if (p->getModel() == PieceItem::whitePiece)
                p->setModel(PieceItem::blackPiece);
            // 刷新棋子显示
            p->update();
        }
    }
}

// 设置游戏规则
void GameController::setRule(int ruleNo, int stepLimited, int timeLimited)
{
    // 更新规则，原限时和限步不变
    if (ruleNo < 0 || ruleNo >= NineChess::RULE_COUNT)
        return;
    this->ruleNo = ruleNo;

    if (stepLimited != -1 && timeLimited != -1) {
        stepsLimit = stepLimited;
        timeLimit = timeLimited;
    }
    // 设置模型规则，重置游戏
    chess.setRule(static_cast<uint32_t>(ruleNo));
    chessTemp = chess;

    // 重置游戏
    gameReset();
}

// 设置电脑执先手
void GameController::setEngine1(bool arg)
{
    isEngine1 = arg;
    if (arg) {
        ai1.setAi(chess);
        if (ai1.isRunning())
            ai1.resume();
        else
            ai1.start();
    }
    else {
        ai1.stop();
    }
}

// 设置电脑执后手
void GameController::setEngine2(bool arg)
{
    isEngine2 = arg;
    if (arg) {
        ai2.setAi(chess);
        if (ai2.isRunning())
            ai2.resume();
        else
            ai2.start();
    }
    else {
        ai2.stop();
    }
}

// 设置AI深度和时限
void GameController::setAiDepthTime(int depth1, int time1, int depth2, int time2)
{
    if (isEngine1) {
        ai1.stop();
        ai1.wait();
    }
    if (isEngine2) {
        ai2.stop();
        ai2.wait();
    }

    ai1.setAi(chess, depth1, time1);
    ai2.setAi(chess, depth2, time2);

    if (isEngine1) {
        ai1.start();
    }
    if (isEngine2) {
        ai2.start();
    }
}

// 获取AI深度和时限
void GameController::getAiDepthTime(int &depth1, int &time1, int &depth2, int &time2)
{
    ai1.getDepthTime(depth1, time1);
    ai2.getDepthTime(depth2, time2);
}

// 设置是否有落子动画
void GameController::setAnimation(bool arg)
{
    hasAnimation = arg;
    // 默认动画时间250ms
    if (hasAnimation)
        durationTime = 250;
    else
        durationTime = 0;
}

// 设置是否有落子音效
void GameController::setSound(bool arg)
{
    hasSound = arg;
}

// 播放指定音效
void GameController::playSound(const QString &soundPath)
{
    if (!hasSound || !soundCache.contains(soundPath))
        return;

    soundCache[soundPath]->play();
}

// ==================== 视图变换 ====================
// 上下翻转
void GameController::flip()
{
    if (isEngine1) {
        ai1.stop();
        ai1.wait();
    }
    if (isEngine2) {
        ai2.stop();
        ai2.wait();
    }

    chess.mirror();
    chess.rotate(180);
    advanceGameStateRevision();
    chessTemp = chess;
    // 更新棋谱
    int row = 0;
    for (auto str : *(chess.getCmdList())) {
        manualListModel.setData(manualListModel.index(row++), str.c_str());
    }
    // 刷新显示
    if (currentRow == row - 1)
        updateScence();
    else
        phaseChange(currentRow, true);

    ai1.setAi(chess);
    ai2.setAi(chess);
    if (isEngine1) {
        ai1.start();
    }
    if (isEngine2) {
        ai2.start();
    }
}

// 左右镜像
void GameController::mirror()
{
    if (isEngine1) {
        ai1.stop();
        ai1.wait();
    }
    if (isEngine2) {
        ai2.stop();
        ai2.wait();
    }

    chess.mirror();
    advanceGameStateRevision();
    chessTemp = chess;
    // 更新棋谱
    int row = 0;
    for (auto str : *(chess.getCmdList())) {
        manualListModel.setData(manualListModel.index(row++), str.c_str());
    }
    qDebug() << "list: " << row;
    // 刷新显示
    if (currentRow == row - 1)
        updateScence();
    else
        phaseChange(currentRow, true);

    ai1.setAi(chess);
    ai2.setAi(chess);
    if (isEngine1) {
        ai1.start();
    }
    if (isEngine2) {
        ai2.start();
    }
}

// 视图顺时针旋转90°
void GameController::turnRight()
{
    if (isEngine1) {
        ai1.stop();
        ai1.wait();
    }
    if (isEngine2) {
        ai2.stop();
        ai2.wait();
    }

    chess.rotate(-90);
    advanceGameStateRevision();
    chessTemp = chess;
    // 更新棋谱
    int row = 0;
    for (auto str : *(chess.getCmdList())) {
        manualListModel.setData(manualListModel.index(row++), str.c_str());
    }
    // 刷新显示
    if (currentRow == row - 1)
        updateScence();
    else
        phaseChange(currentRow, true);

    ai1.setAi(chess);
    ai2.setAi(chess);
    if (isEngine1) {
        ai1.start();
    }
    if (isEngine2) {
        ai2.start();
    }
}

// 视图逆时针旋转90°
void GameController::turnLeft()
{
    if (isEngine1) {
        ai1.stop();
        ai1.wait();
    }
    if (isEngine2) {
        ai2.stop();
        ai2.wait();
    }

    chess.rotate(90);
    advanceGameStateRevision();
    chessTemp = chess;
    // 更新棋谱
    int row = 0;
    for (auto str : *(chess.getCmdList())) {
        manualListModel.setData(manualListModel.index(row++), str.c_str());
    }
    // 刷新显示
    if (currentRow == row - 1)
        updateScence();
    else
        phaseChange(currentRow, true);

    ai1.setAi(chess);
    ai2.setAi(chess);
    if (isEngine1) {
        ai1.start();
    }
    if (isEngine2) {
        ai2.start();
    }
}

// ==================== 定时器事件 ====================
void GameController::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)
    refreshTimeDisplays();

    if (chess.whoWin() == NineChess::NOBODY) {
        handleTimeout();
    }
    // 如果胜负已分
    if (chess.whoWin() != NineChess::NOBODY)
    {
        // 停止计时
        killTimer(timeID);
        // 定时器ID为0
        timeID = 0;
        // 发信号更新状态栏
        message = QString::fromStdString(chess.getTip());
        emit statusBarChanged(message);

        //updateScence();
    }

    // 测试用代码
    /*
    int ti = time.elapsed();
    static QTime t;
    if (ti < 0)
        ti += 86400; // 防止过24:00引起的时间误差，加上一天中总秒数
    if (timeWhos == 1)
    {
        time1 = ti - time2;
        // 用于显示时间的临时变量，多出的50毫秒用于消除计时器误差产生的跳动
        t = QTime(0, 0, 0, 50).addMSecs(time1);
        //qDebug() << t;
        emit time1Changed(t.toString("hh:mm:ss"));
    }
    else if (timeWhos == 2)
    {
        time2 = ti - time1;
        // 用于显示时间的临时变量，多出的50毫秒用于消除计时器误差产生的跳动
        t = QTime(0, 0, 0, 50).addMSecs(time2);
        //qDebug() << t;
        emit time2Changed(t.toString("hh:mm:ss"));
    }
    */
}

// ==================== 棋子动作处理 ====================
// 处理棋子动作（选子、落子、去子）
bool GameController::actionPiece(QPointF pos)
{
    // 点击非落子点，不执行
    int c, p;
    if (!scene.pos2cp(pos, c, p)) {
        return false;
    }

    // 电脑走棋时，点击无效
    if (chess.getTurn() == NineChess::PLAYER1 && isEngine1)
        return false;
    if (chess.getTurn() == NineChess::PLAYER2 && isEngine2)
        return false;

    // 在浏览历史记录时点击棋盘，则认为是悔棋
    if (currentRow != manualListModel.rowCount() - 1)
    {
        // 定义新对话框
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setMinimumSize(600, 400);
        msgBox.setText(tr("当前正在浏览历史局面。"));
        msgBox.setInformativeText(tr("是否在此局面下重新开始？悔棋者将承担时间损失！"));
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        (msgBox.button(QMessageBox::Ok))->setText(tr("确定"));
        (msgBox.button(QMessageBox::Cancel))->setText(tr("取消"));

        if (QMessageBox::Ok == msgBox.exec())
        {
            chess = chessTemp;
            advanceGameStateRevision();
            manualListModel.removeRows(currentRow + 1, manualListModel.rowCount() - currentRow - 1);
            // 如果再决出胜负后悔棋，则重新启动计时
            if (chess.whoWin() == NineChess::NOBODY) {
                // 重新启动计时
                if (timeID == 0) {
                    timeID = startTimer(100);
                }
                restartTurnClock();
                // 发信号更新状态栏
                message = QString::fromStdString(chess.getTip());
                emit statusBarChanged(message);
                syncAiState();
            }
            else {
                turnStartTimeMS = 0;
                forcedAiTimeoutTurn = NineChess::NOBODY;
            }
        }
        else
            return false;
    }

    // 如果未开局则开局
    if (chess.getPhase() == NineChess::GAME_NOTSTARTED)
        gameStart();

    const NineChess::Players previousTurn = chess.getTurn();

    // 判断执行选子、落子或去子
    bool result = false;
    PieceItem *piece = nullptr;
    QGraphicsItem *item = scene.itemAt(pos, QTransform());

    // 当前状态
    const uint16_t previousStatus = chess.getStatus();
    const int32_t previousSelectedPos = chess.getCurrentPos();
    SoundAction soundAction = SoundAction::None;
    bool needDirectRefresh = true;

    switch (chess.getAction())
    {
    case NineChess::ACTION_PLACE:
        if (chess.place(c, p))
        {
            soundAction = SoundAction::Place;
            result = true;
            break;
        }
        // 如果移子不成功，尝试重新选子，这里不break

    case NineChess::ACTION_CHOOSE:
        piece = qgraphicsitem_cast<PieceItem *>(item);
        if (!piece)
            break;
        if (chess.choose(c, p)) {
            soundAction = SoundAction::Choose;
            result = true;
        }
        break;

    case NineChess::ACTION_CAPTURE:
        if (chess.capture(c, p)) {
            soundAction = SoundAction::Capture;
            result = true;
        }
        break;

    default:
        // 如果是结局状态，不做任何响应
        break;
    }

    if (result)
    {
        const int oldRowCount = manualListModel.rowCount();
        advanceGameStateRevision();

        if (!applyStepLimit(previousTurn)
            && (chess.whoWin() != NineChess::NOBODY || chess.getTurn() != previousTurn)) {
            finishTurnClock(previousTurn);
        }

        syncManualListFromChess();
        needDirectRefresh = manualListModel.rowCount() <= oldRowCount;
        message = QString::fromStdString(chess.getTip());
        emit statusBarChanged(message);
        if (&chess == &(this->chess)) {
            syncAiState();
        }
    }

    playActionSound(soundAction, result, previousStatus, previousSelectedPos, &chess);
    if (needDirectRefresh) {
        updateScence();
    }
    return result;
}

// ==================== 认输与命令执行 ====================
// 认输
bool GameController::giveUp()
{
    bool result = false;
    const NineChess::Players previousTurn = chess.getTurn();
    if (chess.getTurn() == NineChess::PLAYER1)
        result = chess.giveup(NineChess::PLAYER1);
    else if (chess.getTurn() == NineChess::PLAYER2)
        result = chess.giveup(NineChess::PLAYER2);
    if (result)
    {
        advanceGameStateRevision();
        finishTurnClock(previousTurn);
        syncManualListFromChess();
        message = QString::fromStdString(chess.getTip());
        emit statusBarChanged(message);
        syncAiState();
        playActionSound(SoundAction::GameOver, true, 0, -1, &chess);
    }
    return result;
}

// 执行棋谱命令（供AI调用）
bool GameController::command(const QString &cmd, bool update)
{
    const AiThread *sourceAi = aiSourceFromSender();
    if (sourceAi != nullptr) {
        AiDispatchState &state = aiDispatchState(sourceAi);
        if (state.calcRevision != gameStateRevision) {
            return false;
        }

        const int64_t elapsed = std::max<int64_t>(0, currentTimeMS() - state.calcStartedMS);
        if (update && elapsed < kMinAiActionDelayMS) {
            const uint64_t sequence = state.calcSequence;
            state.pendingCommand = cmd;
            state.pendingUpdate = update;
            state.pendingSequence = sequence;
            state.hasPendingCommand = true;
            QTimer::singleShot(static_cast<int>(kMinAiActionDelayMS - elapsed), this,
                [this, sourceAi, sequence]() {
                    dispatchPendingAiCommand(sourceAi, sequence);
                });
            return true;
        }
    }

    return executeCommandInternal(cmd, update, sourceAi);

    // 防止接收滞后结束的线程发送的指令
    // 记录执行前的动作类型
    // 如果未开局则开局
    // 当前状态
}

// 浏览历史局面，通过command函数刷新局面显示
bool GameController::phaseChange(int row, bool forceUpdate)
{
    // 如果row是当前浏览的棋谱行，则不需要刷新
    if (currentRow == row && !forceUpdate)
        return false;

    // 当前状态
    uint16_t previousStatus = 0;
    int32_t previousSelectedPos = -1;
    SoundAction soundAction = SoundAction::NewGame;

    // 需要刷新
    currentRow = row;
    int rows = manualListModel.rowCount();
    QStringList mlist = manualListModel.stringList();
    chessTemp.setRule(chess.getRuleIndex());
    qDebug() << "rows:" << rows << " current:" << row;
    // row 是当前要显示的棋谱行，所以回放时必须包含这一行。
    for (int i = 0; i <= row; i++)
    {
        qDebug() << mlist.at(i);
        previousStatus = chessTemp.getStatus();
        previousSelectedPos = chessTemp.getCurrentPos();
        soundAction = soundActionFromCommand(mlist.at(i), previousStatus);
        chessTemp.command(mlist.at(i).toStdString().c_str());
    }

    // 刷新棋局场景
    playActionSound(soundAction, true, previousStatus, previousSelectedPos, &chessTemp);
    updateScence(&chessTemp);
    return true;
}

// ==================== 音效与显示更新 ====================
// 播放动作音效
GameController::SoundAction GameController::soundActionFromCommand(const QString &cmd,
    uint16_t previousStatus)
{
    const QString trimmed = cmd.trimmed();
    if (trimmed.isEmpty()) {
        return SoundAction::NewGame;
    }
    if (trimmed == "==") {
        return SoundAction::GameOver;
    }
    if (trimmed.startsWith('-')) {
        return (trimmed == "-0" || trimmed == "-1")
            ? SoundAction::GameOver
            : SoundAction::Capture;
    }
    if (trimmed.contains("->")) {
        return SoundAction::Place;
    }

    const uint16_t previousAction = previousStatus
        & (NineChess::ACTION_CHOOSE | NineChess::ACTION_PLACE | NineChess::ACTION_CAPTURE);
    if (previousAction == NineChess::ACTION_CAPTURE) {
        return SoundAction::Capture;
    }
    if (previousAction == NineChess::ACTION_CHOOSE) {
        return SoundAction::Choose;
    }
    return SoundAction::Place;
}

void GameController::playActionSound(SoundAction action, bool succeeded,
    uint16_t previousStatus, int32_t previousSelectedPos, const NineChess* chess)
{
    if (chess == nullptr) {
        chess = &(this->chess);
    }

    QString sound;
    if (action == SoundAction::Warning) {
        sound = "warning";
    }
    else if (action == SoundAction::NewGame) {
        sound = "newgame";
    }
    else if (!succeeded) {
        sound = "forbidden";
    }
    else if (chess->whoWin() != NineChess::NOBODY
        || chess->getPhase() == NineChess::GAME_OVER
        || action == SoundAction::GameOver) {
        sound = "win";
        const NineChess::Players winner = chess->whoWin();
        if (winner == NineChess::PLAYER1 || winner == NineChess::PLAYER2) {
            const bool singleAiMatch = (isEngine1 != isEngine2);
            const NineChess::Players loser = winner == NineChess::PLAYER1
                ? NineChess::PLAYER2 : NineChess::PLAYER1;
            const bool loserIsHuman = (loser == NineChess::PLAYER1) ? !isEngine1 : !isEngine2;
            if (singleAiMatch && loserIsHuman) {
                sound = "loss";
            }
        }
    }
    else {
        switch (action)
        {
        case SoundAction::Choose:
            sound = "choose";
            break;

        case SoundAction::Place:
        {
            const uint16_t previousPhase = previousStatus
                & (NineChess::GAME_OPENING | NineChess::GAME_MID | NineChess::GAME_OVER);
            const uint16_t previousAction = previousStatus
                & (NineChess::ACTION_CHOOSE | NineChess::ACTION_PLACE | NineChess::ACTION_CAPTURE);
            if (chess->getAction() == NineChess::ACTION_CAPTURE) {
                sound = "capture";
            }
            else if (previousPhase == NineChess::GAME_MID
                && previousAction == NineChess::ACTION_PLACE
                && chess->getAction() == NineChess::ACTION_PLACE
                && chess->getCurrentPos() != previousSelectedPos) {
                sound = "choose";
            }
            else {
                sound = "drog";
            }
            break;
        }

        case SoundAction::Capture:
            sound = "remove";
            break;

        case SoundAction::GameOver:
            sound = "win";
            break;

        default:
            sound = "forbidden";
            break;
        }
    }

    playSound(sound);
}

void GameController::updateScence(const NineChess* chess /* = nullptr */)
{
    if (chess == nullptr)
        chess = &(this->chess);
    emitPieceCountsChanged(chess);
    const NineChess::Rule* rule = chess->getRule();
    const NineChess::ChessData& data = chess->getData();
    const int piecesPerSide = static_cast<int>(rule->piecesPerSide);
    const int n = piecesPerSide * 2;
    const int player1Placed = piecesPerSide - static_cast<int>(chess->getPlayer1_InHand());
    const int player2Placed = piecesPerSide - static_cast<int>(chess->getPlayer2_InHand());
    const uint32_t player1Board = data.player1Board & NineChess::ChessData::VALID_BOARD_MASK;
    const uint32_t player2Board = data.player2Board & NineChess::ChessData::VALID_BOARD_MASK;

    QVector<int> player1NextDisplaySlots;
    QVector<int> player2NextDisplaySlots;
    if (rule->allowRepeatedMills) {
        buildUnnumberedDisplaySlots(player1DisplaySlots, player1DisplayBoard, player1Board,
            static_cast<int>(chess->getPlayer1_InHand()), piecesPerSide, player1NextDisplaySlots);
        buildUnnumberedDisplaySlots(player2DisplaySlots, player2DisplayBoard, player2Board,
            static_cast<int>(chess->getPlayer2_InHand()), piecesPerSide, player2NextDisplaySlots);
    }

    // 动画组
    QParallelAnimationGroup *animationGroup = new QParallelAnimationGroup;

    // 1. 摆放先后手棋子。
    for (int i = 0; i < n; ++i)
    {
        PieceItem *piece = pieceList.at(i);
        const bool isPlayer1Piece = (i % 2 == 0);
        const NineChess::Players player = isPlayer1Piece ? NineChess::PLAYER1 : NineChess::PLAYER2;
        const int number = i / 2;
        QPointF target = isPlayer1Piece ? scene.pos_p1 : scene.pos_p2;
        bool onBoard = false;

        if (rule->allowRepeatedMills)
        {
            const QVector<int>& displaySlots = isPlayer1Piece
                ? player1NextDisplaySlots : player2NextDisplaySlots;
            const int slotState = number < displaySlots.size()
                ? displaySlots.at(number) : kPieceInHand;
            if (slotState >= 0)
            {
                int c = 0;
                int p = 0;
                chess->posToCP(slotState, c, p);
                target = scene.cp2pos(c, p);
                onBoard = true;
            }
            else if (slotState == kPieceCaptured) {
                target = isPlayer1Piece ? scene.pos_p2_g : scene.pos_p1_g;
            }
            else {
                target = isPlayer1Piece ? scene.pos_p1 : scene.pos_p2;
            }
        }
        else
        {
            int c = 0;
            int p = 0;
            if (chess->getPieceCP(player, static_cast<uint32_t>(number), c, p))
            {
                target = scene.cp2pos(c, p);
                onBoard = true;
            }
        }

        // 不在棋盘上的棋子，只分两种位置：墓地或手牌区。
        if (!onBoard && !rule->allowRepeatedMills)
        {
            const int placedCount = isPlayer1Piece ? player1Placed : player2Placed;
            if (number < placedCount) {
                target = isPlayer1Piece ? scene.pos_p2_g : scene.pos_p1_g;
            }
            else {
                target = isPlayer1Piece ? scene.pos_p1 : scene.pos_p2;
            }
        }

        if (piece->pos() != target)
        {
            piece->setZValue(1);
            QPropertyAnimation *animation = new QPropertyAnimation(piece, "pos");
            animation->setDuration(durationTime);
            animation->setStartValue(piece->pos());
            animation->setEndValue(target);
            animation->setEasingCurve(QEasingCurve::InOutQuad);
            animationGroup->addAnimation(animation);
        }
        else
        {
            piece->setZValue(0);
        }
        piece->setSelected(false);
    }

    // 2. 摆放或清除禁点虚拟棋子。
    int markerIndex = n;
    if (rule->hasForbiddenPoints && chess->getPhase() == NineChess::GAME_OPENING)
    {
        for (int pos = 0; pos < NineChess::BOARD_SIZE; ++pos)
        {
            if ((data.forbiddenBoard & (1u << pos)) == 0u) {
                continue;
            }

            int c = 0;
            int p = 0;
            chess->posToCP(pos, c, p);
            const QPointF target = scene.cp2pos(c, p);

            if (markerIndex < pieceList.size())
            {
                pieceList.at(markerIndex)->setDeleted();
                pieceList.at(markerIndex)->setPos(target);
            }
            else
            {
                PieceItem *newP = new PieceItem;
                newP->setDeleted();
                newP->setPos(target);
                pieceList.append(newP);
                scene.addItem(newP);
            }
            ++markerIndex;
        }
    }
    while (markerIndex < pieceList.size())
    {
        delete pieceList.at(markerIndex);
        pieceList.removeAt(markerIndex);
    }

    if (rule->allowRepeatedMills) {
        player1DisplaySlots = player1NextDisplaySlots;
        player2DisplaySlots = player2NextDisplaySlots;
    }
    player1DisplayBoard = player1Board;
    player2DisplayBoard = player2Board;

    // 3. 给当前选中的棋子加选中标记。
    currentPiece = nullptr;
    if (!rule->allowRepeatedMills)
    {
        // 编号规则下，直接按“玩家 + 编号”找到前台棋子。
        NineChess::Players player = NineChess::NOBODY;
        uint32_t number = 0u;
        if (chess->getCurrentPiece(player, number))
        {
            const int index = static_cast<int>(number) * 2
                + (player == NineChess::PLAYER2 ? 1 : 0);
            if (index >= 0 && index < n) {
                currentPiece = pieceList.at(index);
            }
        }
    }
    else if (chess->getCurrentPos() >= 0)
    {
        // 非编号规则下，用前台槽位找到被选中的那颗棋子。
        const int currentPos = chess->getCurrentPos();
        const NineChess::Players player = chess->getWhosPiecePos(currentPos);
        if (player == NineChess::PLAYER1 || player == NineChess::PLAYER2)
        {
            const QVector<int>& displaySlots =
                (player == NineChess::PLAYER1) ? player1DisplaySlots : player2DisplaySlots;
            for (int i = 0; i < displaySlots.size(); ++i)
            {
                if (displaySlots.at(i) == currentPos)
                {
                    const int index = i * 2 + (player == NineChess::PLAYER2 ? 1 : 0);
                    if (index >= 0 && index < n) {
                        currentPiece = pieceList.at(index);
                    }
                    break;
                }
            }
        }
    }

    if (currentPiece != nullptr) {
        currentPiece->setSelected(true);
    }

    animationGroup->start(QAbstractAnimation::DeleteWhenStopped);
}
