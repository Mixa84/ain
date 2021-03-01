#include <masternodes/mn_rpc.h>

UniValue createorder(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWallet(request);

    RPCHelpMan{"createorder",
                "\nCreates (and submits to local node and network) a order creation transaction.\n" +
                HelpRequiringPassphrase(pwallet) + "\n",
                {
                    {"order", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                        {
                            {"ownerAddress", RPCArg::Type::STR, RPCArg::Optional::NO, "Address of the owner of token"},
                            {"tokenFrom", RPCArg::Type::STR, RPCArg::Optional::NO, "Symbol or id of selling token"},
                            {"tokenTo", RPCArg::Type::STR, RPCArg::Optional::NO, "Symbol or id of buying token"},
                            {"amountFrom", RPCArg::Type::NUM, RPCArg::Optional::NO, "tokenFrom coins amount"},
                            {"orderPrice", RPCArg::Type::NUM, RPCArg::Optional::NO, "Price per unit"},
                            {"expiry", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Number of blocks until the order expires (Default: 2880 blocks)"}
                        },
                    },
                },
                RPCResult{
                        "\"hash\"                  (string) The hex-encoded hash of broadcasted transaction\n"
                },
                RPCExamples{
                        HelpExampleCli("createorder", "'{\"ownerAddress\":\"tokenAddress\","
                                                        "\"tokenFrom\":\"MyToken\",\"tokenTo\":\"Token1\","
                                                        "'\"amountFrom\":\"10\",\"orderPrice\":\"0.02\"}'")
                        + HelpExampleCli("createorder", "'{\"ownerAddress\":\"tokenAddress\","
                                                        "\"tokenFrom\":\"MyToken\",\"tokenTo\":\"Token2\","
                                                        "'\"amountFrom\":\"5\",\"orderPrice\":\"0.1\","
                                                        "\"expiry\":\"120\"}'")
                },
     }.Check(request);

    if (pwallet->chain().isInitialBlockDownload()) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot create order while still in Initial Block Download");
    }
    pwallet->BlockUntilSyncedToCurrentChain();
    LockedCoinsScopedGuard lcGuard(pwallet);

    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VARR}, true);
    if (request.params[0].isNull()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Invalid parameters, arguments 1 must be non-null and expected as object at least with "
                           "{\"ownerAddress\",\"tokenFrom\",\"tokenTo\",\"amountFrom\",\"orderPrice\"}");
    }
    UniValue metaObj = request.params[0].get_obj();
    COrder order;
    std::string tokenFromSymbol, tokenToSymbol;

    if (!metaObj["ownerAddress"].isNull()) {
        order.ownerAddress = trim_ws(metaObj["ownerAddress"].getValStr());
    }
    if (!metaObj["tokenFrom"].isNull()) {
        tokenFromSymbol = trim_ws(metaObj["tokenFrom"].getValStr()).substr(0, CToken::MAX_TOKEN_SYMBOL_LENGTH);
    }
    if (!metaObj["tokenTo"].isNull()) {
        tokenToSymbol = trim_ws(metaObj["tokenTo"].getValStr()).substr(0, CToken::MAX_TOKEN_SYMBOL_LENGTH);
    }
    if (!metaObj["amountFrom"].isNull()) {
        order.amountFrom = metaObj["amountFrom"].get_int64();
    }
    if (!metaObj["expiry"].isNull()) {
        order.expiry = metaObj["expiry"].get_int();
    }
    CTxDestination ownerDest = DecodeDestination(order.ownerAddress);
    if (ownerDest.which() == 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "ownerAdress (" + order.ownerAddress + ") does not refer to any valid address");
    }

    int targetHeight;
    {
        LOCK(cs_main);
        DCT_ID id;
        auto tokenFrom = pcustomcsview->GetTokenGuessId(tokenFromSymbol, id);
        if (!tokenFrom) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", order.tokenFrom));
        }
        auto tokenTo = pcustomcsview->GetTokenGuessId(tokenToSymbol, id);
        if (!tokenTo) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", order.tokenTo));
        }
        order.tokenFrom=tokenFrom->symbol;
        order.tokenTo=tokenTo->symbol;

        CBalances totalBalances;
        pcustomcsview->ForEachBalance([&](CScript const & owner, CTokenAmount const & balance) {
            if (IsMineCached(*pwallet, owner) == ISMINE_SPENDABLE) {
                totalBalances.Add(balance);
            }
            return true;
        });
        auto it = totalBalances.balances.begin();
        for (int i = 0; it != totalBalances.balances.end(); it++, i++) {
            CTokenAmount bal = CTokenAmount{(*it).first, (*it).second};
            std::string tokenIdStr = bal.nTokenId.ToString();
            auto token = pcustomcsview->GetToken(bal.nTokenId);
            if (token->CreateSymbolKey(bal.nTokenId)==order.tokenFrom && bal.nValue<order.amountFrom)
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Not enough balance for Token %s for order amount %s!", order.tokenFrom, order.amountFrom));
        }
    
        targetHeight = ::ChainActive().Height() + 1;
    }

    CDataStream metadata(DfTxMarker, SER_NETWORK, PROTOCOL_VERSION);
    metadata << static_cast<unsigned char>(CustomTxType::CreateOrder)
             << order;

    CScript scriptMeta;
    scriptMeta << OP_RETURN << ToByteVector(metadata);

    const auto txVersion = GetTransactionVersion(targetHeight);
    CMutableTransaction rawTx(txVersion);

    rawTx.vout.push_back(CTxOut(0, scriptMeta));

    fund(rawTx, pwallet, {});

    // check execution
    {
        LOCK(cs_main);
        CCustomCSView mnview_dummy(*pcustomcsview); // don't write into actual DB
        CCoinsViewCache coinview(&::ChainstateActive().CoinsTip());
        const auto res = ApplyCreateOrderTx(mnview_dummy, coinview, CTransaction(rawTx), targetHeight,
                                      ToByteVector(CDataStream{SER_NETWORK, PROTOCOL_VERSION, order}), Params().GetConsensus());
        if (!res.ok) {
            throw JSONRPCError(RPC_INVALID_REQUEST, "Execution test failed:\n" + res.msg);
        }
    }
    return signsend(rawTx, pwallet, {})->GetHash().GetHex();
}
UniValue fulfillorder(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWallet(request);

    RPCHelpMan{"fulfillorder",
                "\nCreates (and submits to local node and network) a order creation transaction.\n" +
                HelpRequiringPassphrase(pwallet) + "\n",
                {
                    {"order", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                        {
                            {"ownerAddress", RPCArg::Type::STR, RPCArg::Optional::NO, "Address of the owner of token"},
                            {"orderTx", RPCArg::Type::STR, RPCArg::Optional::NO, "txid of maker order"},
                            {"amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "coins amount to fulfill the order"},
                        },
                    },
                },
                RPCResult{
                        "\"hash\"                  (string) The hex-encoded hash of broadcasted transaction\n"
                },
                RPCExamples{
                        HelpExampleCli("fulfillorder", "'{\"ownerAddress\":\"tokenAddress\","
                                                        "\"orderTx\":\"txid\",\"amount\":\"10\"}'")
                },
     }.Check(request);

    if (pwallet->chain().isInitialBlockDownload()) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot create token while still in Initial Block Download");
    }
    pwallet->BlockUntilSyncedToCurrentChain();
    LockedCoinsScopedGuard lcGuard(pwallet);

    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VARR}, true);
    if (request.params[0].isNull()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Invalid parameters, arguments 1 must be non-null and expected as object at least with "
                           "{\"ownerAddress\",\"orderTx\",\"amount\"}");
    }
    UniValue metaObj = request.params[0].get_obj();
    CFulfillOrder order;

    if (!metaObj["ownerAddress"].isNull()) {
        order.ownerAddress = trim_ws(metaObj["ownerAddress"].getValStr());
    }
    if (!metaObj["orderTx"].isNull()) {
        order.orderTx = uint256S(metaObj["orderTx"].getValStr());
    }
    if (!metaObj["amount"].isNull()) {
        order.amount = metaObj["amount"].get_int64();
    }
    CTxDestination ownerDest = DecodeDestination(order.ownerAddress);
    if (ownerDest.which() == 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "ownerAdress (" + order.ownerAddress + ") does not refer to any valid address");
    }

    int targetHeight;
    {
        LOCK(cs_main);
        // DCT_ID id;
        // auto tokenFrom = pcustomcsview->GetTokenGuessId(order.tokenFrom, id);
        // if (id == DCT_ID{0}) {
        //     throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Can't alter DFI token!"));
        // }
        // if (!tokenFrom) {
        //     throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", order.tokenFrom));
        // }
        // auto tokenTo = pcustomcsview->GetTokenGuessId(order.tokenTo, id);
        // if (id == DCT_ID{0}) {
        //     throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Can't alter DFI token!"));
        // }
        // if (!tokenTo) {
        //     throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", order.tokenTo));
        // }
        // CBalances totalBalances;
        // pcustomcsview->ForEachBalance([&](CScript const & owner, CTokenAmount const & balance) {
        //     if (IsMineCached(*pwallet, owner) == ISMINE_SPENDABLE) {
        //         totalBalances.Add(balance);
        //     }
        //     return true;
        // });
        // auto it = totalBalances.balances.begin();
        // for (int i = 0; it != totalBalances.balances.end(); it++, i++) {
        //     CTokenAmount bal = CTokenAmount{(*it).first, (*it).second};
        //     std::string tokenIdStr = bal.nTokenId.ToString();
        //     auto token = pcustomcsview->GetToken(bal.nTokenId);
        //     if (token->CreateSymbolKey(bal.nTokenId)==order.tokenFrom && bal.nValue<order.amountFrom)
        //         throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Not enough balance for Token %s for order amount %s!", order.tokenFrom, order.amountFrom));
        // }
    
        targetHeight = ::ChainActive().Height() + 1;
    }


    CDataStream metadata(DfTxMarker, SER_NETWORK, PROTOCOL_VERSION);
    metadata << static_cast<unsigned char>(CustomTxType::FulfillOrder)
             << order;

    CScript scriptMeta;
    scriptMeta << OP_RETURN << ToByteVector(metadata);

    const auto txVersion = GetTransactionVersion(targetHeight);
    CMutableTransaction rawTx(txVersion);

    rawTx.vout.push_back(CTxOut(0, scriptMeta));

    fund(rawTx, pwallet, {});

    // check execution
    {
        LOCK(cs_main);
        CCustomCSView mnview_dummy(*pcustomcsview); // don't write into actual DB
        CCoinsViewCache coinview(&::ChainstateActive().CoinsTip());
        const auto res = ApplyCreateOrderTx(mnview_dummy, coinview, CTransaction(rawTx), targetHeight,
                                      ToByteVector(CDataStream{SER_NETWORK, PROTOCOL_VERSION, order}), Params().GetConsensus());
        if (!res.ok) {
            throw JSONRPCError(RPC_INVALID_REQUEST, "Execution test failed:\n" + res.msg);
        }
    }
    return signsend(rawTx, pwallet, {})->GetHash().GetHex();
}


static const CRPCCommand commands[] =
{ 
//  category        name                     actor (function)        params
//  --------------- ----------------------   ---------------------   ----------
    {"orderbook", "createorder",             &createorder,           {"ownerAddress", "tokenFrom", "tokenTo", "amountFrom", "orderPrice"}},
    {"orderbook", "fulfillorder",            &fulfillorder,          {"ownerAddress", "orderTx", "amount"}},
    // {"orderbook", "closeorder",              &closeorder,            {"pagination", "verbose"}},
};

void RegisterMasternodesRPCCommands(CRPCTable& tableRPC) {
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
