#include <amount.h>
#include <script/script.h>
#include <serialize.h>
#include <uint256.h>

class COrder
{
public:
    static const int DEFAULT_ORDER_EXPIRY = 2880;

    //! basic properties
    std::string ownerAddress;
    std::string tokenFrom;
    std::string tokenTo;
    CAmount amountFrom;
    CAmount orderPrice;
    int expiry;
    

    COrder()
        : ownerAddress("")
        , tokenFrom("")
        , tokenTo("")
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
        READWRITE(amountFrom);
        READWRITE(orderPrice);
        READWRITE(expiry);
    }
};

// class COrderBookImplementation : public COrderBook
// {
// public:
//     std::string ownerAddress;
//     std::string tokenFrom;
//     std::string tokenTo;
//     CAmount amountFrom;
//     CAmount orderPrice;

//     COrderBookImplementation()
//         : COrderBook()
//         , ownerAddress(0)
//         , tokenFrom()
//         , tokenTo()
//         , amountFrom(-1)
//         , orderPrice(-1)
//     {}
//     ~CTokenImplementation() override = default;

//     ADD_SERIALIZE_METHODS;

//     template <typename Stream, typename Operation>
//     inline void SerializationOp(Stream& s, Operation ser_action) {
//         READWRITEAS(CToken, *this);
//         READWRITE(minted);
//         READWRITE(creationTx);
//         READWRITE(destructionTx);
//         READWRITE(creationHeight);
//         READWRITE(destructionHeight);
//     }
// };