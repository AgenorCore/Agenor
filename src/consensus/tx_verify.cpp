// Copyright (c) 2017-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tx_verify.h"

#include "consensus/consensus.h"
#include "consensus/zerocoin_verify.h"
#include "main.h"
#include "script/interpreter.h"

bool IsFinalTx(const CTransaction& tx, int nBlockHeight, int64_t nBlockTime)
{
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = chainActive.Height();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    for (const CTxIn& txin : tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    for (const CTxIn& txin : tx.vin) {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const CTxOut& txout : tx.vout) {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase() || tx.HasZerocoinSpendInputs())
        // a tx containing a zc spend can have only zc inputs
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prevout = inputs.AccessCoin(tx.vin[i].prevout).out;
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

bool CheckTransaction(const CTransaction& tx, bool fZerocoinActive, bool fRejectBadUTXO, CValidationState& state, bool fFakeSerialAttack, bool fColdStakingActive)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");

    // Size limits
    unsigned int nMaxSize = MAX_ZEROCOIN_TX_SIZE;

    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > nMaxSize)
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    const Consensus::Params& consensus = Params().GetConsensus();
    CAmount nValueOut = 0;
    for (const CTxOut& txout : tx.vout) {
        if (txout.IsEmpty() && !tx.IsCoinBase() && !tx.IsCoinStake())
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-empty");
        if (txout.nValue < 0)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > consensus.nMaxMoneyOut)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!consensus.MoneyRange(nValueOut))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge");
        // check cold staking enforcement (for delegations) and value out
        if (txout.scriptPubKey.IsPayToColdStaking()) {
            if (!fColdStakingActive)
                return state.DoS(10, false, REJECT_INVALID, "cold-stake-inactive");
            if (txout.nValue < MIN_COLDSTAKING_AMOUNT)
                return state.DoS(100, false, REJECT_INVALID, "cold-stake-vout-toosmall");
        }
    }

    std::set<COutPoint> vInOutPoints;
    std::set<CBigNum> vZerocoinSpendSerials;
    int nZCSpendCount = 0;

    for (const CTxIn& txin : tx.vin) {
        // Check for duplicate inputs
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");

        //duplicate zcspend serials are checked in CheckZerocoinSpend()
        if (!txin.IsZerocoinSpend()) {
            vInOutPoints.insert(txin.prevout);
        } else if (!txin.IsZerocoinPublicSpend()) {
            nZCSpendCount++;
        }
    }

    if (fZerocoinActive) {
        if (nZCSpendCount > consensus.ZC_MaxSpendsPerTx)
            return state.DoS(100, error("CheckTransaction() : there are more zerocoin spends than are allowed in one transaction"));

        //require that a zerocoinspend only has inputs that are zerocoins
        if (tx.HasZerocoinSpendInputs()) {
            for (const CTxIn& in : tx.vin) {
                if (!in.IsZerocoinSpend() && !in.IsZerocoinPublicSpend())
                    return state.DoS(100,
                                     error("CheckTransaction() : zerocoinspend contains inputs that are not zerocoins"));
            }

            // Do not require signature verification if this is initial sync and a block over 24 hours old
            bool fVerifySignature = !IsInitialBlockDownload() && (GetTime() - chainActive.Tip()->GetBlockTime() < (60*60*24));
            if (!CheckZerocoinSpend(tx, fVerifySignature, state, fFakeSerialAttack))
                return state.DoS(100, error("CheckTransaction() : invalid zerocoin spend"));
        }
    }

    if (tx.IsCoinBase()) {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 150)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
    } else if (fZerocoinActive && tx.HasZerocoinSpendInputs()) {
        if (tx.vin.size() < 1)
            return state.DoS(10, false, REJECT_INVALID, "bad-zc-spend-min-inputs");
        if (tx.HasZerocoinPublicSpendInputs()) {
            // tx has public zerocoin spend inputs
            if(static_cast<int>(tx.vin.size()) > consensus.ZC_MaxPublicSpendsPerTx)
                return state.DoS(10, false, REJECT_INVALID, "bad-zc-spend-max-inputs");
        } else {
            // tx has regular zerocoin spend inputs
            if(static_cast<int>(tx.vin.size()) > consensus.ZC_MaxSpendsPerTx)
                return state.DoS(10, false, REJECT_INVALID, "bad-zc-spend-max-inputs");
        }

    } else {
        for (const CTxIn& txin : tx.vin)
            if (txin.prevout.IsNull() && (fZerocoinActive && !txin.IsZerocoinSpend()))
                return state.DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null");
    }

    return true;
}
