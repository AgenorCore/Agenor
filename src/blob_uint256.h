// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Agenor_BLOB_UINT256_H
#define Agenor_BLOB_UINT256_H

#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

/** Template base class for fixed-sized opaque blobs. */
template<unsigned int BITS>
class base_blob
{
public:
    // todo: make this protected
    enum { WIDTH=BITS/8 };
    uint8_t data[WIDTH];

    base_blob()
    {
        SetNull();
    }

    explicit base_blob(const std::vector<unsigned char>& vch);

    bool IsNull() const
    {
        for (int i = 0; i < WIDTH; i++)
            if (data[i] != 0)
                return false;
        return true;
    }

    void SetNull()
    {
        memset(data, 0, sizeof(data));
    }

    friend inline bool operator==(const base_blob& a, const base_blob& b) { return memcmp(a.data, b.data, sizeof(a.data)) == 0; }
    friend inline bool operator!=(const base_blob& a, const base_blob& b) { return memcmp(a.data, b.data, sizeof(a.data)) != 0; }
    friend inline bool operator<(const base_blob& a, const base_blob& b) { return memcmp(a.data, b.data, sizeof(a.data)) < 0; }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);
    std::string ToString() const;

    unsigned char* begin()
    {
        return &data[0];
    }

    unsigned char* end()
    {
        return &data[WIDTH];
    }

    const unsigned char* begin() const
    {
        return &data[0];
    }

    const unsigned char* end() const
    {
        return &data[WIDTH];
    }

    unsigned int size() const
    {
        return sizeof(data);
    }

    uint64_t GetUint64(int pos) const
    {
        const uint8_t* ptr = data + pos * 8;
        return ((uint64_t)ptr[0]) | \
               ((uint64_t)ptr[1]) << 8 | \
               ((uint64_t)ptr[2]) << 16 | \
               ((uint64_t)ptr[3]) << 24 | \
               ((uint64_t)ptr[4]) << 32 | \
               ((uint64_t)ptr[5]) << 40 | \
               ((uint64_t)ptr[6]) << 48 | \
               ((uint64_t)ptr[7]) << 56;
    }

    template<typename Stream>
    void Serialize(Stream& s) const
    {
        s.write((char*)data, sizeof(data));
    }

    template<typename Stream>
    void Unserialize(Stream& s)
    {
        s.read((char*)data, sizeof(data));
    }
};

/** 88-bit opaque blob.
 */
class blob88 : public base_blob<88> {
public:
    blob88() {}
    blob88(const base_blob<88>& b) : base_blob<88>(b) {}
    explicit blob88(const std::vector<unsigned char>& vch) : base_blob<88>(vch) {}
};

/** 160-bit opaque blob.
 * @note This type is called uint160 for historical reasons only. It is an opaque
 * blob of 160 bits and has no integer operations.
 */
class blob_uint160 : public base_blob<160> {
public:
    blob_uint160() {}
    blob_uint160(const base_blob<160>& b) : base_blob<160>(b) {}
    explicit blob_uint160(const std::vector<unsigned char>& vch) : base_blob<160>(vch) {}
};

/** 256-bit opaque blob.
 * @note This type is called uint256 for historical reasons only. It is an
 * opaque blob of 256 bits and has no integer operations. Use arith_uint256 if
 * those are required.
 */
class blob_uint256 : public base_blob<256> {
public:
    blob_uint256() {}
    blob_uint256(const base_blob<256>& b) : base_blob<256>(b) {}
    explicit blob_uint256(const std::vector<unsigned char>& vch) : base_blob<256>(vch) {}

    /** A cheap hash function that just returns 64 bits from the result, it can be
     * used when the contents are considered uniformly random. It is not appropriate
     * when the value can easily be influenced from outside as e.g. a network adversary could
     * provide values to trigger worst-case behavior.
     * @note The result of this function is not stable between little and big endian.
     */
    uint64_t GetCheapHash() const
    {
        uint64_t result;
        memcpy((void*)&result, (void*)data, 8);
        return result;
    }

};

/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching uint256(0).
 */
inline blob_uint256 blob_uint256S(const char *str)
{
    blob_uint256 rv;
    rv.SetHex(str);
    return rv;
}
/* uint256 from std::string.
 * This is a separate function because the constructor uint256(const std::string &str) can result
 * in dangerously catching uint256(0) via std::string(const char*).
 */
inline blob_uint256 blob_uint256S(const std::string& str)
{
    return blob_uint256S(str.c_str());
}

/** constant uint256 instances */
const blob_uint256 BLOB_UINT256_ZERO = blob_uint256();
const blob_uint256 BLOB_UINT256_ONE = blob_uint256S("0000000000000000000000000000000000000000000000000000000000000001");

#endif // PIVX_BLOB_UINT256_H
