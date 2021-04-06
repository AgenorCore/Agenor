// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAIN_H
#define BITCOIN_CHAIN_H

#include "chainparams.h"
#include "pow.h"
#include "primitives/block.h"
#include "timedata.h"
#include "tinyformat.h"
#include "uint256.h"
#include "util.h"
#include "libzerocoin/Denominations.h"

#include <vector>

class CBlockFileInfo
{
public:
    unsigned int nBlocks;      //!< number of blocks stored in file
    unsigned int nSize;        //!< number of used bytes of block file
    unsigned int nUndoSize;    //!< number of used bytes in the undo file
    unsigned int nHeightFirst; //!< lowest height of block in file
    unsigned int nHeightLast;  //!< highest height of block in file
    uint64_t nTimeFirst;       //!< earliest time of block in file
    uint64_t nTimeLast;        //!< latest time of block in file

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(VARINT(nBlocks));
        READWRITE(VARINT(nSize));
        READWRITE(VARINT(nUndoSize));
        READWRITE(VARINT(nHeightFirst));
        READWRITE(VARINT(nHeightLast));
        READWRITE(VARINT(nTimeFirst));
        READWRITE(VARINT(nTimeLast));
    }

    void SetNull()
    {
        nBlocks = 0;
        nSize = 0;
        nUndoSize = 0;
        nHeightFirst = 0;
        nHeightLast = 0;
        nTimeFirst = 0;
        nTimeLast = 0;
    }

    CBlockFileInfo()
    {
        SetNull();
    }

    std::string ToString() const;

    /** update statistics (does not update nSize) */
    void AddBlock(unsigned int nHeightIn, uint64_t nTimeIn)
    {
        if (nBlocks == 0 || nHeightFirst > nHeightIn)
            nHeightFirst = nHeightIn;
        if (nBlocks == 0 || nTimeFirst > nTimeIn)
            nTimeFirst = nTimeIn;
        nBlocks++;
        if (nHeightIn > nHeightLast)
            nHeightLast = nHeightIn;
        if (nTimeIn > nTimeLast)
            nTimeLast = nTimeIn;
    }
};

struct CDiskBlockPos {
    int nFile;
    unsigned int nPos;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(VARINT(nFile));
        READWRITE(VARINT(nPos));
    }

    CDiskBlockPos()
    {
        SetNull();
    }

    CDiskBlockPos(int nFileIn, unsigned int nPosIn)
    {
        nFile = nFileIn;
        nPos = nPosIn;
    }

    friend bool operator==(const CDiskBlockPos& a, const CDiskBlockPos& b)
    {
        return (a.nFile == b.nFile && a.nPos == b.nPos);
    }

    friend bool operator!=(const CDiskBlockPos& a, const CDiskBlockPos& b)
    {
        return !(a == b);
    }

    void SetNull()
    {
        nFile = -1;
        nPos = 0;
    }
    bool IsNull() const { return (nFile == -1); }
};

enum BlockStatus {
    //! Unused.
    BLOCK_VALID_UNKNOWN = 0,

    //! Parsed, version ok, hash satisfies claimed PoW, 1 <= vtx count <= max, timestamp not in future
    BLOCK_VALID_HEADER = 1,

    //! All parent headers found, difficulty matches, timestamp >= median previous, checkpoint. Implies all parents
    //! are also at least TREE.
    BLOCK_VALID_TREE = 2,

    /**
     * Only first tx is coinbase, 2 <= coinbase input script length <= 100, transactions valid, no duplicate txids,
     * sigops, size, merkle root. Implies all parents are at least TREE but not necessarily TRANSACTIONS. When all
     * parent blocks also have TRANSACTIONS, CBlockIndex::nChainTx will be set.
     */
    BLOCK_VALID_TRANSACTIONS = 3,

    //! Outputs do not overspend inputs, no double spends, coinbase output ok, immature coinbase spends.
    //! Implies all parents are also at least CHAIN.
    BLOCK_VALID_CHAIN = 4,

    //! Scripts & signatures ok. Implies all parents are also at least SCRIPTS.
    BLOCK_VALID_SCRIPTS = 5,

    //! All validity bits.
    BLOCK_VALID_MASK = BLOCK_VALID_HEADER | BLOCK_VALID_TREE | BLOCK_VALID_TRANSACTIONS |
                       BLOCK_VALID_CHAIN |
                       BLOCK_VALID_SCRIPTS,

    BLOCK_HAVE_DATA = 8,  //! full block available in blk*.dat
    BLOCK_HAVE_UNDO = 16, //! undo data available in rev*.dat
    BLOCK_HAVE_MASK = BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO,

    BLOCK_FAILED_VALID = 32, //! stage after last reached validness failed
    BLOCK_FAILED_CHILD = 64, //! descends from failed block
    BLOCK_FAILED_MASK = BLOCK_FAILED_VALID | BLOCK_FAILED_CHILD,
};

// BlockIndex flags
enum {
    BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
    BLOCK_STAKE_ENTROPY = (1 << 1),  // entropy bit for stake modifier
    BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier
};

/** The block chain is a tree shaped structure starting with the
 * genesis block at the root, with each block potentially having multiple
 * candidates to be the next block. A blockindex may have multiple pprev pointing
 * to it, but at most one of them can be part of the currently active branch.
 */
class CBlockIndex
{
public:
    //! pointer to the hash of the block, if any. memory is owned by this CBlockIndex
    const uint256* phashBlock{nullptr};

    //! pointer to the index of the predecessor of this block
    CBlockIndex* pprev{nullptr};

    //! pointer to the index of some further predecessor of this block
    CBlockIndex* pskip{nullptr};

    //! height of the entry in the chain. The genesis block has height 0
    int nHeight{0};

    //! Which # file this block is stored in (blk?????.dat)
    int nFile{0};

    //! Byte offset within blk?????.dat where this block's data is stored
    unsigned int nDataPos{0};

    //! Byte offset within rev?????.dat where this block's undo data is stored
    unsigned int nUndoPos{0};

    //! (memory only) Total amount of work (expected number of hashes) in the chain up to and including this block
    uint256 nChainWork{};

    //! Number of transactions in this block.
    //! Note: in a potential headers-first mode, this number cannot be relied upon
    unsigned int nTx{0};

    //! (memory only) Number of transactions in the chain up to and including this block.
    //! This value will be non-zero only if and only if transactions for this block and all its parents are available.
    //! Change to 64-bit type when necessary; won't happen before 2030
    unsigned int nChainTx{0};

    //! Verification status of this block. See enum BlockStatus
    unsigned int nStatus{0};

    // proof-of-stake specific fields
    // char vector holding the stake modifier bytes. It is empty for PoW blocks.
    // Modifier V1 is 64 bit while modifier V2 is 256 bit.
    std::vector<unsigned char> vStakeModifier{};
    unsigned int nFlags{0};

    //! block header
    int nVersion{0};
    uint256 hashMerkleRoot{};
    unsigned int nTime{0};
    unsigned int nBits{0};
    unsigned int nNonce{0};
    uint256 nAccumulatorCheckpoint{};

    //! (memory only) Sequential id assigned to distinguish order in which blocks are received.
    uint32_t nSequenceId{0};

    CBlockIndex() {}
    CBlockIndex(const CBlock& block);

    std::string ToString() const;

    CDiskBlockPos GetBlockPos() const;
    CDiskBlockPos GetUndoPos() const;
    CBlockHeader GetBlockHeader() const;
    uint256 GetBlockHash() const { return *phashBlock; }
    int64_t GetBlockTime() const { return (int64_t)nTime; }
    int64_t GetMedianTimePast() const;

    int64_t MaxFutureBlockTime() const;
    int64_t MinPastBlockTime() const;

    bool IsProofOfStake() const { return (nFlags & BLOCK_PROOF_OF_STAKE); }
    bool IsProofOfWork() const { return !IsProofOfStake(); }
    void SetProofOfStake() { nFlags |= BLOCK_PROOF_OF_STAKE; }

    // Stake Modifier
    unsigned int GetStakeEntropyBit() const;
    bool SetStakeEntropyBit(unsigned int nEntropyBit);
    bool GeneratedStakeModifier() const { return (nFlags & BLOCK_STAKE_MODIFIER); }
    void SetStakeModifier(const uint64_t nStakeModifier, bool fGeneratedStakeModifier);
    void SetNewStakeModifier();                             // generates and sets new v1 modifier
    void SetStakeModifier(const uint256& nStakeModifier);
    void SetNewStakeModifier(const uint256& prevoutId);     // generates and sets new v2 modifier
    uint64_t GetStakeModifierV1() const;
    uint256 GetStakeModifierV2() const;

    //! Check whether this block index entry is valid up to the passed validity level.
    bool IsValid(enum BlockStatus nUpTo = BLOCK_VALID_TRANSACTIONS) const;
    //! Raise the validity level of this block index entry.
    //! Returns true if the validity was changed.
    bool RaiseValidity(enum BlockStatus nUpTo);
    //! Build the skiplist pointer for this entry.
    void BuildSkip();
    //! Efficiently find an ancestor of this block.
    CBlockIndex* GetAncestor(int height);
    const CBlockIndex* GetAncestor(int height) const;
};

/** Used to marshal pointers into hashes for db storage. */

// New serialization introduced with 4.0.99
static const int DBI_OLD_SER_VERSION = 9900;
static const int DBI_SER_VERSION_NO_ZC = 9902;   // removes mapZerocoinSupply, nMoneySupply

class CDiskBlockIndex : public CBlockIndex
{
public:
    uint256 hashPrev;

    CDiskBlockIndex()
    {
        hashPrev = UINT256_ZERO;
    }

    explicit CDiskBlockIndex(const CBlockIndex* pindex) : CBlockIndex(*pindex)
    {
        hashPrev = (pprev ? pprev->GetBlockHash() : UINT256_ZERO);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nSerVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(VARINT(nSerVersion));

        READWRITE(VARINT(nHeight));
        READWRITE(VARINT(nStatus));
        READWRITE(VARINT(nTx));
        if (nStatus & (BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO))
            READWRITE(VARINT(nFile));
        if (nStatus & BLOCK_HAVE_DATA)
            READWRITE(VARINT(nDataPos));
        if (nStatus & BLOCK_HAVE_UNDO)
            READWRITE(VARINT(nUndoPos));

        if (nSerVersion >= DBI_SER_VERSION_NO_ZC) {
            // Serialization with CLIENT_VERSION = 4009902+
            READWRITE(nFlags);
            READWRITE(this->nVersion);
            READWRITE(vStakeModifier);
            READWRITE(hashPrev);
            READWRITE(hashMerkleRoot);
            READWRITE(nTime);
            READWRITE(nBits);
            READWRITE(nNonce);

        } else if (nSerVersion > DBI_OLD_SER_VERSION && ser_action.ForRead()) {
            // Serialization with CLIENT_VERSION = 4009901
            std::map<libzerocoin::CoinDenomination, int64_t> mapZerocoinSupply;
            int64_t nMoneySupply = 0;
            READWRITE(nMoneySupply);
            READWRITE(nFlags);
            READWRITE(this->nVersion);
            READWRITE(vStakeModifier);
            READWRITE(hashPrev);
            READWRITE(hashMerkleRoot);
            READWRITE(nTime);
            READWRITE(nBits);
            READWRITE(nNonce);
            if(this->nVersion > 3) {
                READWRITE(mapZerocoinSupply);
            }

        } else if (ser_action.ForRead()) {
            // Serialization with CLIENT_VERSION = 4009900-
            int64_t nMint = 0;
            uint256 hashNext{};
            int64_t nMoneySupply = 0;
            READWRITE(nMint);
            READWRITE(nMoneySupply);
            READWRITE(nFlags);
            if (!Params().GetConsensus().NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V3_4)) {
                uint64_t nStakeModifier = 0;
                READWRITE(nStakeModifier);
                this->SetStakeModifier(nStakeModifier, this->GeneratedStakeModifier());
            } else {
                uint256 nStakeModifierV2;
                READWRITE(nStakeModifierV2);
                this->SetStakeModifier(nStakeModifierV2);
            }
            if (IsProofOfStake()) {
                COutPoint prevoutStake;
                unsigned int nStakeTime = 0;
                READWRITE(prevoutStake);
                READWRITE(nStakeTime);
            }
            READWRITE(this->nVersion);
            READWRITE(hashPrev);
            READWRITE(hashNext);
            READWRITE(hashMerkleRoot);
            READWRITE(nTime);
            READWRITE(nBits);
            READWRITE(nNonce);
            if(this->nVersion > 3) {
                std::map<libzerocoin::CoinDenomination, int64_t> mapZerocoinSupply;
                std::vector<libzerocoin::CoinDenomination> vMintDenominationsInBlock;
                READWRITE(nAccumulatorCheckpoint);
                READWRITE(mapZerocoinSupply);
                READWRITE(vMintDenominationsInBlock);
            }
        }
    }


    uint256 GetBlockHash() const
    {
        CBlockHeader block;
        block.nVersion = nVersion;
        block.hashPrevBlock = hashPrev;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime = nTime;
        block.nBits = nBits;
        block.nNonce = nNonce;
        return block.GetHash();
    }


    std::string ToString() const
    {
        return strprintf("CDiskBlockIndex(%s\n                hashBlock=%s, hashPrev=%s)",
                CBlockIndex::ToString(),
                GetBlockHash().ToString(),
                hashPrev.ToString());
    }
};

/** Legacy block index - used to retrieve old serializations */

class CLegacyBlockIndex : public CBlockIndex
{
public:
    std::map<libzerocoin::CoinDenomination, int64_t> mapZerocoinSupply{};
    int64_t nMint = 0;
    uint256 hashNext{};
    uint256 hashPrev{};
    uint64_t nStakeModifier = 0;
    uint256 nStakeModifierV2{};
    COutPoint prevoutStake{};
    unsigned int nStakeTime = 0;
    std::vector<libzerocoin::CoinDenomination> vMintDenominationsInBlock;
    int64_t nMoneySupply = 0;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        int nSerVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(VARINT(nSerVersion));

        if (nSerVersion >= DBI_SER_VERSION_NO_ZC) {
            // no extra serialized field
            return;
        }

        if (!ser_action.ForRead()) {
            // legacy block index shouldn't be used to write
            return;
        }

        READWRITE(VARINT(nHeight));
        READWRITE(VARINT(nStatus));
        READWRITE(VARINT(nTx));
        if (nStatus & (BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO))
            READWRITE(VARINT(nFile));
        if (nStatus & BLOCK_HAVE_DATA)
            READWRITE(VARINT(nDataPos));
        if (nStatus & BLOCK_HAVE_UNDO)
            READWRITE(VARINT(nUndoPos));

        if (nSerVersion > DBI_OLD_SER_VERSION) {
            // Serialization with CLIENT_VERSION = 4009901
            READWRITE(nMoneySupply);
            READWRITE(nFlags);
            READWRITE(this->nVersion);
            READWRITE(vStakeModifier);
            READWRITE(hashPrev);
            READWRITE(hashMerkleRoot);
            READWRITE(nTime);
            READWRITE(nBits);
            READWRITE(nNonce);
            if(this->nVersion > 3) {
                READWRITE(mapZerocoinSupply);
                if(this->nVersion < 7) READWRITE(nAccumulatorCheckpoint);
            }

        } else {
            // Serialization with CLIENT_VERSION = 4009900-
            READWRITE(nMint);
            READWRITE(nMoneySupply);
            READWRITE(nFlags);
            if (!Params().GetConsensus().NetworkUpgradeActive(nHeight, Consensus::UPGRADE_V3_4)) {
                READWRITE(nStakeModifier);
            } else {
                READWRITE(nStakeModifierV2);
            }
            if (IsProofOfStake()) {
                READWRITE(prevoutStake);
                READWRITE(nStakeTime);
            }
            READWRITE(this->nVersion);
            READWRITE(hashPrev);
            READWRITE(hashNext);
            READWRITE(hashMerkleRoot);
            READWRITE(nTime);
            READWRITE(nBits);
            READWRITE(nNonce);
            if(this->nVersion > 3) {
                READWRITE(nAccumulatorCheckpoint);
                READWRITE(mapZerocoinSupply);
                READWRITE(vMintDenominationsInBlock);
            }
        }
    }
};

/** An in-memory indexed chain of blocks. */
class CChain
{
private:
    std::vector<CBlockIndex*> vChain;

public:
    /** Returns the index entry for the genesis block of this chain, or NULL if none. */
    CBlockIndex* Genesis() const
    {
        return vChain.size() > 0 ? vChain[0] : NULL;
    }

    /** Returns the index entry for the tip of this chain, or NULL if none. */
    CBlockIndex* Tip(bool fProofOfStake = false) const
    {
        if (vChain.size() < 1)
            return NULL;

        CBlockIndex* pindex = vChain[vChain.size() - 1];

        if (fProofOfStake) {
            while (pindex && pindex->pprev && !pindex->IsProofOfStake())
                pindex = pindex->pprev;
        }
        return pindex;
    }

    /** Returns the index entry at a particular height in this chain, or NULL if no such height exists. */
    CBlockIndex* operator[](int nHeight) const
    {
        if (nHeight < 0 || nHeight >= (int)vChain.size())
            return NULL;
        return vChain[nHeight];
    }

    /** Compare two chains efficiently. */
    friend bool operator==(const CChain& a, const CChain& b)
    {
        return a.vChain.size() == b.vChain.size() &&
               a.vChain[a.vChain.size() - 1] == b.vChain[b.vChain.size() - 1];
    }

    /** Efficiently check whether a block is present in this chain. */
    bool Contains(const CBlockIndex* pindex) const
    {
        return (*this)[pindex->nHeight] == pindex;
    }

    /** Find the successor of a block in this chain, or NULL if the given index is not found or is the tip. */
    CBlockIndex* Next(const CBlockIndex* pindex) const
    {
        if (Contains(pindex))
            return (*this)[pindex->nHeight + 1];
        else
            return NULL;
    }

    /** Return the maximal height in the chain. Is equal to chain.Tip() ? chain.Tip()->nHeight : -1. */
    int Height() const
    {
        return vChain.size() - 1;
    }

    /** Set/initialize a chain with a given tip. */
    void SetTip(CBlockIndex* pindex);

    /** Return a CBlockLocator that refers to a block in this chain (by default the tip). */
    CBlockLocator GetLocator(const CBlockIndex* pindex = NULL) const;

    /** Find the last common block between this chain and a block index entry. */
    const CBlockIndex* FindFork(const CBlockIndex* pindex) const;
};

#endif // BITCOIN_CHAIN_H
