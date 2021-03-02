#include <masternodes/order.h>
#include <masternodes/masternodes.h>

/// @attention make sure that it does not overlap with other views !!!
const unsigned char COrderView::CreationTx  ::prefix = 'O';
const unsigned char COrderView::TokenFromID  ::prefix = 'P';
const unsigned char COrderView::TokenToID  ::prefix = 'R';

const unsigned char CFulfillOrderView::CreationTx  ::prefix = 'S';

std::unique_ptr<COrderView::COrderImpl> COrderView::GetOrderByCreationTx(const uint256 & txid) const
{
    auto orderImpl=ReadBy<CreationTx, COrderImpl>(txid);
    if (orderImpl) 
        return MakeUnique<COrderImpl>(*orderImpl);
    return nullptr;
}

ResVal<uint256> COrderView::CreateOrder(const COrderView::COrderImpl& order)
{
    //this should not happen, but for sure
    if (GetOrderByCreationTx(order.creationTx)) {
        return Res::Err("order with creation tx %s already exists!", order.creationTx.GetHex());
    }

    auto tokenFrom = pcustomcsview->GetToken(order.idTokenFrom);
    if (!tokenFrom) {
        return Res::Err("%s: token %s does not exist!", tokenFrom->symbol);
    }
    auto tokenTo = pcustomcsview->GetToken(order.idTokenTo);
    if (!tokenTo) {
        return Res::Err("%s: token %s does not exist!", tokenTo->symbol);
    }

    WriteBy<CreationTx>(order.creationTx, order);
    WriteBy<TokenFromID>(order.idTokenFrom, order.creationTx);
    WriteBy<TokenToID>(order.idTokenTo, order.creationTx);
    return {order.creationTx, Res::Ok()};
}

std::unique_ptr<CFulfillOrderView::CFulfillOrderImpl> CFulfillOrderView::GetFulfillOrderByCreationTx(const uint256 & txid) const
{
    auto fillorderImpl=ReadBy<CreationTx, CFulfillOrderImpl>(txid);
    if (fillorderImpl)
        return MakeUnique<CFulfillOrderImpl>(*fillorderImpl);
    return nullptr;
}

ResVal<uint256> CFulfillOrderView::FulfillOrder(const CFulfillOrderView::CFulfillOrderImpl & fillorder)
{
    //this should not happen, but for sure
    if (GetFulfillOrderByCreationTx(fillorder.creationTx)) {
        return Res::Err("fillorder with creation tx %s already exists!", fillorder.creationTx.GetHex());
    }

    WriteBy<CreationTx>(fillorder.creationTx, fillorder);
    return {fillorder.creationTx, Res::Ok()};
}