# Contributing to ADTimePix3

Thank you for contributing to the EPICS areaDetector driver for TimePix3 (Serval).

## Fork, branch, and pull request

Use `origin` for the contributor's GitHub fork and `upstream` for the canonical
[`areaDetector/ADTimePix3`](https://github.com/areaDetector/ADTimePix3)
repository.

### One-time remote setup

Clone the contributor fork as `origin`, then add the canonical repository as
`upstream`:

```bash
git clone https://github.com/kgofron/ADTimePix3.git
cd ADTimePix3
git remote add upstream https://github.com/areaDetector/ADTimePix3.git
git remote -v
```

The expected remote names are:

- `origin` -- the contributor's fork (`kgofron/ADTimePix3` in this example).
- `upstream` -- the canonical `areaDetector/ADTimePix3` repository.

Contributors using another fork should replace `kgofron` with their GitHub
username.

### Submit one cohesive change

Create each feature or fix branch from the current `upstream/master`. Each
branch should contain one cohesive change.

```bash
git fetch --prune upstream
git switch -c fix/short-description upstream/master
# edit, build, and test
git push -u origin fix/short-description
gh pr create \
  --repo areaDetector/ADTimePix3 \
  --base master \
  --head kgofron:fix/short-description
```

The pull request should report the module build, all applicable automated test
results, and relevant hardware or emulator evidence. Clearly identify any
validation that was not applicable or could not be performed.

### After the pull request is merged

Synchronize the local `master` branch with `upstream/master`, update the fork,
and remove the completed feature branch:

```bash
git switch master
git fetch --prune upstream
git merge --ff-only upstream/master
git push origin master
git branch -d fix/short-description
git push origin --delete fix/short-description
```

## License and copyright

- The project is released under the **MIT License**. The full text is in [`LICENSE`](../LICENSE) at the repository root.
- Copyright is held by **Brookhaven Science Associates** (operator of Brookhaven National Laboratory) and **UT-Battelle, LLC** (operator of Oak Ridge National Laboratory). Development began at BNL through late 2022 and continued at ORNL from November 2022 onward.
- New **driver source** files under `tpx3App/src/` should copy the copyright and SPDX header block from an existing file such as `ADTimePix.cpp` (dual BNL/ORNL notice, MIT license identifier).

## REUSE and SPDX

License metadata is maintained for [REUSE](https://reuse.software/) Specification 3.0:

| Artifact | Purpose |
|----------|---------|
| [`.reuse/dep5`](../.reuse/dep5) | Default copyright and license for paths without long file headers (Db, OPI, IOC scripts, docs, tests, etc.) |
| [`LICENSES/MIT.txt`](../LICENSES/MIT.txt) | Canonical MIT license text required by REUSE |
| [`SPDX.spdx`](../SPDX.spdx) | Minimal software bill of materials (driver, bundled CPR/json, external Serval) |

Before opening a pull request or tagging a release, run from the repository root:

```bash
reuse lint
```

The command should finish with **REUSE Specification 3.0** compliance. If you add files under new directories, extend `.reuse/dep5` (or add SPDX headers in source) so `reuse lint` stays green.

**Database and OPI files:** `tpx3App/Db/*.template` use short `#` copyright and SPDX lines; Phoebus `.bob` / `.opi` files under `tpx3App/op/` rely on `.reuse/dep5` only - do not paste multi-line C-style headers into screens.

## Code changes

- Match the style and structure of the module you edit (`serval_http.cpp`, `serval_stream.cpp`, `acquire.cpp`, `mask_io.cpp`, etc.). See [README.md](../README.md) *Driver Analysis* and [RELEASE.md](../RELEASE.md) R1-6-3 for the current layout.
- Prefer **`ADTimePixLog.h`** (`ERR`, `WARN`, `LOG`, `FLOW`) over raw `printf` in driver code.
- Coordinate mapping: see [COORDINATE_MAP.md](COORDINATE_MAP.md) and run `python3 test/verify_coordinate_map.py` when changing `pelIndex` / `bpc2ImgIndex`.

## Build and test

```bash
make -j
make -C test runtests
python3 test/verify_coordinate_map.py
```

Run IOC/integration tests on your facility hardware or emulator as appropriate; there is no single CI matrix in this repository yet.

## Questions

Open an issue or pull request on [areaDetector/ADTimePix3](https://github.com/areaDetector/ADTimePix3). For release history and operator-facing changes, see [RELEASE.md](../RELEASE.md).
