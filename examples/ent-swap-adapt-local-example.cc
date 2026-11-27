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
 * Swap entanglement along a long chain of nodes, exactly.
 *
 * EntSwapAdaptLocalApp hands the correction flags from node to node, so the
 * tensor network of the simulation is itself a chain and stays cheap to
 * contract. Try --nNodes=1024.
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EntSwapAdaptLocalExample");

/// Set once the protocol has finished and its results have been printed.
static bool g_finished = false;

/**
 * Print the fidelity of the final pair. Sink of the Finished trace source.
 *
 * @param qphyent The quantum physical entity.
 * @param pair Names of the qubits of the first and of the last node.
 * @param app The application that finished.
 */
static void
ReportFidelity(Ptr<QuantumPhyEntity> qphyent,
               const std::pair<std::string, std::string>& pair,
               Ptr<const QuantumApplication> app)
{
    std::cout << "Fidelity of the pair shared by the first and the last node: "
              << qphyent->CalculateFidelity(pair) << std::endl;
    g_finished = true;
}

int
main(int argc, char* argv[])
{
    uint32_t nNodes = 64;
    double fidelity = 0.95;
    Time correctionCoherenceTime = Seconds(1.2);

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of nodes of the chain", nNodes);
    cmd.AddValue("fidelity", "Fidelity of each distributed Bell pair", fidelity);
    cmd.AddValue("correctionCoherenceTime",
                 "Coherence time of the correction gates of the last node",
                 correctionCoherenceTime);
    cmd.Parse(argc, argv);
    NS_ABORT_MSG_IF(nNodes < 3, "The chain needs at least 3 nodes");

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
    Ptr<QuantumPhyEntity> qphyent =
        stack.Install(nodes, owners, QuantumNetStackHelper::Connectivity::NONE);

    // The last node applies the corrections with noisy gates.
    Ptr<QuantumNode> lastNode = qphyent->GetQuantumNode(owners.back());
    lastNode->SetDephaseModel(QNS_GATE_PX, correctionCoherenceTime);
    lastNode->SetDephaseModel(QNS_GATE_PZ, correctionCoherenceTime);

    // Name the qubits: node i shares "former" with node i - 1 and "latter" with node i + 1.
    std::vector<std::string> former;
    std::vector<std::string> latter;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        former.push_back(i == 0 ? "" : owners[i] + "_Former");
        latter.push_back(i + 1 == nNodes ? "" : owners[i] + "_Latter");
    }

    EntSwapAdaptLocalHelper helper(qphyent);
    helper.SetAttribute("QubitsFormer", PointerValue(CreateObject<QuantumMemory>(former)));
    helper.SetAttribute("QubitsLatter", PointerValue(CreateObject<QuantumMemory>(latter)));
    helper.SetAttribute("EprFidelity", DoubleValue(fidelity));
    ApplicationContainer apps = helper.Install(lastNode);
    // What to report, and when: the fidelity of the final pair, once the chain is done.
    apps.Get(0)->TraceConnectWithoutContext(
        "Finished",
        MakeBoundCallback(&ReportFidelity,
                          qphyent,
                          std::pair<std::string, std::string>{latter.front(), former.back()}));

    Simulator::Stop(Seconds(1));
    Simulator::Run();
    NS_ABORT_MSG_UNLESS(g_finished, "The protocol did not finish before the simulation stopped");

    Simulator::Destroy();
    return 0;
}
