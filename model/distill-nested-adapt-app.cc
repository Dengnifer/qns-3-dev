/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distill-nested-adapt-app.h"

#include "distribute-epr-protocol.h"
#include "quantum-channel.h"
#include "quantum-memory.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DistillNestedAdaptApp");

NS_OBJECT_ENSURE_REGISTERED(DistillNestedAdaptApp);

TypeId
DistillNestedAdaptApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DistillNestedAdaptApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<DistillNestedAdaptApp>()
            .AddAttribute("QChannel",
                          "The quantum channel from Alice to Bob.",
                          PointerValue(),
                          MakePointerAccessor(&DistillNestedAdaptApp::m_channel),
                          MakePointerChecker<QuantumChannel>())
            .AddAttribute("SrcQubits",
                          "Alice's qubits, one per Bell pair; their number must be a power "
                          "of two, at least two.",
                          PointerValue(),
                          MakePointerAccessor(&DistillNestedAdaptApp::m_srcQubits),
                          MakePointerChecker<QuantumMemory>())
            .AddAttribute("DstQubits",
                          "Bob's qubits, one per Bell pair.",
                          PointerValue(),
                          MakePointerAccessor(&DistillNestedAdaptApp::m_dstQubits),
                          MakePointerChecker<QuantumMemory>())
            .AddAttribute("FlagQubit",
                          "Name of the qubit that records whether every round succeeded.",
                          StringValue("Flag"),
                          MakeStringAccessor(&DistillNestedAdaptApp::m_flagQubit),
                          MakeStringChecker())
            .AddAttribute("EprInterval",
                          "Time allotted to the distribution of one Bell pair. Two such "
                          "intervals must cover the classical message that announces a "
                          "pair, including the address resolution before the first one.",
                          TimeValue(MilliSeconds(10)),
                          MakeTimeAccessor(&DistillNestedAdaptApp::m_interval),
                          MakeTimeChecker(Time(0)));
    return tid;
}

void
DistillNestedAdaptApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    m_srcQubits = nullptr;
    m_dstQubits = nullptr;
    QuantumApplication::DoDispose();
}

void
DistillNestedAdaptApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_UNLESS(m_srcQubits && m_dstQubits &&
                            m_srcQubits->GetSize() == m_dstQubits->GetSize(),
                        "SrcQubits and DstQubits must name the two qubits of the same pairs");
    const std::size_t nPairs = m_srcQubits->GetSize();
    NS_ABORT_MSG_IF(nPairs < 2 || (nPairs & (nPairs - 1)) != 0,
                    "Nested distillation needs a power of two of pairs, got " << nPairs);

    // The flag starts as 1, "every round so far succeeded".
    Simulator::Schedule(m_occupied,
                        &QuantumPhyEntity::GenerateQubitsPure,
                        GetQuantumPhyEntity(),
                        GetOwner(),
                        QNS_KET_1,
                        std::vector<std::string>{m_flagQubit});
    Simulator::Schedule(m_occupied,
                        &DistillNestedAdaptApp::Distillate,
                        this,
                        m_srcQubits->GetQubits(),
                        m_dstQubits->GetQubits());
}

void
DistillNestedAdaptApp::Distillate(const std::vector<std::string>& srcQubits,
                                  const std::vector<std::string>& dstQubits)
{
    NS_LOG_FUNCTION(this << srcQubits.size());
    if (srcQubits.size() > 2)
    {
        Distillate(GetFirstHalf(srcQubits), GetFirstHalf(dstQubits));
        Distillate(GetSecondHalf(srcQubits), GetSecondHalf(dstQubits));
    }
    // The rounds are scheduled in post-order of the distillation tree, which is
    // the order the "distill" contraction optimizer expects.
    Simulator::Schedule(m_occupied,
                        &DistillNestedAdaptApp::DistillateOnce,
                        this,
                        srcQubits,
                        dstQubits);
}

void
DistillNestedAdaptApp::DistillateOnce(const std::vector<std::string>& srcQubits,
                                      const std::vector<std::string>& dstQubits)
{
    NS_LOG_FUNCTION(this << srcQubits.size());
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::string owner = GetOwner();
    const std::size_t half = srcQubits.size() / 2;
    const std::pair<std::string, std::string> goal{srcQubits[0], dstQubits[0]};
    const std::pair<std::string, std::string> meas{srcQubits[half], dstQubits[half]};

    // Each round is one subcircuit for the "distill" contraction optimizer.
    Simulator::Schedule(m_occupied, &QuantumPhyEntity::Checkpoint, qphyent);

    if (srcQubits.size() == 2)
    {
        auto distributor = DynamicCast<DistributeEprSrcProtocol>(
            qphyent->GetConn2Apps(m_channel, QNS_APP_DIST_EPR).first);
        for (const auto& epr : {goal, meas})
        {
            Simulator::Schedule(m_occupied,
                                &DistributeEprSrcProtocol::GenerateAndDistributeEpr,
                                distributor,
                                epr);
            m_occupied += m_interval;
        }
    }

    auto applyGate = [&](const std::string& gateOwner,
                         const std::string& gate,
                         const std::vector<std::string>& qubits) {
        Simulator::Schedule(m_occupied,
                            &QuantumPhyEntity::ApplyGate,
                            qphyent,
                            gateOwner,
                            gate,
                            QuantumTensorData{},
                            qubits);
    };
    auto traceOut = [&](const std::string& qubit) {
        Simulator::Schedule(m_occupied,
                            &QuantumPhyEntity::PartialTrace,
                            qphyent,
                            std::vector<std::string>{qubit});
    };

    if (srcQubits.size() == 2)
    {
        Simulator::Schedule(m_occupied,
                            &DistillNestedAdaptApp::RequireArrived,
                            this,
                            std::vector<std::string>{goal.second, meas.second});
    }

    // The local gates of the round, by Alice and by Bob.
    applyGate(owner, QNS_GATE_CNOT, {meas.first, goal.first});
    applyGate(m_channel->GetDstOwner(), QNS_GATE_CNOT, {meas.second, goal.second});

    // Bob's qubit takes the parity of the two would-be outcomes, then its
    // negation: 1 means that the round succeeds.
    applyGate(QNS_GOD_OWNER, QNS_GATE_CNOT, {meas.second, meas.first});
    traceOut(meas.first);
    applyGate(QNS_GOD_OWNER, QNS_GATE_PX, {meas.second});

    // flag := flag AND success, computed into an ancilla and swapped back.
    const std::string ancilla = qphyent->AllocAncillaQubit();
    Simulator::Schedule(m_occupied,
                        &QuantumPhyEntity::GenerateQubitsPure,
                        qphyent,
                        owner,
                        QNS_KET_0,
                        std::vector<std::string>{ancilla});
    applyGate(QNS_GOD_OWNER, QNS_GATE_TOFF, {ancilla, meas.second, m_flagQubit});
    traceOut(meas.second);
    applyGate(QNS_GOD_OWNER, QNS_GATE_SWAP, {ancilla, m_flagQubit});
    traceOut(ancilla);

    if (srcQubits.size() == m_srcQubits->GetSize())
    {
        // This was the last round.
        Simulator::Schedule(m_occupied,
                            &QuantumPhyEntity::Contract,
                            qphyent,
                            std::string("distill"));
        Simulator::Schedule(m_occupied, &DistillNestedAdaptApp::NotifyFinished, this);
    }
}

void
DistillNestedAdaptApp::RequireArrived(const std::vector<std::string>& qubits) const
{
    NS_ABORT_MSG_UNLESS(GetQuantumPhyEntity()->CheckOwned(m_channel->GetDstOwner(), qubits),
                        "The Bell pairs have not reached "
                            << m_channel->GetDstOwner()
                            << " in time; increase the EprInterval attribute, now "
                            << m_interval.As(Time::MS));
}

} // namespace ns3
