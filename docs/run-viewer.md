# Offline simulation run reports

`sts-report` records one ordinary Ironclad simulation and exports a self-contained, read-only HTML timeline. It does not add browser game controls or make network requests. Reports can be opened directly with `file://` in a modern browser.

## Build

Use CMake 3.31 (tested), a C++17 compiler, and Python 3 for embedding the viewer resources. The project minimum is CMake 3.19; the pinned legacy submodules may require compatibility-policy configuration with CMake 4. From the repository root:

```sh
git submodule update --init --recursive
cmake -S . -B cmake-build-report -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-report --target sts-report report-tests report-fixtures -j2
```

The report targets are separate from Python bindings. The pinned pybind11 2.7.1 does not compile against this environment's Python 3.14 headers (`PyFrameObject` / `PyThreadState::frame` errors). This pre-existing binding limitation does not prevent building the report targets. Do not infer successful binding compatibility from a successful report build.

## Generate reports

Game seeds use the canonical game seed notation. Existing HTML and `.trace.jsonl` files are never overwritten. The search agent's default budget is 50,000 simulations; use a smaller positive budget such as 100 for a quick local example.

```sh
mkdir -p files
./cmake-build-report/sts-report --seed 4J8K2 --agent simple --output files/simple-run.html
./cmake-build-report/sts-report --seed 4J8K2 --agent search --simulations 100 --output files/search-run.html
```

Each output has an adjacent `*.html.trace.jsonl` recovery/audit trace. A handled simulation interruption can leave a nonzero exit status while still exporting an explicitly interrupted report and preserving the trace. A fatal process termination may leave only the complete trace lines written before termination.

### Optional artwork

Artwork acquisition is explicit and separate from report generation. It requires network access only when you run the preparation command; the generated report itself is offline. Proprietary game art is not included in this repository. Keep imported assets local and do not commit them.

```sh
python3 tools/prepare_report_assets.py --sources report/assets/sources.json --output files/report-assets
./cmake-build-report/sts-report --seed 4J8K2 --agent simple --output files/simple-run.html \
  --assets files/report-assets/manifest.json
```

You may instead prepare an asset manifest from a local directory with the catalog's original image filenames:

```sh
python3 tools/prepare_report_assets.py --sources report/assets/sources.json \
  --output files/local-report-assets --local-dir /path/to/original-images
```

The currently catalogued 23 assets are not a complete art set; absent or unprovided images are shown with readable text fallbacks. Use a fresh output directory when changing a prepared catalog; different existing artwork/manifests and symlinks are not overwritten. No external URL is loaded by the viewer. Game artwork is © Mega Crit, sourced from the Slay the Spire Wiki; it is not covered by this repository's source-code license.

## Controlled fixtures and verification

The fixture executable writes two deterministic HTML examples through the production `TraceWriter`, snapshot adapters, and HTML exporter. These controlled examples include states chosen to exercise combat and screen rendering; they are not full game runs and must not be used as performance or win-rate evidence.

```sh
./cmake-build-report/report-fixtures --output-dir cmake-build-report/report-fixtures
REPORT_FIXTURE_DIR="$PWD/cmake-build-report/report-fixtures" npm --prefix tests/report/browser test
```

Browser development dependencies are local to `tests/report/browser`:

```sh
npm ci --prefix tests/report/browser
npx --prefix tests/report/browser playwright install chromium
```

For generated full-run verification, create both reports above in a directory, then run:

```sh
REPORT_GENERATED_DIR="$PWD/files" npm --prefix tests/report/browser test -- generated-run.spec.mjs
```

That suite skips when `REPORT_GENERATED_DIR` is unset; it checks the two named reports and their trace sidecars when supplied. The complete report verification commands are:

```sh
ctest --test-dir cmake-build-report -R '^report\.' --output-on-failure
python3 -m unittest discover -s tests/report -p 'test_*.py'
REPORT_FIXTURE_DIR="$PWD/cmake-build-report/report-fixtures" npm --prefix tests/report/browser test
```

## Verification measurements

Recording-enabled and disabled full runs matched for seeds `1`, `2`, and `3`, for both agents (search budget `10`), including histories/logged actions, display state, and RNG states. These are invariance checks, not win-rate estimates.

Seven alternating, single-thread benchmark pairs compared revision `5728ec3` against recording-disabled reporting code on GCC 15.2, using 500,000 fixed combat-search operations per trial. Median elapsed time was **2.17809 s baseline / 2.22913 s current (+2.34%)**; checksums matched, with **7,560,000 allocations** in each run. The archived baseline was compiled with `-include algorithm` for the same missing-header compatibility repair. Timing is machine/workload-specific and is not a CTest assertion.

Build `report-benchmark` and use `tools/benchmark_report.py --baseline /path/to/baseline/report-benchmark --current cmake-build-report/report-benchmark` to repeat the seven-pair comparison. Compile the same `tests/report/BenchmarkReport.cpp` against each revision's engine sources; do not benchmark recording-enabled export against disabled search.
