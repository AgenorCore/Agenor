// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Agenor_SCRIPT_KEYORIGIN_H
#define Agenor_SCRIPT_KEYORIGIN_H

#include <serialize.h>
#include <vector>

struct KeyOriginInfo
{
    unsigned char fingerprint[4]; //!< First 32 bits of the Hash160 of the public key at the root of the path
    std::vector<uint32_t> path;

    friend bool operator==(const KeyOriginInfo& a, const KeyOriginInfo& b)
    {
        return std::equal(std::begin(a.fingerprint), std::end(a.fingerprint), std::begin(b.fingerprint)) && a.path == b.path;
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(FLATDATA(fingerprint));
        READWRITE(path);
    }

    void clear()
    {
        memset(fingerprint, 0, 4);
        path.clear();
    }

    std::string pathToString() const
    {
        std::string keypath_str = "m";
        for (uint32_t num : path) {
            keypath_str += "/";
            bool hardened = false;
            if (num & 0x80000000) {
                hardened = true;
                num &= ~0x80000000;
            }

            keypath_str += std::to_string(num);
            if (hardened) {
                keypath_str += "'";
            }
        }
        return keypath_str;
    }
};

#endif // PIVX_SCRIPT_KEYORIGIN_H
