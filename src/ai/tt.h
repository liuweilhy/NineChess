﻿#ifndef TT_H
#define TT_H

#include "config.h"
#include "types.h"
#include "millgame.h"
#include "search.h"
#include "hashmap.h"

using namespace CTSL;

#ifdef TRANSPOSITION_TABLE_ENABLE

class TranspositionTable
{
public:
    // 定义哈希值的类型
    enum HashType : uint8_t
    {
        hashfEMPTY = 0,
        hashfALPHA = 1, // 结点的值最多是 value
        hashfBETA = 2,  // 结点的值至少是 value
        hashfEXACT = 3  // 结点值 value 是准确值
    };

    // 定义哈希表的值
    struct HashValue
    {
        value_t value;
        depth_t depth;
        enum HashType type;
        move_t bestMove;
    };

    // 查找哈希表
    static bool findHash(hash_t hash, HashValue &hashValue);
    static value_t probeHash(hash_t hash, depth_t depth, value_t alpha, value_t beta, move_t &bestMove, HashType &type);

    // 插入哈希表
    static int recordHash(value_t value, depth_t depth, HashType type, hash_t hash, move_t bestMove);

    // 清空置换表
    static void clearTranspositionTable();
};

extern HashMap<hash_t, TranspositionTable::HashValue> transpositionTable;

#endif  // TRANSPOSITION_TABLE_ENABLE

#endif /* TT_H */
