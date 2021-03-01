#include <masternodes/order.h>
#include <masternodes/masternodes.h>

/// @attention make sure that it does not overlap with other views !!!
const unsigned char COrderView::CreationTx  ::prefix = 'O';
const unsigned char COrderView::TokenFromID  ::prefix = 'P';
const unsigned char COrderView::TokenToID  ::prefix = 'Q';

boost::optional<COrderView::CorderImpl> COrderView::GetOrderByCreationTx(const uint256 & txid) const
{
    DCT_ID id;
    auto orderImpl=ReadBy<CreationTx, CorderImpl>(txid);
    if (orderImpl)
        return { std::move(*orderImpl) };
    return {};
}

ResVal<uint256> COrderView::CreateOrder(const COrderView::CorderImpl & order)
{
    //this should not happen, but for sure
    if (GetOrderByCreationTx(order.creationTx)) {
        return Res::Err("order with creation tx %s already exists!", order.creationTx.GetHex());
    }

    auto pairFrom = pcustomcsview->GetToken(order.tokenFrom);
    if (!pairFrom->second) {
        return Res::Err("%s: token %s does not exist!", order.tokenFrom);
    }
    auto pairTo = pcustomcsview->GetToken(order.tokenTo);
    if (!pairTo->second) {
        return Res::Err("%s: token %s does not exist!", order.tokenTo);
    }

    WriteBy<CreationTx>(order.creationTx, order);
    WriteBy<TokenFromID>(pairFrom->first, order.creationTx);
    WriteBy<TokenToID>(pairTo->first, order.creationTx);
    return {order.creationTx, Res::Ok()};
}