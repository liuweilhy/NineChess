/****************************************************************************
** GameScene - 游戏场景
**
** 继承自QGraphicsScene，作为游戏的视图层
** 负责处理鼠标事件并传递给控制器
**
** 主要职责：
**   - 管理棋盘和棋子的显示
**   - 处理鼠标输入（点击、释放）
**   - 提供坐标转换工具方法
****************************************************************************/

#pragma once

#include <QGraphicsScene>

class BoardItem;

/**
 * @brief 游戏场景类
 *
 * 继承自QGraphicsScene，作为MVC模型中的视图组件。
 * 负责管理棋盘和棋子的显示，处理用户鼠标输入。
 */
class GameScene : public QGraphicsScene
{
    Q_OBJECT

public:
    // ==================== 构造函数 ====================
    // @brief 构造函数
    explicit GameScene(QObject *parent = nullptr);
    // @brief 析构函数
    ~GameScene();

    // ==================== 坐标转换 ====================
    // @brief 将圈位坐标(c,p)转换为场景坐标
    QPointF cp2pos(int c, int p);
    // @brief 将场景坐标转换为圈位坐标(c,p)
    bool pos2cp(QPointF pos, int &c, int &p);

    // ==================== 棋盘设置 ====================
    // @brief 设置棋盘斜线显示
    void setDiagonal(bool arg = true);

    // ==================== 棋子盒位置（常量） ====================
    // @brief 玩家1的己方棋盒位置
    const QPointF pos_p1;
    // @brief 玩家1的对方棋盒位置（被吃掉的棋子放置处）
    const QPointF pos_p1_g;
    // @brief 玩家2的己方棋盒位置
    const QPointF pos_p2;
    // @brief 玩家2的对方棋盒位置（被吃掉的棋子放置处）
    const QPointF pos_p2_g;

    // ==================== 事件处理 ====================
protected:
    // @brief 鼠标双击事件（屏蔽双击操作）
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *mouseEvent);
    // @brief 鼠标按下事件
    void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
    // @brief 鼠标释放事件
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);

    // ==================== 信号 ====================
signals:
    // @brief 鼠标释放信号，携带场景坐标
    void mouseReleased(QPointF);

    // ==================== 私有成员变量 ====================
private:
    BoardItem *board;              // 棋盘对象
};

