/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ent-swap-app.h"

#include "quantum-channel.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/pair.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("EntSwapApp");

NS_OBJECT_ENSURE_REGISTERED(EntSwapSrcApp);

TypeId
EntSwapSrcApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::EntSwapSrcApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<EntSwapSrcApp>()
            .AddAttribute("QChannel",
                          "The quantum channel from this node to the last node of the chain.",
                          PointerValue(),
                          MakePointerAccessor(&EntSwapSrcApp::m_channel),
                          MakePointerChecker<QuantumChannel>())
            .AddAttribute("Qubits",
                          "The qubit entangled with the previous node and the qubit "
                          "entangled with the next node.",
                          PairValue<StringValue, StringValue>(),
                          MakePairAccessor<StringValue, StringValue>(&EntSwapSrcApp::m_qubits),
                          MakePairChecker<StringValue, StringValue>());
    return tid;
}

void
EntSwapSrcApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    QuantumApplication::DoDispose();
}

void
EntSwapSrcApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    Simulator::ScheduleNow(&EntSwapSrcApp::MeasureAndSend, this);
}

void
EntSwapSrcApp::MeasureAndSend()
{
    NS_LOG_FUNCTION(this);
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = m_channel->GetSrcOwner();

    // The rotation into the Bell basis is modeled as noiseless.
    qphyent->ApplyGate(QNS_GOD_OWNER, QNS_GATE_CNOT, {}, {m_qubits.second, m_qubits.first});
    qphyent->ApplyGate(QNS_GOD_OWNER, QNS_GATE_H, {}, {m_qubits.first});
    const unsigned zFlag = qphyent->Measure(owner, m_qubits.first).outcome;
    const unsigned xFlag = qphyent->Measure(owner, m_qubits.second).outcome;
    Simulator::ScheduleNow(&QuantumPhyEntity::PartialTrace,
                           qphyent,
                           std::vector<std::string>{m_qubits.first, m_qubits.second});

    SendMessage(std::to_string(zFlag) + std::to_string(xFlag));
}

NS_OBJECT_ENSURE_REGISTERED(EntSwapDstApp);

TypeId
EntSwapDstApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::EntSwapDstApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<EntSwapDstApp>()
            .AddAttribute("Qubit",
                          "The qubit of this node, entangled with the previous node.",
                          StringValue(),
                          MakeStringAccessor(&EntSwapDstApp::m_qubit),
                          MakeStringChecker())
            .AddAttribute("Count",
                          "Number of repeater nodes that report their outcomes.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&EntSwapDstApp::m_count),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

void
EntSwapDstApp::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    NS_ABORT_MSG_IF(message.size() != 2 || message.find_first_not_of("01") != std::string::npos,
                    "Malformed swapping outcomes \"" << message << "\"");
    NS_ABORT_MSG_IF(m_count == 0, "More repeater nodes reported than the Count attribute allows");
    m_flagZ ^= message[0] == '1';
    m_flagX ^= message[1] == '1';
    if (--m_count > 0)
    {
        return;
    }

    // The identity is applied as a gate so that both branches suffer gate noise.
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = GetOwner();
    qphyent->ApplyGate(owner, m_flagX ? QNS_GATE_PX : QNS_GATE_I, {}, {m_qubit});
    qphyent->ApplyGate(owner, m_flagZ ? QNS_GATE_PZ : QNS_GATE_I, {}, {m_qubit});
    NotifyFinished();
}

} // namespace ns3
