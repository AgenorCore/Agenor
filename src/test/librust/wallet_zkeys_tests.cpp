// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"

#include "sapling/util.h"
#include "sapling/address.hpp"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "util.h"
#include <boost/test/unit_test.hpp>

/**
 * This test covers methods on CWallet
 * GenerateNewZKey()
 * AddZKey()
 * LoadZKey()
 * LoadZKeyMetadata()
 */

BOOST_FIXTURE_TEST_SUITE(wallet_zkeys_tests, WalletTestingSetup)

/**
  * This test covers Sapling methods on CWallet
  * GenerateNewSaplingZKey()
  * AddSaplingZKey()
  * LoadSaplingZKey()
  * LoadSaplingZKeyMetadata()
  */
BOOST_AUTO_TEST_CASE(StoreAndLoadSaplingZkeys) {
    SelectParams(CBaseChainParams::MAIN);

    CWallet wallet;
    LOCK(wallet.cs_wallet);
    // wallet should be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(0, addrs.size());

    // No HD seed in the wallet
    BOOST_CHECK_THROW(wallet.GenerateNewSaplingZKey(), std::runtime_error);

    // Random seed
    CKey seed;
    seed.MakeNewKey(true);
    wallet.AddKeyPubKey(seed, seed.GetPubKey());
    wallet.GetSaplingScriptPubKeyMan()->SetHDSeed(seed.GetPubKey(), false, true);

    // wallet should have one key
    auto address = wallet.GenerateNewSaplingZKey();
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(1, addrs.size());

    // verify wallet has incoming viewing key for the address
    BOOST_CHECK(wallet.HaveSaplingIncomingViewingKey(address));

    // manually add new spending key to wallet
    HDSeed seed1(seed.GetPrivKey());
    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed1);
    auto sk = m.Derive(0);
    BOOST_CHECK(wallet.AddSaplingZKey(sk, sk.DefaultAddress()));

    // verify wallet did add it
    auto fvk = sk.expsk.full_viewing_key();
    BOOST_CHECK(wallet.HaveSaplingSpendingKey(fvk));

    // verify spending key stored correctly
    libzcash::SaplingExtendedSpendingKey keyOut;
    wallet.GetSaplingSpendingKey(fvk, keyOut);
    BOOST_CHECK(sk == keyOut);

    // verify there are two keys
    wallet.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(2, addrs.size());
    BOOST_CHECK_EQUAL(1, addrs.count(address));
    BOOST_CHECK_EQUAL(1, addrs.count(sk.DefaultAddress()));

    // Generate a diversified address different to the default
    // If we can't get an early diversified address, we are very unlucky
    blob88 diversifier;
    diversifier.begin()[0] = 10;
    auto dpa = sk.ToXFVK().Address(diversifier).get().second;

    // verify wallet only has the default address
    BOOST_CHECK(wallet.HaveSaplingIncomingViewingKey(sk.DefaultAddress()));
    BOOST_CHECK(!wallet.HaveSaplingIncomingViewingKey(dpa));

    // manually add a diversified address
    auto ivk = fvk.in_viewing_key();
    BOOST_CHECK(wallet.AddSaplingIncomingViewingKeyW(ivk, dpa));

    // verify wallet did add it
    BOOST_CHECK(wallet.HaveSaplingIncomingViewingKey(sk.DefaultAddress()));
    BOOST_CHECK(wallet.HaveSaplingIncomingViewingKey(dpa));

    // Load a third key into the wallet
    auto sk2 = m.Derive(1);
    BOOST_CHECK(wallet.LoadSaplingZKey(sk2));

    // attach metadata to this third key
    auto ivk2 = sk2.expsk.full_viewing_key().in_viewing_key();
    int64_t now = GetTime();
    CKeyMetadata meta(now);
    BOOST_CHECK(wallet.LoadSaplingZKeyMetadata(ivk2, meta));

    // check metadata is the same
    BOOST_CHECK_EQUAL(wallet.GetSaplingScriptPubKeyMan()->mapSaplingZKeyMetadata[ivk2].nCreateTime, now);

    // Load a diversified address for the third key into the wallet
    auto dpa2 = sk2.ToXFVK().Address(diversifier).get().second;
    BOOST_CHECK(wallet.HaveSaplingIncomingViewingKey(sk2.DefaultAddress()));
    BOOST_CHECK(!wallet.HaveSaplingIncomingViewingKey(dpa2));
    BOOST_CHECK(wallet.LoadSaplingPaymentAddress(dpa2, ivk2));
    BOOST_CHECK(wallet.HaveSaplingIncomingViewingKey(dpa2));
}

/**
  * This test covers methods on CWalletDB to load/save crypted sapling z keys.
  */
BOOST_AUTO_TEST_CASE(WriteCryptedSaplingZkeyDirectToDb) {
    SelectParams(CBaseChainParams::TESTNET);

    BOOST_CHECK(!pwalletMain->HasSaplingSPKM());
    assert(pwalletMain->SetupSPKM(true));

    // wallet should be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    pwalletMain->GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(0, addrs.size());

    // Add random key to the wallet
    auto address = pwalletMain->GenerateNewSaplingZKey();

    // wallet should have one key
    pwalletMain->GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(1, addrs.size());

    // encrypt wallet
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = "hello";
    BOOST_CHECK(pwalletMain->EncryptWallet(strWalletPass));

    // adding a new key will fail as the wallet is locked
    BOOST_CHECK_THROW(pwalletMain->GenerateNewSaplingZKey(), std::runtime_error);

    // unlock wallet and then add
    pwalletMain->Unlock(strWalletPass);
    libzcash::SaplingPaymentAddress address2 = pwalletMain->GenerateNewSaplingZKey();

    // Create a new wallet from the existing wallet path
    bool fFirstRun;
    CWallet wallet2(pwalletMain->strWalletFile);
    BOOST_CHECK_EQUAL(DB_LOAD_OK, wallet2.LoadWallet(fFirstRun));

    // Confirm it's not the same as the other wallet
    BOOST_CHECK(pwalletMain != &wallet2);
    BOOST_CHECK(wallet2.HasSaplingSPKM());

    // wallet should have two keys
    wallet2.GetSaplingPaymentAddresses(addrs);
    BOOST_CHECK_EQUAL(2, addrs.size());

    //check we have entries for our payment addresses
    BOOST_CHECK(addrs.count(address));
    BOOST_CHECK(addrs.count(address2));

    // spending key is crypted, so we can't extract valid payment address
    libzcash::SaplingExtendedSpendingKey keyOut;
    BOOST_CHECK(!wallet2.GetSaplingExtendedSpendingKey(address, keyOut));

    // unlock wallet to get spending keys and verify payment addresses
    wallet2.Unlock(strWalletPass);

    BOOST_CHECK(wallet2.GetSaplingExtendedSpendingKey(address, keyOut));
    BOOST_CHECK(address == keyOut.DefaultAddress());

    BOOST_CHECK(wallet2.GetSaplingExtendedSpendingKey(address2, keyOut));
    BOOST_CHECK(address2 == keyOut.DefaultAddress());
}


BOOST_AUTO_TEST_SUITE_END()
