// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"

#include "rpc/server.h"
#include "sapling/util.h"
#include "wallet/db.h"
#include "wallet/wallet.h"
#include "wallet/rpcwallet.h"

#include <librustzcash.h>

void clean()
{
    delete pwalletMain;
    pwalletMain = nullptr;

    bitdb.Flush(true);
    bitdb.Reset();
}

WalletTestingSetup::WalletTestingSetup(): TestingSetup()
{
    //initZKSNARKS(); // init zk-snarks lib

    clean(); // todo: research why we have an initialized bitdb here.
    bitdb.MakeMock();
    walletRegisterRPCCommands();

    bool fFirstRun;
    pwalletMain = new CWallet("test_wallet.dat");
    pwalletMain->LoadWallet(fFirstRun);
    RegisterValidationInterface(pwalletMain);
}

WalletTestingSetup::~WalletTestingSetup()
{
    UnregisterValidationInterface(pwalletMain);
    clean();
}
