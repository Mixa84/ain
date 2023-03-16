#include <masternodes/evm.h>

std::string CEvmInOutTypeToString(const CEvmInOutType type) {
    switch (type) {
        case CEvmInOutType::EvmIn:
            return "EvmIn";
        case CEvmInOutType::EvmOut:
            return "EvmOut";
    }
    return "Unknown";
}

Res CEvmView::CreateEvmInOut(const CEvmInOutObject &evmInOut) {
    WriteBy<EvmInOut>(evmInOut.creationTx, evmInOut);

    return Res::Ok();
}