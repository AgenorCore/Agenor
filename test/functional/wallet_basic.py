#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet."""
from test_framework.test_framework import AgenorCoinTestFramework
from test_framework.util import (
    assert_array_result,
    assert_equal,
    assert_fee_amount,
    assert_raises_rpc_error,
    connect_nodes,
    Decimal,
    wait_until,
)

class WalletTest(AgenorCoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True

    def setup_network(self):
        self.add_nodes(4)
        self.start_node(0)
        self.start_node(1)
        self.start_node(2)
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        connect_nodes(self.nodes[0], 2)
        self.sync_all([self.nodes[0:3]])

    def get_vsize(self, txn):
        return self.nodes[0].decoderawtransaction(txn)['size']

    def run_test(self):
        # Check that there's no UTXO on none of the nodes
        assert_equal(len(self.nodes[0].listunspent()), 0)
        assert_equal(len(self.nodes[1].listunspent()), 0)
        assert_equal(len(self.nodes[2].listunspent()), 0)

        self.log.info("Mining blocks...")

        self.nodes[0].generate(1)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 250)
        assert_equal(walletinfo['balance'], 0)

        self.sync_all([self.nodes[0:3]])
        self.nodes[1].generate(101)
        self.sync_all([self.nodes[0:3]])

        assert_equal(self.nodes[0].getbalance(), 250)
        assert_equal(self.nodes[1].getbalance(), 250)
        assert_equal(self.nodes[2].getbalance(), 0)

        # Check that only first and second nodes have UTXOs
        utxos = self.nodes[0].listunspent()
        assert_equal(len(utxos), 1)
        assert_equal(len(self.nodes[1].listunspent()), 1)
        assert_equal(len(self.nodes[2].listunspent()), 0)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 0)

        # Exercise locking of unspent outputs
        unspent_0 = self.nodes[1].listunspent()[0]
        unspent_0 = {"txid": unspent_0["txid"], "vout": unspent_0["vout"]}
        self.nodes[1].lockunspent(False, [unspent_0])
        assert_raises_rpc_error(-4, "Insufficient funds", self.nodes[1].sendtoaddress, self.nodes[1].getnewaddress(), 20)
        assert_equal([unspent_0], self.nodes[1].listlockunspent())
        self.nodes[1].lockunspent(True, [unspent_0])
        assert_equal(len(self.nodes[1].listlockunspent()), 0)

        # Send 21 AGE from 1 to 0 using sendtoaddress call.
        self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 21)
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])

        # Node0 should have two unspent outputs.
        # Create a couple of transactions to send them to node2, submit them through
        # node1, and make sure both node0 and node2 pick them up properly:
        node0utxos = self.nodes[0].listunspent(1)
        assert_equal(len(node0utxos), 2)

        # create both transactions
        fee_per_kbyte = Decimal('0.001')
        txns_to_send = []
        for utxo in node0utxos:
            inputs = []
            outputs = {}
            inputs.append({ "txid" : utxo["txid"], "vout" : utxo["vout"]})
            outputs[self.nodes[2].getnewaddress()] = float(utxo["amount"]) - float(fee_per_kbyte)
            raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
            txns_to_send.append(self.nodes[0].signrawtransaction(raw_tx))

        # Have node 1 (miner) send the transactions
        self.nodes[1].sendrawtransaction(txns_to_send[0]["hex"], True)
        self.nodes[1].sendrawtransaction(txns_to_send[1]["hex"], True)

        # Have node1 mine a block to confirm transactions:
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])

        assert_equal(self.nodes[0].getbalance(), 0)
        node_2_expected_bal = Decimal('250') + Decimal('21') - 2 * fee_per_kbyte
        node_2_bal = self.nodes[2].getbalance()
        assert_equal(node_2_bal, node_2_expected_bal)

        # Send 10 AGE normal
        address = self.nodes[0].getnewaddress("test")
        self.nodes[2].settxfee(float(fee_per_kbyte))
        txid = self.nodes[2].sendtoaddress(address, 10, "", "")
        fee = self.nodes[2].gettransaction(txid)["fee"]
        node_2_bal -= (Decimal('10') - fee)
        assert_equal(self.nodes[2].getbalance(), node_2_bal)
        self.nodes[2].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_0_bal = self.nodes[0].getbalance()
        assert_equal(node_0_bal, Decimal('10'))

        # Sendmany 10 AGE
        txid = self.nodes[2].sendmany('', {address: 10}, 0, "")
        fee = self.nodes[2].gettransaction(txid)["fee"]
        self.nodes[2].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_0_bal += Decimal('10')
        node_2_bal -= (Decimal('10') - fee)
        assert_equal(self.nodes[2].getbalance(), node_2_bal)
        assert_equal(self.nodes[0].getbalance(), node_0_bal)
        assert_fee_amount(-fee, self.get_vsize(self.nodes[2].getrawtransaction(txid)), fee_per_kbyte)

        # This will raise an exception since generate does not accept a string
        assert_raises_rpc_error(-1, "not an integer", self.nodes[0].generate, "2")

        # Import address and private key to check correct behavior of spendable unspents
        # 1. Send some coins to generate new UTXO
        address_to_import = self.nodes[2].getnewaddress()
        self.nodes[0].sendtoaddress(address_to_import, 1)
        self.nodes[0].generate(1)
        self.sync_all([self.nodes[0:3]])

        # 2. Import address from node2 to node1
        self.nodes[1].importaddress(address_to_import)

        # 3. Validate that the imported address is watch-only on node1
        assert(self.nodes[1].validateaddress(address_to_import)["iswatchonly"])

        # 4. Check that the unspents after import are not spendable
        listunspent = self.nodes[1].listunspent(1, 9999999, [], 2)
        assert_array_result(listunspent,
                           {"address": address_to_import},
                           {"spendable": False})

        # 5. Import private key of the previously imported address on node1
        priv_key = self.nodes[2].dumpprivkey(address_to_import)
        self.nodes[1].importprivkey(priv_key)

        # 6. Check that the unspents are now spendable on node1
        assert_array_result(self.nodes[1].listunspent(),
                           {"address": address_to_import},
                           {"spendable": True})

        # check if wallet or blochchain maintenance changes the balance
        self.sync_all([self.nodes[0:3]])
        blocks = self.nodes[0].generate(2)
        self.sync_all([self.nodes[0:3]])
        balance_nodes = [self.nodes[i].getbalance() for i in range(3)]
        block_count = self.nodes[0].getblockcount()

        maintenance = [
            '-rescan',
            '-reindex',
        ]
        for m in maintenance:
            self.log.info("check " + m)
            self.stop_nodes()
            # set lower ancestor limit for later
            self.start_node(0, [m])
            self.start_node(1, [m])
            self.start_node(2, [m])
            if m == '-reindex':
                # reindex will leave rpc warm up "early"; Wait for it to finish
                wait_until(lambda: [block_count] * 3 == [self.nodes[i].getblockcount() for i in range(3)])
            assert_equal(balance_nodes, [self.nodes[i].getbalance() for i in range(3)])

        # Exercise listsinceblock with the last two blocks
        coinbase_tx_1 = self.nodes[0].listsinceblock(blocks[0])
        assert_equal(coinbase_tx_1["lastblock"], blocks[1])
        assert_equal(len(coinbase_tx_1["transactions"]), 1)
        assert_equal(coinbase_tx_1["transactions"][0]["blockhash"], blocks[1])
        assert_equal(len(self.nodes[0].listsinceblock(blocks[1])["transactions"]), 0)


if __name__ == '__main__':
    WalletTest().main()
