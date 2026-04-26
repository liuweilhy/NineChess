#pragma once

#include <QGraphicsView>

class QResizeEvent;

// GameView only handles viewport fitting.
// Board-state transforms live in GameController + NineChess.
class GameView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit GameView(QWidget *parent);
    ~GameView();

protected:
    void resizeEvent(QResizeEvent *event) override;
};
