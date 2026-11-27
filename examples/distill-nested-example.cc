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
 * Distill Bell pairs between two nodes, level by level, sampling every measurement.
 *
 * Alice and Bob run DistillNestedSrcApp and DistillNestedDstApp. Each round
 * succeeds or fails at random; the run reports whether all rounds succeeded.
 * Compare with distill-nested-adapt-example, which computes the success
 * probability and the distilled fidelity exactly.
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistillNestedExample");

/// Set once the protocol has finished and its results have been printed.
static bool g_finished = false;

/**
 * Print the outcome of the distillation. Sink of the Finished trace source.
 *
 * @param qphyent The quantum physical entity.
 * @param app Alice's application, which finished.
 */
static void
ReportDistillation(Ptr<QuantumPhyEntity> qphyent, Ptr<const QuantumApplication> app)
{
    const bool win = DynamicCast<const DistillNestedSrcApp>(app)->GetWin();
    std::cout << "All rounds succeeded: " << (win ? "yes" : "no") << std::endl;
    std::cout << "Fidelity of the remaining pair: " << qphyent->CalculateFidelity({"A0", "B0"})
              << (win ? "" : " (not meaningful after a failed round)") << std::endl;
    g_finished = true;
}

int
main(int argc, char* argv[])
{
    uint32_t nPairs = 4;
    double fidelity = 0.93;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nPairs", "Number of Bell pairs to distill, a power of two", nPairs);
    cmd.AddValue("fidelity", "Fidelity of each distributed Bell pair", fidelity);
    cmd.Parse(argc, argv);

    // The classical network: all nodes on one CSMA segment, with IPv6.
    NodeContainer nodes;
    nodes.Create(2);
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
    const std::vector<std::string> owners{"Alice", "Bob"};
    QuantumNetStackHelper stack;
    Ptr<QuantumPhyEntity> qphyent =
        stack.Install(nodes, owners, QuantumNetStackHelper::Connectivity::CHAIN);

    // Noise: Alice's memory, Bob's CNOT gates, and the quantum channel.
    qphyent->GetQuantumNode("Alice")->SetTimeModel(MilliSeconds(130));
    qphyent->GetQuantumNode("Bob")->SetDephaseModel(QNS_GATE_CNOT, MilliSeconds(230));
    Ptr<QuantumChannel> channel = CreateObject<QuantumChannel>("Alice", "Bob");
    qphyent->SetDepolarModel(channel, fidelity);

    // Name the qubits of the pairs: A0, A1, ... at Alice and B0, B1, ... at Bob.
    std::vector<std::string> aliceQubits;
    std::vector<std::string> bobQubits;
    for (uint32_t i = 0; i < nPairs; ++i)
    {
        aliceQubits.push_back("A" + std::to_string(i));
        bobQubits.push_back("B" + std::to_string(i));
    }

    DistillNestedSrcHelper srcHelper(qphyent, channel);
    srcHelper.SetAttribute("SrcQubits", PointerValue(CreateObject<QuantumMemory>(aliceQubits)));
    srcHelper.SetAttribute("DstQubits", PointerValue(CreateObject<QuantumMemory>(bobQubits)));
    ApplicationContainer srcApps = srcHelper.Install();
    // What to report, and when: the verdict and the fidelity, once Alice has heard of all rounds.
    srcApps.Get(0)->TraceConnectWithoutContext("Finished",
                                               MakeBoundCallback(&ReportDistillation, qphyent));
    DistillNestedDstHelper dstHelper(qphyent, channel);
    dstHelper.Install();

    // Each pair and each round takes one step of 100 ms.
    Simulator::Stop(MilliSeconds(100) * (2 * nPairs + 1));
    Simulator::Run();
    NS_ABORT_MSG_UNLESS(g_finished, "The protocol did not finish before the simulation stopped");

    Simulator::Destroy();
    return 0;
}
