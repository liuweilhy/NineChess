#include <QDebug>
#include "aithread.h"

AiThread::AiThread(int id, QObject *parent) : QThread(parent),
    waiting_(false),
    aiDepth(8),
    aiTime(10)
{
    this->id = id;
    // 每手棋的时间预算直接交给 AI 原生的时间控制（SearchOptions::timeLimitMs），
    // 由搜索在预算内自行截断并保留最后一次完整迭代的走法，
    // 不再用外部 QTimer 定时强行 quit（也就不需要补偿信号延迟的魔数）。
    NineChess_AI_AB::SearchOptions options;
    options.timeLimitMs = static_cast<int64_t>(aiTime) * 1000;
    ai_ab.setOptions(options);
}

AiThread::~AiThread()
{
    stop();
    quit();
    wait();
}

void AiThread::setAi(const NineChess &chess)
{
    mutex.lock();
    this->chess = &chess;
    ai_ab.setChess(*(this->chess));
    mutex.unlock();
}

void AiThread::setAi(const NineChess &chess, int depth, int time)
{
    mutex.lock();
    this->chess = &chess;
    ai_ab.setChess(chess);
    aiDepth = depth;
    aiTime = time;
    // 同步更新 AI 原生时间预算，其余搜索配置保持不变
    NineChess_AI_AB::SearchOptions options = ai_ab.getOptions();
    options.timeLimitMs = static_cast<int64_t>(time) * 1000;
    ai_ab.setOptions(options);
    mutex.unlock();
}

void AiThread::run()
{
    // 测试用数据
//    int iTemp = 0;
    // 设一个标识，1号线程只管玩家1，2号线程只管玩家2
    int i = 0;

    qDebug() << "Thread" << id << "start";

    while (!isInterruptionRequested()) {
        mutex.lock();
        if (chess->getTurn() == NineChess::PLAYER1)
            i = 1;
        else if (chess->getTurn() == NineChess::PLAYER2)
            i = 2;
        else
            i = 0;

        if (i != id || waiting_) {
            pauseCondition.wait(&mutex);
            mutex.unlock();
            continue;
        }

        ai_ab.setChess(*chess);
        emit calcStarted();
        mutex.unlock();

        ai_ab.alphaBetaPruning(aiDepth);

        // 检查是否因 pause() 而被强制中断，若是则不发出招法，避免在人类回合发出 AI 指令
        mutex.lock();
        bool wasPaused = waiting_;
        mutex.unlock();

        const char * str = ai_ab.bestMove();
        qDebug() << str;
        if (!wasPaused && strcmp(str, "error!"))
            emit command(str);
//        qDebug() << "Thread" << id << "run" << ++iTemp << "times";
        emit calcFinished();

        // 执行完毕后继续判断
        mutex.lock();
        if (!isInterruptionRequested()) {
            pauseCondition.wait(&mutex);
        }
        mutex.unlock();
    }
    qDebug() << "Thread" << id << "quit";
}

void AiThread::act()
{
    if (isFinished() || !isRunning())
        return;

    mutex.lock();
    waiting_ = false;
    ai_ab.quit();
    mutex.unlock();
}

void AiThread::pause()
{
    mutex.lock();
    waiting_ = true;
    // 同时中断正在进行的搜索，防止搜索完成后仍然 emit command
    ai_ab.quit();
    mutex.unlock();
}

void AiThread::resume()
{
    mutex.lock();
    waiting_ = false;
    pauseCondition.wakeAll();
    mutex.unlock();
}

void AiThread::stop()
{
    if (isFinished() || !isRunning())
        return;

    if (!isInterruptionRequested()) {
        requestInterruption();
        mutex.lock();
        waiting_ = false;
        ai_ab.quit();
        pauseCondition.wakeAll();
        mutex.unlock();
    }
}

