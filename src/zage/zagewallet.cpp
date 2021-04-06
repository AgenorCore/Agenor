// Copyright (c) 2017-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zagewallet.h"
#include "main.h"
#include "txdb.h"
#include "wallet/walletdb.h"
#include "init.h"
#include "wallet/wallet.h"
#include "deterministicmint.h"
#include "zagechain.h"


CzAGEWallet::CzAGEWallet(CWallet* parent)
{
    this->wallet = parent;
    CWalletDB walletdb(wallet->strWalletFile);
    bool fRegtest = Params().IsRegTestNet();

    uint256 hashSeed;
    bool fFirstRun = !walletdb.ReadCurrentSeedHash(hashSeed);

    //Check for old db version of storing zpiv seed
    if (fFirstRun) {
        uint256 seed;
        if (walletdb.ReadZAgenoreed_deprecated(seed)) {
            //Update to new format, erase old
            seedMaster = seed;
            hashSeed = Hash(seed.begin(), seed.end());
            if (wallet->AddDeterministicSeed(seed)) {
                if (walletdb.EraseZAgenoreed_deprecated()) {
                    LogPrintf("%s: Updated zAGE seed databasing\n", __func__);
                    fFirstRun = false;
                } else {
                    LogPrintf("%s: failed to remove old zage seed\n", __func__);
                }
            }
        }
    }

    //Don't try to do anything if the wallet is locked.
    if (wallet->IsLocked() || (!fRegtest && fFirstRun)) {
        seedMaster.SetNull();
        nCountLastUsed = 0;
        this->mintPool = CMintPool();
        return;
    }

    //First time running, generate master seed
    uint256 seed;
    if (fRegtest && fFirstRun) {
        // Borrow random generator from the key class so that we don't have to worry about randomness
        CKey key;
        key.MakeNewKey(true);
        seed = key.GetPrivKey_256();
        seedMaster = seed;
        LogPrintf("%s: first run of zage wallet detected, new seed generated. Seedhash=%s\n", __func__, Hash(seed.begin(), seed.end()).GetHex());
    } else if (!parent->GetDeterministicSeed(hashSeed, seed)) {
        LogPrintf("%s: failed to get deterministic seed for hashseed %s\n", __func__, hashSeed.GetHex());
        return;
    }

    if (!SetMasterSeed(seed)) {
        LogPrintf("%s: failed to save deterministic seed for hashseed %s\n", __func__, hashSeed.GetHex());
        return;
    }
    this->mintPool = CMintPool(nCountLastUsed);
}

bool CzAGEWallet::SetMasterSeed(const uint256& seedMaster, bool fResetCount)
{

    CWalletDB walletdb(wallet->strWalletFile);
    if (wallet->IsLocked())
        return false;

    if (!seedMaster.IsNull() && !wallet->AddDeterministicSeed(seedMaster)) {
        return error("%s: failed to set master seed.", __func__);
    }

    this->seedMaster = seedMaster;

    nCountLastUsed = 0;

    if (fResetCount)
        walletdb.WriteZAGECount(nCountLastUsed);
    else if (!walletdb.ReadZAGECount(nCountLastUsed))
        nCountLastUsed = 0;

    mintPool.Reset();

    return true;
}

void CzAGEWallet::Lock()
{
    seedMaster.SetNull();
}

void CzAGEWallet::AddToMintPool(const std::pair<uint256, uint32_t>& pMint, bool fVerbose)
{
    mintPool.Add(pMint, fVerbose);
}

//Add the next 20 mints to the mint pool
void CzAGEWallet::GenerateMintPool(uint32_t nCountStart, uint32_t nCountEnd)
{

    //Is locked
    if (seedMaster.IsNull())
        return;

    uint32_t n = nCountLastUsed + 1;

    if (nCountStart > 0)
        n = nCountStart;

    uint32_t nStop = n + 20;
    if (nCountEnd > 0)
        nStop = std::max(n, n + nCountEnd);

    bool fFound;

    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    LogPrintf("%s : n=%d nStop=%d\n", __func__, n, nStop - 1);
    for (uint32_t i = n; i < nStop; ++i) {
        if (ShutdownRequested())
            return;

        fFound = false;

        // Prevent unnecessary repeated minted
        for (auto& pair : mintPool) {
            if(pair.second == i) {
                fFound = true;
                break;
            }
        }

        if(fFound)
            continue;

        uint512 seedZerocoin = GetZerocoinSeed(i);
        CBigNum bnValue;
        CBigNum bnSerial;
        CBigNum bnRandomness;
        CKey key;
        SeedToZAGE(seedZerocoin, bnValue, bnSerial, bnRandomness, key);

        mintPool.Add(bnValue, i);
        CWalletDB(wallet->strWalletFile).WriteMintPoolPair(hashSeed, GetPubCoinHash(bnValue), i);
        LogPrintf("%s : %s count=%d\n", __func__, bnValue.GetHex().substr(0, 6), i);
    }
}

// pubcoin hashes are stored to db so that a full accounting of mints belonging to the seed can be tracked without regenerating
bool CzAGEWallet::LoadMintPoolFromDB()
{
    std::map<uint256, std::vector<std::pair<uint256, uint32_t> > > mapMintPool = CWalletDB(wallet->strWalletFile).MapMintPool();

    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    for (auto& pair : mapMintPool[hashSeed])
        mintPool.Add(pair);

    return true;
}

void CzAGEWallet::RemoveMintsFromPool(const std::vector<uint256>& vPubcoinHashes)
{
    for (const uint256& hash : vPubcoinHashes)
        mintPool.Remove(hash);
}

void CzAGEWallet::GetState(int& nCount, int& nLastGenerated)
{
    nCount = this->nCountLastUsed + 1;
    nLastGenerated = mintPool.CountOfLastGenerated();
}

//Catch the counter up with the chain
void CzAGEWallet::SyncWithChain(bool fGenerateMintPool)
{
    uint32_t nLastCountUsed = 0;
    bool found = true;

    std::set<uint256> setAddedTx;
    while (found) {
        found = false;
        if (fGenerateMintPool)
            GenerateMintPool();
        LogPrintf("%s: Mintpool size=%d\n", __func__, mintPool.size());

        std::set<uint256> setChecked;
        std::list<std::pair<uint256,uint32_t> > listMints = mintPool.List();
        for (std::pair<uint256, uint32_t> pMint : listMints) {
            LOCK(cs_main);
            if (setChecked.count(pMint.first))
                return;
            setChecked.insert(pMint.first);

            if (ShutdownRequested())
                return;

            if (wallet->zageTracker->HasPubcoinHash(pMint.first)) {
                mintPool.Remove(pMint.first);
                continue;
            }

            uint256 txHash;
            CZerocoinMint mint;
            if (zerocoinDB->ReadCoinMint(pMint.first, txHash)) {
                //this mint has already occurred on the chain, increment counter's state to reflect this
                LogPrintf("%s : Found wallet coin mint=%s count=%d tx=%s\n", __func__, pMint.first.GetHex(), pMint.second, txHash.GetHex());
                found = true;

                uint256 hashBlock;
                CTransaction tx;
                if (!GetTransaction(txHash, tx, hashBlock, true)) {
                    LogPrintf("%s : failed to get transaction for mint %s!\n", __func__, pMint.first.GetHex());
                    found = false;
                    nLastCountUsed = std::max(pMint.second, nLastCountUsed);
                    continue;
                }

                //Find the denomination
                libzerocoin::CoinDenomination denomination = libzerocoin::CoinDenomination::ZQ_ERROR;
                bool fFoundMint = false;
                CBigNum bnValue = 0;
                for (const CTxOut& out : tx.vout) {
                    if (!out.IsZerocoinMint())
                        continue;

                    libzerocoin::PublicCoin pubcoin(Params().GetConsensus().Zerocoin_Params(false));
                    CValidationState state;
                    if (!TxOutToPublicCoin(out, pubcoin, state)) {
                        LogPrintf("%s : failed to get mint from txout for %s!\n", __func__, pMint.first.GetHex());
                        continue;
                    }

                    // See if this is the mint that we are looking for
                    uint256 hashPubcoin = GetPubCoinHash(pubcoin.getValue());
                    if (pMint.first == hashPubcoin) {
                        denomination = pubcoin.getDenomination();
                        bnValue = pubcoin.getValue();
                        fFoundMint = true;
                        break;
                    }
                }

                if (!fFoundMint || denomination == libzerocoin::ZQ_ERROR) {
                    LogPrintf("%s : failed to get mint %s from tx %s!\n", __func__, pMint.first.GetHex(), tx.GetHash().GetHex());
                    found = false;
                    break;
                }

                CBlockIndex* pindex = nullptr;
                if (mapBlockIndex.count(hashBlock))
                    pindex = mapBlockIndex.at(hashBlock);

                if (!setAddedTx.count(txHash)) {
                    CBlock block;
                    CWalletTx wtx(wallet, tx);
                    if (pindex && ReadBlockFromDisk(block, pindex)) {
                        int posInBlock;
                        for (posInBlock = 0; posInBlock < (int)block.vtx.size(); posInBlock++) {
                            if (wtx.GetHash() == txHash) {
                                wtx.SetMerkleBranch(pindex, posInBlock);
                                break;
                            }
                        }
                    }

                    //Fill out wtx so that a transaction record can be created
                    wtx.nTimeReceived = pindex->GetBlockTime();
                    wallet->AddToWallet(wtx);
                    setAddedTx.insert(txHash);
                }

                SetMintSeen(bnValue, pindex->nHeight, txHash, denomination);
                nLastCountUsed = std::max(pMint.second, nLastCountUsed);
                nCountLastUsed = std::max(nLastCountUsed, nCountLastUsed);
                LogPrint(BCLog::LEGACYZC, "%s: updated count to %d\n", __func__, nCountLastUsed);
            }
        }
    }
}

bool CzAGEWallet::SetMintSeen(const CBigNum& bnValue, const int& nHeight, const uint256& txid, const libzerocoin::CoinDenomination& denom)
{
    if (!mintPool.Has(bnValue))
        return error("%s: value not in pool", __func__);
    std::pair<uint256, uint32_t> pMint = mintPool.Get(bnValue);

    // Regenerate the mint
    uint512 seedZerocoin = GetZerocoinSeed(pMint.second);
    CBigNum bnValueGen;
    CBigNum bnSerial;
    CBigNum bnRandomness;
    CKey key;
    SeedToZAGE(seedZerocoin, bnValueGen, bnSerial, bnRandomness, key);

    //Sanity check
    if (bnValueGen != bnValue)
        return error("%s: generated pubcoin and expected value do not match!", __func__);

    // Create mint object and database it
    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    uint256 hashSerial = GetSerialHash(bnSerial);
    uint256 hashPubcoin = GetPubCoinHash(bnValue);
    uint256 nSerial = bnSerial.getuint256();
    uint256 hashStake = Hash(nSerial.begin(), nSerial.end());
    CDeterministicMint dMint(libzerocoin::PrivateCoin::CURRENT_VERSION, pMint.second, hashSeed, hashSerial, hashPubcoin, hashStake);
    dMint.SetDenomination(denom);
    dMint.SetHeight(nHeight);
    dMint.SetTxHash(txid);

    // Check if this is also already spent
    int nHeightTx;
    uint256 txidSpend;
    CTransaction txSpend;
    if (IsSerialInBlockchain(hashSerial, nHeightTx, txidSpend, txSpend)) {
        //Find transaction details and make a wallettx and add to wallet
        dMint.SetUsed(true);
        CWalletTx wtx(wallet, txSpend);
        CBlockIndex* pindex = chainActive[nHeightTx];
        CBlock block;
        if (ReadBlockFromDisk(block, pindex)) {
            int posInBlock;
            for (posInBlock = 0; posInBlock < (int) block.vtx.size(); posInBlock++) {
                if (wtx.GetHash() == txidSpend) {
                    wtx.SetMerkleBranch(pindex, posInBlock);
                    break;
                }
            }
        }

        wtx.nTimeReceived = pindex->nTime;
        wallet->AddToWallet(wtx);
    }

    // Add to zpivTracker which also adds to database
    wallet->zageTracker->Add(dMint, true);

    //Update the count if it is less than the mint's count
    if (nCountLastUsed < pMint.second) {
        CWalletDB walletdb(wallet->strWalletFile);
        nCountLastUsed = pMint.second;
        walletdb.WriteZAGECount(nCountLastUsed);
    }

    //remove from the pool
    mintPool.Remove(dMint.GetPubcoinHash());

    return true;
}

// Check if the value of the commitment meets requirements
bool IsValidCoinValue(const CBigNum& bnValue)
{
    libzerocoin::ZerocoinParams* params = Params().GetConsensus().Zerocoin_Params(false);
    return bnValue >= params->accumulatorParams.minCoinValue && bnValue <= params->accumulatorParams.maxCoinValue && bnValue.isPrime();
}

void CzAGEWallet::SeedToZAGE(const uint512& seedZerocoin, CBigNum& bnValue, CBigNum& bnSerial, CBigNum& bnRandomness, CKey& key)
{
    libzerocoin::ZerocoinParams* params = Params().GetConsensus().Zerocoin_Params(false);

    //convert state seed into a seed for the private key
    uint256 nSeedPrivKey = seedZerocoin.trim256();

    bool isValidKey = false;
    key = CKey();
    while (!isValidKey) {
        nSeedPrivKey = Hash(nSeedPrivKey.begin(), nSeedPrivKey.end());
        isValidKey = libzerocoin::GenerateKeyPair(params->coinCommitmentGroup.groupOrder, nSeedPrivKey, key, bnSerial);
    }

    //hash randomness seed with Bottom 256 bits of seedZerocoin & attempts256 which is initially 0
    uint256 randomnessSeed = uint512(seedZerocoin >> 256).trim256();
    uint256 hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end());
    bnRandomness.setuint256(hashRandomness);
    bnRandomness = bnRandomness % params->coinCommitmentGroup.groupOrder;

    //See if serial and randomness make a valid commitment
    // Generate a Pedersen commitment to the serial number
    CBigNum commitmentValue = params->coinCommitmentGroup.g.pow_mod(bnSerial, params->coinCommitmentGroup.modulus).mul_mod(
                        params->coinCommitmentGroup.h.pow_mod(bnRandomness, params->coinCommitmentGroup.modulus),
                        params->coinCommitmentGroup.modulus);

    CBigNum random;
    uint256 attempts256;
    // Iterate on Randomness until a valid commitmentValue is found
    while (true) {
        // Now verify that the commitment is a prime number
        // in the appropriate range. If not, we'll throw this coin
        // away and generate a new one.
        if (IsValidCoinValue(commitmentValue)) {
            bnValue = commitmentValue;
            return;
        }

        //Did not create a valid commitment value.
        //Change randomness to something new and random and try again
        attempts256++;
        hashRandomness = Hash(randomnessSeed.begin(), randomnessSeed.end(),
                              attempts256.begin(), attempts256.end());
        random.setuint256(hashRandomness);
        bnRandomness = (bnRandomness + random) % params->coinCommitmentGroup.groupOrder;
        commitmentValue = commitmentValue.mul_mod(params->coinCommitmentGroup.h.pow_mod(random, params->coinCommitmentGroup.modulus), params->coinCommitmentGroup.modulus);
    }
}

uint512 CzAGEWallet::GetZerocoinSeed(uint32_t n)
{
    CDataStream ss(SER_GETHASH, 0);
    ss << seedMaster << n;
    uint512 zerocoinSeed = Hash512(ss.begin(), ss.end());
    return zerocoinSeed;
}

void CzAGEWallet::UpdateCount()
{
    nCountLastUsed++;
    CWalletDB walletdb(wallet->strWalletFile);
    walletdb.WriteZAGECount(nCountLastUsed);
}

void CzAGEWallet::GenerateDeterministicZAGE(libzerocoin::CoinDenomination denom, libzerocoin::PrivateCoin& coin, CDeterministicMint& dMint, bool fGenerateOnly)
{
    GenerateMint(nCountLastUsed + 1, denom, coin, dMint);
    if (fGenerateOnly)
        return;

    //TODO remove this leak of seed from logs before merge to master
    //LogPrintf("%s : Generated new deterministic mint. Count=%d pubcoin=%s seed=%s\n", __func__, nCount, coin.getPublicCoin().getValue().GetHex().substr(0,6), seedZerocoin.GetHex().substr(0, 4));
}

void CzAGEWallet::GenerateMint(const uint32_t& nCount, const libzerocoin::CoinDenomination denom, libzerocoin::PrivateCoin& coin, CDeterministicMint& dMint)
{
    uint512 seedZerocoin = GetZerocoinSeed(nCount);
    CBigNum bnValue;
    CBigNum bnSerial;
    CBigNum bnRandomness;
    CKey key;
    SeedToZAGE(seedZerocoin, bnValue, bnSerial, bnRandomness, key);
    coin = libzerocoin::PrivateCoin(Params().GetConsensus().Zerocoin_Params(false), denom, bnSerial, bnRandomness);
    coin.setPrivKey(key.GetPrivKey());
    coin.setVersion(libzerocoin::PrivateCoin::CURRENT_VERSION);

    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    uint256 hashSerial = GetSerialHash(bnSerial);
    uint256 nSerial = bnSerial.getuint256();
    uint256 hashStake = Hash(nSerial.begin(), nSerial.end());
    uint256 hashPubcoin = GetPubCoinHash(bnValue);
    dMint = CDeterministicMint(coin.getVersion(), nCount, hashSeed, hashSerial, hashPubcoin, hashStake);
    dMint.SetDenomination(denom);
}

bool CzAGEWallet::CheckSeed(const CDeterministicMint& dMint)
{
    //Check that the seed is correct    todo:handling of incorrect, or multiple seeds
    uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
    return hashSeed == dMint.GetSeedHash();
}

bool CzAGEWallet::RegenerateMint(const CDeterministicMint& dMint, CZerocoinMint& mint)
{
    if (!CheckSeed(dMint)) {
        uint256 hashSeed = Hash(seedMaster.begin(), seedMaster.end());
        return error("%s: master seed does not match!\ndmint:\n %s \nhashSeed: %s\nseed: %s", __func__, dMint.ToString(), hashSeed.GetHex(), seedMaster.GetHex());
    }

    //Generate the coin
    libzerocoin::PrivateCoin coin(Params().GetConsensus().Zerocoin_Params(false), dMint.GetDenomination(), false);
    CDeterministicMint dMintDummy;
    GenerateMint(dMint.GetCount(), dMint.GetDenomination(), coin, dMintDummy);

    //Fill in the zerocoinmint object's details
    CBigNum bnValue = coin.getPublicCoin().getValue();
    if (GetPubCoinHash(bnValue) != dMint.GetPubcoinHash())
        return error("%s: failed to correctly generate mint, pubcoin hash mismatch", __func__);
    mint.SetValue(bnValue);

    CBigNum bnSerial = coin.getSerialNumber();
    if (GetSerialHash(bnSerial) != dMint.GetSerialHash())
        return error("%s: failed to correctly generate mint, serial hash mismatch", __func__);
    mint.SetSerialNumber(bnSerial);

    mint.SetRandomness(coin.getRandomness());
    mint.SetPrivKey(coin.getPrivKey());
    mint.SetVersion(coin.getVersion());
    mint.SetDenomination(dMint.GetDenomination());
    mint.SetUsed(dMint.IsUsed());
    mint.SetTxHash(dMint.GetTxHash());
    mint.SetHeight(dMint.GetHeight());

    return true;
}
