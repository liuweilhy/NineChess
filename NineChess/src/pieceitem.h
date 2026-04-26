/****************************************************************************
** PieceItem - 棋子图形项
**
** 继承自QObject和QGraphicsItem，作为棋子的可视化组件
** 每个棋子包含类型（空/黑/白）、序号、选中状态等信息
**
** 主要职责：
**   - 绘制棋子图形（黑子、白子）
**   - 显示棋子序号（九连棋规则）
**   - 显示选中状态和删除状态
****************************************************************************/

#pragma once

#include <QObject>
#include <QGraphicsItem>

/**
 * @brief 棋子图形项类
 *
 * 继承自QObject和QGraphicsItem，负责棋子的绘制和交互。
 * 每个棋子有三种类型：空、黑、白。
 */
class PieceItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

    // ==================== 属性定义 ====================
    // @brief 位置属性，用于动画
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)

public:
    // ==================== 棋子模型枚举 ====================
    // @brief 棋子类型枚举
    enum Models {
        noPiece = 0x1,      ///< 空棋子（无棋子）
        blackPiece = 0x2,   ///< 黑色棋子
        whitePiece = 0x4,   ///< 白色棋子
    };

    // ==================== 构造函数 ====================
    // @brief 构造函数
    explicit PieceItem(QGraphicsItem *parent = nullptr);
    // @brief 析构函数
    ~PieceItem();

    // ==================== 图形项虚函数 ====================
    // @brief 获取边界矩形
    QRectF boundingRect() const;
    // @brief 获取形状路径
    QPainterPath shape() const;
    // @brief 绘制棋子
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
        QWidget * widget = nullptr);

    // ==================== 类型常量 ====================
    // @brief 图形项类型值，用于qgraphicsitem_cast判断
    enum { Type = UserType + 2 };
    // @brief 返回类型值
    int type() const { return Type; }

    // ==================== 模型操作 ====================
    // @brief 获取棋子模型
    Models getModel() const { return model; }
    // @brief 设置棋子模型
    void setModel(Models model);

    // @brief 获取棋子序号
    int getNum() const { return num; }
    // @brief 设置棋子序号
    void setNum(int n) { num = n; }

    // @brief 获取是否已删除
    bool isDeleted() const { return deleted; }
    // @brief 设置删除状态
    void setDeleted(bool deleted = true);

    // @brief 设置是否显示序号
    void setShowNum(bool show = true) { showNum = show; }

    // ==================== 事件处理 ====================
protected:
    // @brief 鼠标按下事件
    void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
    // @brief 鼠标移动事件
    void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
    // @brief 鼠标释放事件
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);

    // ==================== 私有成员变量 ====================
private:
    Models model;                 ///< 棋子类型
    int num;                     ///< 棋子序号（黑白各从1开始）
    qreal size;                  ///< 棋子尺寸
    bool deleted;                ///< 是否已删除（有删除线）
    bool showNum;               ///< 是否显示序号
    qreal chooseLineWeight;       ///< 选中线宽度
    qreal removeLineWeight;       ///< 删除线宽度
    QColor chooseLineColor;       ///< 选中线颜色
    QColor removeLineColor;       ///< 删除线颜色
};

