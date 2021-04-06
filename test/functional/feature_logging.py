#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Copyright (c) 2021 The AgenorCoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test debug logging."""

import os

from test_framework.test_framework import AgenorCoinTestFramework

class LoggingTest(AgenorCoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        # test default log file name
        assert os.path.isfile(os.path.join(self.nodes[0].datadir, "regtest", "debug.log"))
        self.log.info("Default filename ok")

        # test alternative log file name in datadir
        self.restart_node(0, ["-debuglogfile=foo.log"])
        assert os.path.isfile(os.path.join(self.nodes[0].datadir, "regtest", "foo.log"))
        self.log.info("Alternative filename ok")

        # test alternative log file name outside datadir
        tempname = os.path.join(self.options.tmpdir, "foo.log")
        self.restart_node(0, ["-debuglogfile=%s" % tempname])
        assert os.path.isfile(tempname)
        self.log.info("Alternative filename outside datadir ok")

        # check that invalid log (relative) will cause error
        invdir = os.path.join(self.nodes[0].datadir, "regtest", "foo")
        invalidname = os.path.join("foo", "foo.log")
        self.stop_node(0)
        self.assert_start_raises_init_error(0, ["-debuglogfile=%s" % (invalidname)],
                                                "Error: Could not open debug log file")
        assert not os.path.isfile(os.path.join(invdir, "foo.log"))
        self.log.info("Invalid relative filename throws")

        # check that a previously invalid log (relative) works after path exists
        os.mkdir(invdir)
        self.start_node(0, ["-debuglogfile=%s" % (invalidname)])
        assert os.path.isfile(os.path.join(invdir, "foo.log"))
        self.log.info("Relative filename ok when path exists")

        # check that invalid log (absolute) will cause error
        self.stop_node(0)
        invdir = os.path.join(self.options.tmpdir, "foo")
        invalidname = os.path.join(invdir, "foo.log")
        self.assert_start_raises_init_error(0, ["-debuglogfile=%s" % invalidname],
                                               "Error: Could not open debug log file")
        assert not os.path.isfile(os.path.join(invdir, "foo.log"))
        self.log.info("Invalid absolute filename throws")

        # check that a previously invalid log (relative) works after path exists
        os.mkdir(invdir)
        self.start_node(0, ["-debuglogfile=%s" % (invalidname)])
        assert os.path.isfile(os.path.join(invdir, "foo.log"))
        self.log.info("Absolute filename ok when path exists")


if __name__ == '__main__':
    LoggingTest().main()
