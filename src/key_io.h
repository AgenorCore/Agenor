// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Agenor_KEY_IO_H
#define Agenor_KEY_IO_H

#include "chainparams.h"
#include "key.h"
#include "pubkey.h"
#include "script/standard.h"

#include <string>

namespace KeyIO {

    CKey DecodeSecret(const std::string &str);

    std::string EncodeSecret(const CKey &key);

    CExtKey DecodeExtKey(const std::string &str);

    std::string EncodeExtKey(const CExtKey &extkey);

}

#endif //PIVX_KEY_IO_H
