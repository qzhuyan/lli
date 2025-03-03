cmake_minimum_required(VERSION 3.16)
project(lli)

set(SOURCES
  c_src/lli_nif.c
)

SET(Erlang_EI_INCLUDE_DIRS   ${Erlang_OTP_LIB_DIR}/${Erlang_EI_DIR}/include)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/priv/)

find_package(OpenSSL REQUIRED)

if (DEFINED ENV{Erlang_OTP_ROOT_DIR})
  SET(Erlang_OTP_ROOT_DIR $ENV{Erlang_OTP_ROOT_DIR})
else()
EXECUTE_PROCESS(
        COMMAND         erl -noshell -eval "io:format(\"~s\", [code:root_dir()])" -s erlang halt
        OUTPUT_VARIABLE Erlang_OTP_ROOT_DIR
        )
endif()

add_library(lli_nif SHARED ${SOURCES})

target_include_directories(lli_nif PRIVATE ${Erlang_OTP_ROOT_DIR}/usr/include/ )

target_link_libraries(lli_nif PRIVATE ${OPENSSL_CRYPTO_LIBRARY})

set_target_properties(lli_nif
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/priv/
)
