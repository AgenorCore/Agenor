// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "main.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"

#include <math.h>


unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pblock)
{
    if (Params().IsRegTestNet())
        return pindexLast->nBits;

    /* current difficulty formula, agenor - DarkGravity v3, written by Evan Duffield - evan@dashpay.io */
    const Consensus::Params& consensus = Params().GetConsensus();
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(consensus.powLimit).GetCompact();

    if (consensus.NetworkUpgradeActive(pindexLast->nHeight + 1, Consensus::UPGRADE_POS)) {
        const bool fTimeV2 = !Params().IsRegTestNet() && consensus.IsTimeProtocolV2(pindexLast->nHeight+1);
        const uint256& bnTargetLimit = consensus.ProofOfStakeLimit(fTimeV2);
        const int64_t& nTargetTimespan = consensus.TargetTimespan(fTimeV2);

        int64_t nActualSpacing = 0;
        if (pindexLast->nHeight != 0)
            nActualSpacing = pindexLast->GetBlockTime() - pindexLast->pprev->GetBlockTime();
        if (nActualSpacing < 0)
            nActualSpacing = 1;
        if (fTimeV2 && nActualSpacing > consensus.nTargetSpacing*10)
            nActualSpacing = consensus.nTargetSpacing*10;

        // ppcoin: target change every block
        // ppcoin: retarget with exponential moving toward target spacing
        uint256 bnNew;
        bnNew.SetCompact(pindexLast->nBits);

        // on first block with V2 time protocol, reduce the difficulty by a factor 16
        if (fTimeV2 && !consensus.IsTimeProtocolV2(pindexLast->nHeight))
            bnNew <<= 4;

        int64_t nInterval = nTargetTimespan / consensus.nTargetSpacing;
        bnNew *= ((nInterval - 1) * consensus.nTargetSpacing + nActualSpacing + nActualSpacing);
        bnNew /= ((nInterval + 1) * consensus.nTargetSpacing);

        if (bnNew <= 0 || bnNew > bnTargetLimit)
            bnNew = bnTargetLimit;

        return bnNew.GetCompact();
    }

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % consensus.DifficultyAdjustmentInterval() != 0)
    {
        if (consensus.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + consensus.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % consensus.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (consensus.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), consensus);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    int64_t LimUp = params.nPowTargetTimespan * 100 / 120; // 110% up
    int64_t LimDown = params.nPowTargetTimespan * 2; // 200% down    
    if (nActualTimespan < LimUp)
        nActualTimespan = LimUp;
    if (nActualTimespan > LimDown)
        nActualTimespan = LimDown;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(Params().GetConsensus().powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

uint256 GetBlockProof(const CBlockIndex& block)
{
    uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget.IsNull())
        return UINT256_ZERO;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}
