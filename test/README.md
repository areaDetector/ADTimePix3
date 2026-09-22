# Deterministic protocol fixtures

This directory contains hardware-free C++ tests and reusable fake Serval
peers. The test product also compiles and exercises the production
`NetworkClient` implementation. Both peers bind only to the IPv4 loopback
address and allocate an ephemeral port, so tests cannot contact a real Serval
instance or detector.

Build and run the fixture tests with one command:

```sh
make -C test runtests
```

The fake TCP peer releases scripted chunks only when directed by the test. It
can close after the final chunk or remain silent until teardown. The fake HTTP
peer records one request and returns a configurable status, headers, and body;
its response can also be held until the test releases it. All fixture waits
take explicit deadlines, and fixture destruction stops and joins peer threads.

These fixtures intentionally do not change or correct production protocol
behavior. Later remediation branches can use them to characterize production
streaming, timeout, and error-handling paths before making those changes.

## Live IOC PixelConfig validation

After configuring Channel Access for the target IOC, validate all detected
chips with:

```sh
test/validate_pixel_config.sh TPX3-TEST:cam1:
# or
test/validate_pixel_config.sh MPX3-TEST:cam1:
```

The script reads `DetType_RBV` and `NChips_RBV`, triggers
`RefreshPixelConfig`, and validates the length, BPC match, mismatch count, and
status for every chip. TPX3 expects 65536 bytes per chip; dual-threshold MPX3
expects 131072. It exits nonzero if Channel Access fails or any chip does not
match the selected BPC file. Network-specific `EPICS_CA_*` values are
intentionally left to the caller's environment.
