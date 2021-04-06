#!/usr/bin/env python3
# Copyright (c) 2019 The AgenorCoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC commands for budget proposal creation, submission, and verification."""

from test_framework.test_framework import AgenorCoinTestFramework
from test_framework.util import *


class BudgetProposalTest(AgenorCoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        budgetcycleblocks = 144
        nextsuperblock = self.nodes[0].getnextsuperblock()
        address = self.nodes[0].getnewaddress()
        scheme = 'http://'
        url = 'test.com/url-with-length-of-57-characters-will-pass-00001'
        longurl = 'test.com/url-with-length-of-58-characters-will-not-pass-01'
        name = 'proposalwith20chars0'
        longname = 'proposalwith21chars01'
        numcycles = 2
        cycleamount = 100

        self.log.info("Test with long name")
        assert_raises_rpc_error(-8, "Invalid proposal name, limit of 20 characters.", self.nodes[0].preparebudget,
                                longname, scheme + url, numcycles, nextsuperblock, address, cycleamount)

        self.log.info("Test with long URL")
        assert_raises_rpc_error(-8, "Invalid URL: 65 exceeds limit of 64 characters.", self.nodes[0].preparebudget,
                                name, scheme + longurl, numcycles, nextsuperblock, address, cycleamount)

        self.log.info("Test with invalid (0) cycles")
        assert_raises_rpc_error(-8, "Invalid payment count, must be more than zero.", self.nodes[0].preparebudget,
                                name, scheme + url, 0, nextsuperblock, address, cycleamount)

        self.log.info("Test with invalid block start")
        assert_raises_rpc_error(-8, "Invalid block start", self.nodes[0].preparebudget,
                                name, scheme + url, numcycles, nextsuperblock - 12, address, cycleamount)
        assert_raises_rpc_error(-8, "Invalid block start", self.nodes[0].preparebudget,
                                name, scheme + url, numcycles, nextsuperblock - budgetcycleblocks, address, cycleamount)

        self.log.info("Test with invalid Agenor address")
        assert_raises_rpc_error(-5, "Invalid Agenor address", self.nodes[0].preparebudget,
                                name, scheme + url, numcycles, nextsuperblock, "DBREvBPNQguwuC4YMoCG5FoH1sA2YntvZm", cycleamount)

        self.log.info("Test with too low amount")
        assert_raises_rpc_error(-8, "Invalid amount - Payment of 9.00 is less than minimum 10 AGE allowed", self.nodes[0].preparebudget,
                                name, scheme + url, numcycles, nextsuperblock, address, 9)

        self.log.info("Test with too high amount")
        assert_raises_rpc_error(-8, "Invalid amount - Payment of 648001.00 more than max of 648000.00", self.nodes[0].preparebudget,
                                name, scheme + url, numcycles, nextsuperblock, address, 648001)


        self.log.info("Test without URL scheme")
        scheme = ''
        assert_raises_rpc_error(-8, "Invalid URL, check scheme (e.g. https://)", self.nodes[0].preparebudget, name, scheme + url, 1, nextsuperblock, address, 100)

        self.log.info('Test with invalid URL scheme: ftp://')
        scheme = 'ftp://'
        assert_raises_rpc_error(-8, "Invalid URL, check scheme (e.g. https://)", self.nodes[0].preparebudget, name, scheme + url, 1, nextsuperblock, address, 100)

        self.log.info("Test with invalid double character scheme: hhttps://")
        scheme = 'hhttps://'
        url = 'test.com'
        assert_raises_rpc_error(-8, "Invalid URL, check scheme (e.g. https://)", self.nodes[0].preparebudget, name, scheme + url, 1, nextsuperblock, address, 100)

        self.log.info("Test with valid scheme: http://")
        name = 'testvalid1'
        scheme = 'http://'
        feehashret = self.nodes[0].preparebudget(name, scheme + url, numcycles, nextsuperblock, address, cycleamount)
        txinfo = self.nodes[0].gettransaction(feehashret)
        assert_equal(txinfo['amount'], -50.00)

        self.log.info("Generate 7 blocks to confirm fee transaction")
        self.nodes[0].generate(7)

        self.log.info("Submit the budget proposal")
        submitret = self.nodes[0].submitbudget(name, scheme + url, numcycles, nextsuperblock, address, cycleamount, feehashret)

        self.log.info("Ensure that the budget proposal details are correct")
        budgetinfo = self.nodes[0].getbudgetinfo(name)[0]
        assert_equal(budgetinfo["Name"], name)
        assert_equal(budgetinfo["URL"], scheme + url)
        assert_equal(budgetinfo["Hash"], submitret)
        assert_equal(budgetinfo["FeeHash"], feehashret)
        assert_equal(budgetinfo["BlockStart"], nextsuperblock)
        assert_equal(budgetinfo["PaymentAddress"], address)
        assert_equal(budgetinfo["MonthlyPayment"], cycleamount)
        assert_equal(budgetinfo["TotalPayment"], cycleamount * numcycles)


if __name__ == '__main__':
    BudgetProposalTest().main()
