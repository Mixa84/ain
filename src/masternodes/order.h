#ifndef DEFI_MASTERNODES_ORDER_H
#define DEFI_MASTERNODES_ORDER_H

#include <amount.h>
#include <script/script.h>
#include <serialize.h>
#include <uint256.h>

#include <masternodes/tokens.h>
#include <masternodes/res.h>

class COrder
{
public:
    static const int DEFAULT_ORDER_EXPIRY = 2880;

    //! basic properties
    std::string ownerAddress;
    std::string tokenFrom;
    std::string tokenTo;
    DCT_ID idTokenFrom;
    DCT_ID idTokenTo;
    CAmount amountFrom;
    CAmount orderPrice;
    uint32_t expiry;

    COrder()
        : ownerAddress("")
        , tokenFrom("")
        , tokenTo("")
        , idTokenFrom({0})
        , idTokenTo({0})
        , amountFrom(0)
        , orderPrice(0)
        , expiry(DEFAULT_ORDER_EXPIRY)
    {}
    virtual ~COrder() = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ownerAddress);
        READWRITE(tokenFrom);
        READWRITE(tokenTo);
        READWRITE(VARINT(idTokenFrom.v));
        READWRITE(VARINT(idTokenTo.v));
        READWRITE(amountFrom);
        READWRITE(orderPrice);
        READWRITE(expiry);
    }
};

class COrderImplemetation : public COrder
{
public:
    //! tx related properties
    uint256 creationTx;
    uint256 closeTx;
    uint32_t creationHeight; 
    uint32_t closeHeight;

    COrderImplemetation()
        : COrder()
        , creationTx()
        , closeTx()
        , creationHeight(-1)
        , closeHeight(-1)
    {}
    ~COrderImplemetation() override = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(COrder, *this);
        READWRITE(creationTx);
        READWRITE(closeTx);
        READWRITE(creationHeight);
        READWRITE(closeHeight);
    }
};

class COrderView : public virtual CStorageView {
public:
    using COrderImpl = COrderImplemetation;

    std::unique_ptr<COrderImpl> GetOrderByCreationTx(const uint256 & txid) const;
    ResVal<uint256> CreateOrder(const COrderImpl& order);
    ResVal<uint256> CloseOrder(const COrderImpl& order);


    struct CreationTx { static const unsigned char prefix; };
    struct CloseTx { static const unsigned char prefix; };
    struct TokenFromID { static const unsigned char prefix; };
    struct TokenToID { static const unsigned char prefix; };

};

class CFulfillOrder
{
public:
    //! basic properties
    std::string ownerAddress;
    uint256 orderTx;
    CAmount amount;

    CFulfillOrder()
        : ownerAddress("")
        , orderTx(uint256())
        , amount(0)
    {}
    virtual ~CFulfillOrder() = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ownerAddress);
        READWRITE(orderTx);
        READWRITE(amount);
    }
};

class CFulfillOrderImplemetation : public CFulfillOrder
{
public:
    //! tx related properties
    uint256 creationTx;
    uint256 closeTx;
    uint32_t creationHeight; 
    uint32_t closeHeight;

    CFulfillOrderImplemetation()
        : CFulfillOrder()
        , creationTx()
        , closeTx()
        , creationHeight(-1)
        , closeHeight(-1)
    {}
    ~CFulfillOrderImplemetation() override = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CFulfillOrder, *this);
        READWRITE(creationTx);
        READWRITE(closeTx);
        READWRITE(creationHeight);
        READWRITE(closeHeight);
    }
};

class CFulfillOrderView : public virtual CStorageView {
public:
    using CFulfillOrderImpl = CFulfillOrderImplemetation;

    std::unique_ptr<CFulfillOrderImpl> GetFulfillOrderByCreationTx(const uint256 & txid) const;
    ResVal<uint256> FulfillOrder(CFulfillOrderImpl const & fillorder);
    ResVal<uint256> CloseOrder(const CFulfillOrderImpl& fillorder);

    struct CreationTx { static const unsigned char prefix; };
    struct CloseTx { static const unsigned char prefix; };
};

#endif // DEFI_MASTERNODES_ORDER_H
