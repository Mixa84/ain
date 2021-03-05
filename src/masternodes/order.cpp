#include <masternodes/order.h>
#include <masternodes/masternodes.h>

/// @attention make sure that it does not overlap with other views !!!
const unsigned char COrderView::OrderCreationTx  ::prefix = 'O';
const unsigned char COrderView::FulfillCreationTx  ::prefix = 'D';
const unsigned char COrderView::CloseCreationTx  ::prefix = 'E';
const unsigned char COrderView::TokenFromID  ::prefix = 'P';
const unsigned char COrderView::TokenToID  ::prefix = 'R';
const unsigned char COrderView::CloseTx  ::prefix = 'C';


std::unique_ptr<COrderView::COrderImpl> COrderView::GetOrderByCreationTx(const uint256 & txid) const
{
    auto orderImpl=ReadBy<OrderCreationTx, COrderImpl>(txid);
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
    WriteBy<OrderCreationTx>(order.creationTx, order);
    WriteBy<TokenFromID>(WrapVarInt(idTokenFrom.v), order.creationTx);
    WriteBy<TokenToID>(WrapVarInt(idTokenTo.v), order.creationTx);

    return {order.creationTx, Res::Ok()};
}

ResVal<uint256> COrderView::CloseOrderTx(const COrderView::COrderImpl& order)
{
    if (!GetOrderByCreationTx(order.creationTx)) {
        return Res::Err("order with creation tx %s doesn't exists!", order.creationTx.GetHex());
    }
    EraseBy<OrderCreationTx>(order.creationTx);
    WriteBy<OrderCreationTx>(order.creationTx, order);
    WriteBy<CloseTx>(order.closeTx, order.creationTx);

    return {order.closeTx, Res::Ok()};
}

void COrderView::ForEachOrder(std::function<bool (const DCT_ID&, uint256)> callback, DCT_ID const & start)
{
    DCT_ID tokenId = start;
    auto hint = WrapVarInt(tokenId.v);

    ForEach<TokenFromID, CVarInt<VarIntMode::DEFAULT, uint32_t>, uint256>([&tokenId, &callback] (CVarInt<VarIntMode::DEFAULT, uint32_t> const &, uint256 orderTx) {
        return callback(tokenId, orderTx);
    }, hint);
}

std::unique_ptr<COrderView::CFulfillOrderImpl> COrderView::GetFulfillOrderByCreationTx(const uint256 & txid) const
{
    auto fillorderImpl=ReadBy<FulfillCreationTx, CFulfillOrderImpl>(txid);
    if (fillorderImpl)
        return MakeUnique<CFulfillOrderImpl>(*fillorderImpl);
    return nullptr;
}

ResVal<uint256> COrderView::FulfillOrder(const COrderView::CFulfillOrderImpl & fillorder)
{
    //this should not happen, but for sure
    if (GetFulfillOrderByCreationTx(fillorder.creationTx)) {
        return Res::Err("fillorder with creation tx %s already exists!", fillorder.creationTx.GetHex());
    }

    WriteBy<FulfillCreationTx>(fillorder.creationTx, fillorder);
    return {fillorder.creationTx, Res::Ok()};
}

std::unique_ptr<COrderView::CCloseOrderImpl> COrderView::GetCloseOrderByCreationTx(const uint256 & txid) const
{
    auto closeorderImpl=ReadBy<CloseCreationTx, CCloseOrderImpl>(txid);
    if (closeorderImpl)
        return MakeUnique<CCloseOrderImpl>(*closeorderImpl);
    return nullptr;
}

ResVal<uint256> COrderView::CloseOrder(const COrderView::CCloseOrderImpl& closeorder)
{
    if (GetCloseOrderByCreationTx(closeorder.creationTx)) {
        return Res::Err("close with creation tx %s already exists!", closeorder.creationTx.GetHex());
    }
    WriteBy<CloseCreationTx>(closeorder.creationTx, closeorder);

    return {closeorder.creationTx, Res::Ok()};
}
