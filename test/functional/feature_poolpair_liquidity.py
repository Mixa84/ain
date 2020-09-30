#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Copyright (c) DeFi Blockchain Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test pool's RPC.

- verify basic accounts operation
"""

from test_framework.test_framework import DefiTestFramework

from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, \
    connect_nodes_bi

from decimal import Decimal, ROUND_DOWN

from pprint import pprint

def make_rounded_decimal(value) :
    return Decimal(value).quantize(Decimal('0.00000001'), rounding=ROUND_DOWN)

class PoolLiquidityTest (DefiTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        # node0: main
        # node1: secondary tester
        # node2: revert create (all)
        self.setup_clean_chain = True
        self.extra_args = [['-txnotokens=0'], ['-txnotokens=0'], ['-txnotokens=0'], ['-txnotokens=0']]

    def run_test(self):
        assert_equal(len(self.nodes[0].listtokens()), 1) # only one token == DFI

        print("Generating initial chain...")
        self.setup_tokens()

        # stop node #2 for future revert
        self.stop_node(2)
        connect_nodes_bi(self.nodes, 0, 3)

        symbolGOLD = "GOLD#" + self.get_id_token("GOLD")
        symbolSILVER = "SILVER#" + self.get_id_token("SILVER")

        idGold = list(self.nodes[0].gettoken(symbolGOLD).keys())[0]
        idSilver = list(self.nodes[0].gettoken(symbolSILVER).keys())[0]
        accountGold = self.nodes[0].get_genesis_keys().ownerAuthAddress
        accountSilver = self.nodes[1].get_genesis_keys().ownerAuthAddress
        initialGold = self.nodes[0].getaccount(accountGold, {}, True)[idGold]
        initialSilver = self.nodes[1].getaccount(accountSilver, {}, True)[idSilver]
        print("Initial GOLD:", initialGold, ", id", idGold)
        print("Initial SILVER:", initialSilver, ", id", idSilver)

        owner = self.nodes[0].getnewaddress("", "legacy")

        # transfer silver
        self.nodes[1].accounttoaccount(accountSilver, {accountGold: "1000@" + symbolSILVER})
        self.nodes[1].generate(1)

        # create pool
        self.nodes[0].createpoolpair({
            "tokenA": symbolGOLD,
            "tokenB": symbolSILVER,
            "commission": 1,
            "status": True,
            "ownerAddress": owner,
            "pairSymbol": "GS",
        }, [])
        self.nodes[0].generate(1)

        # only 4 tokens = DFI, GOLD, SILVER, GS
        assert_equal(len(self.nodes[0].listtokens()), 4)

        # check tokens id
        pool = self.nodes[0].getpoolpair("GS")
        idGS = list(self.nodes[0].gettoken("GS").keys())[0]
        assert(pool[idGS]['idTokenA'] == idGold)
        assert(pool[idGS]['idTokenB'] == idSilver)

        # Add liquidity
        #========================
        # one token
        try:
            self.nodes[0].addpoolliquidity({
                accountGold: "100@" + symbolSILVER
            }, accountGold, [])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("the pool pair requires two tokens" in errorString)

        # missing amount
        try:
            self.nodes[0].addpoolliquidity({
                accountGold: ["0@" + symbolGOLD, "0@" + symbolSILVER]
            }, accountGold, [])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("Amount out of range" in errorString)

        # missing pool
        try:
            self.nodes[0].addpoolliquidity({
                accountGold: ["100@DFI", "100@" + symbolGOLD]
            }, accountGold, [])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("there is no such pool pair" in errorString)

        # transfer
        self.nodes[0].addpoolliquidity({
            accountGold: ["100@" + symbolGOLD, "100@" + symbolSILVER]
        }, accountGold, [])
        self.nodes[0].generate(1)

        # only 3 tokens = GOLD, SILVER, GS
        accountGoldInfo = self.nodes[0].getaccount(accountGold, {}, True)

        assert_equal(len(accountGoldInfo), 3)
        assert_equal(len(accountGoldInfo), 3)

        amountGS = accountGoldInfo[idGS]
        amountGold = accountGoldInfo[idGold]
        amountSilver = accountGoldInfo[idSilver]

        assert_equal(str(amountGS), "99.99999000")

        assert_equal(amountGold, initialGold - 100)
        assert_equal(amountSilver, initialSilver - 1100)

        accountGoldInfo = self.nodes[1].getaccount(accountGold, {}, True)

        assert_equal(str(accountGoldInfo[idGS]), "99.99999000")
        assert_equal(accountGoldInfo[idGold], initialGold - 100)
        assert_equal(accountGoldInfo[idSilver], initialSilver - 1100)

        # getpoolpair
        pool = self.nodes[0].getpoolpair("GS", True)
        assert_equal(pool['1']['reserveA'], 100)
        assert_equal(pool['1']['reserveB'], 100)
        assert_equal(pool['1']['totalLiquidity'], 100)

        # third tester
        accountTest = self.nodes[3].getnewaddress()

        # transfer utxo
        self.nodes[0].sendtoaddress(accountTest, 1)
        self.nodes[0].sendtoaddress(accountTest, 1)
        self.nodes[0].sendtoaddress(accountTest, 1)

        # transfer tokens
        self.nodes[0].accounttoaccount(accountGold, {accountTest: ["500@" + symbolSILVER, "500@" + symbolGOLD]})
        self.nodes[0].generate(1)

        accountTestInfo = self.nodes[3].getaccount(accountTest, {}, True)
        accountGoldInfo = self.nodes[3].getaccount(accountGold, {}, True)

        assert_equal(accountTestInfo[idGold], 500)
        assert_equal(accountTestInfo[idSilver], 500)
        assert_equal(accountGoldInfo[idGold], amountGold - 500)
        assert_equal(accountGoldInfo[idSilver], amountSilver - 500)

        # transfer liquidity
        self.nodes[3].addpoolliquidity({
            accountTest: ["50@" + symbolGOLD, "400@" + symbolSILVER]
        }, accountTest, [])

        self.sync_all([self.nodes[0], self.nodes[3]])

        self.nodes[0].generate(1)

        accountTestInfo = self.nodes[3].getaccount(accountTest, {}, True)
        pool = self.nodes[3].getpoolpair("GS", True)

        assert_equal(accountTestInfo[idGS], 50)
        assert_equal(accountTestInfo[idGold], 450)
        assert_equal(accountTestInfo[idSilver], 100)

        assert_equal(pool['1']['reserveA'], 150)
        assert_equal(pool['1']['reserveB'], 500)
        assert_equal(pool['1']['totalLiquidity'], 150)

        # Remove liquidity
        #========================

        amountGold = accountGoldInfo[idGold]
        amountSilver = accountGoldInfo[idSilver]

        poolReserveA = pool['1']['reserveA']
        poolReserveB = pool['1']['reserveB']
        poolLiquidity = pool['1']['totalLiquidity']

        # missing pool
        try:
            self.nodes[0].removepoolliquidity(accountGold, "100@DFI", [])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("there is no such pool pair" in errorString)

        # missing amount
        try:
            self.nodes[0].removepoolliquidity(accountGold, "0@GS", [])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("Amount out of range" in errorString)

        # missing (account exists, but does not belong)
        try:
            self.nodes[0].removepoolliquidity(owner, "200@GS", [])
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("Are you an owner?" in errorString)

        resAmountA = make_rounded_decimal(25 * poolReserveA / poolLiquidity)
        resAmountB = make_rounded_decimal(25 * poolReserveB / poolLiquidity)

        # transfer
        self.nodes[0].removepoolliquidity(accountGold, "25@GS", [])
        self.nodes[0].generate(1)

        accountGoldInfo = self.nodes[0].getaccount(accountGold, {}, True)
        pool = self.nodes[0].getpoolpair("GS", True)


        assert_equal(accountGoldInfo[idGS], amountGS - 25)
        assert_equal(accountGoldInfo[idGold], amountGold + resAmountA)
        assert_equal(accountGoldInfo[idSilver], amountSilver + resAmountB)

        assert_equal(pool['1']['reserveA'], poolReserveA - resAmountA)
        assert_equal(pool['1']['reserveB'], poolReserveB - resAmountB)
        assert_equal(pool['1']['totalLiquidity'], poolLiquidity - 25)

        # Sending pool token
        #========================
        list_poolshares = self.nodes[0].listpoolshares()
        assert_equal(len(list_poolshares), 2)

        self.nodes[0].accounttoaccount(accountGold, {accountSilver: "50@GS"})
        self.nodes[0].generate(1)

        list_poolshares = self.nodes[0].listpoolshares()
        assert_equal(len(list_poolshares), 3)

        self.nodes[1].accounttoaccount(accountSilver, {accountGold: "50@GS"})
        self.nodes[1].generate(1)

        list_poolshares = self.nodes[0].listpoolshares()
        assert_equal(len(list_poolshares), 2)

        # FORBIDDEN feature
        # self.nodes[0].accounttoutxos(accountGold, {accountGold: "74.99999000@GS"})
        # self.nodes[0].generate(1)

        # list_poolshares = self.nodes[0].listpoolshares()
        # assert_equal(len(list_poolshares), 1)

        # self.nodes[0].utxostoaccount({accountGold: "74.99999000@GS"})
        # self.nodes[0].generate(1)

        # list_poolshares = self.nodes[0].listpoolshares()
        # assert_equal(len(list_poolshares), 2)

        # Remove all liquidity
        #========================
        pool = self.nodes[0].getpoolpair("GS", True)
        accountGoldInfo = self.nodes[0].getaccount(accountGold, {}, True)
        accountTestInfo = self.nodes[0].getaccount(accountTest, {}, True)

        gsAmountAcc1 = accountGoldInfo[idGS]
        goldAmountAcc1 = accountGoldInfo[idGold]
        silverAmountAcc1 = accountGoldInfo[idSilver]

        gsAmountAcc2 = accountTestInfo[idGS]
        goldAmountAcc2 = accountTestInfo[idGold]
        silverAmountAcc2 = accountTestInfo[idSilver]

        poolReserveA = pool['1']['reserveA']
        poolReserveB = pool['1']['reserveB']
        poolLiquidity = pool['1']['totalLiquidity']

        # remove gold acc liquidity
        resAmountA = make_rounded_decimal(gsAmountAcc1 * poolReserveA / poolLiquidity)
        resAmountB = make_rounded_decimal(gsAmountAcc1 * poolReserveB / poolLiquidity)

        # transfer
        self.nodes[0].removepoolliquidity(accountGold, str(gsAmountAcc1)+"@GS", [])
        self.nodes[0].generate(1)

        accountGoldInfo = self.nodes[0].getaccount(accountGold, {}, True)
        pool = self.nodes[0].getpoolpair("GS", True)

        assert(not idGS in accountGoldInfo)
        assert_equal(accountGoldInfo[idGold], goldAmountAcc1 + resAmountA)
        assert_equal(accountGoldInfo[idSilver], silverAmountAcc1 + resAmountB)

        assert_equal(pool['1']['reserveA'], poolReserveA - resAmountA)
        assert_equal(pool['1']['reserveB'], poolReserveB - resAmountB)
        assert_equal(pool['1']['totalLiquidity'], poolLiquidity - gsAmountAcc1)

        poolReserveA = pool['1']['reserveA']
        poolReserveB = pool['1']['reserveB']
        poolLiquidity = pool['1']['totalLiquidity']

        # remove test acc liquidity
        resAmountA = make_rounded_decimal(gsAmountAcc2 * poolReserveA / poolLiquidity)
        resAmountB = make_rounded_decimal(gsAmountAcc2 * poolReserveB / poolLiquidity)

        # transfer
        self.nodes[3].removepoolliquidity(accountTest, str(gsAmountAcc2)+"@GS", [])
        self.sync_all([self.nodes[0], self.nodes[3]])
        self.nodes[0].generate(1)

        accountTestInfo = self.nodes[0].getaccount(accountTest, {}, True)
        pool = self.nodes[0].getpoolpair("GS", True)

        assert(not idGS in accountTestInfo)
        assert_equal(accountTestInfo[idGold], goldAmountAcc2 + resAmountA)
        assert_equal(accountTestInfo[idSilver], silverAmountAcc2 + resAmountB)

        print("Empty pool:")
        pprint(pool)

        assert_equal(pool['1']['reserveA'], poolReserveA - resAmountA)
        assert_equal(pool['1']['reserveB'], poolReserveB - resAmountB)
        assert_equal(pool['1']['totalLiquidity'], poolLiquidity - gsAmountAcc2)

        # minimum liquidity 0.00001
        assert_equal(pool['1']['totalLiquidity'], Decimal('0.00001'))

        # REVERTING:
        #========================
        print ("Reverting...")
        self.start_node(2)
        self.nodes[2].generate(20)

        connect_nodes_bi(self.nodes, 1, 2)
        self.sync_blocks()

        assert_equal(self.nodes[0].getaccount(accountGold, {}, True)[idGold], initialGold)
        assert_equal(self.nodes[0].getaccount(accountSilver, {}, True)[idSilver], initialSilver)

        assert_equal(len(self.nodes[0].getrawmempool()), 13) # 13 txs


if __name__ == '__main__':
    PoolLiquidityTest ().main ()
