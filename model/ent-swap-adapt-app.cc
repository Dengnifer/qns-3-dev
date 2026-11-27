/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ent-swap-adapt-app.h"

#include "quantum-memory.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("EntSwapAdaptApp");

NS_OBJECT_ENSURE_REGISTERED(EntSwapAdaptApp);

TypeId
EntSwapAdaptApp::GetTypeId()
{
    static TypeId tid = TypeId("ns3::EntSwapAdaptApp")
                            .SetParent<QuantumApplication>()
                            .SetGroupName("Quantum")
                            .AddConstructor<EntSwapAdaptApp>()
                            .AddAttribute("QubitsFormer",
                                          "Per node, the qubit entangled with the previous node.",
                                          PointerValue(),
                                          MakePointerAccessor(&EntSwapAdaptApp::m_qubitsFormer),
                                          MakePointerChecker<QuantumMemory>())
                            .AddAttribute("QubitsLatter",
                                          "Per node, the qubit entangled with the next node.",
                                          PointerValue(),
                                          MakePointerAccessor(&EntSwapAdaptApp::m_qubitsLatter),
                                          MakePointerChecker<QuantumMemory>());
    return tid;
}

void
EntSwapAdaptApp::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_qubitsFormer = nullptr;
    m_qubitsLatter = nullptr;
    QuantumApplication::DoDispose();
}

void
EntSwapAdaptApp::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
    Simulator::ScheduleNow(&EntSwapAdaptApp::EntanglementSwapping, this);
}

void
EntSwapAdaptApp::EntanglementSwapping()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_UNLESS(m_qubitsFormer && m_qubitsLatter &&
                            m_qubitsFormer->GetSize() == m_qubitsLatter->GetSize() &&
                            m_qubitsFormer->GetSize() >= 2,
                        "QubitsFormer and QubitsLatter must name the qubits of the same nodes");
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    const std::size_t nNodes = m_qubitsFormer->GetSize();
    const std::string lastQubit = m_qubitsFormer->GetQubit(nNodes - 1);

    // The ancillas accumulate the parity of the X and Z flags of all repeaters.
    const std::string flagX = qphyent->AllocAncillaQubit();
    const std::string flagZ = qphyent->AllocAncillaQubit();
    Simulator::ScheduleNow(&QuantumPhyEntity::GenerateQubitsPure,
                           qphyent,
                           QNS_GOD_OWNER,
                           QNS_KET_0,
                           std::vector<std::string>{flagX});
    Simulator::ScheduleNow(&QuantumPhyEntity::GenerateQubitsPure,
                           qphyent,
                           QNS_GOD_OWNER,
                           QNS_KET_0,
                           std::vector<std::string>{flagZ});

    for (std::size_t rank = 1; rank + 1 < nNodes; ++rank)
    {
        const std::string former = m_qubitsFormer->GetQubit(rank);
        const std::string latter = m_qubitsLatter->GetQubit(rank);

        // Rotate the two qubits of the repeater into the Bell basis.
        Simulator::ScheduleNow(&QuantumPhyEntity::ApplyGate,
                               qphyent,
                               QNS_GOD_OWNER,
                               QNS_GATE_CNOT,
                               QuantumTensorData{},
                               std::vector<std::string>{latter, former});
        Simulator::ScheduleNow(&QuantumPhyEntity::ApplyGate,
                               qphyent,
                               QNS_GOD_OWNER,
                               QNS_GATE_H,
                               QuantumTensorData{},
                               std::vector<std::string>{former});

        // Fold its flags into the ancillas, then discard its qubits.
        Simulator::ScheduleNow(&QuantumPhyEntity::ApplyGate,
                               qphyent,
                               QNS_GOD_OWNER,
                               QNS_GATE_CNOT,
                               QuantumTensorData{},
                               std::vector<std::string>{flagX, latter});
        Simulator::ScheduleNow(&QuantumPhyEntity::PartialTrace,
                               qphyent,
                               std::vector<std::string>{latter});
        Simulator::ScheduleNow(&QuantumPhyEntity::ApplyGate,
                               qphyent,
                               QNS_GOD_OWNER,
                               QNS_GATE_CNOT,
                               QuantumTensorData{},
                               std::vector<std::string>{flagZ, former});
        Simulator::ScheduleNow(&QuantumPhyEntity::PartialTrace,
                               qphyent,
                               std::vector<std::string>{former});
    }

    // The corrections of the last node, controlled by the ancillas.
    const std::string owner = GetOwner();
    Simulator::ScheduleNow(&QuantumPhyEntity::ApplyControlledOperation,
                           qphyent,
                           owner,
                           QNS_GATE_PX,
                           QNS_GATE_CNOT,
                           QuantumTensorData{},
                           std::vector<std::string>{flagX},
                           std::vector<std::string>{lastQubit});
    Simulator::ScheduleNow(&QuantumPhyEntity::PartialTrace,
                           qphyent,
                           std::vector<std::string>{flagX});
    Simulator::ScheduleNow(&QuantumPhyEntity::ApplyControlledOperation,
                           qphyent,
                           owner,
                           QNS_GATE_PZ,
                           QNS_GATE_CZ,
                           QuantumTensorData{},
                           std::vector<std::string>{flagZ},
                           std::vector<std::string>{lastQubit});
    Simulator::ScheduleNow(&QuantumPhyEntity::PartialTrace,
                           qphyent,
                           std::vector<std::string>{flagZ});

    Simulator::ScheduleNow(&QuantumPhyEntity::Contract, qphyent, std::string("auto"));
    Simulator::ScheduleNow(&EntSwapAdaptApp::NotifyFinished, this);
}

} // namespace ns3
