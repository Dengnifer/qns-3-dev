/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/address.h"
#include "ns3/application-container.h"
#include "ns3/distill-nested-adapt-app.h"
#include "ns3/distill-nested-adapt-helper.h"
#include "ns3/distill-nested-app.h"
#include "ns3/distill-nested-helper.h"
#include "ns3/distribute-epr-protocol.h"
#include "ns3/double.h"
#include "ns3/ent-swap-adapt-app.h"
#include "ns3/ent-swap-adapt-helper.h"
#include "ns3/ent-swap-adapt-local-app.h"
#include "ns3/ent-swap-adapt-local-helper.h"
#include "ns3/ent-swap-app.h"
#include "ns3/ent-swap-helper.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/node-container.h"
#include "ns3/pair.h"
#include "ns3/pointer.h"
#include "ns3/quantum-application.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-error-model.h"
#include "ns3/quantum-memory.h"
#include "ns3/quantum-net-stack-helper.h"
#include "ns3/quantum-node.h"
#include "ns3/quantum-operation.h"
#include "ns3/quantum-phy-entity.h"
#include "ns3/qubit.h"
#include "ns3/simple-net-device-helper.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/telep-adapt-app.h"
#include "ns3/telep-adapt-helper.h"
#include "ns3/telep-app.h"
#include "ns3/telep-helper.h"
#include "ns3/telep-lin-adapt-app.h"
#include "ns3/telep-lin-adapt-helper.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

#include <cmath>
#include <complex>
#include <string>
#include <vector>

/**
 * @file
 * @ingroup quantum-tests
 * Tests of the quantum module.
 */

/**
 * @defgroup quantum-tests Tests for the quantum module
 * @ingroup quantum
 * @ingroup tests
 */

namespace ns3
{

namespace tests
{

namespace
{

/// Tolerance for quantities that the simulation computes exactly.
constexpr double TOLERANCE = 1e-9;

/// The state to teleport in the tests; its amplitudes are not real on purpose.
const QuantumTensorData TEST_STATE{{std::sqrt(5.0 / 7.0), 0.0}, {0.0, std::sqrt(2.0 / 7.0)}};

/**
 * Build the density matrix of a pure state.
 *
 * @param state The state vector.
 * @return The projector onto the state.
 */
QuantumTensorData
DensityMatrix(const QuantumTensorData& state)
{
    QuantumTensorData rho;
    for (const auto& ket : state)
    {
        for (const auto& bra : state)
        {
            rho.push_back(ket * std::conj(bra));
        }
    }
    return rho;
}

/**
 * Measure how far two tensors are apart.
 *
 * @param a The first tensor.
 * @param b The second tensor.
 * @return The largest absolute difference of two entries; infinity if the
 *         sizes differ.
 */
double
Distance(const QuantumTensorData& a, const QuantumTensorData& b)
{
    if (a.size() != b.size())
    {
        return std::numeric_limits<double>::infinity();
    }
    double distance = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        distance = std::max(distance, std::abs(a[i] - b[i]));
    }
    return distance;
}

/**
 * A small quantum network for the tests: the nodes share one classical
 * channel and are named Owner0, Owner1, and so on.
 */
struct TestNetwork
{
    /// Which noise the network has.
    enum class Noise
    {
        DEFAULT,      //!< The default models
        CHANNEL_ONLY, //!< Only the noise of the quantum channels
        NONE          //!< No noise
    };

    /**
     * Constructor.
     *
     * @param nNodes Number of nodes.
     * @param noise Which noise the network has.
     */
    TestNetwork(uint32_t nNodes, Noise noise)
    {
        nodes.Create(nNodes);
        SimpleNetDeviceHelper deviceHelper;
        deviceHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
        NetDeviceContainer devices = deviceHelper.Install(nodes);
        InternetStackHelper internet;
        internet.Install(nodes);
        // ARP delays its requests by a random jitter. Fixing the streams gives
        // every network of the tests the same message timing.
        internet.AssignStreams(nodes, 100);
        Ipv4AddressHelper addresses;
        addresses.SetBase("10.1.1.0", "255.255.255.0");
        addresses.Assign(devices);

        QuantumNetStackHelper stack;
        if (noise != Noise::DEFAULT)
        {
            // The longest representable coherence time and a gate of zero
            // duration leave no memory noise and no gate noise.
            stack.SetPhyAttribute(
                "DefaultTimeModel",
                PointerValue(CreateObjectWithAttributes<TimeModel>("CoherenceTime",
                                                                   TimeValue(Time::Max()))));
            stack.SetPhyAttribute(
                "DefaultDephaseModel",
                PointerValue(
                    CreateObjectWithAttributes<DephaseModel>("GateDuration", TimeValue(Time(0)))));
        }
        if (noise == Noise::NONE)
        {
            stack.SetPhyAttribute(
                "DefaultDepolarModel",
                PointerValue(
                    CreateObjectWithAttributes<DepolarModel>("Fidelity", DoubleValue(1.0))));
        }
        for (uint32_t i = 0; i < nNodes; ++i)
        {
            owners.push_back("Owner" + std::to_string(i));
        }
        qphyent = stack.Install(nodes, owners);
        qphyent->AssignStreams(1);
    }

    /**
     * Create a quantum channel between two nodes.
     *
     * @param src Index of the source node.
     * @param dst Index of the destination node.
     * @return The channel.
     */
    Ptr<QuantumChannel> Channel(uint32_t src, uint32_t dst) const
    {
        return CreateObject<QuantumChannel>(owners[src], owners[dst]);
    }

    NodeContainer nodes;             //!< The nodes
    std::vector<std::string> owners; //!< Their owner names
    Ptr<QuantumPhyEntity> qphyent;   //!< The quantum physical entity
};

/**
 * Compute the fidelity of a Bell pair swapped along a chain of Werner pairs.
 *
 * @param fidelity Fidelity of each pair of the chain.
 * @param nPairs Number of pairs in the chain.
 * @return The fidelity of the final pair.
 */
double
SwappedFidelity(double fidelity, unsigned nPairs)
{
    return 0.25 + 0.75 * std::pow((4 * fidelity - 1) / 3, nPairs);
}

/**
 * A two-qubit state that is diagonal in the Bell basis.
 */
struct BellDiagonal
{
    double phiPlus{0};  //!< Weight of (|00> + |11>) / sqrt(2), the fidelity
    double phiMinus{0}; //!< Weight of (|00> - |11>) / sqrt(2)
    double psiPlus{0};  //!< Weight of (|01> + |10>) / sqrt(2)
    double psiMinus{0}; //!< Weight of (|01> - |10>) / sqrt(2)
};

/**
 * Apply one round of distillation to two copies of a Bell-diagonal state.
 *
 * The round detects bit flips but doubles phase flips, see [Bennett1996].
 *
 * @param in The state of each of the two pairs.
 * @param [out] probability The probability that the round succeeds.
 * @return The state of the kept pair after a successful round.
 */
BellDiagonal
DistillRound(const BellDiagonal& in, double& probability)
{
    const double phi = in.phiPlus + in.phiMinus;
    const double psi = in.psiPlus + in.psiMinus;
    probability = phi * phi + psi * psi;
    BellDiagonal out;
    out.phiPlus = (in.phiPlus * in.phiPlus + in.phiMinus * in.phiMinus) / probability;
    out.phiMinus = 2 * in.phiPlus * in.phiMinus / probability;
    out.psiPlus = (in.psiPlus * in.psiPlus + in.psiMinus * in.psiMinus) / probability;
    out.psiMinus = 2 * in.psiPlus * in.psiMinus / probability;
    return out;
}

} // namespace

/**
 * @ingroup quantum-tests
 * Test the functions of quantum-basis.h and the message encoding.
 */
class QuantumBasisTestCase : public TestCase
{
  public:
    QuantumBasisTestCase()
        : TestCase("Basic vocabulary")
    {
    }

  private:
    void DoRun() override
    {
        NS_TEST_EXPECT_MSG_EQ(CountQubits(1), 0, "a scalar holds no qubit");
        NS_TEST_EXPECT_MSG_EQ(CountQubits(8), 3, "eight amplitudes hold three qubits");
        NS_TEST_EXPECT_MSG_EQ((FindBuiltinGate(QNS_GATE_CNOT) == &QNS_CNOT),
                              true,
                              "CNOT is built in");
        NS_TEST_EXPECT_MSG_EQ((FindBuiltinGate("NoSuchGate") == nullptr), true, "unknown gate");

        const QuantumTensorData werner = MakeWernerState(0.8);
        NS_TEST_EXPECT_MSG_EQ_TOL((werner[0] + werner[5] + werner[10] + werner[15]).real(),
                                  1.0,
                                  TOLERANCE,
                                  "a density matrix has trace one");
        NS_TEST_EXPECT_MSG_EQ_TOL(((werner[0] + werner[3] + werner[12] + werner[15]) / 2.0).real(),
                                  0.8,
                                  TOLERANCE,
                                  "the Werner state has the requested fidelity");

        const std::vector<std::string> half{"a", "b"};
        const std::vector<std::string> all{"a", "b", "c", "d"};
        NS_TEST_EXPECT_MSG_EQ((GetFirstHalf(all) == half), true, "first half");
        NS_TEST_EXPECT_MSG_EQ(GetSecondHalf(all).front(), "c", "second half");

        const std::vector<std::string> fields{"1", "Owner0_Qubit0", ""};
        const std::string message = QuantumApplication::JoinMessage(fields);
        NS_TEST_EXPECT_MSG_EQ(message, "1.Owner0_Qubit0.", "fields are joined by the delimiter");
        NS_TEST_EXPECT_MSG_EQ((QuantumApplication::SplitMessage(message) == fields),
                              true,
                              "splitting undoes joining");
    }
};

/**
 * @ingroup quantum-tests
 * Test the state operations of QuantumPhyEntity, without a network.
 */
class QuantumStateTestCase : public TestCase
{
  public:
    QuantumStateTestCase()
        : TestCase("State preparation, gates, measurement and partial trace")
    {
    }

  private:
    void DoRun() override
    {
        Ptr<QuantumPhyEntity> qphyent = CreateObject<QuantumPhyEntity>();
        const std::string god = QNS_GOD_OWNER;

        // Density matrices are stored row by row.
        qphyent->GenerateQubitsPure(god, TEST_STATE, {"psi"});
        NS_TEST_EXPECT_MSG_LT(
            Distance(qphyent->PeekDensityMatrix(god, {"psi"}), DensityMatrix(TEST_STATE)),
            TOLERANCE,
            "a peek returns the prepared state");

        // H then CNOT make a Bell pair; CNOT takes {target, control}.
        qphyent->GenerateQubitsPure(god, QNS_KET_0, {"a"});
        qphyent->GenerateQubitsPure(god, QNS_KET_0, {"b"});
        qphyent->ApplyGate(god, QNS_GATE_H, {}, {"a"});
        qphyent->ApplyGate(god, QNS_GATE_CNOT, {}, {"b", "a"});
        NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity({"a", "b"}),
                                  1.0,
                                  TOLERANCE,
                                  "H and CNOT make a Bell pair");
        NS_TEST_EXPECT_MSG_LT(
            Distance(qphyent->PeekDensityMatrix(god, {"a"}), {{0.5, 0}, {0, 0}, {0, 0}, {0.5, 0}}),
            TOLERANCE,
            "half of a Bell pair is maximally mixed");

        // Contracting the network must not change the state, whatever the
        // contraction order: the order of this module, the order of creation
        // and an order of ExaTN. Each contraction leaves one tensor, so a gate
        // that cancels itself is applied in between to grow the network again.
        for (const std::string optimizer : {"auto", "ascend", "greed"})
        {
            qphyent->ApplyGate(god, QNS_GATE_H, {}, {"psi"});
            qphyent->ApplyGate(god, QNS_GATE_H, {}, {"psi"});
            qphyent->Contract(optimizer);
            NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity({"a", "b"}),
                                      1.0,
                                      TOLERANCE,
                                      "contraction with " << optimizer << " preserves the state");
            NS_TEST_EXPECT_MSG_LT(
                Distance(qphyent->PeekDensityMatrix(god, {"psi"}), DensityMatrix(TEST_STATE)),
                TOLERANCE,
                "contraction with " << optimizer << " preserves the other qubits");
        }

        // Measuring one half of a Bell pair fixes the other half.
        const QuantumMeasurement first = qphyent->Measure(god, "a");
        NS_TEST_EXPECT_MSG_EQ_TOL(first.distribution[0], 0.5, TOLERANCE, "a fair outcome");
        const QuantumMeasurement second = qphyent->Measure(god, "b");
        NS_TEST_EXPECT_MSG_EQ(second.outcome, first.outcome, "the outcomes are correlated");
        NS_TEST_EXPECT_MSG_EQ_TOL(second.distribution[first.outcome],
                                  1.0,
                                  TOLERANCE,
                                  "the second outcome is certain");

        // Post-selection conditions without sampling.
        qphyent->GenerateQubitsPure(god, QNS_BELL, {"c", "d"});
        NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->PostSelect(god, "c", 1),
                                  0.5,
                                  TOLERANCE,
                                  "probability of the selected outcome");
        NS_TEST_EXPECT_MSG_LT(
            Distance(qphyent->PeekDensityMatrix(god, {"d"}), DensityMatrix(QNS_KET_1)),
            TOLERANCE,
            "post-selection projects the partner");

        // Tracing a qubit out removes it.
        qphyent->PartialTrace({"c"});
        NS_TEST_EXPECT_MSG_EQ(qphyent->CheckValid({"c"}), false, "a traced qubit is gone");
        NS_TEST_EXPECT_MSG_EQ(qphyent->CheckValid({"d"}), true, "the other qubits stay");

        // Mixed states and quantum operations.
        qphyent->GenerateQubitsMixed(god, MakeWernerState(0.8), {"e", "f"});
        NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity({"e", "f"}),
                                  0.8,
                                  TOLERANCE,
                                  "a Werner state keeps its fidelity");
        qphyent->GenerateQubitsPure(god, QNS_BELL, {"g", "h"});
        Ptr<DepolarModel> depolar =
            CreateObjectWithAttributes<DepolarModel>("Fidelity", DoubleValue(0.9));
        qphyent->ApplyOperation(depolar->GetOperation(Time(0)), {"h"});
        NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity({"g", "h"}),
                                  0.9,
                                  TOLERANCE,
                                  "depolarizing half of a Bell pair gives that fidelity");

        Simulator::Destroy();
    }
};

/**
 * @ingroup quantum-tests
 * Test that memory noise and gate noise are applied when and where they should.
 */
class QuantumNoiseTestCase : public TestCase
{
  public:
    QuantumNoiseTestCase()
        : TestCase("Memory noise and gate noise")
    {
    }

  private:
    /**
     * Read the coherence left in a qubit prepared in (|0> + |1>) / sqrt(2).
     *
     * @param qphyent The quantum physical entity.
     * @param qubit Name of the qubit.
     * @return Twice the off-diagonal entry of the density matrix: 1 without noise.
     */
    static double Coherence(Ptr<QuantumPhyEntity> qphyent, const std::string& qubit)
    {
        return 2 * qphyent->PeekDensityMatrix(QNS_GOD_OWNER, {qubit})[1].real();
    }

    void DoRun() override
    {
        TestNetwork network(2, TestNetwork::Noise::DEFAULT);
        Ptr<QuantumPhyEntity> qphyent = network.qphyent;
        const std::string god = QNS_GOD_OWNER;
        const QuantumTensorData plus{{M_SQRT1_2, 0}, {M_SQRT1_2, 0}};
        // Owner1 has a memory of its own; Owner0 uses the default one.
        qphyent->SetTimeModel("Owner1", Seconds(4));
        qphyent->SetDephaseModel("Owner1", QNS_GATE_I, MilliSeconds(1));

        Simulator::Schedule(Seconds(0), [=]() {
            qphyent->GenerateQubitsPure("Owner0", plus, {"q0"});
            qphyent->GenerateQubitsPure("Owner1", plus, {"q1"});
            qphyent->GenerateQubitsPure("Owner1", plus, {"q2"});
        });
        Simulator::Schedule(Seconds(0.5), [=]() {
            // A noiseless gate brings the memory noise up to date.
            qphyent->ApplyGate(god, QNS_GATE_I, {}, {"q0"});
            qphyent->ApplyGate(god, QNS_GATE_I, {}, {"q1"});
            // A gate of a node adds gate noise on top.
            qphyent->ApplyGate("Owner1", QNS_GATE_I, {}, {"q2"});
        });
        Simulator::Run();

        // Default TimeModel: coherence time 1 s. Default gate duration: 200 us.
        NS_TEST_EXPECT_MSG_EQ_TOL(Coherence(qphyent, "q0"),
                                  std::exp(-0.5 / 1.0),
                                  TOLERANCE,
                                  "default memory noise");
        NS_TEST_EXPECT_MSG_EQ_TOL(Coherence(qphyent, "q1"),
                                  std::exp(-0.5 / 4.0),
                                  TOLERANCE,
                                  "memory noise of the owner");
        NS_TEST_EXPECT_MSG_EQ_TOL(Coherence(qphyent, "q2"),
                                  std::exp(-0.5 / 4.0) * std::exp(-200e-6 / 1e-3),
                                  TOLERANCE,
                                  "gate noise follows memory noise");
        Simulator::Destroy();
    }
};

/**
 * @ingroup quantum-tests
 * Test that nodes without a classical network can still hold and operate on
 * qubits.
 */
class QuantumOnlyNetworkTestCase : public TestCase
{
  public:
    QuantumOnlyNetworkTestCase()
        : TestCase("Quantum stack without a classical network")
    {
    }

  private:
    void DoRun() override
    {
        NodeContainer nodes;
        nodes.Create(2);
        QuantumNetStackHelper stack;
        Ptr<QuantumPhyEntity> qphyent = stack.Install(nodes, {"Alice", "Bob"});
        NS_TEST_EXPECT_MSG_EQ(qphyent->GetQuantumNode("Alice")->HasAddress(),
                              false,
                              "a node without a network stack has no address");

        Simulator::Schedule(Seconds(0),
                            [=]() { qphyent->GenerateQubitsPure("Alice", QNS_BELL, {"a", "b"}); });
        Simulator::Schedule(Seconds(0.5),
                            [=]() { qphyent->ApplyGate("Alice", QNS_GATE_I, {}, {"a"}); });
        Simulator::Run();
        // One qubit dephased for 0.5 s with the default coherence time of 1 s,
        // followed by the default gate noise of 200 us.
        const double coherence = std::exp(-0.5) * std::exp(-200e-6);
        NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity({"a", "b"}),
                                  (1 + coherence) / 2,
                                  TOLERANCE,
                                  "local operations work without a classical network");
        Simulator::Destroy();
    }
};

/**
 * @ingroup quantum-tests
 * Test teleportation: the sampled protocol, and the two exact variants
 * against it.
 */
class TeleportationTestCase : public TestCase
{
  public:
    /// The implementation of teleportation to run.
    enum class Variant
    {
        SAMPLED,     //!< TelepSrcApp and TelepDstApp
        ADAPT,       //!< TelepAdaptApp
        ADAPT_CHAIN, //!< TelepLinAdaptApp on both nodes
    };

    TeleportationTestCase()
        : TestCase("Teleportation, sampled and exact")
    {
    }

  private:
    /**
     * Teleport the test state from Owner0 to Owner1.
     *
     * @param variant The implementation to use.
     * @param noise The noise of the network.
     * @return The density matrix of the teleported qubit.
     */
    static QuantumTensorData Teleport(Variant variant, TestNetwork::Noise noise)
    {
        TestNetwork network(2, noise);
        Ptr<QuantumPhyEntity> qphyent = network.qphyent;
        Ptr<QuantumChannel> channel = network.Channel(0, 1);
        Ptr<Qubit> input = CreateObject<Qubit>(TEST_STATE, "A0");
        const PairValue<StringValue, StringValue> aliceQubits({"A0", "A1"});

        switch (variant)
        {
        case Variant::SAMPLED: {
            TelepSrcHelper srcHelper(qphyent, channel);
            srcHelper.SetAttribute("Qubits", aliceQubits);
            srcHelper.SetAttribute("Qubit", StringValue("B0"));
            srcHelper.SetAttribute("Input", PointerValue(input));
            srcHelper.Install();
            TelepDstHelper dstHelper(qphyent, channel);
            dstHelper.SetAttribute("Qubit", StringValue("B0"));
            dstHelper.Install();
            break;
        }
        case Variant::ADAPT: {
            TelepAdaptHelper helper(qphyent, channel);
            helper.SetAttribute("Qubits", aliceQubits);
            helper.SetAttribute("Qubit", StringValue("B0"));
            helper.SetAttribute("Input", PointerValue(input));
            helper.SetAttribute("LastOwner", StringValue("Owner1"));
            helper.Install();
            break;
        }
        case Variant::ADAPT_CHAIN: {
            TelepLinAdaptHelper firstHelper(qphyent, channel);
            firstHelper.SetAttribute("EPR", PairValue<StringValue, StringValue>({"A1", "B0"}));
            firstHelper.SetAttribute("Input", PointerValue(input));
            // Same message size as the sampled protocol, hence the same timing.
            firstHelper.SetAttribute("PacketSize", UintegerValue(1024));
            firstHelper.Install();
            TelepLinAdaptHelper lastHelper(qphyent, nullptr);
            lastHelper.Install(qphyent->GetQuantumNode("Owner1"));
            break;
        }
        }

        Simulator::Stop(Seconds(1));
        Simulator::Run();
        QuantumTensorData output = qphyent->PeekDensityMatrix(QNS_GOD_OWNER, {"B0"});
        Simulator::Destroy();
        return output;
    }

    /**
     * Check that the helpers turn owner names into socket addresses: the
     * source must send to the address on which the destination receives.
     */
    void CheckAddresses()
    {
        TestNetwork network(2, TestNetwork::Noise::NONE);
        Ptr<QuantumChannel> channel = network.Channel(0, 1);
        ApplicationContainer srcApp = TelepSrcHelper(network.qphyent, channel).Install();
        ApplicationContainer dstApp = TelepDstHelper(network.qphyent, channel).Install();

        AddressValue remote;
        AddressValue local;
        srcApp.Get(0)->GetAttribute("Remote", remote);
        dstApp.Get(0)->GetAttribute("Local", local);
        NS_TEST_EXPECT_MSG_EQ(InetSocketAddress::IsMatchingType(remote.Get()),
                              true,
                              "Remote is an IP address with a port");
        NS_TEST_EXPECT_MSG_EQ(remote.Get(),
                              local.Get(),
                              "the source sends to the address of the destination");
        Simulator::Destroy();
    }

    void DoRun() override
    {
        CheckAddresses();

        const QuantumTensorData ideal = DensityMatrix(TEST_STATE);
        for (auto variant : {Variant::SAMPLED, Variant::ADAPT, Variant::ADAPT_CHAIN})
        {
            NS_TEST_EXPECT_MSG_LT(Distance(Teleport(variant, TestNetwork::Noise::NONE), ideal),
                                  TOLERANCE,
                                  "without noise, teleportation is perfect");
        }

        // All noise is Pauli noise, so the sampled protocol ends in the same
        // state whatever the outcomes, and the exact variants must find it.
        const QuantumTensorData sampled =
            Teleport(Variant::SAMPLED, TestNetwork::Noise::CHANNEL_ONLY);
        NS_TEST_EXPECT_MSG_GT(Distance(sampled, ideal), 1e-3, "channel noise degrades the state");
        NS_TEST_EXPECT_MSG_LT(
            Distance(Teleport(Variant::ADAPT, TestNetwork::Noise::CHANNEL_ONLY), sampled),
            TOLERANCE,
            "TelepAdaptApp agrees with sampling under channel noise");

        // With memory noise the timing matters. TelepLinAdaptApp keeps the
        // classical message, so it agrees under all noise.
        NS_TEST_EXPECT_MSG_LT(Distance(Teleport(Variant::ADAPT_CHAIN, TestNetwork::Noise::DEFAULT),
                                       Teleport(Variant::SAMPLED, TestNetwork::Noise::DEFAULT)),
                              TOLERANCE,
                              "TelepLinAdaptApp agrees with sampling under all noise");
    }
};

/**
 * @ingroup quantum-tests
 * Test entanglement swapping along a chain against the closed formula, for
 * the sampled protocol and the two exact variants.
 */
class EntanglementSwappingTestCase : public TestCase
{
  public:
    EntanglementSwappingTestCase()
        : TestCase("Entanglement swapping, sampled and exact")
    {
    }

  private:
    /// Number of nodes of the chain.
    static constexpr uint32_t N_NODES{4};
    /// Fidelity of each Bell pair of the chain.
    static constexpr double EPR_FIDELITY{0.9};

    /**
     * Name the qubits of the chain.
     *
     * @param former Filled with the qubit that each node shares with the previous node.
     * @param latter Filled with the qubit that each node shares with the next node.
     */
    static void NameQubits(std::vector<std::string>& former, std::vector<std::string>& latter)
    {
        for (uint32_t i = 0; i < N_NODES; ++i)
        {
            former.push_back(i == 0 ? "" : "Former" + std::to_string(i));
            latter.push_back(i + 1 == N_NODES ? "" : "Latter" + std::to_string(i));
        }
    }

    /**
     * Schedule the distribution of the Bell pairs of the chain.
     *
     * @param network The network.
     * @param former The qubit that each node shares with the previous node.
     * @param latter The qubit that each node shares with the next node.
     */
    static void DistributePairs(const TestNetwork& network,
                                const std::vector<std::string>& former,
                                const std::vector<std::string>& latter)
    {
        for (uint32_t i = 0; i + 1 < N_NODES; ++i)
        {
            Ptr<QuantumChannel> channel = network.Channel(i, i + 1);
            network.qphyent->SetDepolarModel(channel, EPR_FIDELITY);
            Ptr<DistributeEprProtocol> distributor =
                network.nodes.Get(i)->GetObject<DistributeEprProtocol>();
            Simulator::Schedule(MilliSeconds(100),
                                &DistributeEprProtocol::DistributeEpr,
                                distributor,
                                network.owners[i + 1],
                                std::pair<std::string, std::string>{latter[i], former[i + 1]});
        }
    }

    void DoRun() override
    {
        const double expected = SwappedFidelity(EPR_FIDELITY, N_NODES - 1);
        std::vector<std::string> former;
        std::vector<std::string> latter;
        NameQubits(former, latter);
        const std::pair<std::string, std::string> finalPair{latter.front(), former.back()};

        {
            // Sampled: every repeater measures and reports to the last node.
            TestNetwork network(N_NODES, TestNetwork::Noise::CHANNEL_ONLY);
            Ptr<QuantumPhyEntity> qphyent = network.qphyent;
            DistributePairs(network, former, latter);
            for (uint32_t i = 1; i + 1 < N_NODES; ++i)
            {
                EntSwapSrcHelper srcHelper(qphyent, network.Channel(i, N_NODES - 1));
                srcHelper.SetAttribute("Qubits",
                                       PairValue<StringValue, StringValue>({former[i], latter[i]}));
                srcHelper.Install().Start(MilliSeconds(200 * i));
            }
            Ptr<QuantumNode> lastNode = qphyent->GetQuantumNode(network.owners.back());
            EntSwapDstHelper dstHelper(qphyent, lastNode);
            dstHelper.SetAttribute("Qubit", StringValue(former.back()));
            dstHelper.SetAttribute("Count", UintegerValue(N_NODES - 2));
            dstHelper.Install();
            Simulator::Stop(Seconds(2));
            Simulator::Run();
            NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity(finalPair),
                                      expected,
                                      TOLERANCE,
                                      "sampled swapping matches the closed formula");
            Simulator::Destroy();
        }
        {
            // Exact, with the flags accumulated in two ancilla qubits.
            TestNetwork network(N_NODES, TestNetwork::Noise::CHANNEL_ONLY);
            Ptr<QuantumPhyEntity> qphyent = network.qphyent;
            DistributePairs(network, former, latter);
            EntSwapAdaptHelper helper(qphyent);
            helper.SetAttribute("QubitsFormer", PointerValue(CreateObject<QuantumMemory>(former)));
            helper.SetAttribute("QubitsLatter", PointerValue(CreateObject<QuantumMemory>(latter)));
            ApplicationContainer apps =
                helper.Install(qphyent->GetQuantumNode(network.owners.back()));
            apps.Start(MilliSeconds(200));
            Simulator::Stop(Seconds(2));
            Simulator::Run();
            NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity(finalPair),
                                      expected,
                                      TOLERANCE,
                                      "EntSwapAdaptApp matches the closed formula");
            Simulator::Destroy();
        }
        {
            // Exact, with the flags handed from node to node.
            TestNetwork network(N_NODES, TestNetwork::Noise::CHANNEL_ONLY);
            Ptr<QuantumPhyEntity> qphyent = network.qphyent;
            EntSwapAdaptLocalHelper helper(qphyent);
            helper.SetAttribute("QubitsFormer", PointerValue(CreateObject<QuantumMemory>(former)));
            helper.SetAttribute("QubitsLatter", PointerValue(CreateObject<QuantumMemory>(latter)));
            helper.SetAttribute("EprFidelity", DoubleValue(EPR_FIDELITY));
            ApplicationContainer apps =
                helper.Install(qphyent->GetQuantumNode(network.owners.back()));
            Simulator::Stop(Seconds(2));
            Simulator::Run();
            NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity(finalPair),
                                      expected,
                                      TOLERANCE,
                                      "EntSwapAdaptLocalApp matches the closed formula");
            Simulator::Destroy();
        }
    }
};

/**
 * @ingroup quantum-tests
 * Test nested distillation against the closed formulas of one round, for the
 * sampled protocol and the exact variant, and the exact variant on two levels.
 */
class DistillationTestCase : public TestCase
{
  public:
    DistillationTestCase()
        : TestCase("Nested distillation, sampled and exact")
    {
    }

  private:
    /// Fidelity of each Bell pair before distillation.
    static constexpr double EPR_FIDELITY{0.9};

    /**
     * Name the qubits of the pairs.
     *
     * @param nPairs Number of pairs.
     * @param prefix Prefix of the names.
     * @return A memory holding the names.
     */
    static Ptr<QuantumMemory> NameQubits(uint32_t nPairs, const std::string& prefix)
    {
        std::vector<std::string> names;
        names.reserve(nPairs);
        for (uint32_t i = 0; i < nPairs; ++i)
        {
            names.push_back(prefix + std::to_string(i));
        }
        return CreateObject<QuantumMemory>(names);
    }

    /// The results that a simulation script reads from an exact distillation.
    struct ExactResult
    {
        uint32_t nFinished{0};         //!< How often the Finished trace fired
        double successProbability{-1}; //!< Probability that every round succeeds
        double fidelity{-1};           //!< Fidelity of the distilled pair, given success
    };

    /**
     * Read the results of an exact distillation; sink of the Finished trace.
     *
     * @param qphyent The quantum physical entity.
     * @param result Where to store the results.
     * @param app The application that finished.
     */
    static void ReadExactResult(Ptr<QuantumPhyEntity> qphyent,
                                ExactResult* result,
                                Ptr<const QuantumApplication> app)
    {
        ++result->nFinished;
        // The flag qubit is 1 if every round succeeded.
        result->successProbability = qphyent->PostSelect("Owner0", "Flag", 1);
        result->fidelity = qphyent->CalculateFidelity({"A0", "B0"});
    }

    /**
     * Run the exact nested distillation.
     *
     * @param nPairs Number of pairs to start from.
     * @return The results read when the application finished.
     */
    static ExactResult RunExact(uint32_t nPairs)
    {
        TestNetwork network(2, TestNetwork::Noise::CHANNEL_ONLY);
        Ptr<QuantumChannel> channel = network.Channel(0, 1);
        network.qphyent->SetDepolarModel(channel, EPR_FIDELITY);
        DistillNestedAdaptHelper helper(network.qphyent, channel);
        helper.SetAttribute("SrcQubits", PointerValue(NameQubits(nPairs, "A")));
        helper.SetAttribute("DstQubits", PointerValue(NameQubits(nPairs, "B")));
        helper.SetAttribute("FlagQubit", StringValue("Flag"));
        // Leave time for the address resolution that precedes the first message.
        helper.SetAttribute("EprInterval", TimeValue(MilliSeconds(50)));
        ApplicationContainer apps = helper.Install();
        ExactResult result;
        apps.Get(0)->TraceConnectWithoutContext(
            "Finished",
            MakeBoundCallback(&DistillationTestCase::ReadExactResult, network.qphyent, &result));
        Simulator::Stop(Seconds(10));
        Simulator::Run();
        Simulator::Destroy();
        return result;
    }

    void DoRun() override
    {
        // One round on two Werner pairs, then a second level on two such results.
        const double error = (1 - EPR_FIDELITY) / 3;
        const BellDiagonal werner{EPR_FIDELITY, error, error, error};
        double successProbability = 0;
        const BellDiagonal firstLevel = DistillRound(werner, successProbability);
        const double distilledFidelity = firstLevel.phiPlus;
        double secondProbability = 0;
        const BellDiagonal secondLevel = DistillRound(firstLevel, secondProbability);

        const ExactResult oneRound = RunExact(2);
        NS_TEST_EXPECT_MSG_EQ(oneRound.nFinished, 1, "the application finishes once");
        NS_TEST_EXPECT_MSG_EQ_TOL(oneRound.successProbability,
                                  successProbability,
                                  TOLERANCE,
                                  "exact success probability of one round");
        NS_TEST_EXPECT_MSG_EQ_TOL(oneRound.fidelity,
                                  distilledFidelity,
                                  TOLERANCE,
                                  "exact fidelity after one round");

        // Two levels are three rounds.
        // The rounds do not twirl, so the second level amplifies the phase
        // flips that the first level left; the simulation must reproduce that.
        const ExactResult twoLevels = RunExact(4);
        NS_TEST_EXPECT_MSG_EQ_TOL(twoLevels.successProbability,
                                  successProbability * successProbability * secondProbability,
                                  TOLERANCE,
                                  "exact success probability of two levels");
        NS_TEST_EXPECT_MSG_EQ_TOL(twoLevels.fidelity,
                                  secondLevel.phiPlus,
                                  TOLERANCE,
                                  "exact fidelity after two levels");

        // Sampled: when the round succeeds, the pair has the exact fidelity.
        TestNetwork network(2, TestNetwork::Noise::CHANNEL_ONLY);
        Ptr<QuantumPhyEntity> qphyent = network.qphyent;
        Ptr<QuantumChannel> channel = network.Channel(0, 1);
        qphyent->SetDepolarModel(channel, EPR_FIDELITY);
        DistillNestedSrcHelper srcHelper(qphyent, channel);
        srcHelper.SetAttribute("SrcQubits", PointerValue(NameQubits(2, "A")));
        srcHelper.SetAttribute("DstQubits", PointerValue(NameQubits(2, "B")));
        ApplicationContainer srcApps = srcHelper.Install();
        DistillNestedDstHelper dstHelper(qphyent, channel);
        ApplicationContainer dstApps = dstHelper.Install();
        Simulator::Stop(Seconds(10));
        Simulator::Run();
        Ptr<DistillNestedSrcApp> alice = DynamicCast<DistillNestedSrcApp>(srcApps.Get(0));
        Ptr<DistillNestedDstApp> bob = DynamicCast<DistillNestedDstApp>(dstApps.Get(0));
        NS_TEST_EXPECT_MSG_EQ(alice->GetNRoundsDone(), 1, "Alice heard the verdict");
        NS_TEST_EXPECT_MSG_EQ(alice->GetWin(), bob->GetWin(), "Alice and Bob agree");
        NS_TEST_ASSERT_MSG_EQ(alice->GetWin(), true, "the round succeeds with this random stream");
        NS_TEST_EXPECT_MSG_EQ_TOL(qphyent->CalculateFidelity({"A0", "B0"}),
                                  distilledFidelity,
                                  TOLERANCE,
                                  "sampled fidelity after a successful round");
        Simulator::Destroy();
    }
};

/**
 * @ingroup quantum-tests
 * Test suite of the quantum module.
 */
class QuantumTestSuite : public TestSuite
{
  public:
    QuantumTestSuite()
        : TestSuite("quantum", Type::UNIT)
    {
        AddTestCase(new QuantumBasisTestCase, TestCase::Duration::QUICK);
        AddTestCase(new QuantumStateTestCase, TestCase::Duration::QUICK);
        AddTestCase(new QuantumNoiseTestCase, TestCase::Duration::QUICK);
        AddTestCase(new QuantumOnlyNetworkTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TeleportationTestCase, TestCase::Duration::QUICK);
        AddTestCase(new EntanglementSwappingTestCase, TestCase::Duration::QUICK);
        AddTestCase(new DistillationTestCase, TestCase::Duration::QUICK);
    }
};

/// The test suite instance.
static QuantumTestSuite g_quantumTestSuite;

} // namespace tests

} // namespace ns3
