/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "telep-lin-adapt-app.h"

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

NS_LOG_COMPONENT_DEFINE("TelepLinAdaptApp");

NS_OBJECT_ENSURE_REGISTERED(TelepLinAdaptApp);

TypeId
TelepLinAdaptApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TelepLinAdaptApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<TelepLinAdaptApp>()
            .AddAttribute("QChannel",
                          "The quantum channel to the next node; unset at the last node.",
                          PointerValue(),
                          MakePointerAccessor(&TelepLinAdaptApp::m_channel),
                          MakePointerChecker<QuantumChannel>())
            .AddAttribute("EPR",
                          "The Bell pair to share with the next node: the local qubit and "
                          "the qubit sent onward. Both empty at the last node.",
                          PairValue<StringValue, StringValue>(),
                          MakePairAccessor<StringValue, StringValue>(&TelepLinAdaptApp::m_epr),
                          MakePairChecker<StringValue, StringValue>())
            .AddAttribute("Input",
                          "The qubit to teleport; set at the first node only.",
                          PointerValue(),
                          MakePointerAccessor(&TelepLinAdaptApp::m_input),
                          MakePointerChecker<Qubit>())
            .AddAttribute("LocalOperationDelay",
                          "Wait between sharing the Bell pair and the local operations; it "
                          "must cover the distribution of the pair.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&TelepLinAdaptApp::m_delay),
                          MakeTimeChecker(Time(0)));
    return tid;
}

void
TelepLinAdaptApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    m_input = nullptr;
    QuantumApplication::DoDispose();
}

void
TelepLinAdaptApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    if (!m_input)
    {
        return;
    }
    // The first node of the chain starts the teleportation.
    NS_ABORT_MSG_UNLESS(m_channel, "The first node of a teleportation chain needs a QChannel");
    m_qubits.first = m_input->GetName();
    if (!m_input->GetStateVector().empty())
    {
        Simulator::ScheduleNow(&QuantumPhyEntity::GenerateQubitsPure,
                               GetQuantumPhyEntity(),
                               m_channel->GetSrcOwner(),
                               m_input->GetStateVector(),
                               std::vector<std::string>{m_qubits.first});
    }
    Teleport();
}

void
TelepLinAdaptApp::Teleport()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_UNLESS(m_channel, "The last node of a teleportation chain cannot teleport");
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = m_channel->GetSrcOwner();
    m_qubits.second = m_epr.first;
    m_qubit = m_epr.second;

    auto distributor = DynamicCast<DistributeEprSrcProtocol>(
        qphyent->GetConn2Apps(m_channel, QNS_APP_DIST_EPR).first);
    Simulator::ScheduleNow(&DistributeEprSrcProtocol::GenerateAndDistributeEpr, distributor, m_epr);

    // Rotate the two local qubits into the Bell basis; they become the flags
    // that select the X and Z corrections.
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

    if (!m_input)
    {
        // Fold the flags of the previous node into the local flags. This stands
        // in for forwarding classical outcomes, hence the noiseless owner.
        Simulator::Schedule(m_delay,
                            &QuantumPhyEntity::ApplyGate,
                            qphyent,
                            QNS_GOD_OWNER,
                            QNS_GATE_CNOT,
                            QuantumTensorData{},
                            std::vector<std::string>{m_qubits.second, m_qubitsPred.second});
        Simulator::Schedule(m_delay,
                            &QuantumPhyEntity::PartialTrace,
                            qphyent,
                            std::vector<std::string>{m_qubitsPred.second});
        Simulator::Schedule(m_delay,
                            &QuantumPhyEntity::ApplyGate,
                            qphyent,
                            QNS_GOD_OWNER,
                            QNS_GATE_CNOT,
                            QuantumTensorData{},
                            std::vector<std::string>{m_qubits.first, m_qubitsPred.first});
        Simulator::Schedule(m_delay,
                            &QuantumPhyEntity::PartialTrace,
                            qphyent,
                            std::vector<std::string>{m_qubitsPred.first});
    }

    Simulator::Schedule(m_delay, &TelepLinAdaptApp::Send, this);
}

void
TelepLinAdaptApp::Send()
{
    NS_LOG_FUNCTION(this);
    SendMessage(JoinMessage({m_qubits.first, m_qubits.second, m_qubit}));
}

void
TelepLinAdaptApp::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    const std::vector<std::string> fields = SplitMessage(message);
    NS_ABORT_MSG_IF(fields.size() != 3, "Malformed teleportation message \"" << message << "\"");
    m_qubitsPred = {fields[0], fields[1]};
    m_qubits.first = fields[2];

    if (!m_epr.first.empty() || !m_epr.second.empty())
    {
        Teleport();
        return;
    }

    // The last node applies the corrections, controlled by the flags.
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = GetOwner();
    Simulator::ScheduleNow(&QuantumPhyEntity::ApplyControlledOperation,
                           qphyent,
                           owner,
                           QNS_GATE_PX,
                           QNS_GATE_CNOT,
                           QuantumTensorData{},
                           std::vector<std::string>{m_qubitsPred.second},
                           std::vector<std::string>{m_qubits.first});
    Simulator::ScheduleNow(&QuantumPhyEntity::PartialTrace,
                           qphyent,
                           std::vector<std::string>{m_qubitsPred.second});
    Simulator::ScheduleNow(&QuantumPhyEntity::ApplyControlledOperation,
                           qphyent,
                           owner,
                           QNS_GATE_PZ,
                           QNS_GATE_CZ,
                           QuantumTensorData{},
                           std::vector<std::string>{m_qubitsPred.first},
                           std::vector<std::string>{m_qubits.first});
    Simulator::ScheduleNow(&QuantumPhyEntity::PartialTrace,
                           qphyent,
                           std::vector<std::string>{m_qubitsPred.first});
    Simulator::ScheduleNow(&QuantumPhyEntity::Contract, qphyent, std::string("ascend"));
    Simulator::ScheduleNow(&TelepLinAdaptApp::NotifyFinished, this);
}

} // namespace ns3
