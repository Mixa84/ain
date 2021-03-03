#include <masternodes/order.h>
#include <masternodes/masternodes.h>

/// @attention make sure that it does not overlap with other views !!!
const unsigned char COrderView::CreationTx  ::prefix = 'O';
const unsigned char COrderView::TokenFromID  ::prefix = 'P';
const unsigned char COrderView::TokenToID  ::prefix = 'R';
const unsigned char COrderView::CloseTx  ::prefix = 'C';

const unsigned char CFulfillOrderView::CreationTx  ::prefix = 'E';
const unsigned char CFulfillOrderView::CloseTx  ::prefix = 'D';


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
    DCT_ID idTokenFrom=order.idTokenFrom;
    DCT_ID idTokenTo=order.idTokenTo;
    auto tokenFrom = pcustomcsview->GetToken(idTokenFrom);
    if (!tokenFrom) {
        return Res::Err("%s: token %s does not exist!", tokenFrom->symbol);
    }
    auto tokenTo = pcustomcsview->GetToken(idTokenTo);
    if (!tokenTo) {
        return Res::Err("%s: token %s does not exist!", tokenTo->symbol);
    }
    WriteBy<CreationTx>(order.creationTx, order);
    WriteBy<TokenFromID>(WrapVarInt(idTokenFrom.v), order.creationTx);
    WriteBy<TokenToID>(WrapVarInt(idTokenTo.v), order.creationTx);

    return {order.creationTx, Res::Ok()};
}

ResVal<uint256> COrderView::CloseOrder(const COrderView::COrderImpl& order)
{
    if (!GetOrderByCreationTx(order.creationTx)) {
        return Res::Err("order with creation tx %s doesn't exists!", order.creationTx.GetHex());
    }
    EraseBy<CreationTx>(order.creationTx);
    WriteBy<CreationTx>(order.creationTx, order);
    WriteBy<CloseTx>(order.closeTx, order.creationTx);

    return {order.closeTx, Res::Ok()};
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

ResVal<uint256> CFulfillOrderView::CloseOrder(const CFulfillOrderView::CFulfillOrderImpl& fillorder)
{
    if (!GetFulfillOrderByCreationTx(fillorder.creationTx)) {
        return Res::Err("order with creation tx %s doesn't exists!", fillorder.creationTx.GetHex());
    }
    EraseBy<CreationTx>(fillorder.creationTx);
    WriteBy<CreationTx>(fillorder.creationTx, fillorder);
    WriteBy<CloseTx>(fillorder.closeTx, fillorder.creationTx);

    return {fillorder.closeTx, Res::Ok()};
}