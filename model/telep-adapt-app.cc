/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "telep-adapt-app.h"

#include "distribute-epr-protocol.h"
#include "quantum-channel.h"
#include "quantum-phy-entity.h"
#include "qubit.h"

#include "ns3/log.h"
#include "ns3/pair.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TelepAdaptApp");

NS_OBJECT_ENSURE_REGISTERED(TelepAdaptApp);

TypeId
TelepAdaptApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TelepAdaptApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<TelepAdaptApp>()
            .AddAttribute("QChannel",
                          "The quantum channel from Alice to Bob.",
                          PointerValue(),
                          MakePointerAccessor(&TelepAdaptApp::m_channel),
                          MakePointerChecker<QuantumChannel>())
            .AddAttribute("LastOwner",
                          "Owner at the end of the teleportation chain; the application "
                          "contracts the tensor network if this is Bob.",
                          StringValue(),
                          MakeStringAccessor(&TelepAdaptApp::m_lastOwner),
                          MakeStringChecker())
            .AddAttribute("Qubits",
                          "Alice's qubit to teleport and her half of the Bell pair.",
                          PairValue<StringValue, StringValue>(),
                          MakePairAccessor<StringValue, StringValue>(&TelepAdaptApp::m_qubits),
                          MakePairChecker<StringValue, StringValue>())
            .AddAttribute("Qubit",
                          "Bob's half of the Bell pair.",
                          StringValue(),
                          MakeStringAccessor(&TelepAdaptApp::m_qubit),
                          MakeStringChecker())
            .AddAttribute("Input",
                          "State to prepare Alice's qubit in; leave unset if the qubit exists.",
                          PointerValue(),
                          MakePointerAccessor(&TelepAdaptApp::m_input),
                          MakePointerChecker<Qubit>())
            .AddAttribute("LocalOperationDelay",
                          "Wait between sharing the Bell pair and the local operations; it "
                          "must cover the distribution of the pair.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&TelepAdaptApp::m_delay),
                          MakeTimeChecker(Time(0)));
    return tid;
}

void
TelepAdaptApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    m_input = nullptr;
    QuantumApplication::DoDispose();
}

void
TelepAdaptApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string srcOwner = m_channel->GetSrcOwner();
    const std::string dstOwner = m_channel->GetDstOwner();

    if (m_input)
    {
        Simulator::ScheduleNow(&QuantumPhyEntity::GenerateQubitsPure,
                               qphyent,
                               srcOwner,
                               m_input->GetStateVector(),
                               std::vector<std::string>{m_qubits.first});
    }

    auto distributor = DynamicCast<DistributeEprSrcProtocol>(
        qphyent->GetConn2Apps(m_channel, QNS_APP_DIST_EPR).first);
    Simulator::ScheduleNow(&DistributeEprSrcProtocol::GenerateAndDistributeEpr,
                           distributor,
                           std::pair<std::string, std::string>{m_qubits.second, m_qubit});

    // Rotate Alice's two qubits into the Bell basis.
    Simulator::Schedule(m_delay,
                        &QuantumPhyEntity::ApplyGate,
                        qphyent,
                        srcOwner,
                        QNS_GATE_CNOT,
                        QuantumTensorData{},
                        std::vector<std::string>{m_qubits.second, m_qubits.first});
    Simulator::Schedule(m_delay,
                        &QuantumPhyEntity::ApplyGate,
                        qphyent,
                        srcOwner,
                        QNS_GATE_H,
                        QuantumTensorData{},
                        std::vector<std::string>{m_qubits.first});

    // Bob's corrections, controlled by Alice's qubits instead of her outcomes.
    Simulator::Schedule(m_delay,
                        &QuantumPhyEntity::ApplyControlledOperation,
                        qphyent,
                        dstOwner,
                        QNS_GATE_PX,
                        QNS_GATE_CNOT,
                        QuantumTensorData{},
                        std::vector<std::string>{m_qubits.second},
                        std::vector<std::string>{m_qubit});
    Simulator::Schedule(m_delay,
                        &QuantumPhyEntity::PartialTrace,
                        qphyent,
                        std::vector<std::string>{m_qubits.second});
    Simulator::Schedule(m_delay,
                        &QuantumPhyEntity::ApplyControlledOperation,
                        qphyent,
                        dstOwner,
                        QNS_GATE_PZ,
                        QNS_GATE_CZ,
                        QuantumTensorData{},
                        std::vector<std::string>{m_qubits.first},
                        std::vector<std::string>{m_qubit});
    Simulator::Schedule(m_delay,
                        &QuantumPhyEntity::PartialTrace,
                        qphyent,
                        std::vector<std::string>{m_qubits.first});

    if (m_lastOwner == dstOwner)
    {
        // Collapse the network so that reading the result is cheap.
        Simulator::Schedule(m_delay, &QuantumPhyEntity::Contract, qphyent, std::string("ascend"));
    }
    Simulator::Schedule(m_delay, &TelepAdaptApp::NotifyFinished, this);
}

} // namespace ns3
