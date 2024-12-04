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


