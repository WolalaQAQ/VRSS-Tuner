# Audited driver contracts

The common provider selects an exact NVAPI/UMD image pair before creating the
gaze handler or calling private registration methods. Unknown or mixed images
are rejected; the marketing version alone is not sufficient.

| Driver | Implementation | SHA-256 |
| --- | --- | --- |
| 596.02 | `nvapi64.dll` | `a902fe11d59166285c264aea4695d1978eb6ce6cf0d4c477fcddcac1527ddfb2` |
| 596.02 | `nvwgf2umx.dll` | `ec0cb8ade4bf81e6b6835832e7e4d665d59600c616967577fae162455edb505c` |
| 610.88 | `nvapi64_impl.dll` | `df6455ccf83e43cfe68f405af1eec4e053c7f95da998bf358053b7583980c2f4` |
| 610.88 | `nvwgf2umx.dll` | `04c01cfc849e5b58ad50da89dc83bff15f15847684de7964ff5900fedfab18fe` |

The 610.88 hashes and eight method RVAs are retained from upstream. The 596.02
contract was traced in the installed binaries and checked at runtime; its
vtable RVA is additionally pinned to `0x480280`. Both paths check that the
complete table lies within the selected module before reading its eight slots,
then compare every method against that version's audited RVAs.

`common-vrss-provider/test-driver-identity.ps1` tests selection, mixed/missing
images, changed methods, ASLR, table bounds and arithmetic overflow without
loading NVIDIA. Constants are in `driver_identity.h`; no driver binaries are
distributed or modified.
