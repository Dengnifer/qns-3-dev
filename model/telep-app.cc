/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "telep-app.h"

#include "distribute-epr-protocol.h"
#include "quantum-channel.h"
#include "quantum-phy-entity.h"
#include "qubit.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/pair.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TelepApp");

NS_OBJECT_ENSURE_REGISTERED(TelepSrcApp);

TypeId
TelepSrcApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TelepSrcApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<TelepSrcApp>()
            .AddAttribute("QChannel",
                          "The quantum channel from Alice to Bob.",
                          PointerValue(),
                          MakePointerAccessor(&TelepSrcApp::m_channel),
                          MakePointerChecker<QuantumChannel>())
            .AddAttribute("Qubits",
                          "Alice's qubit to teleport and her half of the Bell pair.",
                          PairValue<StringValue, StringValue>(),
                          MakePairAccessor<StringValue, StringValue>(&TelepSrcApp::m_qubits),
                          MakePairChecker<StringValue, StringValue>())
            .AddAttribute("Qubit",
                          "Bob's half of the Bell pair.",
                          StringValue(),
                          MakeStringAccessor(&TelepSrcApp::m_qubit),
                          MakeStringChecker())
            .AddAttribute("Input",
                          "State to prepare Alice's qubit in; leave unset if the qubit exists.",
                          PointerValue(),
                          MakePointerAccessor(&TelepSrcApp::m_input),
                          MakePointerChecker<Qubit>())
            .AddAttribute("LocalOperationDelay",
                          "Wait between sharing the Bell pair and Alice's measurement; it "
                          "must cover the distribution of the pair.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&TelepSrcApp::m_delay),
                          MakeTimeChecker(Time(0)));
    return tid;
}

void
TelepSrcApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    m_input = nullptr;
    QuantumApplication::DoDispose();
}

void
TelepSrcApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = m_channel->GetSrcOwner();

    if (m_input)
    {
        Simulator::ScheduleNow(&QuantumPhyEntity::GenerateQubitsPure,
                               qphyent,
                               owner,
                               m_input->GetStateVector(),
                               std::vector<std::string>{m_qubits.first});
    }

    Ptr<DistributeEprProtocol> distributor = GetEprProtocol();
    Simulator::ScheduleNow(&DistributeEprProtocol::DistributeEpr,
                           distributor,
                           m_channel->GetDstOwner(),
                           std::pair<std::string, std::string>{m_qubits.second, m_qubit});

    // Bell measurement of Alice's two qubits.
    Simulator::Schedule(m_delay,
                        &QuantumPhyEntity::ApplyGate,
                        qphyent,
                        owner,
                        QNS_GATE_CNOT,
                        QuantumTensorData{},
                        std::vector<std::string>{m_qubits.second, m_qubits.first});
    Simulator::Schedule(m_delay,
                        &QuantumPhyEntity::ApplyGate,
                        qphyent,
                        owner,
                        QNS_GATE_H,
                        QuantumTensorData{},
                        std::vector<std::string>{m_qubits.first});
    Simulator::Schedule(m_delay, &TelepSrcApp::MeasureAndSend, this);
}

void
TelepSrcApp::MeasureAndSend()
{
    NS_LOG_FUNCTION(this);
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = m_channel->GetSrcOwner();
    const unsigned zFlag = qphyent->Measure(owner, m_qubits.first).outcome;
    const unsigned xFlag = qphyent->Measure(owner, m_qubits.second).outcome;
    SendMessage(std::to_string(zFlag) + std::to_string(xFlag));
}

NS_OBJECT_ENSURE_REGISTERED(TelepDstApp);

TypeId
TelepDstApp::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TelepDstApp")
                            .SetParent<QuantumApplication>()
                            .SetGroupName("Quantum")
                            .AddConstructor<TelepDstApp>()
                            .AddAttribute("Qubit",
                                          "Bob's half of the Bell pair.",
                                          StringValue(),
                                          MakeStringAccessor(&TelepDstApp::m_qubit),
                                          MakeStringChecker());
    return tid;
}

void
TelepDstApp::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    NS_ABORT_MSG_IF(message.size() != 2 || message.find_first_not_of("01") != std::string::npos,
                    "Malformed teleportation outcomes \"" << message << "\"");
    const bool zFlag = message[0] == '1';
    const bool xFlag = message[1] == '1';

    // The identity is applied as a gate so that both branches suffer gate noise.
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = GetOwner();
    qphyent->ApplyGate(owner, xFlag ? QNS_GATE_PX : QNS_GATE_I, {}, {m_qubit});
    qphyent->ApplyGate(owner, zFlag ? QNS_GATE_PZ : QNS_GATE_I, {}, {m_qubit});
    NotifyFinished();
}

} // namespace ns3
