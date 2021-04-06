#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the dumpwallet RPC."""

import os

from test_framework.test_framework import AgenorCoinTestFramework
from test_framework.util import (assert_equal, assert_raises_rpc_error)


def read_dump(file_name, addrs, hd_master_addr_old):
    """
    Read the given dump, count the addrs that match, count change and reserve.
    Also check that the old hd_master is inactive
    """
    with open(file_name, encoding='utf8') as inputfile:
        found_addr = 0
        found_addr_chg = 0
        found_addr_rsv = 0
        hd_master_addr_ret = None
        for line in inputfile:
            # only read non comment lines
            if line[0] != "#" and len(line) > 10:
                # split out some data
                key_date_label, comment = line.split("#")
                key_date_label = key_date_label.split(" ")

                date = key_date_label[1]
                keytype = key_date_label[2]

                imported_key = date == '1970-01-01T00:00:01Z'
                if imported_key:
                    # Imported keys have multiple addresses, no label (keypath) and timestamp
                    # Skip them
                    continue

                addr_keypath = comment.split(" addr=")[1]
                addr = addr_keypath.split(" ")[0]
                keypath = None

                if keytype == "hdseed=1":
                    # ensure we have generated a new hd master key
                    assert hd_master_addr_old != addr
                    hd_master_addr_ret = addr
                elif keytype == "script=1":
                    # scripts don't have keypaths
                    keypath = None
                else:
                    keypath = addr_keypath.rstrip().split("hdkeypath=")[1]

                # count key types
                for addrObj in addrs:
                    if addrObj['address'] == addr.split(",")[0] and addrObj['hdkeypath'] == keypath and keytype == "label=":
                        if addr.startswith('x') or addr.startswith('y'):
                            # P2PKH address
                            found_addr += 1
                        # else: todo: add staking/anonymous addresses here
                        break
                    elif keytype == "change=1":
                        found_addr_chg += 1
                        break
                    elif keytype == "reserve=1":
                        found_addr_rsv += 1
                        break

        return found_addr, found_addr_chg, found_addr_rsv, hd_master_addr_ret


class WalletDumpTest(AgenorCoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-keypool=90"]]

    def setup_network(self, split=False):
        # Use 1 minute timeout because the initial getnewaddress RPC can take
        # longer than the default 30 seconds due to an expensive
        # CWallet::TopUpKeyPool call, and the encryptwallet RPC made later in
        # the test often takes even longer.
        self.add_nodes(self.num_nodes, self.extra_args, timewait=60)
        self.start_nodes()

    def run_test (self):
        tmpdir = self.options.tmpdir

        # generate 20 addresses to compare against the dump
        test_addr_count = 20
        addrs = []
        for i in range(0,test_addr_count):
            addr = self.nodes[0].getnewaddress()
            vaddr = self.nodes[0].getaddressinfo(addr)  # required to get hd keypath
            addrs.append(vaddr)
        # Should be a no-op:
        self.nodes[0].keypoolrefill()

        # dump unencrypted wallet
        dumpUnencrypted = os.path.join(tmpdir, "node0", "wallet.unencrypted.dump")
        result = self.nodes[0].dumpwallet(dumpUnencrypted)
        assert_equal(result['filename'], os.path.abspath(dumpUnencrypted))

        found_addr, found_addr_chg, found_addr_rsv, hd_master_addr_unenc = \
            read_dump(dumpUnencrypted, addrs, None)
        assert_equal(found_addr, test_addr_count)  # all keys must be in the dump
        assert_equal(found_addr_chg, 0)  # 0 blocks where mined
        assert_equal(found_addr_rsv, 90 * 3)  # 90 keys external plus 100% internal keys plus 100% staking keys

        #encrypt wallet, restart, unlock and dump
        self.nodes[0].node_encrypt_wallet('test')
        self.start_node(0)
        self.nodes[0].walletpassphrase('test', 10)
        # Should be a no-op:
        self.nodes[0].keypoolrefill()
        dumpEncrypted = os.path.join(tmpdir, "node0", "wallet.encrypted.dump")
        self.nodes[0].dumpwallet(dumpEncrypted)

        found_addr, found_addr_chg, found_addr_rsv, hd_master_addr_enc = \
            read_dump(dumpEncrypted, addrs, hd_master_addr_unenc)
        assert_equal(found_addr, test_addr_count)
        assert_equal(found_addr_chg, 90 * 3 + 1)  # old reserve keys are marked as change now. todo: The +1 needs to be removed once this is updated (master seed taken as an internal key)
        assert_equal(found_addr_rsv, 90 * 3) # 90 external + 90 internal + 90 staking

        # Overwriting should fail
        assert_raises_rpc_error(-8, "already exists", self.nodes[0].dumpwallet, dumpUnencrypted)

if __name__ == '__main__':
    WalletDumpTest().main ()
