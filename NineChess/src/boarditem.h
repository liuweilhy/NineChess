/****************************************************************************
** BoardItem - 棋盘图形项
**
** 继承自QGraphicsItem，作为棋盘的可视化组件
** 棋盘为3圈×8位 = 24个落子点的结构
**
** 主要职责：
**   - 绘制棋盘图形（方框、连线和斜线）
**   - 提供坐标转换工具方法
**   - 查找最近的落子点
****************************************************************************/

#pragma once

#include <QGraphicsItem>

/**
 * @brief 棋盘图形项类
 *
 * 继承自QGraphicsItem，负责棋盘的绘制。
 * 棋盘由3圈×8位 = 24个落子点组成。
 */
class BoardItem : public QGraphicsItem
{
public:
    // ==================== 常量定义 ====================
    // @brief 棋盘圈数（内圈、中圈、外圈），禁止修改
    static const int RING = 3;
    // @brief 每圈落子点数，禁止修改
    static const int SEAT = 8;

    // ==================== 构造函数 ====================
    // @brief 构造函数
    explicit BoardItem(QGraphicsItem *parent = nullptr);
    // @brief 析构函数
    ~BoardItem();

    // ==================== 图形项虚函数 ====================
    // @brief 获取边界矩形
    QRectF boundingRect() const;
    // @brief 获取形状路径
    QPainterPath shape() const;
    // @brief 绘制棋盘
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
        QWidget * widget = nullptr);

    // ==================== 类型常量 ====================
    // @brief 图形项类型值，用于qgraphicsitem_cast判断
    enum { Type = UserType + 1 };
    // @brief 返回类型值
    int type() const { return Type; }

    // ==================== 棋盘操作 ====================
    // @brief 设置是否有斜线
    void setDiagonal(bool arg = true);

    // ==================== 坐标转换 ====================
    // @brief 获取最近的落子点位置
    QPointF nearestPosition(QPointF pos);
    // @brief 将圈位(c,p)转换为场景坐标
    QPointF cp2pos(int c, int p);
    // @brief 将场景坐标转换为圈位(c,p)
    bool pos2cp(QPointF pos, int &c, int &p);

    // ==================== 私有成员变量 ====================
private:
    qreal size;                  // 棋盘尺寸
    qreal sizeShadow;            // 阴影尺寸
    QPointF position[RING * SEAT];  // 24个落子点坐标
    bool hasDiagonalLines;       // 是否有斜线
};


