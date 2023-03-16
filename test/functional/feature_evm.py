#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Copyright (c) DeFi Blockchain Developers
# Distributed under the MIT software license, see the accompanying
# file LICENSE or http://www.opensource.org/licenses/mit-license.php.
"""Test EVM behaviour"""

from test_framework.test_framework import DefiTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error
)

from decimal import Decimal

class EVMTest(DefiTestFramework):
    mns = None
    proposalId = ""

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [
            ['-dummypos=0', '-txnotokens=0', '-amkheight=50', '-bayfrontheight=51', '-eunosheight=80', '-fortcanningheight=82', '-fortcanninghillheight=84', '-fortcanningroadheight=86', '-fortcanningcrunchheight=88', '-fortcanningspringheight=90', '-fortcanninggreatworldheight=94', '-fortcanningepilogueheight=96', '-grandcentralheight=101', '-nextnetworkupgradeheight=105', '-subsidytest=1', '-txindex=1']
        ]

    def run_test(self):
        address = self.nodes[0].get_genesis_keys().ownerAuthAddress
        ethAddress = self.nodes[0].get_genesis_keys().ownerAuthAddress

        # Generate chain
        self.nodes[0].generate(105)
        self.sync_blocks()

        self.nodes[0].utxostoaccount({address: "100@DFI"})
        self.nodes[0].generate(1)
        self.sync_blocks()

        self.nodes[0].utxostoaccount({address: "100@DFI"})
        self.nodes[0].utxostoaccount({address: "90@DFI"})

        self.nodes[0].transferbalance({"map":[{"from": address, "to": ethAddress, "amounts": '100@DFI'}], "type":"evmin"})
        self.nodes[0].evmtx("AABBCCDDEEFF00112233445566778899")
        self.nodes[0].transferbalance({"map":[{"from": ethAddress, "to": address, "amounts": '100@DFI'}], "type":"evmout"})

        self.nodes[0].generate(1)
        self.sync_blocks()

if __name__ == '__main__':
    EVMTest().main()
