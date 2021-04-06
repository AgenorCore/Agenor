#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test HD Wallet keypool restore function.

Two nodes. Node1 is under test. Node0 is providing transactions and generating blocks.

- Start node1, shutdown and backup wallet.
- Generate 110 keys (enough to drain the keypool). Store key 90 (in the initial keypool) and key 110 (beyond the initial keypool). Send funds to key 90 and key 110.
- Stop node1, clear the datadir, move wallet file back into the datadir and restart node1.
- connect node1 to node0. Verify that they sync and node1 receives its funds."""
import shutil

from test_framework.test_framework import AgenorCoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    sync_blocks,
)

class KeypoolRestoreTest(AgenorCoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [['-keypool=3'], ['-keypool=100']]

    def run_test(self):
        isLegacyWallet = '-legacywallet' in self.nodes[0].extra_args
        self.tmpdir = self.options.tmpdir
        self.nodes[0].generate(101)

        self.log.info("Make backup of wallet")

        self.stop_node(1)

        shutil.copyfile(self.tmpdir + "/node1/regtest/wallet.dat", self.tmpdir + "/wallet.bak")
        self.start_node(1, self.extra_args[1])
        connect_nodes(self.nodes[0], 1)

        self.log.info("Generate keys for wallet")

        for _ in range(90):
            addr_oldpool = self.nodes[1].getnewaddress()
        for _ in range(20):
            addr_extpool = self.nodes[1].getnewaddress()

        self.log.info("Send funds to wallet")

        self.nodes[0].sendtoaddress(addr_oldpool, 10)
        self.nodes[0].generate(1)
        self.nodes[0].sendtoaddress(addr_extpool, 5)
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        self.log.info("Restart node with wallet backup")

        self.stop_node(1)

        shutil.copyfile(self.tmpdir + "/wallet.bak", self.tmpdir + "/node1/regtest/wallet.dat")

        self.log.info("Verify keypool is restored and balance is correct")

        self.start_node(1, self.extra_args[1])
        connect_nodes(self.nodes[0], 1)
        self.sync_all()

        # wallet was not backupped after emptying the key pool.
        # Legacy wallet can't recover funds in addr_extpool
        recoveredBalance = 10 if isLegacyWallet else 15
        assert_equal(self.nodes[1].getbalance(), recoveredBalance)
        assert_equal(self.nodes[1].listtransactions()[0]['category'], "receive")

        # Check that we have marked all keys up to the used keypool key as used
        if not isLegacyWallet:
            assert_equal(self.nodes[1].getaddressinfo(self.nodes[1].getnewaddress())['hdkeypath'], "m/44'/119'/0'/0'/110'")

if __name__ == '__main__':
    KeypoolRestoreTest().main()
