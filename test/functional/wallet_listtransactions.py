#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the listtransactions API."""
from decimal import Decimal
from io import BytesIO

from test_framework.mininode import CTransaction, COIN
from test_framework.test_framework import AgenorCoinTestFramework
from test_framework.util import (
    assert_array_result,
    assert_equal,
    bytes_to_hex_str,
    hex_str_to_bytes,
    sync_mempools,
)

def txFromHex(hexstring):
    tx = CTransaction()
    f = BytesIO(hex_str_to_bytes(hexstring))
    tx.deserialize(f)
    return tx

class ListTransactionsTest(AgenorCoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.enable_mocktime()

    def run_test(self):
        # Simple send, 0 to 1:
        txid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)
        self.sync_all()
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid},
                           {"category": "send", "amount": Decimal("-0.1"), "confirmations": 0})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"txid": txid},
                           {"category": "receive", "amount": Decimal("0.1"), "confirmations": 0})
        # mine a block, confirmations should change:
        self.nodes[0].generate(1)
        self.sync_all()
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid},
                           {"category": "send", "amount": Decimal("-0.1"), "confirmations": 1})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"txid": txid},
                           {"category": "receive", "amount": Decimal("0.1"), "confirmations": 1})

        # send-to-self:
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 0.2)
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid, "category": "send"},
                           {"amount": Decimal("-0.2")})
        assert_array_result(self.nodes[0].listtransactions(),
                           {"txid": txid, "category": "receive"},
                           {"amount": Decimal("0.2")})

        # sendmany from node1: twice to self, twice to node2:
        send_to = {self.nodes[0].getnewaddress(): 0.11,
                   self.nodes[1].getnewaddress(): 0.22,
                   self.nodes[0].getnewaddress(): 0.33,
                   self.nodes[1].getnewaddress(): 0.44}
        txid = self.nodes[1].sendmany("", send_to)
        self.sync_all()
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category":"send", "amount": Decimal("-0.11")},
                           {"txid": txid})
        assert_array_result(self.nodes[0].listtransactions(),
                           {"category": "receive", "amount": Decimal("0.11")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "send", "amount": Decimal("-0.22")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "receive", "amount": Decimal("0.22")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "send", "amount": Decimal("-0.33")},
                           {"txid": txid})
        assert_array_result(self.nodes[0].listtransactions(),
                           {"category": "receive", "amount": Decimal("0.33")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "send", "amount": Decimal("-0.44")},
                           {"txid": txid})
        assert_array_result(self.nodes[1].listtransactions(),
                           {"category": "receive", "amount": Decimal("0.44")},
                           {"txid": txid})

        multisig = self.nodes[1].createmultisig(1, [self.nodes[1].getnewaddress()])
        self.nodes[0].importaddress(multisig["redeemScript"], "watchonly", False, True)
        txid = self.nodes[1].sendtoaddress(multisig["address"], 0.1)
        self.nodes[1].generate(1)
        self.sync_all()
        assert not [tx for tx in self.nodes[0].listtransactions("*", 100, 0, False) if "label" in tx and tx["label"] == "watchonly"]
        txs = [tx for tx in self.nodes[0].listtransactions("*", 100, 0, True) if "label" in tx and tx['label'] == 'watchonly']
        assert_array_result(txs, {"category": "receive", "amount": Decimal("0.1")}, {"txid": txid})


if __name__ == '__main__':
    ListTransactionsTest().main()
