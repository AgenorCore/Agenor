// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZC_UTIL_H_
#define ZC_UTIL_H_

#include "fs.h"
#include "sapling/uint252.h"
#include "uint256.h"

#include <sodium.h>
#include <vector>
#include <cstdint>

std::vector<unsigned char> convertIntToVectorLE(const uint64_t val_int);
std::vector<bool> convertBytesVectorToVector(const std::vector<unsigned char>& bytes);
uint64_t convertVectorToInt(const std::vector<bool>& v);

// random number generator using sodium.
uint256 random_uint256();
uint252 random_uint252();

#endif // ZC_UTIL_H_
