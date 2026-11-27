/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distill-app.h"

#include "distribute-epr-protocol.h"
#include "quantum-channel.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/pair.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DistillApp");

NS_OBJECT_ENSURE_REGISTERED(DistillSrcApp);

TypeId
DistillSrcApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DistillSrcApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<DistillSrcApp>()
            .AddAttribute("QChannel",
                          "The quantum channel from Alice to Bob.",
                          PointerValue(),
                          MakePointerAccessor(&DistillSrcApp::m_channel),
                          MakePointerChecker<QuantumChannel>())
            .AddAttribute("EPRGoal",
                          "The pair to keep: Alice's qubit and Bob's qubit.",
                          PairValue<StringValue, StringValue>(),
                          MakePairAccessor<StringValue, StringValue>(&DistillSrcApp::m_eprGoal),
                          MakePairChecker<StringValue, StringValue>())
            .AddAttribute("EPRMeas",
                          "The pair to measure: Alice's qubit and Bob's qubit.",
                          PairValue<StringValue, StringValue>(),
                          MakePairAccessor<StringValue, StringValue>(&DistillSrcApp::m_eprMeas),
                          MakePairChecker<StringValue, StringValue>())
            .AddAttribute("StepInterval",
                          "Time between the distribution of the first pair, the distribution "
                          "of the second pair, and Alice's measurement.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&DistillSrcApp::m_interval),
                          MakeTimeChecker(Time(0)));
    return tid;
}

void
DistillSrcApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    QuantumApplication::DoDispose();
}

bool
DistillSrcApp::IsDone() const
{
    return m_done;
}

bool
DistillSrcApp::GetWin() const
{
    return m_done && m_win;
}

void
DistillSrcApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    Ptr<DistributeEprProtocol> distributor = GetEprProtocol();
    Simulator::ScheduleNow(&DistributeEprProtocol::DistributeEpr,
                           distributor,
                           m_channel->GetDstOwner(),
                           m_eprGoal);
    Simulator::Schedule(m_interval,
                        &DistributeEprProtocol::DistributeEpr,
                        distributor,
                        m_channel->GetDstOwner(),
                        m_eprMeas);
    Simulator::Schedule(2 * m_interval, &DistillSrcApp::MeasureAndSend, this);
}

void
DistillSrcApp::MeasureAndSend()
{
    NS_LOG_FUNCTION(this);
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = GetOwner();
    qphyent->ApplyGate(owner, QNS_GATE_CNOT, {}, {m_eprMeas.first, m_eprGoal.first});
    const unsigned outcome = qphyent->Measure(owner, m_eprMeas.first).outcome;
    SendMessage(std::to_string(outcome));
}

void
DistillSrcApp::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    NS_ABORT_MSG_IF(message != "win" && message != "lose",
                    "Malformed distillation verdict \"" << message << "\"");
    m_done = true;
    m_win = message == "win";
    NotifyFinished();
}

NS_OBJECT_ENSURE_REGISTERED(DistillDstApp);

TypeId
DistillDstApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DistillDstApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<DistillDstApp>()
            .AddAttribute("Qubits",
                          "Bob's qubit of the pair to keep and his qubit of the pair to measure.",
                          PairValue<StringValue, StringValue>(),
                          MakePairAccessor<StringValue, StringValue>(&DistillDstApp::m_qubits),
                          MakePairChecker<StringValue, StringValue>());
    return tid;
}

bool
DistillDstApp::IsDone() const
{
    return m_done;
}

bool
DistillDstApp::GetWin() const
{
    return m_done && m_win;
}

void
DistillDstApp::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    NS_ABORT_MSG_IF(message != "0" && message != "1",
                    "Malformed distillation outcome \"" << message << "\"");
    const unsigned outcomeAlice = message == "1" ? 1 : 0;

    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = GetOwner();
    qphyent->ApplyGate(owner, QNS_GATE_CNOT, {}, {m_qubits.second, m_qubits.first});
    const unsigned outcomeBob = qphyent->Measure(owner, m_qubits.second).outcome;

    m_done = true;
    m_win = outcomeAlice == outcomeBob;
    SendMessageTo(m_win ? "win" : "lose", from);
    NotifyFinished();
}

} // namespace ns3
