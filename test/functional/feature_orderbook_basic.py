#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Copyright (c) DeFi Blockchain Developers
# Distributed under the MIT software license, see the accompanying
# file LICENSE or http://www.opensource.org/licenses/mit-license.php.
"""Test orderbook RPC.

- verify basic orders creation, fill, close
"""

from test_framework.test_framework import DefiTestFramework
from test_framework.util import assert_equal
from decimal import Decimal
from test_framework.authproxy import JSONRPCException

class OrderBasicTest (DefiTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        # node0: main
        # node1: revert of destroy
        # node2: revert create (all)
        self.setup_clean_chain = True
        self.extra_args = [
            ['-txnotokens=0', '-amkheight=50', '-ehardforkheight=50'],
            ['-txnotokens=0', '-amkheight=50', '-ehardforkheight=50'],
            ['-txnotokens=0', '-amkheight=50', '-ehardforkheight=50']]

    def run_test(self):
        assert_equal(len(self.nodes[0].listorders()), 0) # no orders

        print("Generating initial chain...")
        self.setup_tokens()
        self.sync_all()
        collateral0 = self.nodes[0].get_genesis_keys().ownerAuthAddress
        collateral1 = self.nodes[1].get_genesis_keys().ownerAuthAddress
        idGold = self.get_id_token("GOLD")
        idSilver = self.get_id_token("SILVER")

        # At this point, tokens are created
        tokens = self.nodes[0].listtokens()

        order1txid = self.nodes[0].createorder({
                                    'ownerAddress':collateral0,
                                    'tokenFrom':tokens[idGold]["symbolKey"],
                                    'tokenTo':tokens[idSilver]["symbolKey"],
                                    'amountFrom':10,
                                    'orderPrice':0.1})

        self.nodes[0].generate(1)
        self.sync_all()

        order = self.nodes[0].getorder(order1txid)

        assert_equal(len(order), 1)
        assert_equal(order[order1txid]["ownerAddress"], collateral0)
        assert_equal(order[order1txid]["tokenFrom"], tokens[idGold]["symbolKey"])
        assert_equal(order[order1txid]["tokenTo"], tokens[idSilver]["symbolKey"])
        assert_equal(order[order1txid]["amountFrom"], Decimal("10.00000000"))
        assert_equal(order[order1txid]["amountToFill"], Decimal("10.00000000"))
        assert_equal(order[order1txid]["orderPrice"], Decimal("0.10000000"))
        assert_equal(order[order1txid]["optionDFI"], Decimal("8.00000000"))
        assert_equal(order[order1txid]["expiry"], 2880)

        order2txid = self.nodes[1].fulfillorder({
                                    'ownerAddress':collateral1,
                                    'orderTx':order1txid,
                                    'amount':4})

        self.sync_mempools()
        self.nodes[0].generate(1)
        self.sync_all()

        fillorder = self.nodes[1].getorder(order2txid)

        assert_equal(len(fillorder), 1)
        assert_equal(fillorder[order2txid]["ownerAddress"], collateral1)
        assert_equal(fillorder[order2txid]["orderTx"], order1txid)
        assert_equal(fillorder[order2txid]["amount"], Decimal("4.00000000"))

        order = self.nodes[0].getorder(order1txid)

        assert_equal(len(order), 1)
        assert_equal(order[order1txid]["ownerAddress"], collateral0)
        assert_equal(order[order1txid]["tokenFrom"], tokens[idGold]["symbolKey"])
        assert_equal(order[order1txid]["tokenTo"], tokens[idSilver]["symbolKey"])
        assert_equal(order[order1txid]["amountFrom"], Decimal("10.00000000"))
        assert_equal(order[order1txid]["amountToFill"], Decimal("6.00000000"))
        assert_equal(order[order1txid]["orderPrice"], Decimal("0.10000000"))
        assert_equal(order[order1txid]["optionDFI"], Decimal("8.00000000"))
        assert_equal(order[order1txid]["expiry"], 2880)

        listorders = self.nodes[0].listorders()

        assert_equal(len(listorders), 1)

        listfillorders = self.nodes[0].listorders({'order1txid':order1txid})
        assert_equal(len(listfillorders), 1)
        try:
            closetxid = self.nodes[1].closeorder(order1txid)
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("tx must have at least one input from order owner" in errorString)

        closetxid = self.nodes[0].closeorder(order1txid)

        self.sync_mempools()
        self.nodes[0].generate(1)
        self.sync_all()

        listorders = ''
        listorders = self.nodes[0].listorders()

        assert_equal(len(listorders), 0)

        listclosedorders = self.nodes[0].listorders({'closed':True})

        assert_equal(len(listclosedorders), 1)

        assert_equal(listclosedorders[order1txid]["ownerAddress"], collateral0)
        assert_equal(listclosedorders[order1txid]["tokenFrom"], tokens[idGold]["symbolKey"])
        assert_equal(listclosedorders[order1txid]["tokenTo"], tokens[idSilver]["symbolKey"])
        assert_equal(listclosedorders[order1txid]["amountFrom"], Decimal("10.00000000"))
        assert_equal(listclosedorders[order1txid]["amountToFill"], Decimal("6.00000000"))
        assert_equal(listclosedorders[order1txid]["orderPrice"], Decimal("0.10000000"))
        assert_equal(listclosedorders[order1txid]["optionDFI"], Decimal("8.00000000"))
        assert_equal(listclosedorders[order1txid]["expiry"], 2880)
        assert_equal(listclosedorders[order1txid]["closeTx"], closetxid)

if __name__ == '__main__':
    OrderBasicTest ().main ()
