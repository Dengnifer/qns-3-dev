# Design decisions

This file records why the module is built the way it is. Each entry says what
was decided, why, and what was rejected. Add an entry when you make a decision
that a later reader could not guess from the code.

## Structure

### 1. A quantum node is an object attached to a normal node

`QuantumNode` derives from `Object` and is aggregated to an `ns3::Node`:
`node->GetObject<QuantumNode>()`, `quantumNode->GetNode()`.

- Why: this is how ns-3 extends a node (`Ipv4`, `MobilityModel` and so on are
  aggregated). Any topology helper can create the nodes, and a node of another
  module can become a quantum node.
- Rejected: `class QuantumNode : public Node`, created by `QuantumPhyEntity`
  (the original design). Scripts then had to take their nodes from the quantum
  module.

### 2. One global quantum state, local interfaces

`QuantumPhyEntity` holds the density matrix of all qubits of the network. A
node acts on it through its owner name, and every operation checks that the
owner holds the qubits.

- Why: entangled qubits on different nodes have no separate states, and an
  exact simulation needs the joint state.
- Rejected: one state per node.

### 3. The tensor engine is hidden

Only `model/quantum-network-simulator.cc` includes ExaTN. The rest of the
module, and every user, talks to `QuantumPhyEntity`.

- Why: the backend can be replaced by changing one file. ExaTN headers also
  define macros and compiler flags that break ns-3 headers.

### 4. Names, not handles

Qubits, owners, gates and services are named by strings. Owners are looked up
in the `QuantumPhyEntity`.

- Why: names appear in logs and in classical messages, and the protocols of
  the paper are written with them.
- Rejected: the ns-3 `Names` service for owners. It is global to the process,
  so two networks in one program (as in the tests) could not both have an
  "Alice".

## Configuration and results

### 5. Settings are attributes with units

Delays, coherence times and fidelities are attributes of type `Time` or
`double`, with defaults. They replace the `#define` constants.

- Why: a script or the command line can change them without editing the
  library, and a `Time` cannot be read in the wrong unit.

### 6. Applications do not compute results

Every application fires the trace source `Finished`. The script connects to it
and reads what it wants from `QuantumPhyEntity` (a fidelity, a density matrix,
a success probability).

- Why: what counts as the output of a protocol depends on the experiment.
- Rejected: `GetOutput()` on each application, and results printed to the log.

### 7. Examples run until the protocol reports

The report function calls `Simulator::Stop()`. The fixed stop time is only a
deadline.

- Why: a fixed stop time must be recomputed whenever a delay changes.

## Protocols

### 8. Applications hold socket addresses; helpers resolve names

An application has the attributes `Remote` (address and port of its peer) and
`Local` (address and port it receives on), like `SourceApplication` and
`SinkApplication` of ns-3. A script may set them directly. The helpers are
written in terms of owner names and service names, and turn them into
addresses when they install the application: both ends ask
`QuantumNode::GetServicePort(name)` and get the same port.

- Why: this is the ns-3 convention. The names stay in the helpers, where ns-3
  puts convenience, and no application looks into another node while the
  simulation runs. The result does not depend on the order of the `Install`
  calls.
- Consequence: the nodes need their IP addresses before a quantum application
  is installed, as for `UdpEchoClientHelper`.
- Rejected: a port counter (`AllocPort`, `GetNextPort() - 1`), which made a
  helper guess the port its peer would get. Also rejected, after a first
  attempt: string attributes `PeerOwner` and `PeerService` resolved at the
  moment a message is sent. That worked, but it is not how ns-3 applications
  name a peer.
- Not from ns-3: the port registry itself. In ns-3 the script chooses port
  numbers; here `GetServicePort` chooses them, because one node can run
  several instances of a protocol and each needs its own port.

### 9. Bell pair distribution is one protocol object per node

`DistributeEprProtocol` is aggregated to every node. An application calls
`GetEprProtocol()->DistributeEpr(dstOwner, epr)`.

- Why: this is how ns-3 protocols are reached (`node->GetObject<Ipv4>()`).
- It listens on the fixed UDP port `DistributeEprProtocol::PORT`, as ns-3
  routing protocols do (`AODV_PORT`). It finds the IP address of an owner
  through `QuantumPhyEntity`, which acts as a directory. ns-3 has no name
  service, so this lookup is ours.
- Rejected: one pair of applications for every ordered pair of nodes, found
  through a map in `QuantumPhyEntity`. That installed n² applications.

### 10. One class sends and receives classical messages

`QuantumMessenger` wraps the UDP socket. Applications and
`DistributeEprProtocol` share it.

### 11. Helpers derive from `ApplicationHelper`

Each protocol has a helper whose constructor sets the attributes that the
roles of the protocol share. Two helpers (`EntSwapAdaptHelper`,
`EntSwapAdaptLocalHelper`) set nothing beyond the type; they are kept so that
every protocol is installed the same way.

## Physics

### 12. A noise model returns an operation for an elapsed time

`QuantumErrorModel::GetOperation(elapsed)` returns a `QuantumOperation`, a
mixture of gates. `QuantumPhyEntity` applies memory noise to a qubit before
every gate, controlled gate and measurement on it.

- Why: the exact protocols and the sampled protocols then see the same noise.
  Before, the controlled corrections skipped it.

### 13. Random numbers come from ns-3 streams

Measurement outcomes use an ns-3 random variable stream, so `--RngRun` and
`AssignStreams` apply.

## Performance

### 14. The module chooses the contraction order

`Contract()` and every query use the optimizer "auto". It builds three
orders, the order of creation and two greedy orders, counts the
multiplications of each, and gives the cheapest to ExaTN.

- Why: ExaTN's own search took far longer than the contraction itself: 242 s
  for the exact swapping chain of 64 nodes, which now takes 0.3 s. A sampled
  chain of 16 nodes went from more than 580 s to 0.7 s.
- Why three orders: none is best everywhere. The order of creation wins on the
  chain with local flags, one greedy order on distillation, the other on
  swapping and teleportation.
- Rejected: an order chosen by each application ("ascend", or "distill" with
  `Checkpoint()` calls). The application had to know how the engine works, and
  "auto" finds an order at least as cheap in every case measured. "distill"
  and `Checkpoint()` were removed; "ascend" remains as a name because it is
  one of the three candidates.

### 15. The thread count is a hint, not a setting

When `OMP_NUM_THREADS` is not set on a machine with more than four cores, the
module prints one line that recommends `OMP_NUM_THREADS=1`.

- Why: ExaTN reads the variable when the process starts, so the module cannot
  set it. One thread was more than 100 times faster in our runs, because the
  tensors of a network simulation are small.

### 16. ExaTN stays for now

ExaTN has had no commits since 2023, needs Fortran and an old Boost, and
supports NVIDIA GPUs only. It stays because it works: the module passes on
CPU and, with the two patches in `patches/`, on a GPU (`doc/gpu.md`). The GPU
is not faster, because the tensors are far too small.

- Why patches and not a fork of ExaTN: the changes are five lines, and a fork
  would be one more repository to keep.
- Planned for a later change: cuTensorNet (NVIDIA cuQuantum) as the backend,
  behind the interface of decision 3. It runs on NVIDIA GPUs only, so a CPU
  engine is needed next to it.

## Tests

### 17. Tests compare with formulas, and exact with sampled

Each protocol is checked against a closed formula (Werner-state swapping, the
Bell-diagonal distillation recurrence), and its exact variant against its
sampled variant.

## Planned: the quantum physical layer

Not implemented yet. Today a Bell pair "arrives" when a classical message says
so, and `DistillNestedAdaptApp` waits a fixed `EprInterval` for it.

The plan follows the physical layer of ns-3, where a channel owns the delay
and the loss, and the receiver decides what was received:

- `QuantumChannel` becomes an `ns3::Channel` between two quantum devices, with
  a delay (length divided by speed), a loss and a `QuantumErrorModel`.
- A node sends a qubit into the channel. The channel schedules the arrival
  after the delay, applies the noise of the flight, gives the qubit to the
  receiver and fires a trace source.
- Applications act when the qubit arrives, so `EprInterval` disappears.

The classical case is the special case of qubits prepared and measured in the
computational basis: a bit-flip channel is a binary symmetric channel (the
`RateErrorModel` of ns-3), loss is a dropped packet, and dephasing has no
effect. A test should check this limit. Two things do not carry over: a qubit
cannot be copied, so a quantum channel never broadcasts, and the state stays
in `QuantumPhyEntity` rather than in the receiving device.

## Open questions

- `examples/netsquid-examples/` holds modified NetSquid files under the
  NetSquid licence, in a GPL repository.
- The nested distillation does not twirl, so its second level lowers the
  fidelity (0.9, then 0.926, then 0.872).
- Whether to keep the two helpers of decision 11.
