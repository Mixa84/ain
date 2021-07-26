#ifndef DEFI_MASTERNODES_LOAN_H
#define DEFI_MASTERNODES_LOAN_H

#include <amount.h>
#include <uint256.h>

#include <flushablestorage.h>
#include <masternodes/res.h>

class CLoanSetCollateralToken
{
public:
    DCT_ID idToken{UINT_MAX};
    CAmount factor;
    uint256 priceFeedTxid;
    uint32_t activateAfterBlock = 0;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(idToken);
        READWRITE(factor);
        READWRITE(priceFeedTxid);
        READWRITE(activateAfterBlock);
    }
};

class CLoanSetCollateralTokenImplementation : public CLoanSetCollateralToken
{
public:
    uint256 creationTx;
    int32_t creationHeight = -1;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CLoanSetCollateralToken, *this);
        READWRITE(creationTx);
        READWRITE(creationHeight);
    }
};

struct CLoanSetCollateralTokenMessage : public CLoanSetCollateralToken {
    using CLoanSetCollateralToken::CLoanSetCollateralToken;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CLoanSetCollateralToken, *this);
    }
};

class CLoanSetGenToken
{
public:
    std::string symbol;
    std::string name;
    uint256 priceFeedTxid;
    bool mintable;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(symbol);
        READWRITE(name);
        READWRITE(priceFeedTxid);
        READWRITE(mintable);
    }
};

class CLoanSetGenTokenImplementation : public CLoanSetGenToken
{
public:
    uint256 creationTx;
    int32_t creationHeight = -1;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CLoanSetGenToken, *this);
        READWRITE(creationTx);
        READWRITE(creationHeight);
    }
};

struct CLoanSetGenTokenMessage : public CLoanSetGenToken {
    using CLoanSetGenToken::CLoanSetGenToken;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CLoanSetGenToken, *this);
    }
};

struct CLoanSchemeData
{
    uint32_t ratio;
    CAmount rate;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ratio);
        READWRITE(rate);
    }
};

struct CLoanScheme : public CLoanSchemeData
{
    std::string identifier;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CLoanSchemeData,*this);
        READWRITE(identifier);
    }
};

// Add alias consistent with naming scheme for metadata
using CCreateLoanSchemeMessage = CLoanScheme;

class CLoanView : public virtual CStorageView {
public:
    using CollateralTokenKey = std::pair<DCT_ID, uint32_t>;
    using CLoanSetCollateralTokenImpl = CLoanSetCollateralTokenImplementation;
    using CLoanSetGenTokenImpl = CLoanSetGenTokenImplementation;

    std::unique_ptr<CLoanSetCollateralTokenImpl> GetLoanSetCollateralToken(uint256 const & txid) const;
    Res LoanCreateSetCollateralToken(CLoanSetCollateralTokenImpl const & collToken);
    void ForEachLoanSetCollateralToken(std::function<bool (CollateralTokenKey const &, uint256 const &)> callback, CollateralTokenKey const & start = {{0},0});
    std::unique_ptr<CLoanSetCollateralTokenImpl> HasLoanSetCollateralToken(CollateralTokenKey const & key);

    std::unique_ptr<CLoanSetGenTokenImpl> GetLoanSetGenToken(uint256 const & txid) const;
    Res LoanCreateSetGenToken(CLoanSetGenTokenImpl const & genToken);

    Res StoreLoanScheme(const CCreateLoanSchemeMessage& loanScheme);
    void ForEachLoanScheme(std::function<bool (const std::string&, const CLoanSchemeData&)> callback);

    struct LoanSetCollateralTokenCreationTx { static const unsigned char prefix; };
    struct LoanSetCollateralTokenKey { static const unsigned char prefix; };
    struct LoanSetGenTokenCreationTx { static const unsigned char prefix; };
    struct LoanSetGenTokenKey { static const unsigned char prefix; };
    struct CreateLoanSchemeKey { static const unsigned char prefix; };
};

#endif // DEFI_MASTERNODES_LOAN_H
