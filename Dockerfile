FROM alpine:3

ENV OO_PS4_TOOLCHAIN="/opt/OpenOrbis/PS4Toolchain"

RUN apk update && \
    apk add llvm18 clang18 lld18 cmake make git curl && \
    apk cache clean -f && \
    ln -s /usr/bin/clang-18 /usr/bin/clang && \
    ln -s /usr/bin/clang++-18 /usr/bin/clang++ && \
    ln -s /usr/bin/llvm18-ar /usr/bin/llvm-ar && \
    ln -s /usr/bin/llvm18-ranlib /usr/bin/llvm-ranlib && \
    ln -s /usr/bin/llvm18-strip /usr/bin/llvm-strip && \
    wget https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.3/toolchain-llvm-18.2.zip && \
    unzip toolchain-llvm-18.2.zip && \
    rm toolchain-llvm-18.2.zip && \
    tar -xzvf toolchain-llvm-18.tar.gz -C /opt && \
    rm toolchain-llvm-18.tar.gz && \
    wget https://codeload.github.com/GoldHEN/GoldHEN_Plugins_SDK/zip/refs/heads/main -O GoldHEN_Plugins_SDK.zip && \
    unzip GoldHEN_Plugins_SDK.zip && \
    rm GoldHEN_Plugins_SDK.zip && \
    cd GoldHEN_Plugins_SDK-main && \
    make install && \
    cp build/crtprx.o ${OO_PS4_TOOLCHAIN}/lib/ && \
    cd .. && rm -rf GoldHEN_Plugins_SDK-main

