﻿/*
  This file is part of Sanmill.
  Copyright (C) 2019-2021 The Sanmill developers (see AUTHORS file)

  Sanmill is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Sanmill is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPTION_H
#define OPTION_H

#include "config.h"

class GameOptions
{
public:
    void setAutoRestart(bool enabled)
    {
        isAutoRestart = enabled;
    }

    bool getAutoRestart() const
    {
        return isAutoRestart;
    }

    void setAutoChangeFirstMove(bool enabled)
    {
        isAutoChangeFirstMove = enabled;
    }

    bool getAutoChangeFirstMove() const
    {
        return isAutoChangeFirstMove;
    }

    void setResignIfMostLose(bool enabled)
    {
        resignIfMostLose = enabled;
    }

    bool getResignIfMostLose() const
    {
        return resignIfMostLose;
    }

    // Specify whether the successors of a given state should be shuffled if a
    // re-evaluation is required so that the AI algorithm is not favoring one
    // state if multiple ones have equal evaluations. This introduces some
    // variation between different games against an opponent that tries to do the
    // same sequence of moves. By default, shuffling is enabled.

    bool getShufflingEnabled() const
    {
        return shufflingEnabled;
    }

    void setShufflingEnabled(bool enabled)
    {
        shufflingEnabled = enabled;
    }

    void setLearnEndgameEnabled(bool enabled)
    {
#ifdef ENDGAME_LEARNING_FORCE
        learnEndgame = true;
#else
        learnEndgame = enabled;
#endif
    }

    bool isEndgameLearningEnabled() const
    {
#ifdef ENDGAME_LEARNING_FORCE
        return  true;
#else
        return learnEndgame;
#endif
    }

    void setIDSEnabled(bool enabled)
    {
        IDSEnabled = enabled;
    }

    bool getIDSEnabled() const
    {
        return IDSEnabled;
    }

    // DepthExtension

    void setDepthExtension(bool enabled)
    {
        depthExtension = enabled;
    }

    bool getDepthExtension() const
    {
        return depthExtension;
    }

    // OpeningBook

    void setOpeningBook(bool enabled)
    {
        openingBook = enabled;
    }

    bool getOpeningBook() const
    {
        return openingBook;
    }

protected:

private:
    bool isAutoRestart { false };
    bool isAutoChangeFirstMove { false };
    bool resignIfMostLose { false };
    bool shufflingEnabled { true };
#ifdef ENDGAME_LEARNING_FORCE
    bool learnEndgame { true };
#else
    bool learnEndgame { false };
#endif
    bool IDSEnabled { false };
    bool depthExtension {true};
    bool openingBook { false };
};

extern GameOptions gameOptions;

#endif /* OPTION_H */
