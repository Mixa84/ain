#include <masternodes/mn_rpc.h>

extern UniValue tokenToJSON(DCT_ID const& id, CTokenImplementation const& token, bool verbose);

UniValue setCollateralTokenToJSON(CLoanSetCollateralTokenImplementation const& collToken)
{
    UniValue collTokenObj(UniValue::VOBJ);

    auto token = pcustomcsview->GetToken(collToken.idToken);
    if (!token)
        return (UniValue::VNULL);
    collTokenObj.pushKV("token", token->CreateSymbolKey(collToken.idToken));
    collTokenObj.pushKV("factor", ValueFromAmount(collToken.factor));
    collTokenObj.pushKV("priceFeedId", collToken.priceFeedTxid.GetHex());
    if (collToken.activateAfterBlock)
        collTokenObj.pushKV("activateAfterBlock", static_cast<int>(collToken.activateAfterBlock));

    UniValue ret(UniValue::VOBJ);
    ret.pushKV(collToken.creationTx.GetHex(), collTokenObj);
    return (ret);
}

UniValue setLoanTokenToJSON(CLoanSetLoanTokenImplementation const& loanToken, DCT_ID tokenId)
{
    UniValue loanTokenObj(UniValue::VOBJ);

    auto token = pcustomcsview->GetToken(tokenId);
    if (!token)
        return (UniValue::VNULL);

    loanTokenObj.pushKV("token",tokenToJSON(tokenId, *static_cast<CTokenImplementation*>(token.get()), true));
    loanTokenObj.pushKV("priceFeedId", loanToken.priceFeedTxid.GetHex());
    loanTokenObj.pushKV("interest", ValueFromAmount(loanToken.interest));

    UniValue ret(UniValue::VOBJ);
    ret.pushKV(loanToken.creationTx.GetHex(), loanTokenObj);
    return (ret);
}

UniValue setcollateraltoken(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWallet(request);

    RPCHelpMan{"setcollateraltoken",
                "Creates (and submits to local node and network) a set colleteral token transaction.\n" +
                HelpRequiringPassphrase(pwallet) + "\n",
                {
                    {"metadata", RPCArg::Type::OBJ, RPCArg::Optional::NO, "",
                        {
                            {"token", RPCArg::Type::STR, RPCArg::Optional::NO, "Symbol or id of collateral token"},
                            {"factor", RPCArg::Type::NUM, RPCArg::Optional::NO, "Collateralization factor"},
                            {"priceFeedId", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "txid of oracle feeding the price"},
                            {"activateAfterBlock", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "changes will be active after the block height (Optional)"},
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
                        HelpExampleCli("setcollateraltoken", R"('{"token":"XXX","factor":"150","priceFeedId":"txid"}')")
                        },
     }.Check(request);

    if (pwallet->chain().isInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot create order while still in Initial Block Download");

    pwallet->BlockUntilSyncedToCurrentChain();
    LockedCoinsScopedGuard lcGuard(pwallet);

    RPCTypeCheck(request.params, {UniValue::VOBJ}, false);
    if (request.params[0].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Invalid parameters, arguments 1 must be non-null and expected as object at least with "
                           "{\"token\",\"factor\",\"priceFeedId\"}");

    UniValue metaObj = request.params[0].get_obj();
    UniValue const & txInputs = request.params[1];

    std::string tokenSymbol;
    CLoanSetCollateralToken collToken;

    if (!metaObj["token"].isNull())
        tokenSymbol = trim_ws(metaObj["token"].getValStr());
    else
        throw JSONRPCError(RPC_INVALID_PARAMETER,"Invalid parameters, argument \"token\" must not be null");
    if (!metaObj["factor"].isNull())
        collToken.factor = AmountFromValue(metaObj["factor"]);
    else
        throw JSONRPCError(RPC_INVALID_PARAMETER,"Invalid parameters, argument \"factor\" must not be null");
    if (!metaObj["priceFeedId"].isNull())
        collToken.priceFeedTxid = uint256S(metaObj["priceFeedId"].getValStr());
    else
        throw JSONRPCError(RPC_INVALID_PARAMETER,"Invalid parameters, argument \"priceFeedId\" must be non-null");
    if (!metaObj["activateAfterBlock"].isNull())
        collToken.activateAfterBlock = metaObj["activateAfterBlock"].get_int();

    int targetHeight;
    {
        LOCK(cs_main);
        DCT_ID idToken;
        std::unique_ptr<CToken> token;

        token = pcustomcsview->GetTokenGuessId(tokenSymbol, idToken);
        if (!token)
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", tokenSymbol));
        collToken.idToken = idToken;

        targetHeight = ::ChainActive().Height() + 1;
    }

    CDataStream metadata(DfTxMarker, SER_NETWORK, PROTOCOL_VERSION);
    metadata << static_cast<unsigned char>(CustomTxType::LoanSetCollateralToken)
             << collToken;

    CScript scriptMeta;
    scriptMeta << OP_RETURN << ToByteVector(metadata);

    const auto txVersion = GetTransactionVersion(targetHeight);
    CMutableTransaction rawTx(txVersion);

    CTransactionRef optAuthTx;
    std::set<CScript> auths;
    rawTx.vin = GetAuthInputsSmart(pwallet, rawTx.nVersion, auths, true, optAuthTx, txInputs);

    rawTx.vout.push_back(CTxOut(0, scriptMeta));

    CCoinControl coinControl;

    // Return change to auth address
    CTxDestination dest;
    ExtractDestination(*auths.cbegin(), dest);
    if (IsValidDestination(dest))
        coinControl.destChange = dest;

    fund(rawTx, pwallet, optAuthTx, &coinControl);

    // check execution
    {
        LOCK(cs_main);
        CCoinsViewCache coinview(&::ChainstateActive().CoinsTip());
        if (optAuthTx)
            AddCoins(coinview, *optAuthTx, targetHeight);
        const auto metadata = ToByteVector(CDataStream{SER_NETWORK, PROTOCOL_VERSION, collToken});
        execTestTx(CTransaction(rawTx), targetHeight, metadata, CLoanSetCollateralTokenMessage{}, coinview);
    }

    return signsend(rawTx, pwallet, optAuthTx)->GetHash().GetHex();
}

UniValue getcollateraltoken(const JSONRPCRequest& request) {
    RPCHelpMan{"getcollateraltokens",
                "Return list of created collateral tokens.\n",
                {
                    {"by", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                        {
                            {"token", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Symbol or id of collateral token"},
                            {"height", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Valid at specified height"},
                        },
                    },
                },
                RPCResult
                {
                    "{...}     (object) Json object with collateral token information\n"
                },
                RPCExamples{
                    HelpExampleCli("icx_listcollateraltokensgetorder", "")
                },
     }.Check(request);

    UniValue ret(UniValue::VOBJ);
    std::string tokenSymbol;
    DCT_ID idToken = {std::numeric_limits<uint32_t>::max()}, currentToken = {std::numeric_limits<uint32_t>::max()};
    uint32_t height = ::ChainActive().Height();

    if (request.params.size() > 0)
    {
        UniValue byObj = request.params[0].get_obj();

        if (!byObj["token"].isNull())
        {
            auto token = pcustomcsview->GetTokenGuessId(trim_ws(byObj["token"].getValStr()), idToken);
            if (!token)
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", tokenSymbol));
        }
        if (!byObj["height"].isNull())
            height = (size_t) byObj["height"].get_int64();
    }

    CLoanView::CollateralTokenKey start({{0}, ~height});
    if (idToken.v != std::numeric_limits<uint32_t>::max())
    {
        start.first = idToken;
        auto collToken = pcustomcsview->HasLoanSetCollateralToken(start);
        ret.pushKVs(setCollateralTokenToJSON(*collToken));

        return (ret);
    }

    pcustomcsview->ForEachLoanSetCollateralToken([&](CLoanView::CollateralTokenKey const & key, uint256 collTokenTx) {

        if (idToken.v != std::numeric_limits<uint32_t>::max() && key.first != idToken)
            return false;

        if (~key.second >= height || currentToken == key.first) return true;

        currentToken = key.first;
        auto collToken = pcustomcsview->GetLoanSetCollateralToken(collTokenTx);
        if (collToken)
        {
            ret.pushKVs(setCollateralTokenToJSON(*collToken));
        }
        return true;
    }, start);

    return (ret);
}


UniValue listcollateraltokens(const JSONRPCRequest& request) {
    RPCHelpMan{"listcollateraltokens",
                "Return list of all created collateral tokens.\n",
                {},
                RPCResult
                {
                    "{...}     (object) Json object with collateral token information\n"
                },
                RPCExamples{
                    HelpExampleCli("listcollateraltokens", "")
                },
     }.Check(request);

    UniValue ret(UniValue::VOBJ);

    pcustomcsview->ForEachLoanSetCollateralToken([&](CLoanView::CollateralTokenKey const & key, uint256 collTokenTx) {
        auto collToken = pcustomcsview->GetLoanSetCollateralToken(collTokenTx);
        if (collToken)
        {
            ret.pushKVs(setCollateralTokenToJSON(*collToken));
        }
        return true;
    });

    return (ret);
}

UniValue setloantoken(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWallet(request);

    RPCHelpMan{"setloantoken",
                "Creates (and submits to local node and network) a token for a price feed set in collateral token.\n" +
                HelpRequiringPassphrase(pwallet) + "\n",
                {
                    {"metadata", RPCArg::Type::OBJ, RPCArg::Optional::NO, "",
                        {
                            {"symbol", RPCArg::Type::STR, RPCArg::Optional::NO, "Token's symbol (unique), no longer than " + std::to_string(CToken::MAX_TOKEN_SYMBOL_LENGTH)},
                            {"name", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Token's name (optional), no longer than " + std::to_string(CToken::MAX_TOKEN_NAME_LENGTH)},
                            {"priceFeedId", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "txid of oracle feeding the price"},
                            {"mintable", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED, "Token's 'Mintable' property (bool, optional), default is 'True'"},
                            {"factor", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Interest rate (default: 0)"},
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
                        HelpExampleCli("setloantoken", R"('{"token":"XXX","factor":"150","priceFeedId":"txid"}')")
                        },
     }.Check(request);

    if (pwallet->chain().isInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot create order while still in Initial Block Download");

    pwallet->BlockUntilSyncedToCurrentChain();
    LockedCoinsScopedGuard lcGuard(pwallet);

    RPCTypeCheck(request.params, {UniValue::VOBJ}, false);
    if (request.params[0].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Invalid parameters, arguments 1 must be non-null and expected as object at least with "
                           "{\"token\",\"factor\",\"priceFeedId\"}");

    UniValue metaObj = request.params[0].get_obj();
    UniValue const & txInputs = request.params[1];

    CLoanSetLoanToken genToken;

    if (!metaObj["symbol"].isNull())
        genToken.symbol = trim_ws(metaObj["symbol"].getValStr());
    else
        throw JSONRPCError(RPC_INVALID_PARAMETER,"Invalid parameters, argument \"symbol\" must not be null");
    if (!metaObj["name"].isNull())
        genToken.name = trim_ws(metaObj["name"].getValStr());
    if (!metaObj["priceFeedId"].isNull())
        genToken.priceFeedTxid = uint256S(metaObj["priceFeedId"].getValStr());
    else
        throw JSONRPCError(RPC_INVALID_PARAMETER,"Invalid parameters, argument \"priceFeedId\" must be non-null");
    if (!metaObj["mintable"].isNull())
        genToken.mintable = metaObj["mintable"].getBool();
    if (!metaObj["interest"].isNull())
        genToken.interest = AmountFromValue(metaObj["interest"]);
    else
        throw JSONRPCError(RPC_INVALID_PARAMETER,"Invalid parameters, argument \"interest\" must not be null");


    int targetHeight = ::ChainActive().Height() + 1;

    CDataStream metadata(DfTxMarker, SER_NETWORK, PROTOCOL_VERSION);
    metadata << static_cast<unsigned char>(CustomTxType::LoanSetLoanToken)
             << genToken;

    CScript scriptMeta;
    scriptMeta << OP_RETURN << ToByteVector(metadata);

    const auto txVersion = GetTransactionVersion(targetHeight);
    CMutableTransaction rawTx(txVersion);

    CTransactionRef optAuthTx;
    std::set<CScript> auths;
    rawTx.vin = GetAuthInputsSmart(pwallet, rawTx.nVersion, auths, true, optAuthTx, txInputs);

    rawTx.vout.push_back(CTxOut(0, scriptMeta));

    CCoinControl coinControl;

    // Return change to auth address
    CTxDestination dest;
    ExtractDestination(*auths.cbegin(), dest);
    if (IsValidDestination(dest))
        coinControl.destChange = dest;

    fund(rawTx, pwallet, optAuthTx, &coinControl);

    // check execution
    {
        LOCK(cs_main);
        CCoinsViewCache coinview(&::ChainstateActive().CoinsTip());
        if (optAuthTx)
            AddCoins(coinview, *optAuthTx, targetHeight);
        const auto metadata = ToByteVector(CDataStream{SER_NETWORK, PROTOCOL_VERSION, genToken});
        execTestTx(CTransaction(rawTx), targetHeight, metadata, CLoanSetLoanTokenMessage{}, coinview);
    }

    return signsend(rawTx, pwallet, optAuthTx)->GetHash().GetHex();
}

UniValue updateloantoken(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWallet(request);

    RPCHelpMan{"updateloantoken",
                "Creates (and submits to local node and network) a token for a price feed set in collateral token.\n" +
                HelpRequiringPassphrase(pwallet) + "\n",
                {
                    {"token", RPCArg::Type::STR, RPCArg::Optional::NO, "The tokens's symbol, id or creation tx"},
                    {"metadata", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                        {
                            {"symbol", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Token's symbol (unique), no longer than " + std::to_string(CToken::MAX_TOKEN_SYMBOL_LENGTH)},
                            {"name", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Token's name (optional), no longer than " + std::to_string(CToken::MAX_TOKEN_NAME_LENGTH)},
                            {"priceFeedId", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED, "txid of oracle feeding the price"},
                            {"mintable", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED, "Token's 'Mintable' property (bool, optional), default is 'True'"},
                            {"factor", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Interest rate (default: 0)"},
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
                        HelpExampleCli("setloantoken", R"('{"token":"XXX","factor":"150","priceFeedId":"txid"}')")
                        },
     }.Check(request);

    if (pwallet->chain().isInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot create order while still in Initial Block Download");

    pwallet->BlockUntilSyncedToCurrentChain();
    LockedCoinsScopedGuard lcGuard(pwallet);

    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VARR}, false);
    if (request.params[0].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Invalid parameters, arguments 0 must be non-null and expected as string with token symbol, id or creation txid");

    std::string const tokenStr = trim_ws(request.params[0].getValStr());
    UniValue metaObj = request.params[1].get_obj();
    UniValue const & txInputs = request.params[2];

    std::unique_ptr<CLoanSetLoanTokenImplementation> loanToken;
    CTokenImplementation tokenImpl;

    int targetHeight;
    {
        LOCK(cs_main);

        DCT_ID id;
        auto token = pcustomcsview->GetTokenGuessId(tokenStr, id);
        if (id == DCT_ID{0}) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Can't alter DFI token!"));
        }
        loanToken = pcustomcsview->GetLoanSetLoanTokenByID(id);
        if (!token || !loanToken) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s does not exist!", tokenStr));
        }
        tokenImpl = static_cast<CTokenImplementation const& >(*token);
        if (tokenImpl.IsLoanToken()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Token %s is not a loan token! Can't alter other tokens with this tx!", tokenStr));
        }

        targetHeight = ::ChainActive().Height() + 1;
    }

    if (!metaObj["symbol"].isNull())
        loanToken->symbol = trim_ws(metaObj["symbol"].getValStr());
    if (!metaObj["name"].isNull())
        loanToken->name = trim_ws(metaObj["name"].getValStr());
    if (!metaObj["priceFeedId"].isNull())
        loanToken->priceFeedTxid = uint256S(metaObj["priceFeedId"].getValStr());
    if (!metaObj["mintable"].isNull())
        loanToken->mintable = metaObj["mintable"].getBool();
    if (!metaObj["interest"].isNull())
        loanToken->interest = AmountFromValue(metaObj["interest"]);

    CDataStream metadata(DfTxMarker, SER_NETWORK, PROTOCOL_VERSION);
    metadata << static_cast<unsigned char>(CustomTxType::LoanUpdateLoanToken)
             << loanToken->creationTx << static_cast<CLoanSetLoanToken>(*loanToken);

    CScript scriptMeta;
    scriptMeta << OP_RETURN << ToByteVector(metadata);

    const auto txVersion = GetTransactionVersion(targetHeight);
    CMutableTransaction rawTx(txVersion);

    CTransactionRef optAuthTx;
    std::set<CScript> auths;
    rawTx.vin = GetAuthInputsSmart(pwallet, rawTx.nVersion, auths, true, optAuthTx, txInputs);

    rawTx.vout.push_back(CTxOut(0, scriptMeta));

    CCoinControl coinControl;

    // Return change to auth address
    CTxDestination dest;
    ExtractDestination(*auths.cbegin(), dest);
    if (IsValidDestination(dest))
        coinControl.destChange = dest;

    fund(rawTx, pwallet, optAuthTx, &coinControl);

    // check execution
    {
        LOCK(cs_main);
        CCoinsViewCache coinview(&::ChainstateActive().CoinsTip());
        if (optAuthTx)
            AddCoins(coinview, *optAuthTx, targetHeight);
        const auto metadata = ToByteVector(CDataStream{SER_NETWORK, PROTOCOL_VERSION, *loanToken});
        execTestTx(CTransaction(rawTx), targetHeight, metadata, CLoanUpdateLoanTokenMessage{}, coinview);
    }

    return signsend(rawTx, pwallet, optAuthTx)->GetHash().GetHex();
}

UniValue listloantokens(const JSONRPCRequest& request) {
    RPCHelpMan{"listloantokens",
                "Return list of all created loan tokens.\n",
                {},
                RPCResult
                {
                    "{...}     (object) Json object with loan token information\n"
                },
                RPCExamples{
                    HelpExampleCli("listloantokens", "")
                },
     }.Check(request);

    UniValue ret(UniValue::VOBJ);

    pcustomcsview->ForEachLoanSetLoanToken([&](DCT_ID const & key, CLoanView::CLoanSetLoanTokenImpl loanToken) {
        ret.pushKVs(setLoanTokenToJSON(loanToken,key));

        return true;
    });

    return (ret);
}

UniValue createloanscheme(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWallet(request);

    RPCHelpMan{"createloanscheme",
                "Creates a loan scheme transaction.\n" +
                HelpRequiringPassphrase(pwallet) + "\n",
                {
                    {"ratio", RPCArg::Type::NUM, RPCArg::Optional::NO, "Minimum collateralization ratio (integer)."},
                    {"rate", RPCArg::Type::NUM, RPCArg::Optional::NO, "Interest rate (integer or float)."},
                    {"identifier", RPCArg::Type::STR, RPCArg::Optional::NO, "Unique identifier of the loan scheme (8 chars max)."},
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
                   HelpExampleCli("createloanscheme", "150 5 LOAN0001") +
                   HelpExampleRpc("createloanscheme", "150, 5, LOAN0001")
                },
    }.Check(request);

    if (pwallet->chain().isInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot create order while still in Initial Block Download");

    pwallet->BlockUntilSyncedToCurrentChain();
    LockedCoinsScopedGuard lcGuard(pwallet);

    CCreateLoanSchemeMessage loanScheme;
    loanScheme.ratio = request.params[0].get_int();
    loanScheme.rate = AmountFromValue(request.params[1]);
    loanScheme.identifier = request.params[2].get_str();

    if (loanScheme.ratio < 100) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Ratio cannot be less than 100");
    }

    if (loanScheme.rate < 1000000) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Rate cannot be less than 0.01");
    }

    if (loanScheme.identifier.empty() || loanScheme.identifier.length() > 8) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Identifier cannot be empty or more than 8 chars long");
    }

    pcustomcsview->ForEachLoanScheme([&loanScheme](const std::string& key, const CLoanSchemeData& data) {
        if (key == loanScheme.identifier) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Loan scheme already exist with identifier %s", key));
        }

        if (data.ratio == loanScheme.ratio && data.rate == loanScheme.rate) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Loan scheme with same rate and ratio already exists");
        }
        return true;
    });

    int targetHeight;
    {
        LOCK(cs_main);
        targetHeight = ::ChainActive().Height() + 1;
    }

    CDataStream metadata(DfTxMarker, SER_NETWORK, PROTOCOL_VERSION);
    metadata << static_cast<unsigned char>(CustomTxType::CreateLoanScheme)
             << loanScheme;

    CScript scriptMeta;
    scriptMeta << OP_RETURN << ToByteVector(metadata);

    const auto txVersion = GetTransactionVersion(targetHeight);
    CMutableTransaction rawTx(txVersion);

    CTransactionRef optAuthTx;
    std::set<CScript> auths;
    rawTx.vin = GetAuthInputsSmart(pwallet, rawTx.nVersion, auths, true, optAuthTx, request.params[3]);

    rawTx.vout.emplace_back(0, scriptMeta);

    CCoinControl coinControl;

    // Set change to foundation address
    CTxDestination dest;
    ExtractDestination(*auths.cbegin(), dest);
    if (IsValidDestination(dest)) {
        coinControl.destChange = dest;
    }

    fund(rawTx, pwallet, optAuthTx, &coinControl);

    // check execution
    {
        LOCK(cs_main);
        CCoinsViewCache coinview(&::ChainstateActive().CoinsTip());
        if (optAuthTx)
            AddCoins(coinview, *optAuthTx, targetHeight);
        const auto metadata = ToByteVector(CDataStream{SER_NETWORK, PROTOCOL_VERSION, loanScheme});
        execTestTx(CTransaction(rawTx), targetHeight, metadata, CCreateLoanSchemeMessage{}, coinview);
    }

    return signsend(rawTx, pwallet, optAuthTx)->GetHash().GetHex();
}

UniValue listloanschemes(const JSONRPCRequest& request) {

    RPCHelpMan{"listloanschemes",
               "List all available loan schemes\n",
               {},
               RPCResult{
                       "[                         (json array of objects)\n"
                       "  {\n"
                       "    \"identifier\" : n           (string)\n"
                       "    \"ratio\" : n                (numeric)\n"
                       "    \"rate\" : n                 (numeric)\n"
                       "  },\n"
                       "  ...\n"
                       "]\n"
               },
               RPCExamples{
                       HelpExampleCli("createloanscheme", "150 5 LOAN0001") +
                       HelpExampleRpc("createloanscheme", "150, 5, LOAN0001")
               },
    }.Check(request);

    auto cmp = [](const CLoanScheme& a, const CLoanScheme& b) {
        return a.ratio == b.ratio ? a.rate < b.rate : a.ratio < b.ratio;
    };
    std::set<CLoanScheme, decltype(cmp)> loans(cmp);

    pcustomcsview->ForEachLoanScheme([&loans](const std::string& identifier, const CLoanSchemeData& data){
        CLoanScheme loanScheme;
        loanScheme.rate = data.rate;
        loanScheme.ratio = data.ratio;
        loanScheme.identifier = identifier;
        loans.insert(loanScheme);
        return true;
    });

    UniValue ret(UniValue::VARR);
    for (const auto& item : loans) {
        UniValue arr(UniValue::VOBJ);
        arr.pushKV("identifier", item.identifier);
        arr.pushKV("ratio", static_cast<uint64_t>(item.ratio));
        arr.pushKV("rate", ValueFromAmount(item.rate));
        ret.push_back(arr);
    }

    return ret;
}

static const CRPCCommand commands[] =
{
//  category        name                         actor (function)        params
//  --------------- ----------------------       ---------------------   ----------
    {"loan",        "setcollateraltoken",        &setcollateraltoken,    {"metadata", "inputs"}},
    {"loan",        "getcollateraltoken",        &getcollateraltoken,    {"by"}},
    {"loan",        "listcollateraltokens",      &listcollateraltokens,  {}},
    {"loan",        "setloantoken",              &setloantoken,          {"metadata", "inputs"}},
    {"loan",        "updateloantoken",           &updateloantoken,       {"metadata", "inputs"}},
    {"loan",        "listloantokens",            &listloantokens,        {}},
    {"loan",        "createloanscheme",          &createloanscheme,      {"ratio", "rate"}},
    {"loan",        "listloanschemes",           &listloanschemes,       {}},
};

void RegisterLoanRPCCommands(CRPCTable& tableRPC) {
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
