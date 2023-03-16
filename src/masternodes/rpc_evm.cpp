#include <masternodes/mn_rpc.h>

UniValue transferbalance(const JSONRPCRequest& request) {
    auto pwallet = GetWallet(request);

    RPCHelpMan{"transferbalance",
                "Creates (and submits to local node and network) a tx to transfer balance from DFI/ETH address to DFI/ETH address.\n" +
                HelpRequiringPassphrase(pwallet) + "\n",
                {
                    {"metadata", RPCArg::Type::OBJ, RPCArg::Optional::NO, "A json array of json objects",
                        {
                            {"map", RPCArg::Type::ARR, RPCArg::Optional::NO, "",
                                {
                                    {"from", RPCArg::Type::STR, RPCArg::Optional::NO, "From address. If \"from\" value is: \"*\" (star), it's means auto-selection accounts from wallet."},
                                    {"to", RPCArg::Type::STR, RPCArg::Optional::NO, "To address."},
                                    {"context", RPCArg::Type::STR, RPCArg::Optional::NO, "defi-eth or eth-defi."},
                                    {"amounts", RPCArg::Type::STR, RPCArg::Optional::NO, "Amounts in amount@token format."},
                                },
                            },
                            {"type", RPCArg::Type::STR, RPCArg::Optional::NO, "Type of transfer: evmin/evmout."},
                        },
                    },
                    {"inputs", RPCArg::Type::ARR, RPCArg::Optional::OMITTED_NAMED_ARG,
                        "A json array of json objects",
                        {
                            {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                                {
                                    {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction id"},
                                    {"vout", RPCArg::Type::NUM, RPCArg::Optional::NO, "The output number"},
                                },
                            },
                        },
                    },
                },
                RPCResult{
                        "\"hash\"                  (string) The hex-encoded hash of broadcasted transaction\n"
                },
                RPCExamples{
                        HelpExampleCli("transferbalance", R"('{[{"from":"<defi_address>", "to":"<eth_address>", "amounts":"10@DFI"}}]')")
                        },
    }.Check(request);

    if (pwallet->chain().isInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot transferbalance while still in Initial Block Download");

    pwallet->BlockUntilSyncedToCurrentChain();

    RPCTypeCheck(request.params, {UniValue::VOBJ}, false);
    if (request.params[0].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Invalid parameters, argument 1 must be non-null and expected as array of objects at least with "
                           "{\"from\",\"to\",\"amounts\"}");
    UniValue metaObj = request.params[0].get_obj();

    RPCTypeCheck(metaObj, {UniValue::VARR}, false);

    if (metaObj.empty() )
        throw JSONRPCError(RPC_INVALID_PARAMETER,"Invalid parameters, argument 1 must be non-null and expected as array of objects at least with "
                           "{\"from\",\"to\",\"amounts\"}");

    int targetHeight;
    {
        LOCK(cs_main);
        targetHeight = ::ChainActive().Height() + 1;
    }

    CAccountEVMMap accountMap;
    CEvmInOutType type = (metaObj["type"].getValStr() == "evmin" ? CEvmInOutType::EvmIn : CEvmInOutType::EvmOut);

    UniValue array {UniValue::VARR};
    array = metaObj["map"].get_array();

    try {
        for (unsigned int i=0; i<array.size(); i++){
            auto obj = array[i].get_obj();
            RPCTypeCheckObj(obj,
                {
                    {"from",          UniValue::VSTR},
                    {"to",            UniValue::VSTR},
                    {"context",       UniValue::VSTR},
                    {"amounts",       UniValue::VSTR},
                },
                true,
                true);
            auto context = obj["context"].getValStr();
            auto fromStr = obj["from"].getValStr();
            auto toStr = obj["to"].getValStr();
            auto amounts = DecodeAmounts(pwallet->chain(), obj["amounts"], "");
            CScript from, to;
            if (context == "defi-eth") {
                if (fromStr == "*") {
                    auto selectedAccounts = SelectAccountsByTargetBalances(GetAllMineAccounts(pwallet), amounts, SelectionPie);

                    for (auto& account : selectedAccounts) {
                        auto it = amounts.balances.begin();
                        while (it != amounts.balances.end()) {
                            if (account.second.balances[it->first] < it->second)
                                break;
                            it++;
                        }
                        if (it == amounts.balances.end()) {
                            from = account.first;
                            break;
                        }
                    }

                    if (from.empty()) {
                        throw JSONRPCError(RPC_INVALID_REQUEST,
                                "Not enough tokens on account, call sendtokenstoaddress to increase it.\n");
                    }
                } else {
                    from = DecodeScript(fromStr);
                }
                if (!::IsMine(*pwallet, from))
                    throw JSONRPCError(RPC_INVALID_PARAMETER,
                            strprintf("Address (%s) is not owned by the wallet", obj["from"].getValStr()));
                to = DecodeScript(toStr);
            }
            else
            {
                from = DecodeScript(fromStr);
                to = DecodeScript(toStr);
            }

            accountMap.insert({{from, to}, amounts});
        }
    }catch(std::runtime_error& e) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, e.what());
    }

    CDataStream metadata(DfTxMarker, SER_NETWORK, PROTOCOL_VERSION);

    metadata << static_cast<unsigned char>(CustomTxType::EvmInOut)
                << CEvmInOutMessage{accountMap, type};

    CScript scriptMeta;
    scriptMeta << OP_RETURN << ToByteVector(metadata);

    const auto txVersion = GetTransactionVersion(targetHeight);
    CMutableTransaction rawTx(txVersion);

    CTransactionRef optAuthTx;
    std::set<CScript> auths;
    for(auto& addresses : accountMap)
        auths.insert(addresses.first.first);

    const UniValue &txInputs = request.params[1];
    rawTx.vin = GetAuthInputsSmart(pwallet, rawTx.nVersion, auths, false, optAuthTx, txInputs);

    rawTx.vout.emplace_back(0, scriptMeta);

    CCoinControl coinControl;

    // Return change to auth address
    CTxDestination dest;
    ExtractDestination(*auths.cbegin(), dest);
    if (IsValidDestination(dest))
        coinControl.destChange = dest;

    fund(rawTx, pwallet, optAuthTx, &coinControl);

    // check execution
    execTestTx(CTransaction(rawTx), targetHeight, optAuthTx);

    return signsend(rawTx, pwallet, optAuthTx)->GetHash().GetHex();
}

UniValue evmtx(const JSONRPCRequest& request) {
    auto pwallet = GetWallet(request);

    RPCHelpMan{"evmtx",
                "Creates (and submits to local node and network) a tx to send DFI token to EVM address.\n" +
                HelpRequiringPassphrase(pwallet) + "\n",
                {
                    {"rawEvmTx", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Raw tx in hex"},
                    {"inputs", RPCArg::Type::ARR, RPCArg::Optional::OMITTED_NAMED_ARG,
                        "A json array of json objects",
                        {
                            {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                                {
                                    {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction id"},
                                    {"vout", RPCArg::Type::NUM, RPCArg::Optional::NO, "The output number"},
                                },
                            },
                        },
                    },
                },
                RPCResult{
                        "\"hash\"                  (string) The hex-encoded hash of broadcasted transaction\n"
                },
                RPCExamples{
                        HelpExampleCli("evmtx", R"('"<hex>"')")
                        },
    }.Check(request);

    if (pwallet->chain().isInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot evmin while still in Initial Block Download");

    pwallet->BlockUntilSyncedToCurrentChain();

    if (request.params[0].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Invalid parameters, argument 1 must be non-null and expected as hex string");
    auto evmTx = ParseHex(request.params[0].get_str());

    int targetHeight;
    {
        LOCK(cs_main);
        targetHeight = ::ChainActive().Height() + 1;
    }

    CDataStream metadata(DfTxMarker, SER_NETWORK, PROTOCOL_VERSION);
    metadata << static_cast<unsigned char>(CustomTxType::EvmTx)
                << CEvmTxMessage{evmTx};

    CScript scriptMeta;
    scriptMeta << OP_RETURN << ToByteVector(metadata);

    const auto txVersion = GetTransactionVersion(targetHeight);
    CMutableTransaction rawTx(txVersion);

    CTransactionRef optAuthTx;
    std::set<CScript> auths;

    const UniValue &txInputs = request.params[1];
    rawTx.vin = GetAuthInputsSmart(pwallet, rawTx.nVersion, auths, false, optAuthTx, txInputs);

    rawTx.vout.emplace_back(0, scriptMeta);

    CCoinControl coinControl;

    // Return change to auth address
    CTxDestination dest;
    ExtractDestination(*auths.cbegin(), dest);
    if (IsValidDestination(dest))
        coinControl.destChange = dest;

    fund(rawTx, pwallet, optAuthTx, &coinControl);

    // check execution
    execTestTx(CTransaction(rawTx), targetHeight, optAuthTx);

    return signsend(rawTx, pwallet, optAuthTx)->GetHash().GetHex();
}

static const CRPCCommand commands[] =
{
//  category        name                         actor (function)        params
//  --------------- ----------------------       ---------------------   ----------
    {"evm",         "transferbalance",           &transferbalance,       {"metadata", "inputs"}},
    {"evm",         "evmtx",                     &evmtx,                 {"rawEvmTx", "inputs"}},
};

void RegisterEVMRPCCommands(CRPCTable& tableRPC) {
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
