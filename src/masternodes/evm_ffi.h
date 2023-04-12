#ifndef DEFI_EVM_FFI_H
#define DEFI_EVM_FFI_H

#include <chainparams.h>
#include "rust/include/libain_evm.h"

uint64_t getChainId();
bool isMining();

bool publishEthTransaction(rust::Vec<uint8_t> rawTransaction);

#endif // DEFI_EVM_FFI_H
