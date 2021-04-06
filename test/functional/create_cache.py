#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Create a blockchain cache.

Creating a cache of the blockchain speeds up test execution when running
multiple functional tests. This helper script is executed by test_runner when multiple
tests are being run in parallel.
"""

from test_framework.test_framework import AgenorCoinTestFramework

class CreateCache(AgenorCoinTestFramework):
    # Test network and test nodes are not required:
    def setup_chain(self):
        self.log.info("Initializing test directory " + self.options.tmpdir)
        # Initialize PoS chain (it will automatically generate PoW chain too)
        self._initialize_chain(toPosPhase=True)

    def set_test_params(self):
        self.num_nodes = 0
        self.supports_cli = True

    def setup_network(self):
        pass

    def run_test(self):
        pass

if __name__ == '__main__':
    CreateCache().main()
