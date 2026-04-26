/****************************************************************************
** GameController - 娓告垙鎺у埗鍣?
** MVC 妯″瀷涓殑鎺у埗妯″潡锛岃礋璐ｅ崗璋冭鍥句笌妯″瀷
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
    bool isAnimation() const { return hasAnimation; }
    int getDurationTime() const { return durationTime; }
    QStringListModel* getManualListModel() { return &manualListModel; }

    void setAiDepthTime(int depth1, int time1, int depth2, int time2);
    void getAiDepthTime(int &depth1, int &time1, int &depth2, int &time2);

signals:
    void time1Changed(const QString &time);
    void time2Changed(const QString &time);
    void statusBarChanged(const QString &message);
    void pieceCountsChanged(const QString &player1, const QString &player2);

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
    void playSound(const QString &soundPath);
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

    NineChess chess;
    NineChess chessTemp;

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

    QString message;
    QStringListModel manualListModel;
    QMap<QString, QSoundEffect*> soundCache;
    AiDispatchState aiDispatch1;
    AiDispatchState aiDispatch2;
    uint64_t gameStateRevision = 0;
};
