# qns-3: a quantum network simulator on ns-3

qns-3 is a module for the [ns-3](https://www.nsnam.org) network simulator
that simulates quantum network protocols together with the classical network
they run on. Qubits live in the memories of ordinary ns-3 nodes, classical
messages travel through the ns-3 protocol stack, and noise follows the
simulated time.

Its distinguishing feature is *control flow adaptation* (CFA). A quantum
network protocol measures qubits, sends the outcomes, and acts on them. A
conventional simulator samples one outcome per run and needs thousands of runs
to estimate a success probability or a fidelity. qns-3 keeps the state of all
qubits as a tensor network and lets a protocol replace "measure, send, act" by
a controlled gate. One run then covers all outcomes and the result is exact.
The method is described in

> Huiping Lin, Ruixuan Deng, Chris Z. Yao, Zhengfeng Ji and Mingsheng Ying,
> "Control Flow Adaptation: An Efficient Simulation Method For Noisy Quantum
> Networks", [arXiv:2412.08956](https://arxiv.org/abs/2412.08956).

qns-3 is developed at the Research Center for Quantum Software, Department of
Computer Science and Technology, Tsinghua University, by Huiping Lin, Ruixuan
Deng, Chris Z. Yao, Zhengfeng Ji and Mingsheng Ying.

## Requirements

- ns-3.49, which needs a C++23 compiler (g++ 11.1 or later, or clang++ 17 or
  later), CMake 3.25 or later and Python 3.10 or later.
- [ExaTN](https://github.com/ORNL-QCI/exatn), the tensor-network library that
  contracts the tensor networks. ExaTN needs a Fortran compiler and a BLAS and
  LAPACK library. MPI is optional and not used by qns-3.

If ExaTN is not found, ns-3 still configures and builds, without this module.

## Installation

The steps below were tested on Ubuntu 22.04 with gcc, g++ and gfortran 11,
the reference BLAS and LAPACK, CMake 3.31 and ExaTN revision `c528f7e`.

### 1. Build ExaTN

```bash
git clone --recursive https://github.com/ORNL-QCI/exatn.git
cmake -S exatn -B exatn/build -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBLAS_LIB=ATLAS -DBLAS_PATH=/usr/lib/x86_64-linux-gnu -DWITH_LAPACK=ON \
      -DEXATN_BUILD_TESTS=OFF \
      -DCMAKE_DISABLE_FIND_PACKAGE_MPI=ON \
      -DCMAKE_DISABLE_FIND_PACKAGE_Python=ON
cmake --build exatn/build -j 8
cmake --install exatn/build
```

ExaTN installs to `~/.exatn` by default. Notes:

- `-DBLAS_LIB=ATLAS` means "the system BLAS": ExaTN links `libblas.so` and
  `liblapack.so` from `BLAS_PATH`. For OpenBLAS use `-DBLAS_LIB=OPENBLAS` and
  the directory that holds `libopenblas.so`.
- Use the Makefile generator. With Ninja the build of ExaTN fails.
- ExaTN bundles Boost through a script that downloads a repackaged Boost
  1.64.0. To use an official Boost 1.64.0 archive instead, unpack it and add
  `-DBOOST_SOURCE=/path/to/boost_1_64_0`.
- The last option skips the Python bindings of ExaTN, which qns-3 does not
  use and which do not build with recent Python versions.

### 2. Add qns-3 to ns-3

```bash
cd ns-3.49/contrib
git clone https://github.com/Dengnifer/qns-3-dev.git quantum
cd ..
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

The configuration prints `Processing contrib/quantum` when ExaTN is found and
`Skipping contrib/quantum` otherwise. If ExaTN is not in `~/.exatn`, pass its
location:

```bash
./ns3 configure --enable-examples --enable-tests -- -DExaTN_DIR=/path/to/exatn
```

### 3. Check the installation

```bash
export OMP_NUM_THREADS=1
./test.py -s quantum
./ns3 run "distill-nested-adapt-example --nPairs=8"
```

The example prints the exact success probability and fidelity of a nested
distillation of eight Bell pairs:

```text
Probability that all rounds succeed: 0.762455
Fidelity of the distilled pair: 0.86937
```

## Performance: set OMP_NUM_THREADS

ExaTN runs every tensor contraction on all cores unless the environment
variable `OMP_NUM_THREADS` says otherwise. The tensors of a network simulation
are tiny, so starting the threads costs far more than the contraction. On a
128-core server the test suite takes 65 s by default and 0.5 s with
`OMP_NUM_THREADS=1`. Set the variable before you run anything.

## Examples

Every example takes command-line arguments; run it with `--help` to see them.

| Example | Protocol | Result |
| --- | --- | --- |
| `telep-lin-example` | Teleportation along a chain of nodes | Sampled |
| `telep-lin-adapt-example` | Teleportation along a chain of nodes | Exact |
| `ent-swap-example` | Entanglement swapping along a chain | Sampled |
| `ent-swap-adapt-example` | Entanglement swapping along a chain | Exact |
| `ent-swap-adapt-local-example` | Entanglement swapping along a long chain | Exact |
| `distill-nested-example` | Nested entanglement distillation | Sampled |
| `distill-nested-adapt-example` | Nested entanglement distillation | Exact |

`examples/netsquid-examples` holds the [NetSquid](https://netsquid.org)
scripts that the paper compares against. They are not part of the build.

## Writing a simulation

A simulation script builds a classical ns-3 network, turns its nodes into
quantum nodes, and installs quantum applications:

```cpp
// What to report when the protocol has finished.
static void
Report(Ptr<QuantumPhyEntity> qphyent, Ptr<const QuantumApplication> app)
{
    const double probability = qphyent->PostSelect("Alice", "Flag", 1); // condition on success
    std::cout << probability << " " << qphyent->CalculateFidelity({"A0", "B0"}) << std::endl;
}

// In main():
NodeContainer nodes;
nodes.Create(2);
// ... install devices, the Internet stack and addresses as in any ns-3 script ...

QuantumNetStackHelper stack;
Ptr<QuantumPhyEntity> qphyent = stack.Install(nodes, {"Alice", "Bob"});

Ptr<QuantumChannel> channel = CreateObject<QuantumChannel>("Alice", "Bob");
qphyent->SetDepolarModel(channel, 0.95);                        // channel noise
qphyent->GetQuantumNode("Alice")->SetTimeModel(Seconds(20));    // memory noise

DistillNestedAdaptHelper helper(qphyent, channel);
helper.SetAttribute("SrcQubits", PointerValue(aliceQubits));    // A0, A1, ...
helper.SetAttribute("DstQubits", PointerValue(bobQubits));      // B0, B1, ...
helper.SetAttribute("FlagQubit", StringValue("Flag"));
ApplicationContainer apps = helper.Install();
apps.Get(0)->TraceConnectWithoutContext("Finished", MakeBoundCallback(&Report, qphyent));

Simulator::Run();
Simulator::Destroy();
```

The applications run protocols; they do not decide what to measure. Every
application fires the trace source `Finished` when its protocol ends, and the
script reads what it wants from `QuantumPhyEntity` at that moment:
`PeekDensityMatrix`, `CalculateFidelity`, `PostSelect`, `Measure`.

To write a new protocol, derive an application from `QuantumApplication`,
operate on qubits through `QuantumPhyEntity`, and exchange classical messages
with `SendMessage` and `HandleMessage`. `model/telep-app.cc` is a short
example of a sampled protocol and `model/telep-adapt-app.cc` of its exact
counterpart.

To follow what a simulation does, turn on the logs of the state operations:

```bash
NS_LOG="QuantumNetworkSimulator=info:QuantumPhyEntity=info|logic" ./ns3 run telep-lin-adapt-example
```

## Documentation

- `doc/quantum.rst` describes the model, its scope and its limitations, in the
  format of the ns-3 model library.
- `CHANGES.md` lists the changes between versions, including how to update
  scripts written for the version that accompanied the paper.
- The headers carry Doxygen comments for every class and method.

## Development

```bash
./test.py -s quantum                                    # tests
./test.py -e telep-lin-adapt-example                    # one example as a test
python3 utils/check-style-clang-format.py contrib/quantum   # ns-3 coding style
```

## License

qns-3 is free software under the GNU General Public License version 2; see
`LICENSE`.
