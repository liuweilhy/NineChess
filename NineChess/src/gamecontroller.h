/****************************************************************************
** GameController - 游戏控制器实现
** 负责游戏流程控制、棋子显示、AI管理、计时和音效
****************************************************************************/

#pragma once

#include <cstdint>
#include <QObject>
#include <QTime>
#include <QPointF>
#include <QMap>
#include <QList>
#include <QVector>
#include <QSoundEffect>
#include <QTextStream>
#include <QStringListModel>
#include <QModelIndex>
#include "ninechess.h"
#include "gamescene.h"
#include "pieceitem.h"
#include "aithread.h"

class GameController : public QObject
{
    Q_OBJECT

public:
    GameController(GameScene &scene, QObject *parent = nullptr);
    ~GameController();

    const QMap<int, QStringList> getActions();

    int getRuleNo() const { return ruleNo; }
    int getTimeLimit() const { return timeLimit; }
    int getStepsLimit() const { return stepsLimit; }
    // 终局胜者：未分胜负返回 NOBODY，平局返回 DRAW
    NineChess::Players getWinner() const { return chess.getWinner(); }
    // 本局是否由外部裁定结束（超时判负没有命令记录，保存棋谱时需补写结果命令）
    bool isEndedByAdjudication() const { return endedByAdjudication; }
    bool isAnimation() const { return hasAnimation; }
    int getDurationTime() const { return durationTime; }
    // 当前浏览的棋谱行号（以控制器为唯一基准）
    int browseRow() const { return currentRow; }
    QStringListModel* getManualListModel() { return &manualListModel; }

    // 浏览历史局面：统一入口，交由控制器维护 currentRow
    bool browseTo(int row);

    // 语言切换后重新生成状态栏文本：模型持有提示模板，读取时才翻译，
    // 因此换语言后只需重新取一次即可得到新语言的状态栏提示。
    void refreshText();

    void setAiDepthTime(int depth1, int time1, int depth2, int time2);
    void getAiDepthTime(int &depth1, int &time1, int &depth2, int &time2);

signals:
    void time1Changed(const QString &time);
    void time2Changed(const QString &time);
    void statusBarChanged(const QString &message);
    void pieceCountsChanged(const QString &player1, const QString &player2);
    // 浏览行号或棋谱行数变化后通知界面同步（以控制器 currentRow 为唯一基准）
    void browseRowChanged(int row);

public slots:
    void setRule(int ruleNo, int stepLimited = -1, int timeLimited = -1);
    void gameStart();
    void gameReset();

    void setEditing(bool arg = true);
    void setInvert(bool arg = true);

    void setEngine1(bool arg = true);
    void setEngine2(bool arg = true);

    void setAnimation(bool arg = true);
    void setSound(bool arg = true);

    void flip();
    void mirror();
    void turnRight();
    void turnLeft();

    bool actionPiece(QPointF p);
    bool giveUp();
    bool command(const QString &cmd, bool update = true);
    bool phaseChange(int row, bool forceUpdate = false);

    void updateScence(const NineChess* chess = nullptr);

protected:
    void timerEvent(QTimerEvent *event);
    virtual void playSound(const QString &soundPath);
    void refreshTimeDisplays();

private:
    enum class SoundAction {
        None,
        NewGame,
        Choose,
        Place,
        Capture,
        GameOver,
        Warning
    };

    struct AiDispatchState {
        int64_t calcStartedMS = 0;
        uint64_t calcRevision = 0;
        uint64_t calcSequence = 0;
        uint64_t pendingSequence = 0;
        QString pendingCommand;
        bool pendingUpdate = true;
        bool hasPendingCommand = false;
    };

    static constexpr int kMinAiActionDelayMS = 500;

    static int64_t currentTimeMS();
    const AiThread* aiSourceFromSender() const;
    AiDispatchState& aiDispatchState(const AiThread* sourceAi);
    // 停止 AI 线程并等待其完全退出（Qt 在线程结束时复位中断标志，
    // 因此 stop()+wait() 之后可以安全地重新 start()）
    void stopAndWaitAi(AiThread &ai);
    bool executeCommandInternal(const QString &cmd, bool update, const AiThread *sourceAi);
    void onAiCalcStarted();
    void dispatchPendingAiCommand(const AiThread *sourceAi, uint64_t sequence);
    void invalidatePendingAiCommands();
    void advanceGameStateRevision();
    void resetClockState();
    void restartTurnClock();
    void finishTurnClock(NineChess::Players finishedTurn);
    void getElapsedTimesMS(int &elapsed1, int &elapsed2) const;
    static SoundAction soundActionFromCommand(const QString &cmd, uint16_t previousStatus);
    void playActionSound(SoundAction action, bool succeeded = true,
        uint16_t previousStatus = 0, int32_t previousSelectedPos = -1,
        const NineChess* chess = nullptr);
    void syncManualListFromChess();
    void syncAiState();
    void emitPieceCountsChanged(const NineChess* chess = nullptr);
    bool applyStepLimit(NineChess::Players previousTurn);
    void handleTimeout();

    // 历史局面快照：historyStates[i] = 已应用 i 条命令后的局面。
    // 只含模型自身状态（位棋盘、编号层、三连历史、命令历史等，总计 KB 量级），
    // 不含 AI 的置换表等搜索状态；模型命令的原子性本身也依赖 NineChess 拷贝。
    void resetHistorySnapshots();
    void appendHistorySnapshot();
    int historyStateIndexForRow(int row) const;

    // 棋局模型（真值）。historyStates 为其逐手快照，供历史浏览 O(1) 跳转与悔棋回退。
    NineChess chess;
    QVector<NineChess> historyStates;

    // 走子路径通过 syncManualListFromChess 自动推进浏览行时置位：
    // 该链路触发的 phaseChange 不重放音效（音效由走子路径唯一负责），
    // 避免同一 QSoundEffect 在同一事件里被连续 play() 两次互相打断而偶发无声。
    bool skipNextBrowseSound = false;

    AiThread ai1;
    AiThread ai2;

    GameScene &scene;
    QList<PieceItem *> pieceList;
    PieceItem *currentPiece;
    QVector<int> player1DisplaySlots;
    QVector<int> player2DisplaySlots;
    uint32_t player1DisplayBoard;
    uint32_t player2DisplayBoard;

    int currentRow;
    bool isEditing;
    bool isInverted;
    bool isEngine1;
    bool isEngine2;
    bool hasAnimation;
    int durationTime;
    bool hasSound;

    int timeID;
    int ruleNo;
    int timeLimit;
    int stepsLimit;
    int displayTime1MS;
    int displayTime2MS;
    int64_t player1ElapsedMS;
    int64_t player2ElapsedMS;
    int64_t turnStartTimeMS;
    NineChess::Players forcedAiTimeoutTurn;
    // 本局是否由外部裁定结束（超时判负）；gameReset 清除
    bool endedByAdjudication = false;

    QString message;
    QStringListModel manualListModel;
    QMap<QString, QSoundEffect*> soundCache;
    AiDispatchState aiDispatch1;
    AiDispatchState aiDispatch2;
    uint64_t gameStateRevision = 0;
};
