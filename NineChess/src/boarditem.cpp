/****************************************************************************
** BoardItem - 棋盘图形项实现
**
** 负责棋盘的绘制、落子点位置计算
****************************************************************************/

#include "boarditem.h"
#include "graphicsconst.h"
#include <QPainter>

// ==================== 构造函数 ====================
BoardItem::BoardItem(QGraphicsItem *parent) : QGraphicsItem(),
    size(BOARD_SIZE),
    sizeShadow(5.0),
    hasDiagonalLines(false)
{
    Q_UNUSED(parent)
    // 棋盘中心放在场景中心
    setPos(0, 0);
    // 初始化24个落子点
    // position[]顺序：从外圈向，上边中点为0，顺时针旋转排序
    for (int i = 0; i < RING; i++)
    {
        // 外圈的12点钟方向为第一个位置，按顺时针方向排序
        // 然后是中圈和内圈
        qreal a = (RING - i) * LINE_INTERVAL;
        position[i*SEAT+0].rx() = 0;
        position[i*SEAT+0].ry() = -a;
        position[i*SEAT+1].rx() = a;
        position[i*SEAT+1].ry() = -a;
        position[i*SEAT+2].rx() = a;
        position[i*SEAT+2].ry() = 0;
        position[i*SEAT+3].rx() = a;
        position[i*SEAT+3].ry() = a;
        position[i*SEAT+4].rx() = 0;
        position[i*SEAT+4].ry() = a;
        position[i*SEAT+5].rx() = -a;
        position[i*SEAT+5].ry() = a;
        position[i*SEAT+6].rx() = -a;
        position[i*SEAT+6].ry() = 0;
        position[i*SEAT+7].rx() = -a;
        position[i*SEAT+7].ry() = -a;
    }
}

BoardItem::~BoardItem()
{

}

// ==================== 图形项虚函数 ====================
QRectF BoardItem::boundingRect() const
{
    return QRectF(-size/2, -size/2, size+sizeShadow, size+sizeShadow);
}

QPainterPath BoardItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

// 设置是否有斜线
void BoardItem::setDiagonal(bool arg)
{
    hasDiagonalLines = arg;
    update(boundingRect());
}

// ==================== 绘制 ====================
// 绘制棋盘：方框、连线和斜线
void BoardItem::paint(QPainter *painter,
                      const QStyleOptionGraphicsItem *option,
                      QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // 填充阴影
    painter->fillRect(boundingRect(), QBrush(QColor(64, 64, 64)));
    // 填充图片
    painter->drawPixmap(-size/2, -size/2, size, size, QPixmap(":/image/resources/image/board.png"));
    // 黑色实线画笔
    QPen pen(QBrush(Qt::black), LINE_WEIGHT, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin);
    painter->setPen(pen);
    // 空画刷
    painter->setBrush(Qt::NoBrush);
    // 画3个方框
    for (int i = 0; i < RING; i++)
    {
        const QPointF points[4] = {
            position[i * SEAT + 7],    // 左上角
            position[i * SEAT + 1],    // 右上角
            position[i * SEAT + 3],    // 右下角
            position[i * SEAT + 5]     // 左下角
        };
        painter->drawPolygon(points, 4);
    }
    // 画4条纵横线
    painter->drawLine(position[0], position[(RING - 1) * SEAT]);
    painter->drawLine(position[2], position[(RING - 1) * SEAT + 2]);
    painter->drawLine(position[4], position[(RING - 1) * SEAT + 4]);
    painter->drawLine(position[6], position[(RING - 1) * SEAT + 6]);
    if (hasDiagonalLines)
    {
        // 画4条斜线
        painter->drawLine(position[1], position[(RING - 1) * SEAT + 1]);
        painter->drawLine(position[3], position[(RING - 1) * SEAT + 3]);
        painter->drawLine(position[5], position[(RING - 1) * SEAT + 5]);
        painter->drawLine(position[7], position[(RING - 1) * SEAT + 7]);
    }
}

// ==================== 坐标转换 ====================
// 获取最近的落子点位置
QPointF BoardItem::nearestPosition(QPointF const pos)
{
    // 初始最近点设为(0,0)点
    QPointF nearestPos = QPointF(0,0);
    // 寻找最近的落子点
    for (int i = 0; i < RING * SEAT; i++)
    {
        // 如果鼠标点距离落子点在棋子半径内
        if (QLineF(pos, position[i]).length() < PIECE_SIZE/2)
        {
            nearestPos = position[i];
            break;
        }
    }
    return nearestPos;
}

// 将圈位(c,p)转换为场景坐标
QPointF BoardItem::cp2pos(int c, int p)
{
    if (c < 0 || c >= RING || p < 0 || p >= SEAT)
        return QPointF();
    return position[c * SEAT + p];
}

// 将场景坐标转换为圈位(c,p)
bool BoardItem::pos2cp(QPointF pos, int &c, int &p)
{
    // 寻找最近的落子点
    for (int i = 0; i < RING*SEAT; i++)
    {
        // 如果pos点在落子点附近
        if (QLineF(pos, position[i]).length() < PIECE_SIZE/6)
        {
            c = i / SEAT;
            p = i % SEAT;
            return true;
        }
    }
    return false;
}
