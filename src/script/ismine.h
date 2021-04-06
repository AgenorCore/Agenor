// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_ISMINE_H
#define BITCOIN_SCRIPT_ISMINE_H

#include "key.h"
#include "script/standard.h"
#include <bitset>

class CKeyStore;
class CScript;

/** IsMine() return codes */
enum isminetype {
    ISMINE_NO = 0,
    ISMINE_WATCH_ONLY = 1 << 0,
    ISMINE_SPENDABLE  = 1 << 1,
    //! Indicates that we have the staking key of a P2CS
    ISMINE_COLD = 1 << 2,
    //! Indicates that we have the spending key of a P2CS
    ISMINE_SPENDABLE_DELEGATED = 1 << 3,
    ISMINE_SPENDABLE_ALL = ISMINE_SPENDABLE_DELEGATED | ISMINE_SPENDABLE,
    ISMINE_ALL = ISMINE_WATCH_ONLY | ISMINE_SPENDABLE | ISMINE_COLD | ISMINE_SPENDABLE_DELEGATED,
    ISMINE_ENUM_ELEMENTS
};
/** used for bitflags of isminetype */
typedef uint8_t isminefilter;

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey);
isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest);

/**
 * Cachable amount subdivided into watchonly and spendable parts.
 */
struct CachableAmount
{
    // NO and ALL are never (supposed to be) cached
    std::bitset<ISMINE_ENUM_ELEMENTS> m_cached;
    CAmount m_value[ISMINE_ENUM_ELEMENTS];
    inline void Reset()
    {
        m_cached.reset();
    }
    void Set(isminefilter filter, CAmount value)
    {
        m_cached.set(filter);
        m_value[filter] = value;
    }
};

#endif // BITCOIN_SCRIPT_ISMINE_H
