# Patches for ExaTN

These patches are needed only to run on a GPU. A CPU build of ExaTN works
without them. `doc/gpu.md` says how to apply and use them.

| File | Apply in | What it does |
| --- | --- | --- |
| `exatn.patch` | the root of the ExaTN source tree | Fixes a crash when a program that uses a GPU build of ExaTN exits |
| `exatensor.patch` | `tpls/ExaTensor` of the ExaTN source tree | Adds the environment variables `TALSH_GFLOP_THRESH_GPU` and `TALSH_GPU_BUFFER_MB` |

Both are for ExaTN revision `c528f7e`. Each file starts with a longer
description.
