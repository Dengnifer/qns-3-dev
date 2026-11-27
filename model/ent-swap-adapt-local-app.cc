/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ent-swap-adapt-local-app.h"

#include "quantum-memory.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("EntSwapAdaptLocalApp");

NS_OBJECT_ENSURE_REGISTERED(EntSwapAdaptLocalApp);

TypeId
EntSwapAdaptLocalApp::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::EntSwapAdaptLocalApp")
            .SetParent<QuantumApplication>()
            .SetGroupName("Quantum")
            .AddConstructor<EntSwapAdaptLocalApp>()
            .AddAttribute("QubitsFormer",
                          "Per node, the qubit entangled with the previous node.",
                          PointerValue(),
                          MakePointerAccessor(&EntSwapAdaptLocalApp::m_qubitsFormer),
                          MakePointerChecker<QuantumMemory>())
            .AddAttribute("QubitsLatter",
                          "Per node, the qubit entangled with the next node.",
                          PointerValue(),
                          MakePointerAccessor(&EntSwapAdaptLocalApp::m_qubitsLatter),
                          MakePointerChecker<QuantumMemory>())
            .AddAttribute("EprFidelity",
                          "Fidelity of each Bell pair of the chain.",
                          DoubleValue(0.95),
                          MakeDoubleAccessor(&EntSwapAdaptLocalApp::m_eprFidelity),
                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

void
EntSwapAdaptLocalApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_qubitsFormer = nullptr;
    m_qubitsLatter = nullptr;
    QuantumApplication::DoDispose();
}

void
EntSwapAdaptLocalApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    Simulator::ScheduleNow(&EntSwapAdaptLocalApp::EntanglementSwapping, this);
}

void
EntSwapAdaptLocalApp::EntanglementSwapping()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_UNLESS(m_qubitsFormer && m_qubitsLatter &&
                            m_qubitsFormer->GetSize() == m_qubitsLatter->GetSize() &&
                            m_qubitsFormer->GetSize() >= 3,
                        "QubitsFormer and QubitsLatter must name the qubits of the same nodes, "
                        "at least three");
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::size_t nNodes = m_qubitsFormer->GetSize();
    const QuantumTensorData pair = MakeWernerState(m_eprFidelity);

    // Share a pair between two neighbors.
    auto generatePair = [&](std::size_t rank) {
        Simulator::ScheduleNow(&QuantumPhyEntity::GenerateQubitsMixed,
                               qphyent,
                               QNS_GOD_OWNER,
                               pair,
                               std::vector<std::string>{m_qubitsLatter->GetQubit(rank),
                                                        m_qubitsFormer->GetQubit(rank + 1)});
    };
    // Apply a noiseless gate.
    auto applyGate = [&](const std::string& gate, const std::vector<std::string>& qubits) {
        Simulator::ScheduleNow(&QuantumPhyEntity::ApplyGate,
                               qphyent,
                               QNS_GOD_OWNER,
                               gate,
                               QuantumTensorData{},
                               qubits);
    };
    auto traceOut = [&](const std::string& qubit) {
        Simulator::ScheduleNow(&QuantumPhyEntity::PartialTrace,
                               qphyent,
                               std::vector<std::string>{qubit});
    };

    generatePair(0);
    generatePair(1);
    // Rotate the two qubits of node 1 into the Bell basis; they are its flags.
    applyGate(QNS_GATE_CNOT, {m_qubitsLatter->GetQubit(1), m_qubitsFormer->GetQubit(1)});
    applyGate(QNS_GATE_H, {m_qubitsFormer->GetQubit(1)});

    for (std::size_t rank = 2; rank + 1 < nNodes; ++rank)
    {
        generatePair(rank);
        const std::string former = m_qubitsFormer->GetQubit(rank);
        const std::string latter = m_qubitsLatter->GetQubit(rank);
        applyGate(QNS_GATE_CNOT, {latter, former});
        applyGate(QNS_GATE_H, {former});

        // Fold the flags of the previous node into the flags of this node,
        // then discard the qubits of the previous node.
        const std::string formerPred = m_qubitsFormer->GetQubit(rank - 1);
        const std::string latterPred = m_qubitsLatter->GetQubit(rank - 1);
        applyGate(QNS_GATE_CNOT, {latter, latterPred});
        traceOut(latterPred);
        applyGate(QNS_GATE_CNOT, {former, formerPred});
        traceOut(formerPred);
    }

    // The corrections of the last node, controlled by the flags of the last repeater.
    const std::string owner = GetOwner();
    const std::string lastQubit = m_qubitsFormer->GetQubit(nNodes - 1);
    const std::string flagX = m_qubitsLatter->GetQubit(nNodes - 2);
    const std::string flagZ = m_qubitsFormer->GetQubit(nNodes - 2);
    Simulator::ScheduleNow(&QuantumPhyEntity::ApplyControlledOperation,
                           qphyent,
                           owner,
                           QNS_GATE_PX,
                           QNS_GATE_CNOT,
                           QuantumTensorData{},
                           std::vector<std::string>{flagX},
                           std::vector<std::string>{lastQubit});
    traceOut(flagX);
    Simulator::ScheduleNow(&QuantumPhyEntity::ApplyControlledOperation,
                           qphyent,
                           owner,
                           QNS_GATE_PZ,
                           QNS_GATE_CZ,
                           QuantumTensorData{},
                           std::vector<std::string>{flagZ},
                           std::vector<std::string>{lastQubit});
    traceOut(flagZ);

    Simulator::ScheduleNow(&QuantumPhyEntity::Contract, qphyent, std::string("auto"));
    Simulator::ScheduleNow(&EntSwapAdaptLocalApp::NotifyFinished, this);
}

} // namespace ns3
