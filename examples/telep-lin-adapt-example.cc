/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/quantum-module.h"

#include <iostream>
#include <string>
#include <vector>

/**
 * @file
 * @ingroup quantum
 * Teleport a qubit along a chain of nodes, exactly.
 *
 * Every node runs TelepLinAdaptApp. No node measures: the corrections are
 * applied as controlled gates at the end of the chain, so one run yields the
 * exact final state for all measurement branches at once.
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TelepLinAdaptExample");

/// Set once the protocol has finished and its results have been printed.
static bool g_finished = false;

/**
 * Print the state of the teleported qubit. Sink of the Finished trace source.
 *
 * @param qphyent The quantum physical entity.
 * @param state The state that was to be teleported.
 * @param qubit Name of the qubit that now holds the state.
 * @param app The application that finished.
 */
static void
ReportTeleportedState(Ptr<QuantumPhyEntity> qphyent,
                      const QuantumTensorData& state,
                      const std::string& qubit,
                      Ptr<const QuantumApplication> app)
{
    const QuantumTensorData output = qphyent->PeekDensityMatrix(QNS_GOD_OWNER, {qubit});
    // The fidelity of the output with the input state is <state| output |state>.
    std::complex<double> overlap = 0;
    for (std::size_t row = 0; row < 2; ++row)
    {
        for (std::size_t col = 0; col < 2; ++col)
        {
            overlap += std::conj(state[row]) * output[2 * row + col] * state[col];
        }
    }
    std::cout << "Density matrix of the teleported qubit, row by row:";
    for (const auto& entry : output)
    {
        std::cout << " " << entry;
    }
    std::cout << std::endl;
    std::cout << "Fidelity with the input state: " << overlap.real() << std::endl;
    g_finished = true;
    Simulator::Stop();
}

int
main(int argc, char* argv[])
{
    uint32_t nNodes = 8;
    double firstFidelity = 0.95;
    double fidelity = 0.98;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of nodes of the chain", nNodes);
    cmd.AddValue("firstFidelity", "Fidelity of the Bell pair of the first hop", firstFidelity);
    cmd.AddValue("fidelity", "Fidelity of the Bell pairs of the other hops", fidelity);
    cmd.Parse(argc, argv);
    NS_ABORT_MSG_IF(nNodes < 2, "The chain needs at least 2 nodes");

    // The classical network: all nodes on one CSMA segment, with IPv6.
    NodeContainer nodes;
    nodes.Create(nNodes);
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("1000kbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(100)));
    NetDeviceContainer devices = csma.Install(nodes);
    InternetStackHelper internet;
    internet.Install(nodes);
    Ipv6AddressHelper addresses;
    addresses.SetBase("2001:1::", Ipv6Prefix(64));
    addresses.Assign(devices);

    // The quantum network on top of it.
    std::vector<std::string> owners;
    owners.reserve(nNodes);
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        owners.push_back("Owner" + std::to_string(i));
    }
    QuantumNetStackHelper stack;
    Ptr<QuantumPhyEntity> qphyent = stack.Install(nodes, owners);

    // The state to teleport: sqrt(5/7) |0> + i sqrt(2/7) |1>.
    const QuantumTensorData state{{std::sqrt(5.0 / 7.0), 0.0}, {0.0, std::sqrt(2.0 / 7.0)}};

    for (uint32_t i = 0; i + 1 < nNodes; ++i)
    {
        Ptr<QuantumChannel> channel = CreateObject<QuantumChannel>(owners[i], owners[i + 1]);
        qphyent->SetDepolarModel(channel, i == 0 ? firstFidelity : fidelity);

        TelepLinAdaptHelper helper(qphyent, channel);
        helper.SetAttribute("EPR",
                            PairValue<StringValue, StringValue>(
                                {owners[i] + "_Qubit1", owners[i + 1] + "_Qubit0"}));
        if (i == 0)
        {
            helper.SetAttribute("Input",
                                PointerValue(CreateObject<Qubit>(state, owners[0] + "_Qubit0")));
        }
        helper.Install();
    }
    // The last node only receives.
    TelepLinAdaptHelper lastHelper(qphyent, nullptr);
    ApplicationContainer lastApp = lastHelper.Install(qphyent->GetQuantumNode(owners.back()));
    // What to report, and when: the state at the last node, once the chain is done.
    lastApp.Get(0)->TraceConnectWithoutContext(
        "Finished",
        MakeBoundCallback(&ReportTeleportedState, qphyent, state, owners.back() + "_Qubit0"));

    // The report stops the simulation; this is only a deadline.
    Simulator::Stop(Hours(1));
    Simulator::Run();
    NS_ABORT_MSG_UNLESS(g_finished, "The protocol did not finish");

    Simulator::Destroy();
    return 0;
}
