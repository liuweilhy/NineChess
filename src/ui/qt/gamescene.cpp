﻿/*****************************************************************************
 * Copyright (C) 2018-2019 MillGame authors
 *
 * Authors: liuweilhy <liuweilhy@163.com>
 *          Calcitem <calcitem@outlook.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QDebug>

#include "gamescene.h"
#include "pieceitem.h"
#include "boarditem.h"
#include "graphicsconst.h"

GameScene::GameScene(QObject *parent) :
    QGraphicsScene(parent),
    pos_p1(LINE_INTERVAL * 4, LINE_INTERVAL * 6),
    pos_p1_g(LINE_INTERVAL *(-4), LINE_INTERVAL * 6),
    pos_p2(LINE_INTERVAL *(-4), LINE_INTERVAL *(-6)),
    pos_p2_g(LINE_INTERVAL * 4, LINE_INTERVAL *(-6))
{
    // 添加棋盘
    board = new BoardItem;
    board->setDiagonal(false);
    addItem(board);
}

GameScene::~GameScene()
{
    delete board;
}

// 屏蔽掉Shift和Control按键，事实证明没用，按键事件未必由视图类处理
#if 0
void GameScene::keyPressEvent(QKeyEvent *keyEvent)
{
    if(keyEvent->key() == Qt::Key_Shift || keyEvent->key() == Qt::Key_Control)
        return;
    QGraphicsScene::keyPressEvent(keyEvent);
}
#endif

void GameScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    //屏蔽双击事件
    mouseEvent->accept();
}


void GameScene::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    // 屏蔽鼠标按下事件
    mouseEvent->accept();

#if 0
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
#endif
}

void GameScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    // 只处理左键事件
    if (mouseEvent->button() != Qt::LeftButton) {
        mouseEvent->accept();
        return;
    }

    // 如果是棋盘
    QGraphicsItem *item = itemAt(mouseEvent->scenePos(), QTransform());

    if (!item || item->type() == BoardItem::Type) {
        QPointF p = mouseEvent->scenePos();
        p = board->nearestPosition(p);
        if (p != QPointF(0, 0))
            // 发送鼠标点最近的落子点
            emit mouseReleased(p);
    } // 如果是棋子
    else if (item->type() == PieceItem::Type) {
        // 将当前棋子在场景中的位置发送出去
        emit mouseReleased(item->scenePos());
    }

    mouseEvent->accept();

    // 调用默认事件处理函数
    //QGraphicsScene::mouseReleaseEvent(mouseEvent);
}

QPointF GameScene::rs2pos(int r, int s)
{
    return board->rs2pos(r, s);
}

bool GameScene::pos2rs(QPointF pos, int &r, int &s)
{
    return board->pos2rs(pos, r, s);
}

void GameScene::setDiagonal(bool arg /*= true*/)
{
    if (board) {
        board->setDiagonal(arg);
    }
}