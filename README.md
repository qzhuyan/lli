# LLI (Low Level Inspection)

Inspect low level resources of the BEAM runtime with NIFs.

# Get LLI

## Opt1: Build release

``` sh
make release
```

## Opt2: Download the prebuilds (ubuntu x86_64/aarch64)

Prebuilds for Ubuntu are available here:

https://github.com/qzhuyan/lli/releases

# Load LLI

## Option 1: As depedency

Be part of you application release.

## Option 2: As Patch (Recommended)
1. ensure `$CWD/patches/` where $CWD is from the return of `file:get_cwd()`. 
2. Untar the release file (`lli*.tar.gz`) then put the `*.beam` and `*.so` to the
   $CWD/patches/. 

# Use LLI

## Check mac refcnt of hmac in openssl3 (only)

``` erlang
lli:mac_refcnt(show).
```

## Copy an off-heap binary allocation from a process

`erlang:process_info(Pid, binary)` returns entries of the form
`{BinaryId, BinarySize, BinaryRefcCount}`. `BinaryId` is an address of ERTS'
internal `Binary` structure. To copy the allocation for debugging:

```erlang
{binary, [{BinaryId, _Size, _Refc} | _]} =
    erlang:process_info(Pid, binary),
{ok, Data} = lli:process_binary(Pid, BinaryId).
```

`lli:process_binary/2` synchronously suspends the process, fetches fresh binary
metadata, verifies that the ID is still present, copies the bytes, and resumes
the process in an `after` block.

This API is unsafe, Linux-only debugging functionality:

- Process suspension does not prevent process-system work or termination, so a
  small lifetime race remains.
- The NIF mirrors an internal ERTS structure and must be rebuilt for the exact
  OTP version and architecture in use.
- It returns the complete underlying allocation, not a logical subbinary view.
- Magic and NIF resource binaries are rejected because their payload can be
  stored elsewhere.
- Copies are limited to 64 MiB by default. Override
  `LLI_MAX_BINARY_COPY_BYTES` at compile time if required.
- Never expose this operation to untrusted Erlang code or use it as a normal
  production API.

The lower-level `lli:unsafe_copy_binary(BinaryId, BinarySize)` skips process
suspension and membership checks and should normally not be called directly.

