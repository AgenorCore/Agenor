// Copyright (c) 2017-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "reverse_iterate.h"
#include "util.h"
#include "libzerocoin/Denominations.h"
#include "zage/zerocoin.h"
#include <list>
#include <map>
std::vector<CMintMeta> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue,
                                               int nMaxNumberOfSpends,
                                               int& nCoinsReturned,
                                               const std::list<CMintMeta>& listMints,
                                               const std::map<libzerocoin::CoinDenomination, CAmount> mapDenomsHeld,
                                               int& nNeededSpends
                                               );

int calculateChange(
    int nMaxNumberOfSpends,
    const CAmount nValueTarget,
    const std::map<libzerocoin::CoinDenomination, CAmount>& mapOfDenomsHeld,
    std::map<libzerocoin::CoinDenomination, CAmount>& mapOfDenomsUsed);

void listSpends(const std::vector<CMintMeta>& vSelectedMints);
