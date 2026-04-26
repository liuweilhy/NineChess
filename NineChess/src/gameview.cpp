#include "gameview.h"

GameView::GameView(QWidget *parent)
    : QGraphicsView(parent)
{
}

GameView::~GameView()
{
}

void GameView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    fitInView(sceneRect(), Qt::KeepAspectRatio);
}
