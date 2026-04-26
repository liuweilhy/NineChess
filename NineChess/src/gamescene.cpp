/****************************************************************************
** GameScene - 游戏场景实现
**
** 负责棋盘初始化和鼠标事件处理
****************************************************************************/

#include "gamescene.h"
#include "pieceitem.h"
#include "boarditem.h"
#include "graphicsconst.h"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QDebug>

// ==================== 构造函数 ====================
GameScene::GameScene(QObject *parent) : QGraphicsScene(parent),
    board(nullptr),
    pos_p1(LINE_INTERVAL * 4, LINE_INTERVAL * 6),
    pos_p1_g(LINE_INTERVAL* (-4), LINE_INTERVAL * 6),
    pos_p2(LINE_INTERVAL * (-4), LINE_INTERVAL * (-6)),
    pos_p2_g(LINE_INTERVAL * 4, LINE_INTERVAL * (-6))
{
    // 添加棋盘
    board = new BoardItem;
    board->setDiagonal(false);
    addItem(board);
}

GameScene::~GameScene()
{
    if(board)
        delete board;
}

// 屏蔽掉Shift和Control按键，事实证明没用，按键事件未必由视图类处理
/*
void GameScene::keyPressEvent(QKeyEvent *keyEvent)
{
    if(keyEvent->key() == Qt::Key_Shift || keyEvent->key() == Qt::Key_Control)
        return;
    QGraphicsScene::keyPressEvent(keyEvent);
}
*/

// ==================== 事件处理 ====================
// 屏蔽双击事件
void GameScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    //屏蔽双击事件
    mouseEvent->accept();
}

// 屏蔽鼠标按下事件
void GameScene::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    //屏蔽鼠标按下事件
    mouseEvent->accept();
    /*
    // 只处理左键事件
    if(mouseEvent->button() != Qt::LeftButton)
        return;
    // 如果不是棋子则结束
    QGraphicsItem *item = itemAt(mouseEvent->scenePos(), QTransform());
    if (!item || item->type() != PieceItem::Type)
    {
        return;
    }

    // 调用默认事件处理函数
    //QGraphicsScene::mousePressEvent(mouseEvent);
    */
}

// 处理鼠标释放事件，发送点击位置信号
void GameScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    // 只处理左键事件
    if (mouseEvent->button() != Qt::LeftButton) {
        mouseEvent->accept();
        return;
    }

    // 如果是棋盘
    QGraphicsItem *item = itemAt(mouseEvent->scenePos(), QTransform());
    if (!item || item->type() == BoardItem::Type)
    {
        QPointF p = mouseEvent->scenePos();
        p = board->nearestPosition(p);
        if (p != QPointF(0, 0))
            // 发送鼠标点最近的落子点
            emit mouseReleased(p);
    }// 如果是棋子
    else if (item->type() == PieceItem::Type)
    {
        // 将当前棋子在场景中的位置发送出去
        emit mouseReleased(item->scenePos());
    }

    mouseEvent->accept();

    // 调用默认事件处理函数
    //QGraphicsScene::mouseReleaseEvent(mouseEvent);
}

// ==================== 坐标转换 ====================
// 将圈位坐标(c,p)转换为场景坐标
QPointF GameScene::cp2pos(int c, int p)
{
    return board->cp2pos(c, p);
}

// 将场景坐标转换为圈位坐标(c,p)
bool GameScene::pos2cp(QPointF pos, int &c, int &p)
{
    return board->pos2cp(pos, c, p);
}

// 设置棋盘斜线显示
void GameScene::setDiagonal(bool arg)
{
    if (board)
        board->setDiagonal(arg);
}

