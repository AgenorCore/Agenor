#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
# Copyright (c) 20202 The AgenorCoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import AgenorCoinTestFramework
from test_framework.util import *

class SaplingWalletPersistenceTest(AgenorCoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        sapling_addr = self.nodes[0].getnewshieldedaddress()
        addresses = self.nodes[0].listshieldedaddresses()
        # make sure the node has the addresss
        assert_true(sapling_addr in addresses, "Should contain address before restart")
        # restart the nodes
        self.stop_node(0)
        self.start_node(0)
        addresses = self.nodes[0].listshieldedaddresses()
        # make sure we still have the address after restarting
        assert_true(sapling_addr in addresses, "Should contain address after restart")

if __name__ == '__main__':
    SaplingWalletPersistenceTest().main()
