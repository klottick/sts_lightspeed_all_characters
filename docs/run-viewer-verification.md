# Run viewer implementation verification

Verified on 2026-10-23, branch `odd-baboon`, implementation commit `3cbd26e`.

- **51/51 C++ report tests**, including recorded/unrecorded invariance for both agents with seeds 1, 2, and 3, and per-action battle RNG comparisons.
- **18/18 Python tests**, including actual CLI runs, validation/no-overwrite behavior, asset preparation, write-failure trace retention, and fatal process termination.
- **26/26 offline Chromium tests**, with actual simple/search reports for game seed `4J8K2` (search budget 100), C++-exported controlled fixtures, original embedded images, navigation, security, and desktop/mobile layouts.
- Existing `main` and `test` targets build. `test simple_agent_mt 1 1 1` completes.
- Python bindings remain blocked by pinned pybind11 2.7.1 against Python 3.14. Compiling the archived baseline reproduces the same `PyFrameObject` errors. No binding or dependency upgrade was included.
- Seven alternating benchmark pairs show **+2.34% median disabled overhead**, with identical checksums and allocation counts; see [measurements](run-viewer.md#verification-measurements).
- Fresh-context whole-branch Luna 6 review of `cf88766..3cbd26e` reported no Critical, Important, or Minor findings. Its review explicitly covered selection continuation, same-slot replacement, nested healing/revival, interrupted output, and special card costs.

## Execution decisions

| Decision | Reason | Cost if wrong |
| --- | --- | --- |
| Use local CMake 3.31.10 tooling | No host CMake; legacy pinned submodules predate CMake 4 | Tooling may need reconfiguration |
| Generate CTest's registry directly | Standard testing setup reserves the existing `test` build-target name | Future report tests must use the shared registry |
| Use bounded Luna 6 assistance alongside native execution | User explicitly requested that assistant; primary retained engine/invariance ownership | Shared interfaces require integration verification |
| Observe status/Block/energy changes at queue checkpoints | Primitive HP/healing hooks plus resolved queue boundaries avoid invasive hot-header changes | Future mutations outside those boundaries need an explicit observation hook |
| Force `<algorithm>` only in baseline benchmark compilation | GCC 15 needs the same missing-header repair authorized for the working branch | Compiler-layout differences may add timing noise |
| Combine exporter/frontend/CLI changes in one integration commit | Shared resources and CMake targets evolved together | Larger integration commit to bisect; earlier core commits remain separate |
| Leave the pre-existing binding incompatibility unchanged | No unrelated dependency/API upgrade authorized | Bindings cannot build with this environment's Python 3.14 |
| Keep the reporting CLI Ironclad-only | Approved CLI has no character option; it runs the existing single-run policy configuration | Other character classes need a later CLI integration |
| Never overwrite report/trace destinations | Preserve existing user artifacts and atomically publish new output | Repeated runs require a new filename |

## Deferred minor

The resolution inspector renders status-list before/after values as compact JSON. Actor panels separately show readable status names and amounts; improving the inspector's formatting remains presentation polish.

Nothing was pushed or merged as part of implementation.
