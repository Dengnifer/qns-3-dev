/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distill-nested-app.h"

#include "distribute-epr-protocol.h"
#include "quantum-basis.h"
#include "quantum-channel.h"
#include "quantum-memory.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DistillNestedApp");

NS_OBJECT_ENSURE_REGISTERED(DistillNestedSrcApp);

TypeId
DistillNestedSrcApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DistillNestedSrcApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<DistillNestedSrcApp>()
            .AddAttribute("QChannel",
                          "The quantum channel from Alice to Bob.",
                          PointerValue(),
                          MakePointerAccessor(&DistillNestedSrcApp::m_channel),
                          MakePointerChecker<QuantumChannel>())
            .AddAttribute("SrcQubits",
                          "Alice's qubits, one per Bell pair; their number must be a power "
                          "of two, at least two.",
                          PointerValue(),
                          MakePointerAccessor(&DistillNestedSrcApp::m_srcQubits),
                          MakePointerChecker<QuantumMemory>())
            .AddAttribute("DstQubits",
                          "Bob's qubits, one per Bell pair.",
                          PointerValue(),
                          MakePointerAccessor(&DistillNestedSrcApp::m_dstQubits),
                          MakePointerChecker<QuantumMemory>())
            .AddAttribute("StepInterval",
                          "Time between two consecutive steps of the protocol: the "
                          "distribution of a pair or Alice's part of a round.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&DistillNestedSrcApp::m_interval),
                          MakeTimeChecker(Time(0)));
    return tid;
}

void
DistillNestedSrcApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    m_srcQubits = nullptr;
    m_dstQubits = nullptr;
    QuantumApplication::DoDispose();
}

uint32_t
DistillNestedSrcApp::GetNRoundsDone() const
{
    return m_nRoundsDone;
}

bool
DistillNestedSrcApp::GetWin() const
{
    return m_nRoundsDone > 0 && m_win;
}

void
DistillNestedSrcApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_UNLESS(m_srcQubits && m_dstQubits &&
                            m_srcQubits->GetSize() == m_dstQubits->GetSize(),
                        "SrcQubits and DstQubits must name the two qubits of the same pairs");
    const std::size_t nPairs = m_srcQubits->GetSize();
    NS_ABORT_MSG_IF(nPairs < 2 || (nPairs & (nPairs - 1)) != 0,
                    "Nested distillation needs a power of two of pairs, got " << nPairs);
    Distillate(m_srcQubits->GetQubits(), m_dstQubits->GetQubits());
}

void
DistillNestedSrcApp::Distillate(const std::vector<std::string>& srcQubits,
                                const std::vector<std::string>& dstQubits)
{
    NS_LOG_FUNCTION(this << srcQubits.size());
    if (srcQubits.size() > 2)
    {
        Distillate(GetFirstHalf(srcQubits), GetFirstHalf(dstQubits));
        Distillate(GetSecondHalf(srcQubits), GetSecondHalf(dstQubits));
    }
    else
    {
        auto distributor = DynamicCast<DistributeEprSrcProtocol>(
            GetQuantumPhyEntity()->GetConn2Apps(m_channel, QNS_APP_DIST_EPR).first);
        for (std::size_t i = 0; i < 2; ++i)
        {
            Simulator::Schedule(m_occupied,
                                &DistributeEprSrcProtocol::GenerateAndDistributeEpr,
                                distributor,
                                std::pair<std::string, std::string>{srcQubits[i], dstQubits[i]});
            m_occupied += m_interval;
        }
    }
    Simulator::Schedule(m_occupied,
                        &DistillNestedSrcApp::DistillateOnce,
                        this,
                        srcQubits,
                        dstQubits);
    m_occupied += m_interval;
}

void
DistillNestedSrcApp::DistillateOnce(const std::vector<std::string>& srcQubits,
                                    const std::vector<std::string>& dstQubits)
{
    NS_LOG_FUNCTION(this << srcQubits.size());
    // Each half has been distilled into its first pair.
    const std::size_t half = srcQubits.size() / 2;
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = GetOwner();
    qphyent->ApplyGate(owner, QNS_GATE_CNOT, {}, {srcQubits[half], srcQubits[0]});
    const unsigned outcome = qphyent->Measure(owner, srcQubits[half]).outcome;
    SendMessage(JoinMessage({std::to_string(outcome), dstQubits[0], dstQubits[half]}));
}

void
DistillNestedSrcApp::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    NS_ABORT_MSG_IF(message != "win" && message != "lose",
                    "Malformed distillation verdict \"" << message << "\"");
    ++m_nRoundsDone;
    m_win = m_win && message == "win";
    // Distilling n pairs into one takes n - 1 rounds.
    if (m_nRoundsDone + 1 == m_srcQubits->GetSize())
    {
        NotifyFinished();
    }
}

NS_OBJECT_ENSURE_REGISTERED(DistillNestedDstApp);

TypeId
DistillNestedDstApp::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DistillNestedDstApp")
                            .SetParent<QuantumApplication>()
                            .SetGroupName("Quantum")
                            .AddConstructor<DistillNestedDstApp>();
    return tid;
}

uint32_t
DistillNestedDstApp::GetNRoundsDone() const
{
    return m_nRoundsDone;
}

bool
DistillNestedDstApp::GetWin() const
{
    return m_nRoundsDone > 0 && m_win;
}

void
DistillNestedDstApp::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    const std::vector<std::string> fields = SplitMessage(message);
    NS_ABORT_MSG_IF(fields.size() != 3 || (fields[0] != "0" && fields[0] != "1"),
                    "Malformed distillation message \"" << message << "\"");
    const unsigned outcomeAlice = fields[0] == "1" ? 1 : 0;
    const std::string& goal = fields[1];
    const std::string& meas = fields[2];

    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = GetOwner();
    qphyent->ApplyGate(owner, QNS_GATE_CNOT, {}, {meas, goal});
    const unsigned outcomeBob = qphyent->Measure(owner, meas).outcome;

    const bool win = outcomeAlice == outcomeBob;
    ++m_nRoundsDone;
    m_win = m_win && win;
    SendMessageTo(win ? "win" : "lose", from);
}

} // namespace ns3
