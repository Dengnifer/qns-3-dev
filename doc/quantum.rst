.. include:: replace.txt
.. highlight:: cpp

.. heading hierarchy:
   ------------- Chapter
   ************* Section (#.#)
   ============= Subsection (#.#.#)
   ############# Paragraph (no number)

Quantum
-------

The quantum module simulates quantum network protocols on top of the classical
networks of |ns3|. Qubits live in the quantum memories of ordinary nodes, the
classical messages of a protocol travel through the |ns3| protocol stack with
its delays and losses, and the noise that qubits suffer follows the simulated
time.

The joint state of all qubits is kept as a tensor network and is only
contracted when a number is asked for. Combined with *control flow adaptation*
(CFA) [Lin2025]_, this yields exact results: an adapted protocol replaces "measure,
send the outcome, act on it" by a controlled gate, so that one run covers all
measurement branches. Where a sampling simulator needs thousands of runs to
estimate a success probability or a fidelity, the module computes it once.

The source code lives in ``contrib/quantum``.

Scope and Limitations
*********************

* The module depends on the ExaTN tensor-network library, which is not part of
  |ns3|. Without ExaTN the module is skipped at configuration time.
* All noise is Pauli noise on single qubits: dephasing in memories and gates,
  depolarization on quantum channels. There is no photon loss model.
* A quantum channel has no ``NetDevice`` and moves no packets. The arrival of a
  qubit is announced by a message on the classical network, and the qubit
  changes owner when the message arrives.
* The module provides entanglement distribution, teleportation, entanglement
  swapping and entanglement distillation. It has no routing or transport
  protocols.
* An adapted protocol must have control flow whose timing does not depend on
  measurement outcomes. Protocols that repeat until success have to be sampled.
* The adapted applications act at fixed times. Their attributes
  ``LocalOperationDelay`` and ``EprInterval`` must be long enough for the
  classical messages they stand in for, including the address resolution
  before the first message. ``DistillNestedAdaptApp`` aborts if they are not.
* The distillation rounds neither twirl nor alternate bases. A round detects
  bit flips and doubles phase flips, so a second level of nested distillation
  can lower the fidelity; the tests check the simulation against this
  recurrence.
* The sampled nested distillation does not stop after a failed round.
* The cost of a simulation is the cost of contracting its tensor network. It
  grows quickly when many qubits are entangled and none are traced out.
* ExaTN starts one thread per core for every contraction, which is slow for
  small tensor networks. Set the environment variable ``OMP_NUM_THREADS`` to 1.
* There are no Python bindings.

Model Description
*****************

Nodes, owners and the physical entity
=====================================

Entangled qubits have no state of their own, so the state of all qubits is
held by one ``QuantumPhyEntity`` per network rather than by the nodes.
``QuantumNetStackHelper`` creates it and aggregates a ``QuantumNode`` to each
node. A ``QuantumNode`` has an owner name, such as "Alice", and a
``QuantumMemory`` listing the qubits the node owns. Qubits and gates are also
identified by name.

``QuantumPhyEntity`` offers the operations on qubits: ``GenerateQubitsPure``,
``GenerateQubitsMixed``, ``ApplyGate``, ``ApplyOperation``, ``Measure``,
``PostSelect``, ``PartialTrace``, ``PeekDensityMatrix`` and
``CalculateFidelity``. A node may only operate on qubits it owns. The owner
``QNS_GOD_OWNER`` is exempt from this rule and from gate noise; adapted
protocols use it for the operations that stand in for classical communication.

Noise
=====

``QuantumPhyEntity`` applies three kinds of noise, each described by a
``QuantumErrorModel``:

* memory noise, ``TimeModel``: before a qubit is touched, it suffers the
  dephasing accumulated since it was last touched;
* gate noise, ``DephaseModel``: a gate applied by a node is followed by
  dephasing of its qubits;
* channel noise, ``DepolarModel``: a qubit that arrives over a quantum channel
  is depolarized.

Each kind has a default model, and can be set per owner, per owner and gate,
and per channel respectively.

Tensor network engine
=====================

``QuantumNetworkSimulator`` stores the density matrix of all qubits as a tensor
network and appends tensors for state preparations, gates, noise and partial
traces. It hides ExaTN from the rest of the module. ``Contract`` collapses the
network into one tensor; the order of contraction is chosen by an ExaTN
optimizer or by one of two orders of the module, "ascend" for chains and
"distill" for trees.

Applications
============

The protocols are applications derived from ``QuantumApplication``, which
exchanges short messages over UDP, on IPv4 or IPv6. Applications address each
other by owner name and service name.

.. list-table::
   :header-rows: 1

   * - Protocol
     - Sampled
     - Exact (adapted)
   * - Bell pair distribution
     - ``DistributeEprSrcProtocol``, ``DistributeEprDstProtocol``
     -
   * - Teleportation
     - ``TelepSrcApp``, ``TelepDstApp``
     - ``TelepAdaptApp``, ``TelepLinAdaptApp`` (chain)
   * - Entanglement swapping
     - ``EntSwapSrcApp``, ``EntSwapDstApp``
     - ``EntSwapAdaptApp``, ``EntSwapAdaptLocalApp``
   * - Distillation
     - ``DistillSrcApp``, ``DistillDstApp``, ``DistillNestedSrcApp``,
       ``DistillNestedDstApp``
     - ``DistillNestedAdaptApp``

Usage
*****

Create the classical network first, then install the quantum stack and the
applications::

    NodeContainer nodes;
    nodes.Create(2);
    // ... devices, InternetStackHelper, addresses ...

    QuantumNetStackHelper stack;
    Ptr<QuantumPhyEntity> qphyent = stack.Install(nodes, {"Alice", "Bob"});

    Ptr<QuantumChannel> channel = CreateObject<QuantumChannel>("Alice", "Bob");
    qphyent->SetDepolarModel(channel, 0.95);

    DistillNestedAdaptHelper helper(qphyent, channel);
    helper.SetAttribute("SrcQubits", PointerValue(aliceQubits));
    helper.SetAttribute("DstQubits", PointerValue(bobQubits));
    ApplicationContainer apps = helper.Install();
    apps.Get(0)->TraceConnectWithoutContext("Finished", MakeBoundCallback(&Report, qphyent));

    Simulator::Run();

where ``Report`` states what the script wants to know::

    void
    Report(Ptr<QuantumPhyEntity> qphyent, Ptr<const QuantumApplication> app)
    {
        const double probability = qphyent->PostSelect("Alice", "Flag", 1);
        std::cout << probability << " " << qphyent->CalculateFidelity({"A0", "B0"}) << std::endl;
    }

Helpers
=======

``QuantumNetStackHelper`` installs the quantum stack, with Bell pair
distribution between every pair of nodes, along a chain, or only where
``Connect`` asks for it. Each application has a helper derived from
``QuantumApplicationHelper``; a helper constructed for a quantum channel
installs its application on the right end of the channel with ``Install()``.

Attributes
==========

* ``QuantumPhyEntity``: ``DefaultTimeModel``, ``DefaultDephaseModel``,
  ``DefaultDepolarModel``.
* ``TimeModel``: ``CoherenceTime``. ``DephaseModel``: ``CoherenceTime``,
  ``GateDuration``. ``DepolarModel``: ``Fidelity``.
* ``QuantumApplication``: ``QPhyEntity``, ``Service``, ``PeerOwner``,
  ``PeerService``, ``PacketSize``.
* The applications add the names of their qubits and their timing, for example
  ``LocalOperationDelay``, ``StepInterval`` and ``EprInterval``.

Output
======

The applications run protocols and do not decide what to measure. Every
application fires the trace source ``Finished`` when its protocol ends. A
script connects to it and reads what it wants from ``QuantumPhyEntity`` at that
moment: ``PeekDensityMatrix``, ``CalculateFidelity``, ``PostSelect`` or
``Measure``. The sampled distillation applications additionally report their
verdict with ``GetWin``.

``QuantumPhyEntity`` has the trace source ``Measure``, fired at every
measurement. The log components ``QuantumPhyEntity`` and
``QuantumNetworkSimulator`` report every operation at level ``info``.

Examples and Tests
==================

The directory ``examples`` holds one sampled and one exact example of chained
teleportation, chained entanglement swapping and nested distillation, and
``examples/netsquid-examples`` holds the NetSquid scripts used for comparison
in [Lin2025]_. All examples take command-line arguments; try ``--help``.

The test suite ``quantum`` checks the state operations, the noise models, and
every protocol: the exact variants against the sampled ones and against
closed formulas.

Validation
**********

The tests compare teleportation with adapted control flow against the sampled
protocol under all noise, chained entanglement swapping against the formula
for Werner states, and one round of distillation against the formulas of
[Bennett1996]_. The performance of the module is evaluated in [Lin2025]_.

References
**********

.. [Lin2025] Huiping Lin, Ruixuan Deng, Chris Z. Yao, Zhengfeng Ji and
   Mingsheng Ying, "Control Flow Adaptation: An Efficient Simulation Method
   For Noisy Quantum Networks", arXiv:2412.08956.

.. [Bennett1996] Charles H. Bennett, Gilles Brassard, Sandu Popescu, Benjamin
   Schumacher, John A. Smolin and William K. Wootters, "Purification of Noisy
   Entanglement and Faithful Teleportation via Noisy Channels", Physical
   Review Letters 76, 722 (1996).
