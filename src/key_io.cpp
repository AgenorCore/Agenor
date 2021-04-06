// Copyright (c) 2014-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key_io.h"

#include "base58.h"
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <assert.h>
#include <string.h>
#include <algorithm>

namespace KeyIO {

    CKey DecodeSecret(const std::string &str) {
        CKey key;
        std::vector<unsigned char> data;
        if (DecodeBase58Check(str, data)) {
            const std::vector<unsigned char> &privkey_prefix = Params().Base58Prefix(CChainParams::SECRET_KEY);
            if ((data.size() == 32 + privkey_prefix.size() ||
                 (data.size() == 33 + privkey_prefix.size() && data.back() == 1)) &&
                std::equal(privkey_prefix.begin(), privkey_prefix.end(), data.begin())) {
                bool compressed = data.size() == 33 + privkey_prefix.size();
                key.Set(data.begin() + privkey_prefix.size(), data.begin() + privkey_prefix.size() + 32, compressed);
            }
        }
        if (!data.empty()) {
            memory_cleanse(data.data(), data.size());
        }
        return key;
    }

    std::string EncodeSecret(const CKey &key) {
        assert(key.IsValid());
        std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::SECRET_KEY);
        data.insert(data.end(), key.begin(), key.end());
        if (key.IsCompressed()) {
            data.push_back(1);
        }
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

    CExtKey DecodeExtKey(const std::string &str) {
        CExtKey key;
        std::vector<unsigned char> data;
        if (DecodeBase58Check(str, data)) {
            const std::vector<unsigned char> &prefix = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
            if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() &&
                std::equal(prefix.begin(), prefix.end(), data.begin())) {
                key.Decode(data.data() + prefix.size());
            }
        }
        return key;
    }

    std::string EncodeExtKey(const CExtKey &key) {
        std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
        size_t size = data.size();
        data.resize(size + BIP32_EXTKEY_SIZE);
        key.Encode(data.data() + size);
        std::string ret = EncodeBase58Check(data);
        memory_cleanse(data.data(), data.size());
        return ret;
    }

}// namespace