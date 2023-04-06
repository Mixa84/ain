fn main() {
    cxx_build::bridge("src/main.rs")
        .define("CLIENT_VERSION_MAJOR", Some("3"))
        .define("CLIENT_VERSION_MINOR", Some("2"))
        .define("CLIENT_VERSION_REVISION", Some("8"))
        .define("CLIENT_VERSION_BUILD", Some("0"))
        .define("CLIENT_VERSION_RC", Some("0"))
        .define("CLIENT_VERSION_IS_RELEASE", Some("true"))
        .define("COPYRIGHT_YEAR", Some("2021"))
        .include("../../../../depends/x86_64-pc-linux-gnu/include/")
        .include("../../../")
        .include("../../../leveldb/include/")
        .flag_if_supported("-std=c++17")
        .compile("ain-evm-cpp-ffi");

    println!("cargo:rerun-if-changed=src/main.rs");
    println!("cargo:rerun-if-changed=../../../masternodes/accounts.cpp");
    println!("cargo:rerun-if-changed=../../../masternodes/accounts.h");
    println!("cargo:rerun-if-changed=../../../masternodes/masternodes.h");
}
