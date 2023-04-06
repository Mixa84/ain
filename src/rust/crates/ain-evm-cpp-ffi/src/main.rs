use cxx::UniquePtr;
use ffi::CScript;
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("masternodes/accounts.h");
        include!("masternodes/balances.h");
        include!("masternodes/masternodes.h");

        type CBalances;
        type CTokenAmount;
        type CScript;

        fn FFIGetBalance(owner: UniquePtr<CScript>) -> UniquePtr<CBalances>;

    }
    impl UniquePtr<CScript> {}
}

fn main() {
    let owner = CScript{};
    let balance = ffi::FFIGetBalance(owner);
}