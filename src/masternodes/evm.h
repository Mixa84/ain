#ifndef DEFI_MASTERNODES_EVM_H
#define DEFI_MASTERNODES_EVM_H

#include <amount.h>
#include <flushablestorage.h>
#include <masternodes/balances.h>
#include <masternodes/res.h>
#include <script/script.h>
#include <serialize.h>
#include <uint256.h>

using CAccountEVMMap = std::map<std::pair<CScript, CScript>, CBalances>;
using CRawEvmTx = TBytes;

constexpr const uint16_t EVM_TX_SIZE = 32768;

enum CEvmInOutType : uint8_t {
    EvmIn  = 0x01,
    EvmOut = 0x02,
};

std::string CEvmInOutTypeToString(const CEvmInOutType type);


struct CEvmInOutMessage {
    CAccountEVMMap fromToMap;
    uint8_t type;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(fromToMap);
        READWRITE(type);
    }
};

class CEvmInOutObject : public CEvmInOutMessage {
public:
    uint256 creationTx;
    int32_t creationHeight = -1;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITEAS(CEvmInOutMessage, *this);
        READWRITE(creationTx);
        READWRITE(creationHeight);
    }
};

struct CEvmTxMessage {
    CRawEvmTx evmTx;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(evmTx);
    }
};

class CEvmView : public virtual CStorageView {
public:
    Res CreateEvmInOut(const CEvmInOutObject&);

    struct EvmInOut {
        static constexpr uint8_t prefix() { return 0x1F; }
    };
};

#endif // DEFI_MASTERNODES_EVM_H
