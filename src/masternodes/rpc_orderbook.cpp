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
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot create token while still in Initial Block Download");
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

    if (!metaObj["ownerAddress"].isNull()) {
        order.ownerAddress = trim_ws(metaObj["ownerAddress"].getValStr()).substr(0, CToken::MAX_TOKEN_SYMBOL_LENGTH);
    }
    if (!metaObj["tokenFrom"].isNull()) {
        order.tokenFrom = trim_ws(metaObj["tokenFrom"].getValStr()).substr(0, CToken::MAX_TOKEN_SYMBOL_LENGTH);
    }
    if (!metaObj["tokenTo"].isNull()) {
        order.tokenTo = trim_ws(metaObj["tokenTo"].getValStr()).substr(0, CToken::MAX_TOKEN_SYMBOL_LENGTH);
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
        auto tokenFrom = pcustomcsview->GetTokenGuessId(order.tokenFrom, id);
        if (id == DCT_ID{0}) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Can't alter DFI token!"));
        }
        if (!tokenFrom) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", order.tokenFrom));
        }
        auto tokenTo = pcustomcsview->GetTokenGuessId(order.tokenTo, id);
        if (id == DCT_ID{0}) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Can't alter DFI token!"));
        }
        if (!tokenTo) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", order.tokenTo));
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

static const CRPCCommand commands[] =
{ 
//  category        name                     actor (function)        params
//  --------------- ----------------------   ---------------------   ----------
    {"orderbook", "createorder",             &createorder,           {"ownerAddress", "tokenFrom", "tokenTo", "amountFrom", "orderPrice"}},
    // {"orderbook", "fulfillorder",            &fulfillorder,          {"mn_id", "inputs"}},
    // {"orderbook", "closeorder",              &closeorder,            {"pagination", "verbose"}},
};

void RegisterMasternodesRPCCommands(CRPCTable& tableRPC) {
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
