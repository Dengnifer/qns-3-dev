# Running on a GPU

qns-3 can run its tensor contractions on an NVIDIA GPU through ExaTN. The
module itself needs no change: you build ExaTN with CUDA, with the two patches
in `patches/`, and build ns-3 against it.

Read "What to expect" first. For the protocols of this module a GPU gives the
same results and is slower than one CPU core, because the tensors are small.

## Requirements

- An NVIDIA GPU and the CUDA toolkit. Tested with an RTX 4090 and CUDA 12.6.
- ExaTN revision `c528f7e`, as for the CPU build.

## 1. Build ExaTN for the GPU

```bash
git clone --recursive https://github.com/ORNL-QCI/exatn.git exatn-gpu
git -C exatn-gpu apply /path/to/quantum/patches/exatn.patch
git -C exatn-gpu/tpls/ExaTensor apply /path/to/quantum/patches/exatensor.patch

export PATH=/usr/local/cuda/bin:$PATH    # the build calls nvcc
cmake -S exatn-gpu -B exatn-gpu/build -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$HOME/.exatn-gpu \
      -DENABLE_CUDA=True \
      -DCUDA_TOOLKIT_ROOT_DIR=/usr/local/cuda \
      -DCUDA_HOST_COMPILER=/usr/bin/g++ \
      -DCUDA_ARCH_BIN=89 \
      -DBLAS_LIB=ATLAS -DBLAS_PATH=/usr/lib/x86_64-linux-gnu -DWITH_LAPACK=ON \
      -DEXATN_BUILD_TESTS=OFF \
      -DCMAKE_DISABLE_FIND_PACKAGE_MPI=ON \
      -DCMAKE_DISABLE_FIND_PACKAGE_Python=ON
cmake --build exatn-gpu/build -j 8
cmake --install exatn-gpu/build
```

Notes:

- `CUDA_ARCH_BIN` is the compute capability of the GPU without the dot: 89 for
  an RTX 4090, whose capability is 8.9.
- Install to a prefix of its own and keep the CPU build. A program linked with
  the GPU build aborts at start on a machine where it sees no GPU.
- The other options are those of the CPU build; see `README.md`.
- To rebuild after changing a patch, first remove the files that the build
  left in the source tree: `git -C exatn-gpu/tpls/ExaTensor clean -fdxq`.

### The two patches

- `patches/exatn.patch` fixes a crash at exit. `exatn::initialize()` could
  return before ExaTN had started CUDA. A program that then registers
  `exatn::finalize()` to run at exit, as this module does, ended with
  `#FATAL(exatn::runtime::TalshNodeExecutor): Unable to shut down TAL-SH!`
  after printing its results.
- `patches/exatensor.patch` adds two environment variables, described below.
  Without it ExaTN sends only very large contractions to the GPU, and no
  setting changes that.

## 2. Build ns-3 against it

```bash
./ns3 configure --enable-examples --enable-tests -- -DExaTN_DIR=$HOME/.exatn-gpu
./ns3 build
```

## 3. Run

```bash
export OMP_NUM_THREADS=1
export CUDA_VISIBLE_DEVICES=0        # the GPU to use
export TALSH_GPU_BUFFER_MB=2048      # memory to reserve on it
export TALSH_GFLOP_THRESH_GPU=0      # send every contraction to the GPU
./ns3 run "distill-nested-adapt-example --nPairs=128"
```

| Variable | Meaning | Without it |
| --- | --- | --- |
| `CUDA_VISIBLE_DEVICES` | The GPUs that ExaTN may use | ExaTN uses every GPU of the machine |
| `TALSH_GPU_BUFFER_MB` | Memory that ExaTN reserves on each GPU, in MiB. The largest tensor on a GPU can take one sixth of it | 90 % of the free memory of each GPU |
| `TALSH_GFLOP_THRESH_GPU` | A contraction goes to the GPU if it is larger than this many billion multiplications | 4, which no example of this module reaches |

On a shared machine, pick an idle GPU with `nvidia-smi` and always set the
first two variables. The module prints a reminder when `CUDA_VISIBLE_DEVICES`
is not set.

To check that the GPU did the work, look at the statistics that ExaTN prints
when the program ends:

```text
#MSG(TAL-SH::NV-TAL): Statistics on GPU #0:
 Number of tasks submitted: 2978
 Number of tasks completed: 2978
 Number of tasks failed   : 0
 Number of Flops processed: 1.21346E+06
#MSG(TAL-SH::CP-TAL): Statistics on CPU:
 Number of Flops processed    :      0.00000000000000D+00
```

## What to expect

Measured on one RTX 4090 against one CPU core of the same server, with every
contraction sent to the GPU. The results are identical in every case.

| Run | CPU | GPU | Contractions on the GPU |
| --- | --- | --- | --- |
| Test suite | 0.2 s | 2.2 s | 2978 |
| `ent-swap-adapt-example --nNodes=64` | 0.5 s | 3.0 s | 2271 |
| `telep-lin-adapt-example --nNodes=64` | 0.6 s | 3.3 s | 3543 |
| `ent-swap-adapt-local-example --nNodes=1024` | 3.2 s | 6.8 s | 24094 |
| `distill-nested-adapt-example --nPairs=128` | 0.9 s | 4.1 s | 9351 |
| `distill-nested-adapt-example --nPairs=512` | 4.1 s | 8.9 s | 36931 |
| `ent-swap-example --nNodes=16` | 1.6 s | 6.4 s | 28107 |

The GPU is slower here for two reasons. Starting CUDA takes about 2 s. And a
contraction of these networks is a few thousand multiplications, which costs
less to compute on the CPU than to hand to a GPU.

A GPU pays off when single contractions are large. The product of two complex
2048 x 2048 matrices, 7e10 multiplications, takes 0.14 s on the RTX 4090 and
36 s on one core with the reference BLAS (an optimized BLAS would narrow the
gap). A simulation reaches this size when single tensors have more than about
twenty qubit legs, far beyond the protocols of this module.

## Limits

- ExaTN supports NVIDIA GPUs only and has had no commits since 2023.
- All GPU work goes through TAL-SH, the library inside ExaTN. Its settings
  are those of the table above.
