/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-phy-entity.h"

#include "quantum-channel.h"
#include "quantum-error-model.h"
#include "quantum-memory.h"
#include "quantum-node.h"
#include "quantum-operation.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumPhyEntity");

NS_OBJECT_ENSURE_REGISTERED(QuantumPhyEntity);

TypeId
QuantumPhyEntity::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::QuantumPhyEntity")
            .SetParent<Object>()
            .SetGroupName("Quantum")
            .AddConstructor<QuantumPhyEntity>()
            .AddAttribute("DefaultTimeModel",
                          "Memory noise of the qubits of an owner without a model of its own. "
                          "A TimeModel with default attributes if left unset.",
                          PointerValue(),
                          MakePointerAccessor(&QuantumPhyEntity::m_defaultTimeModel),
                          MakePointerChecker<QuantumErrorModel>())
            .AddAttribute("DefaultDephaseModel",
                          "Noise of a gate without a model of its own. "
                          "A DephaseModel with default attributes if left unset.",
                          PointerValue(),
                          MakePointerAccessor(&QuantumPhyEntity::m_defaultDephaseModel),
                          MakePointerChecker<QuantumErrorModel>())
            .AddAttribute("DefaultDepolarModel",
                          "Noise of a quantum channel without a model of its own. "
                          "A DepolarModel with default attributes if left unset.",
                          PointerValue(),
                          MakePointerAccessor(&QuantumPhyEntity::m_defaultDepolarModel),
                          MakePointerChecker<QuantumErrorModel>())
            .AddTraceSource("Measure",
                            "A qubit has been measured.",
                            MakeTraceSourceAccessor(&QuantumPhyEntity::m_measureTrace),
                            "ns3::QuantumPhyEntity::MeasureTracedCallback");
    return tid;
}

QuantumPhyEntity::QuantumPhyEntity()
    : m_engine(std::make_unique<QuantumNetworkSimulator>()),
      m_godMemory(CreateObject<QuantumMemory>()),
      m_random(CreateObject<UniformRandomVariable>())
{
    NS_LOG_FUNCTION(this);
}

QuantumPhyEntity::~QuantumPhyEntity()
{
    NS_LOG_FUNCTION(this);
}

void
QuantumPhyEntity::NotifyConstructionCompleted()
{
    NS_LOG_FUNCTION(this);
    Object::NotifyConstructionCompleted();
    // Attribute construction has run: fill in the models the user did not set.
    if (!m_defaultTimeModel)
    {
        m_defaultTimeModel = CreateObject<TimeModel>();
    }
    if (!m_defaultDephaseModel)
    {
        m_defaultDephaseModel = CreateObject<DephaseModel>();
    }
    if (!m_defaultDepolarModel)
    {
        m_defaultDepolarModel = CreateObject<DepolarModel>();
    }
}

void
QuantumPhyEntity::DoDispose()
{
    NS_LOG_FUNCTION(this);
    // Release the objects that point back to this entity. The engine stays, so
    // that results can still be read after Simulator::Destroy().
    m_nodes.clear();
    Object::DoDispose();
}

Ptr<QuantumNode>
QuantumPhyEntity::AddNode(Ptr<Node> node, const std::string& owner)
{
    NS_LOG_FUNCTION(this << node << owner);
    NS_ABORT_MSG_IF(owner.empty(), "A quantum node needs an owner name");
    NS_ABORT_MSG_IF(owner == QNS_GOD_OWNER, "The owner name " << owner << " is reserved");
    NS_ABORT_MSG_IF(m_nodes.count(owner), "The owner name " << owner << " is already in use");
    NS_ABORT_MSG_IF(node->GetObject<QuantumNode>(), "The node is already a quantum node");

    Ptr<QuantumNode> quantumNode = CreateObject<QuantumNode>(Ptr<QuantumPhyEntity>(this), owner);
    node->AggregateObject(quantumNode);
    m_nodes[owner] = quantumNode;
    m_owners.push_back(owner);
    return quantumNode;
}

Ptr<QuantumNode>
QuantumPhyEntity::GetQuantumNode(const std::string& owner) const
{
    auto it = m_nodes.find(owner);
    NS_ABORT_MSG_IF(it == m_nodes.end(), "No quantum node is owned by " << owner);
    return it->second;
}

std::vector<std::string>
QuantumPhyEntity::GetOwners() const
{
    return m_owners;
}

Ptr<QuantumMemory>
QuantumPhyEntity::GetMemory(const std::string& owner) const
{
    return owner == QNS_GOD_OWNER ? m_godMemory : GetQuantumNode(owner)->GetQuantumMemory();
}

void
QuantumPhyEntity::RegisterQubits(const std::string& owner, const std::vector<std::string>& qubits)
{
    Ptr<QuantumMemory> memory = GetMemory(owner);
    auto it = m_ownerModel.find(owner);
    Ptr<QuantumErrorModel> model = it == m_ownerModel.end() ? m_defaultTimeModel : it->second;
    for (const auto& qubit : qubits)
    {
        memory->AddQubit(qubit);
        m_qubitModel[qubit] = model;
        m_qubitTime[qubit] = Simulator::Now();
    }
}

void
QuantumPhyEntity::GenerateQubitsPure(const std::string& owner,
                                     const QuantumTensorData& data,
                                     const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << owner << qubits.size());
    m_engine->GenerateQubitsPure(owner, data, qubits);
    RegisterQubits(owner, qubits);
}

void
QuantumPhyEntity::GenerateQubitsMixed(const std::string& owner,
                                      const QuantumTensorData& data,
                                      const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << owner << qubits.size());
    m_engine->GenerateQubitsMixed(owner, data, qubits);
    RegisterQubits(owner, qubits);
}

void
QuantumPhyEntity::ApplyGate(const std::string& owner,
                            const std::string& gate,
                            const QuantumTensorData& data,
                            const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << owner << gate << qubits.size());
    NS_ABORT_MSG_UNLESS(CheckOwned(owner, qubits),
                        owner << " applies " << gate << " to a qubit it does not own");
    for (const auto& qubit : qubits)
    {
        ApplyMemoryNoise(qubit);
    }
    m_engine->ApplyGate(owner, gate, data, qubits);
    if (owner != QNS_GOD_OWNER)
    {
        ApplyGateNoise(owner, gate, qubits);
    }
}

void
QuantumPhyEntity::ApplyOperation(const QuantumOperation& operation,
                                 const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << qubits.size());
    m_engine->ApplyOperation(operation, qubits);
    for (const auto& qubit : qubits)
    {
        m_qubitTime[qubit] = Simulator::Now();
    }
}

void
QuantumPhyEntity::ApplyControlledOperation(const std::string& origOwner,
                                           const std::string& origGate,
                                           const std::string& gate,
                                           const QuantumTensorData& data,
                                           const std::vector<std::string>& controlQubits,
                                           const std::vector<std::string>& targetQubits)
{
    NS_LOG_FUNCTION(this << origOwner << origGate << gate);
    // A controlled gate takes its targets first; see the standard gates.
    std::vector<std::string> qubits = targetQubits;
    qubits.insert(qubits.end(), controlQubits.begin(), controlQubits.end());
    for (const auto& qubit : qubits)
    {
        ApplyMemoryNoise(qubit);
    }
    m_engine->ApplyGate(QNS_GOD_OWNER, gate, data, qubits);
    if (origOwner != QNS_GOD_OWNER)
    {
        ApplyGateNoise(origOwner, origGate, targetQubits);
    }
}

QuantumMeasurement
QuantumPhyEntity::Measure(const std::string& owner, const std::string& qubit)
{
    NS_LOG_FUNCTION(this << owner << qubit);
    NS_ABORT_MSG_UNLESS(CheckOwned(owner, {qubit}),
                        owner << " measures the qubit " << qubit << " it does not own");
    // The outcome depends on the whole state, so bring every qubit up to date.
    for (const auto& valid : m_engine->GetValidQubits())
    {
        ApplyMemoryNoise(valid);
    }
    const QuantumMeasurement result = m_engine->Measure(owner, qubit, m_random->GetValue());
    m_measureTrace(owner, qubit, result.outcome, result.distribution[result.outcome]);
    return result;
}

double
QuantumPhyEntity::PostSelect(const std::string& owner, const std::string& qubit, unsigned outcome)
{
    NS_LOG_FUNCTION(this << owner << qubit << outcome);
    NS_ABORT_MSG_UNLESS(CheckOwned(owner, {qubit}),
                        owner << " post-selects the qubit " << qubit << " it does not own");
    for (const auto& valid : m_engine->GetValidQubits())
    {
        ApplyMemoryNoise(valid);
    }
    return m_engine->PostSelect(owner, qubit, outcome);
}

QuantumTensorData
QuantumPhyEntity::PeekDensityMatrix(const std::string& owner,
                                    const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << owner << qubits.size());
    NS_ABORT_MSG_UNLESS(CheckOwned(owner, qubits), owner << " peeks at a qubit it does not own");
    return m_engine->PeekDensityMatrix(owner, qubits);
}

void
QuantumPhyEntity::PartialTrace(const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << qubits.size());
    for (const auto& qubit : qubits)
    {
        ApplyMemoryNoise(qubit);
    }
    m_engine->PartialTrace(qubits);
}

QuantumTensorData
QuantumPhyEntity::Contract(const std::string& optimizer)
{
    NS_LOG_FUNCTION(this << optimizer);
    return m_engine->Contract(optimizer);
}

double
QuantumPhyEntity::CalculateFidelity(const std::pair<std::string, std::string>& epr)
{
    NS_LOG_FUNCTION(this << epr.first << epr.second);
    return m_engine->CalculateFidelity(epr);
}

std::string
QuantumPhyEntity::AllocAncillaQubit()
{
    return QNS_ANCILLA_PREFIX + std::to_string(m_ancillaCount++);
}

void
QuantumPhyEntity::SetErrorModel(Ptr<QuantumErrorModel> model, const std::string& qubit)
{
    NS_LOG_FUNCTION(this << model << qubit);
    NS_LOG_LOGIC("The qubit " << qubit << " gets the " << model->GetDescription());
    m_qubitModel[qubit] = model;
}

void
QuantumPhyEntity::SetTimeModel(const std::string& owner, Time coherenceTime)
{
    NS_LOG_FUNCTION(this << owner << coherenceTime);
    m_ownerModel[owner] =
        CreateObjectWithAttributes<TimeModel>("CoherenceTime", TimeValue(coherenceTime));
}

void
QuantumPhyEntity::SetDephaseModel(const std::string& owner,
                                  const std::string& gate,
                                  Time coherenceTime)
{
    NS_LOG_FUNCTION(this << owner << gate << coherenceTime);
    m_gateModel[{owner, gate}] =
        CreateObjectWithAttributes<DephaseModel>("CoherenceTime", TimeValue(coherenceTime));
}

void
QuantumPhyEntity::SetDepolarModel(Ptr<QuantumChannel> channel, double fidelity)
{
    NS_LOG_FUNCTION(this << channel << fidelity);
    m_channelModel[channel->GetOwners()] =
        CreateObjectWithAttributes<DepolarModel>("Fidelity", DoubleValue(fidelity));
}

void
QuantumPhyEntity::ApplyMemoryNoise(const std::string& qubit)
{
    auto timeIt = m_qubitTime.find(qubit);
    NS_ABORT_MSG_IF(timeIt == m_qubitTime.end(), "The qubit " << qubit << " does not exist");
    const Time elapsed = Simulator::Now() - timeIt->second;
    if (!elapsed.IsStrictlyPositive())
    {
        return;
    }
    Ptr<QuantumErrorModel> model = m_qubitModel.at(qubit);
    NS_LOG_LOGIC("The qubit " << qubit << " waited " << elapsed.As(Time::S) << " under the "
                              << model->GetDescription());
    ApplyOperation(model->GetOperation(elapsed), {qubit});
}

void
QuantumPhyEntity::ApplyGateNoise(const std::string& owner,
                                 const std::string& gate,
                                 const std::vector<std::string>& qubits)
{
    auto it = m_gateModel.find({owner, gate});
    Ptr<QuantumErrorModel> model = it == m_gateModel.end() ? m_defaultDephaseModel : it->second;
    for (const auto& qubit : qubits)
    {
        NS_LOG_LOGIC("The gate " << gate << " of " << owner << " leaves the qubit " << qubit
                                 << " with the " << model->GetDescription());
        ApplyOperation(model->GetOperation(Time(0)), {qubit});
    }
}

void
QuantumPhyEntity::ApplyChannelNoise(const std::string& srcOwner,
                                    const std::string& dstOwner,
                                    const std::string& qubit)
{
    NS_LOG_FUNCTION(this << srcOwner << dstOwner << qubit);
    auto it = m_channelModel.find({srcOwner, dstOwner});
    Ptr<QuantumErrorModel> model = it == m_channelModel.end() ? m_defaultDepolarModel : it->second;
    NS_LOG_LOGIC("The qubit " << qubit << " crossed the channel from " << srcOwner << " to "
                              << dstOwner << " with the " << model->GetDescription());
    ApplyOperation(model->GetOperation(Time(0)), {qubit});
}

bool
QuantumPhyEntity::CheckValid(const std::vector<std::string>& qubits) const
{
    return m_engine->AreValid(qubits);
}

bool
QuantumPhyEntity::CheckOwned(const std::string& owner, const std::vector<std::string>& qubits) const
{
    if (owner == QNS_GOD_OWNER)
    {
        return true;
    }
    Ptr<QuantumMemory> memory = GetMemory(owner);
    return std::all_of(qubits.begin(), qubits.end(), [&memory](const std::string& qubit) {
        return memory->HasQubit(qubit);
    });
}

int64_t
QuantumPhyEntity::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_random->SetStream(stream);
    return 1;
}

} // namespace ns3
