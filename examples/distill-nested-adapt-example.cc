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
 * Distill Bell pairs between two nodes, level by level, exactly.
 *
 * DistillNestedAdaptApp folds the success of every round into a flag qubit
 * instead of measuring. One run yields the exact probability that all rounds
 * succeed and the exact fidelity of the distilled pair.
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistillNestedAdaptExample");

/// Set once the protocol has finished and its results have been printed.
static bool g_finished = false;

/**
 * Print the exact results of the distillation. Sink of the Finished trace source.
 *
 * @param qphyent The quantum physical entity.
 * @param app The application that finished.
 */
static void
ReportDistillation(Ptr<QuantumPhyEntity> qphyent, Ptr<const QuantumApplication> app)
{
    // The flag qubit is 1 if every round succeeded. Post-selecting it returns
    // that probability and conditions the state on success.
    const double probability = qphyent->PostSelect("Alice", "Flag", 1);
    std::cout << "Probability that all rounds succeed: " << probability << std::endl;
    std::cout << "Fidelity of the distilled pair: " << qphyent->CalculateFidelity({"A0", "B0"})
              << std::endl;
    g_finished = true;
}

int
main(int argc, char* argv[])
{
    uint32_t nPairs = 8;
    double fidelity = 0.95;
    Time coherenceTime = Seconds(20);

    CommandLine cmd(__FILE__);
    cmd.AddValue("nPairs", "Number of Bell pairs to distill, a power of two", nPairs);
    cmd.AddValue("fidelity", "Fidelity of each distributed Bell pair", fidelity);
    cmd.AddValue("coherenceTime", "Coherence time of the memories of both nodes", coherenceTime);
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

    qphyent->GetQuantumNode("Alice")->SetTimeModel(coherenceTime);
    qphyent->GetQuantumNode("Bob")->SetTimeModel(coherenceTime);
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

    DistillNestedAdaptHelper helper(qphyent, channel);
    helper.SetAttribute("SrcQubits", PointerValue(CreateObject<QuantumMemory>(aliceQubits)));
    helper.SetAttribute("DstQubits", PointerValue(CreateObject<QuantumMemory>(bobQubits)));
    helper.SetAttribute("FlagQubit", StringValue("Flag"));
    ApplicationContainer apps = helper.Install();
    // What to report, and when: the success probability and the fidelity, once all rounds are done.
    apps.Get(0)->TraceConnectWithoutContext("Finished",
                                            MakeBoundCallback(&ReportDistillation, qphyent));

    // Each pair is allotted 10 ms, the default EprInterval.
    Simulator::Stop(MilliSeconds(10) * (nPairs + 2));
    Simulator::Run();
    NS_ABORT_MSG_UNLESS(g_finished, "The protocol did not finish before the simulation stopped");

    Simulator::Destroy();
    return 0;
}
