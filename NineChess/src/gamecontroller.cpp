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
    timeLimit(10),
    stepsLimit(100),
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
    stopAndWaitAi(ai1);
    stopAndWaitAi(ai2);
}

void GameController::stopAndWaitAi(AiThread &ai)
{
    if (ai.isRunning()) {
        ai.stop();
        ai.wait();
    }
}

void GameController::resetHistorySnapshots()
{
    historyStates.clear();
    historyStates.append(chess);
}

void GameController::appendHistorySnapshot()
{
    // 只有命令历史真正增长时才追加快照（choose 只改选中态、不落命令行，不能追加），
    // 严格保持不变式 historyStates.size() == cmdCount + 1，
    // 否则中盘“选子+落子”会让快照数组多涨一格，历史浏览会整体向前偏移一手。
    if (historyStates.size() < static_cast<int>(chess.getCmdList()->size()) + 1) {
        historyStates.append(chess);
    }
}

int GameController::historyStateIndexForRow(int row) const
{
    // 第 0 行在没有棋谱时对应初始局面，有棋谱时对应第一招之后的局面
    const int cmdCount = static_cast<int>(chess.getCmdList()->size());
    return (cmdCount <= 0) ? 0 : row + 1;
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

    // 开局属于对局状态流转而非显示刷新：无论是否刷新界面都要先开局，
    // 否则命令在未开局阶段无法执行（例如打开棋谱文件回放时）。
    if (chess.getPhase() == NineChess::GAME_NOTSTARTED) {
        gameStart();
    }

    const NineChess::Players previousTurn = chess.getTurn();
    const uint16_t previousStatus = chess.getStatus();
    const int32_t previousSelectedPos = chess.getCurrentPos();
    const SoundAction soundAction = soundActionFromCommand(cmd, previousStatus);

    if (!chess.command(cmd.toStdString().c_str())) {
        return false;
    }

    advanceGameStateRevision();
    appendHistorySnapshot();

    if (update) {
        if (!applyStepLimit(previousTurn)
            && (chess.whoWin() != NineChess::NOBODY || chess.getTurn() != previousTurn)) {
            finishTurnClock(previousTurn);
        }
    }

    syncManualListFromChess();
    message = QString::fromStdString(chess.getTip());
    emit statusBarChanged(message);

    if (update) {
        // 先播音效、刷新界面，再唤醒 AI：AI 唤醒后 Lazy SMP 会立刻吃满 CPU，
        // 让音效启动先于搜索线程启动，避免相互争抢调度
        playActionSound(soundAction, true, previousStatus, previousSelectedPos, &chess);
        updateScence();
    }

    if (update && (&chess == &(this->chess))) {
        syncAiState();
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
        emit browseRowChanged(currentRow);
        return;
    }

    // 第 0 行始终对应第一招；首招落下时只覆盖第 0 行，不整表重建。
    if (manualListModel.rowCount() <= 0)
    {
        manualListModel.insertRow(0);
    }
    manualListModel.setData(manualListModel.index(0), cmdList->front().c_str());

    // 后续只追加新增的棋谱行，避免 removeRows() + 全量插回去。
    // 追加会让浏览链路自动同步到最新行，这属于走子的实时推进而非用户浏览，
    // 置位 skipNextBrowseSound 让链路里的 phaseChange 不重放音效，
    // 音效由走子路径（actionPiece / executeCommandInternal）唯一负责。
    const bool appendedRows = cmdCount > manualListModel.rowCount();
    if (appendedRows)
        skipNextBrowseSound = true;
    for (int row = manualListModel.rowCount(); row < cmdCount; ++row)
    {
        manualListModel.insertRow(row);
        manualListModel.setData(manualListModel.index(row), cmdList->at(row).c_str());
    }
    skipNextBrowseSound = false;

    currentRow = cmdCount - 1;
    emit browseRowChanged(currentRow);
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
    appendHistorySnapshot();
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

    // 外部裁定结束没有命令记录，保存棋谱时据此补写结果命令（-0/-1）
    endedByAdjudication = true;
    advanceGameStateRevision();
    appendHistorySnapshot();
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
    resetHistorySnapshots();
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
    resetHistorySnapshots();
    resetClockState();
    endedByAdjudication = false;

    // 停掉线程
    stopAndWaitAi(ai1);
    stopAndWaitAi(ai2);
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
    // 通知界面同步浏览行号与导航键状态
    emit browseRowChanged(currentRow);
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
    // 更新规则，原限时和限步不变；-1 表示“不修改”该项
    if (ruleNo < 0 || ruleNo >= NineChess::RULE_COUNT)
        return;
    this->ruleNo = ruleNo;

    if (stepLimited != -1)
        stepsLimit = stepLimited;
    if (timeLimited != -1)
        timeLimit = timeLimited;
    // 设置模型规则，重置游戏
    chess.setRule(static_cast<uint32_t>(ruleNo));

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
        stopAndWaitAi(ai1);
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
        stopAndWaitAi(ai2);
    }
}

// 设置AI深度和时限
void GameController::setAiDepthTime(int depth1, int time1, int depth2, int time2)
{
    stopAndWaitAi(ai1);
    stopAndWaitAi(ai2);

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
    stopAndWaitAi(ai1);
    stopAndWaitAi(ai2);

    chess.flipVertical();
    advanceGameStateRevision();
    // 快照同步变换，保证历史浏览与悔棋的坐标一致
    for (NineChess &state : historyStates) {
        state.flipVertical();
    }
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
    stopAndWaitAi(ai1);
    stopAndWaitAi(ai2);

    chess.mirror();
    advanceGameStateRevision();
    // 快照同步变换，保证历史浏览与悔棋的坐标一致
    for (NineChess &state : historyStates) {
        state.mirror();
    }
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
    stopAndWaitAi(ai1);
    stopAndWaitAi(ai2);

    chess.rotate(-90);
    advanceGameStateRevision();
    // 快照同步变换，保证历史浏览与悔棋的坐标一致
    for (NineChess &state : historyStates) {
        state.rotate(-90);
    }
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
    stopAndWaitAi(ai1);
    stopAndWaitAi(ai2);

    chess.rotate(90);
    advanceGameStateRevision();
    // 快照同步变换，保证历史浏览与悔棋的坐标一致
    for (NineChess &state : historyStates) {
        state.rotate(90);
    }
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
            // 回退到当前浏览的历史局面快照，并截断其后的快照与棋谱行
            const int stateIndex = historyStateIndexForRow(currentRow);
            if (stateIndex >= 0 && stateIndex < historyStates.size()) {
                chess = historyStates.at(stateIndex);
                historyStates.resize(stateIndex + 1);
            }
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
            // 棋谱行数变化，通知界面同步导航键状态
            emit browseRowChanged(currentRow);
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
        appendHistorySnapshot();

        if (!applyStepLimit(previousTurn)
            && (chess.whoWin() != NineChess::NOBODY || chess.getTurn() != previousTurn)) {
            finishTurnClock(previousTurn);
        }

        syncManualListFromChess();
        needDirectRefresh = manualListModel.rowCount() <= oldRowCount;
        message = QString::fromStdString(chess.getTip());
        emit statusBarChanged(message);
    }

    playActionSound(soundAction, result, previousStatus, previousSelectedPos, &chess);

    if (result) {
        // 音效启动之后再唤醒 AI（见 executeCommandInternal 中的说明）
        syncAiState();
    }

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
        appendHistorySnapshot();
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
    // 对局配置命令 "r<规则>s<限步>t<限时>"（棋谱头部的会话级命令）：
    // 模型不识别，由控制层应用。规则段会重开局；省略的段保持当前设置。
    {
        int rule = -1;
        int steps = -1;
        int timeLimit = -1;
        if (parseSetupCommand(cmd.toStdString(), rule, steps, timeLimit)) {
            if (rule >= 0) {
                if (rule >= NineChess::RULE_COUNT)
                    return false;
                setRule(rule);
            }
            if (steps >= 0)
                stepsLimit = steps;
            if (timeLimit >= 0)
                this->timeLimit = timeLimit;
            refreshTimeDisplays();
            return true;
        }
    }

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
}

// 浏览历史局面，直接取对应快照刷新局面显示
bool GameController::phaseChange(int row, bool forceUpdate)
{
    // 如果row是当前浏览的棋谱行，则不需要刷新
    if (currentRow == row && !forceUpdate)
        return false;

    const int rows = manualListModel.rowCount();
    if (row < 0 || row >= rows)
        return false;

    currentRow = row;

    // 从快照直接取出目标局面，避免每次浏览都从头回放全部命令
    const int stateIndex = historyStateIndexForRow(row);
    if (stateIndex < 0 || stateIndex >= historyStates.size())
        return false;

    const QStringList mlist = manualListModel.stringList();
    const NineChess &previous = historyStates.at(stateIndex > 0 ? stateIndex - 1 : 0);
    const NineChess &target = historyStates.at(stateIndex);
    const SoundAction soundAction = soundActionFromCommand(mlist.at(row), previous.getStatus());

    // 刷新棋局场景；音效只在真正的用户浏览时重放（走子链路的自动推进不重放，
    // 见 syncManualListFromChess 中 skipNextBrowseSound 的说明）
    const bool replaySound = !skipNextBrowseSound;
    skipNextBrowseSound = false;
    if (replaySound)
        playActionSound(soundAction, true, previous.getStatus(), previous.getCurrentPos(), &target);
    updateScence(&target);
    emit browseRowChanged(currentRow);
    return true;
}

// 浏览历史局面：以控制器 currentRow 为唯一行号基准的统一入口
bool GameController::browseTo(int row)
{
    const int rows = manualListModel.rowCount();
    if (rows <= 0 || row < 0 || row >= rows)
        return false;
    return phaseChange(row);
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
