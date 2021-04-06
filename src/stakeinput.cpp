// Copyright (c) 2017-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stakeinput.h"

#include "chain.h"
#include "main.h"
#include "txdb.h"
#include "zage/deterministicmint.h"
#include "wallet/wallet.h"

bool CAgeStake::InitFromTxIn(const CTxIn& txin)
{
    if (txin.IsZerocoinSpend())
        return error("%s: unable to initialize CAgeStake from zerocoin spend", __func__);

    // Find the previous transaction in database
    uint256 hashBlock;
    CTransaction txPrev;
    if (!GetTransaction(txin.prevout.hash, txPrev, hashBlock, true))
        return error("%s : INFO: read txPrev failed, tx id prev: %s", __func__, txin.prevout.hash.GetHex());
    SetPrevout(txPrev, txin.prevout.n);

    // Find the index of the block of the previous transaction
    if (mapBlockIndex.count(hashBlock)) {
        CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
        if (chainActive.Contains(pindex)) pindexFrom = pindex;
    }
    // Check that the input is in the active chain
    if (!pindexFrom)
        return error("%s : Failed to find the block index for stake origin", __func__);

    // All good
    return true;
}

bool CAgeStake::SetPrevout(CTransaction txPrev, unsigned int n)
{
    this->txFrom = txPrev;
    this->nPosition = n;
    return true;
}

bool CAgeStake::GetTxFrom(CTransaction& tx) const
{
    if (txFrom.IsNull())
        return false;
    tx = txFrom;
    return true;
}

bool CAgeStake::GetTxOutFrom(CTxOut& out) const
{
    if (txFrom.IsNull() || nPosition >= txFrom.vout.size())
        return false;
    out = txFrom.vout[nPosition];
    return true;
}

bool CAgeStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    txIn = CTxIn(txFrom.GetHash(), nPosition);
    return true;
}

CAmount CAgeStake::GetValue() const
{
    return txFrom.vout[nPosition].nValue;
}

bool CAgeStake::CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal, const bool onlyP2PK)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    CScript scriptPubKeyKernel = txFrom.vout[nPosition].scriptPubKey;
    if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
        return error("%s: failed to parse kernel", __func__);

    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH && whichType != TX_COLDSTAKE)
        return error("%s: type=%d (%s) not supported for scriptPubKeyKernel", __func__, whichType, GetTxnOutputType(whichType));

    CScript scriptPubKey;
    CKey key;
    if (whichType == TX_PUBKEYHASH || whichType == TX_COLDSTAKE) {
        // if P2PKH or P2CS check that we have the input private key
        if (!pwallet->GetKey(CKeyID(uint160(vSolutions[0])), key))
            return error("%s: Unable to get staking private key", __func__);
    }

    // Consensus check: P2PKH block signatures were not accepted before v5 update.
    // This can be removed after v5.0 enforcement
    if (whichType == TX_PUBKEYHASH && onlyP2PK) {
        // convert to P2PK inputs
        scriptPubKey << key.GetPubKey() << OP_CHECKSIG;
    } else {
        // keep the same script
        scriptPubKey = scriptPubKeyKernel;
    }

    vout.emplace_back(CTxOut(0, scriptPubKey));

    // Calculate if we need to split the output
    if (pwallet->nStakeSplitThreshold > 0) {
        int nSplit = static_cast<int>(nTotal / pwallet->nStakeSplitThreshold);
        if (nSplit > 1) {
            // if nTotal is twice or more of the threshold; create more outputs
            int txSizeMax = MAX_STANDARD_TX_SIZE >> 11; // limit splits to <10% of the max TX size (/2048)
            if (nSplit > txSizeMax)
                nSplit = txSizeMax;
            for (int i = nSplit; i > 1; i--) {
                LogPrintf("%s: StakeSplit: nTotal = %d; adding output %d of %d\n", __func__, nTotal, (nSplit-i)+2, nSplit);
                vout.emplace_back(CTxOut(0, scriptPubKey));
            }
        }
    }

    return true;
}

CDataStream CAgeStake::GetUniqueness() const
{
    //The unique identifier for a PIV stake is the outpoint
    CDataStream ss(SER_NETWORK, 0);
    ss << nPosition << txFrom.GetHash();
    return ss;
}

//The block that the UTXO was added to the chain
CBlockIndex* CAgeStake::GetIndexFrom()
{
    if (pindexFrom)
        return pindexFrom;
    uint256 hashBlock = UINT256_ZERO;
    CTransaction tx;
    if (GetTransaction(txFrom.GetHash(), tx, hashBlock, true)) {
        // If the index is in the chain, then set it as the "index from"
        if (mapBlockIndex.count(hashBlock)) {
            CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
            if (chainActive.Contains(pindex))
                pindexFrom = pindex;
        }
    } else {
        LogPrintf("%s : failed to find tx %s\n", __func__, txFrom.GetHash().GetHex());
    }

    return pindexFrom;
}

// Verify stake contextual checks
bool CAgeStake::ContextCheck(int nHeight, uint32_t nTime)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    // Get Stake input block time/height
    CBlockIndex* pindexFrom = GetIndexFrom();
    if (!pindexFrom)
        return error("%s: unable to get previous index for stake input", __func__);
    const int nHeightBlockFrom = pindexFrom->nHeight;
    const uint32_t nTimeBlockFrom = pindexFrom->nTime;

    // Check that the stake has the required depth/age
    if (consensus.NetworkUpgradeActive(nHeight + 1, Consensus::UPGRADE_ZC_PUBLIC) &&
            !consensus.HasStakeMinAgeOrDepth(nHeight, nTime, nHeightBlockFrom, nTimeBlockFrom))
        return error("%s : min age violation - height=%d - time=%d, nHeightBlockFrom=%d, nTimeBlockFrom=%d",
                         __func__, nHeight, nTime, nHeightBlockFrom, nTimeBlockFrom);
    // All good
    return true;
}

