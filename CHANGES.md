# Changes

This file lists the changes that users of the quantum module need to know
about, most recent first.

## qns-3-dev: port to ns-3.49 and cleanup

The module now builds against ns-3.49 (C++23) and follows the ns-3 coding
style and object model. User scripts written for the previous version need to
be updated; this section says how.

### Changed behavior

These changes alter simulation results.

- **Memory noise before controlled corrections.** `ApplyControlledOperation`
  now applies memory noise to its qubits before the gate, like `ApplyGate`
  does. Before, the time a target qubit had waited for the classical message
  was dropped, so `TelepLinAdaptApp` and `TelepAdaptApp` reported a higher
  fidelity than the sampled `TelepSrcApp`/`TelepDstApp` under memory noise.
  The exact and the sampled protocols now agree; a test checks this.
- **Exact distillation no longer samples.** `DistillNestedAdaptApp` used to
  measure its flag qubit with a random outcome and report the fidelity of
  whichever branch came up. The script now conditions on success with
  `QuantumPhyEntity::PostSelect`, which returns the exact success probability
  and leaves the state given success.
- **Density matrices are returned row by row.** `PeekDensityMatrix` (formerly
  `PeekDM`) returned the transpose of the density matrix, which differs from
  the matrix itself for states with complex amplitudes. It now uses the same
  layout as the inputs of `GenerateQubitsMixed` and `ApplyGate`.
- **Random numbers come from ns-3.** Measurement outcomes used the C function
  `rand()` without a seed. They now use an ns-3 random variable stream, so
  `RngSeedManager` and `--RngRun` apply; see `QuantumPhyEntity::AssignStreams`.
- **Exact distillation leaves more time for the Bell pairs.** The time that
  `DistillNestedAdaptApp` allots to the distribution of one pair is now the
  attribute `EprInterval`, default 10 ms. It was fixed at 5 ms, which is less
  than the random delay of up to 10 ms that neighbor discovery (IPv6) or ARP
  (IPv4) adds to the first message. Whether the example ran or aborted
  therefore depended on the random stream. The application now also checks
  that the pairs have arrived and says what to change if they have not.
- **Gate noise of controlled corrections uses the right gate.** The adapted
  teleportation applications looked up the noise of gates named `QNS_GATE_X`
  and `QNS_GATE_Z`, which do not exist, and therefore ignored a
  `SetDephaseModel` on `QNS_GATE_PX` or `QNS_GATE_PZ`.

### Bugs fixed

- Calls to ExaTN were wrapped in `assert()`, so an optimized or release build
  silently skipped them. Errors are now reported with `NS_ABORT_MSG`.
- `examples/CMakeLists.txt` listed seven examples whose sources do not exist,
  so configuring ns-3 with examples enabled failed.
- The ExaTN path could only be changed by editing `CMakeLists.txt`, and a
  missing ExaTN stopped the configuration of all of ns-3. See "Build".
- `QuantumNetworkSimulator` finalized ExaTN in its destructor and reused the
  tensor names `QNS_EXATN0`, `QNS_EXATN1`, ... in every instance, so a process
  could only ever run one simulation. ExaTN is now started once per process,
  and every engine uses its own names and frees its tensors.
- The copy constructor and the default constructor of
  `QuantumNetworkSimulator` left counters uninitialized.
- Results scheduled with `Simulator::Schedule(..., &QuantumPhyEntity::PeekDM,
  ..., m_output)` were written to a copy of the argument, so `GetOutput()`
  always returned an empty vector. The same held for `CalculateFidelity`.
- A measurement could divide by zero when `rand()` returned `RAND_MAX`.
- Applying the same custom gate name with a different matrix silently reused
  the first matrix. This now aborts.
- `QuantumOperation` on more than two qubits built a tensor of the wrong
  shape, and multi-qubit operations used an inconsistent leg order.
- The contraction orders "ascend" and "distill" kept their state in global
  variables, as did the counter of ancilla qubit names.
- `QuantumPhyEntity` and `QuantumNode` referred to each other and were never
  released.
- The owner `"God"` crashed unless a node with that name was created. It is
  now always available, as `QNS_GOD_OWNER`, and needs no node.
- Applications read received packets into variable-length arrays and printed
  buffers that were not null-terminated.
- A TAL-SH macro broke ns-3 headers included after the ExaTN headers.
- `CLASSICAL_DELAY` was used both as 0.1 s and, in the examples, as
  `MilliSeconds(0.1)`. The latter converts 0.1 to an integer, so the examples
  ran with a CSMA delay of 0 ms instead of 0.1 ms.
- The log printer for complex numbers called the integer `abs`, so it printed
  every real or imaginary part smaller than 1 as 0. Logged density matrices
  lost their imaginary off-diagonal entries.

### Changes to the API

Setting up a network:

| Before | Now |
| --- | --- |
| `CreateObject<QuantumPhyEntity>(owners)` creates the nodes | Create nodes as usual, then `QuantumNetStackHelper::Install(nodes, owners)` returns the `QuantumPhyEntity` |
| `QuantumNode` is a `Node` | `QuantumNode` is aggregated to a `Node`: `node->GetObject<QuantumNode>()`, `quantumNode->GetNode()` |
| `qphyent->GetNode(owner)` | `qphyent->GetQuantumNode(owner)` |
| `qphyent->SetOwnerAddress(owner, address)` | Not needed: the address of the node is used. `QuantumNode::SetAddress` overrides it |
| `qphyent->SetOwnerRank(owner, rank)` | Removed; it had no effect |
| `QuantumNetStackHelper::Install(nodes)` installs a pair of EPR applications for every ordered pair of nodes | `Install(nodes, owners)` gives every node one `DistributeEprProtocol`; any node can then send a pair to any other |
| `QuantumNode::AllocPort()`, `GetNextPort()` | Removed. `QuantumNode::GetServicePort(name)` gives a service one fixed port |
| The helpers guessed the port of the peer (`GetNextPort()`) | The helper sets `Remote` and `Local`, socket addresses as in `SourceApplication` and `SinkApplication` of ns-3, when it installs the application. The nodes need their IP addresses before `Install()` |
| Bell pair applications on ports from the counter | `DistributeEprProtocol` listens on the fixed port `DistributeEprProtocol::PORT`, as ns-3 routing protocols do |

Noise:

| Before | Now |
| --- | --- |
| `node->SetTimeModel(double)`, `SetDephaseModel(gate, double)` | Take a `Time`, the coherence time: `SetTimeModel(Seconds(20))` |
| `channel->SetDepolarModel(fidelity, qphyent)` | `qphyent->SetDepolarModel(channel, fidelity)` |
| `TimeModel(rate)`, `DephaseModel(rate)`, `DepolarModel(fidelity)` | Attributes `CoherenceTime`, `GateDuration`, `Fidelity` |
| `QuantumErrorModel::ApplyErrorModel(qphyent, qubits, moment)` | `QuantumErrorModel::GetOperation(elapsed)` returns the noise; `QuantumPhyEntity` applies it |
| Three overloads of `QuantumPhyEntity::ApplyErrorModel` | `ApplyChannelNoise(srcOwner, dstOwner, qubit)`; memory and gate noise are applied automatically |
| `default_time_model`, `default_dephase_model`, `default_depolar_model` | Attributes `DefaultTimeModel`, `DefaultDephaseModel`, `DefaultDepolarModel` of `QuantumPhyEntity` |
| Macro `GATE_DURATION` | Attribute `GateDuration` of `DephaseModel` |

Operations on qubits:

| Before | Now |
| --- | --- |
| `PeekDM(owner, qubits, dm)` | `PeekDensityMatrix(owner, qubits)` returns the matrix |
| `CalculateFidelity(epr, fidel)` | `CalculateFidelity(epr)` returns the fidelity |
| `Measure(owner, {qubit})` returns a `std::pair` | `Measure(owner, qubit)` returns a `QuantumMeasurement` |
| `GenerateEPR(channel, epr)` | Removed; use `DistributeEprProtocol::DistributeEpr`, or `GenerateQubitsPure(owner, QNS_BELL, qubits)` for a local pair |
| Functions return `bool`, always `true` | Functions return nothing and abort with a message on misuse |
| (none) | `PostSelect(owner, qubit, outcome)` |
| Free function `AllocAncillaQubit()` | `QuantumPhyEntity::AllocAncillaQubit()` |
| `QuantumOperation::getOprs()` and other `get...` | `GetKrausOperators()`, `GetName()`, `GetProbability()`, `GetSize()` |
| `QuantumMemory::ContainQubit`, `QuantumNode::OwnQubit` | `HasQubit` |

Constants and helper functions (`quantum-basis.h`):

| Before | Now |
| --- | --- |
| `q_ket_0`, `q_ket_1`, `q_bell` | `QNS_KET_0`, `QNS_KET_1`, `QNS_BELL` |
| `pauli_I`, `pauli_X`, `pauli_Y`, `pauli_Z`, `hadamard`, `cnot`, `cz`, `swap`, `toffoli`, `qor` | `QNS_PAULI_I`, ..., `QNS_HADAMARD`, `QNS_CNOT`, `QNS_CZ`, `QNS_SWAP`, `QNS_TOFFOLI`, `QNS_QOR` |
| `QNS_GATE_PREFIX + "CNOT"` | Still works; `QNS_GATE_CNOT` and the like are preferred |
| `"God"` | `QNS_GOD_OWNER` |
| `APP_DIST_EPR`, `APP_TELEP`, ... | `QNS_APP_DIST_EPR`, `QNS_APP_TELEP`, ... |
| `GetEPRwithFidelity(f)` | `MakeWernerState(f)` |
| `GetPreHalf`, `GetSufHalf`, `Log2` | `GetFirstHalf`, `GetSecondHalf`, `CountQubits` |
| `std::vector<std::complex<double>>` | Alias `QuantumTensorData` |
| Macros `EPS`, `ETERNITY`, `DELIM`, `CLASSICAL_DELAY`, `TELEP_DELAY`, `DIST_EPR_DELAY`, `SETUP_DELAY`, colour codes | Removed. `QNS_EPSILON`; the delays are attributes of the applications |
| `operator<<`, `operator*`, `operator+`, `operator==` on standard types | Removed |
| `P0I`, `P1X`, `P1Z`, `pauli_XZ`, `depol` | Removed; they were unused |

Applications and helpers:

| Before | Now |
| --- | --- |
| Every application derives from `Application` | Every application derives from `QuantumApplication`, which provides `SendMessage` and `HandleMessage` |
| Attributes `PeerAddress`, `PeerPort`, `Port`, `DataSize`, `Pnode` | Attributes `Remote`, `Local`, `PacketSize`; the helpers set them |
| `DistributeEPRSrcProtocol` and `DistributeEPRDstProtocol`, two applications per ordered pair of nodes, and their helpers | `DistributeEprProtocol`, one object aggregated to each node; no helper is needed |
| `qphyent->GetConn2Apps(conn, APP_DIST_EPR).first->GetObject<DistributeEPRSrcProtocol>()->GenerateAndDistributeEPR(epr)` | `node->GetObject<DistributeEprProtocol>()->DistributeEpr(dstOwner, epr)`; inside an application, `GetEprProtocol()` |
| `QuantumPhyEntity::AddConn2Apps`, `GetConn2Apps` | Removed; protocols are found on the node with `GetObject` |
| The arrival of a qubit could not be observed | Trace sources `EprSent` and `EprReceived` of `DistributeEprProtocol` |
| `DistillApp` with attribute `Checker` | `DistillSrcApp` (Alice) and `DistillDstApp` (Bob), `DistillSrcHelper`, `DistillDstHelper` |
| `DistillNestedApp` with attribute `Checker` | `DistillNestedSrcApp`, `DistillNestedDstApp`, `DistillNestedSrcHelper`, `DistillNestedDstHelper` |
| `DistillNestedAdaptApp` with attribute `Checker`; `GetWin()` measures | One role, on Alice's node. When it has finished, `PostSelect(owner, flag, 1)` returns the success probability |
| Fixed delays of 0.1 s, 5 ms and 2.1 s in the applications | Attributes `LocalOperationDelay`, `StepInterval`, `EprInterval` |
| Fidelity 0.95 fixed in `EntSwapAdaptLocalApp` | Attribute `EprFidelity` |
| Results appear in the log only | Every application fires the trace source `Finished`; the script connects to it and reads what it wants from `QuantumPhyEntity`. Trace source `Measure` of `QuantumPhyEntity` |
| `GetOutput()` of the applications, always empty | Removed; see the line above |
| Helpers implement `Install` themselves | Helpers derive from `ApplicationHelper`; `Install()` without argument installs on the right end of the channel |
| `helper.Install(qphyent->GetNode(owner))` | `helper.Install()` or `helper.Install(qphyent->GetQuantumNode(owner))` |
| `quantum-basis-helper.h` | Removed; it was empty |

Other:

- `QuantumNetworkSimulator` is no longer an `ns3::Object` and no longer part
  of the user API. Use `QuantumPhyEntity`.
- `QuantumChannel` keeps the constructors from two owner names and from two
  quantum nodes; the two constructors from a `std::pair` are gone.
- Public headers no longer include ExaTN headers.
- `Contract()` takes the new contraction order "auto" by default; see
  Performance below. The order "distill" and `Checkpoint()` are removed:
  "auto" finds a cheaper order by itself, so an application no longer has to
  tell the engine where its subcircuits end.

### Performance

- **The module now chooses the contraction order itself.** ExaTN's own search
  for an order ("greed") took far longer than the contraction: 242 s of a
  242 s run for the exact swapping chain of 64 nodes. The new order "auto"
  builds three orders (the order of creation and two greedy orders), counts
  the multiplications of each, and hands the cheapest to ExaTN. It is the
  default of `Contract()` and of every query (`Measure`, `PostSelect`,
  `PeekDensityMatrix`, `CalculateFidelity`), and all applications use it.
  Results are unchanged. Times on one core:

  | Run | Before | Now |
  | --- | --- | --- |
  | `ent-swap-adapt-example --nNodes=16` | 3.7 s | 0.09 s |
  | `ent-swap-adapt-example --nNodes=64` | 242 s | 0.31 s |
  | `ent-swap-example --nNodes=16` (sampled) | more than 580 s | 0.73 s |
  | `telep-lin-example --nNodes=16` (sampled) | more than 60 s | 1.4 s |
  | `distill-nested-example --nPairs=16` (sampled) | more than 60 s | 1.7 s |
  | `distill-nested-adapt-example --nPairs=512` | 2.4 s | 2.3 s |
  | `ent-swap-adapt-local-example --nNodes=1024` | 2.2 s | 2.4 s |

- ExaTN runs every contraction on all cores unless `OMP_NUM_THREADS` is set.
  For the tiny tensors of a network simulation this is far slower than one
  thread: on a 128-core machine the test suite takes 65 s by default and 0.5 s
  with `OMP_NUM_THREADS=1`. The thread count can only be set from the
  environment, so the module prints a one-line hint when the variable is not
  set on a machine with more than four cores.

### ExaTN and GPU

- **Two patches for ExaTN**, in `patches/`. One fixes a crash when a program
  that uses a GPU build of ExaTN exits. The other adds the environment
  variables `TALSH_GFLOP_THRESH_GPU` and `TALSH_GPU_BUFFER_MB`; without them
  ExaTN sends only contractions above 4 billion multiplications to the GPU,
  which no network simulation reaches, and reserves 90 % of the memory of
  every GPU.
- **The module runs on a GPU**, unchanged, with ExaTN built for CUDA; see
  `doc/gpu.md`. On an RTX 4090 the test suite and the examples give the same
  results as on the CPU. They are slower, by a factor of 2 to 6 for the large
  examples, because the tensors are small.
- The module prints a reminder when it runs on a GPU build of ExaTN and
  `CUDA_VISIBLE_DEVICES` is not set, because ExaTN then reserves memory on
  every GPU of the machine.

### Build

- The compiler flags that the ExaTN package file adds are applied to
  `model/quantum-network-simulator.cc` only. Applied to the whole module, they
  disabled the precompiled headers of ns-3.
- ExaTN is found through the CMake variable or environment variable
  `ExaTN_DIR`, default `$HOME/.exatn`. If ExaTN is not found, the module is
  skipped with a message and the rest of ns-3 builds.
- The module depends on the `internet` module only; the examples also use
  `csma`.

### Tests and examples

- The test suite `quantum` replaces the empty template: `./test.py -s quantum`.
- The examples take command-line arguments instead of `#define N`, print their
  results, and are listed in `test/examples-to-run.py`.
- A GitHub Actions workflow builds ExaTN and ns-3 with the module and runs the
  test suite, the examples and the ns-3 style check on every push.
