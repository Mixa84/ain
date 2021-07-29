#include <masternodes/loan.h>

const unsigned char CLoanView::LoanSetCollateralTokenCreationTx           ::prefix = 0x10;
const unsigned char CLoanView::LoanSetCollateralTokenKey                  ::prefix = 0x11;
const unsigned char CLoanView::CreateLoanSchemeKey                        ::prefix = 0x12;
const unsigned char CLoanView::LoanSetLoanTokenCreationTx                 ::prefix = 0x13;
const unsigned char CLoanView::LoanSetLoanTokenByID                       ::prefix = 0x14;

std::unique_ptr<CLoanView::CLoanSetCollateralTokenImpl> CLoanView::GetLoanSetCollateralToken(uint256 const & txid) const
{
    auto collToken = ReadBy<LoanSetCollateralTokenCreationTx,CLoanSetCollateralTokenImpl>(txid);
    if (collToken)
        return MakeUnique<CLoanSetCollateralTokenImpl>(*collToken);
    return {};
}

Res CLoanView::LoanCreateSetCollateralToken(CLoanSetCollateralTokenImpl const & collToken)
{
    //this should not happen, but for sure
    if (GetLoanSetCollateralToken(collToken.creationTx))
        return Res::Err("setCollateralToken with creation tx %s already exists!", collToken.creationTx.GetHex());
    if (collToken.factor > COIN)
        return Res::Err("setCollateralToken factor must be lower or equal than 1!");
    if (collToken.factor < 0)
        return Res::Err("setCollateralToken factor must not be negative!");

    WriteBy<LoanSetCollateralTokenCreationTx>(collToken.creationTx, collToken);

    // invert height bytes so that we can find <= key with givven height
    uint32_t height = ~collToken.activateAfterBlock;
    WriteBy<LoanSetCollateralTokenKey>(CollateralTokenKey(collToken.idToken, height), collToken.creationTx);

    return Res::Ok();
}

void CLoanView::ForEachLoanSetCollateralToken(std::function<bool (CollateralTokenKey const &, uint256 const &)> callback, CollateralTokenKey const & start)
{
    ForEach<LoanSetCollateralTokenKey, CollateralTokenKey, uint256>(callback, start);
}

std::unique_ptr<CLoanView::CLoanSetCollateralTokenImpl> CLoanView::HasLoanSetCollateralToken(CollateralTokenKey const & key)
{
    auto it = LowerBound<LoanSetCollateralTokenKey>(key);
    if (it.Valid())
        return GetLoanSetCollateralToken(it.Value());
    return {};
}

std::unique_ptr<CLoanView::CLoanSetLoanTokenImpl> CLoanView::GetLoanSetLoanToken(uint256 const & txid) const
{
    auto loanToken = ReadBy<LoanSetLoanTokenCreationTx,CLoanSetLoanTokenImpl>(txid);
    if (loanToken)
        return MakeUnique<CLoanSetLoanTokenImpl>(*loanToken);
    return {};
}

std::unique_ptr<CLoanView::CLoanSetLoanTokenImpl> CLoanView::GetLoanSetLoanTokenByID(DCT_ID const & id) const
{
    auto txid = ReadBy<LoanSetLoanTokenByID, uint256>(id);
    auto loanToken = ReadBy<LoanSetLoanTokenCreationTx,CLoanSetLoanTokenImpl>(txid.get());
    if (loanToken)
        return MakeUnique<CLoanSetLoanTokenImpl>(*loanToken);
    return {};
}

Res CLoanView::LoanCreateSetLoanToken(CLoanSetLoanTokenImpl const & loanToken, DCT_ID const & id)
{
    //this should not happen, but for sure
    if (GetLoanSetCollateralToken(loanToken.creationTx))
        return Res::Err("setLoanToken with creation tx %s already exists!", loanToken.creationTx.GetHex());

    if (loanToken.interest < 0)
        return Res::Err("interest rate must be positive number!");

    WriteBy<LoanSetLoanTokenCreationTx>(loanToken.creationTx, loanToken);
    WriteBy<LoanSetLoanTokenByID>(id, loanToken.creationTx);

    return Res::Ok();
}

Res CLoanView::StoreLoanScheme(const CCreateLoanSchemeMessage& loanScheme)
{
    WriteBy<CreateLoanSchemeKey>(loanScheme.identifier, static_cast<CLoanSchemeData>(loanScheme));

    return Res::Ok();
}

void CLoanView::ForEachLoanScheme(std::function<bool (const std::string&, const CLoanSchemeData&)> callback)
{
    ForEach<CreateLoanSchemeKey, std::string, CLoanSchemeData>(callback);
}
