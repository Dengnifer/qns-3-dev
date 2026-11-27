/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distribute-epr-protocol.h"

#include "quantum-basis.h"
#include "quantum-node.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DistributeEprProtocol");

NS_OBJECT_ENSURE_REGISTERED(DistributeEprProtocol);

TypeId
DistributeEprProtocol::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DistributeEprProtocol")
            .SetParent<Object>()
            .SetGroupName("Quantum")
            .AddConstructor<DistributeEprProtocol>()
            .AddTraceSource("EprSent",
                            "This node has created a Bell pair and sent one qubit of it.",
                            MakeTraceSourceAccessor(&DistributeEprProtocol::m_sentTrace),
                            "ns3::DistributeEprProtocol::EprTracedCallback")
            .AddTraceSource("EprReceived",
                            "One qubit of a Bell pair has arrived at this node.",
                            MakeTraceSourceAccessor(&DistributeEprProtocol::m_receivedTrace),
                            "ns3::DistributeEprProtocol::EprTracedCallback");
    return tid;
}

DistributeEprProtocol::DistributeEprProtocol()
{
    NS_LOG_FUNCTION(this);
}

DistributeEprProtocol::~DistributeEprProtocol()
{
    NS_LOG_FUNCTION(this);
}

void
DistributeEprProtocol::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    // A network without a classical side cannot receive qubits, and need not.
    Ptr<QuantumNode> quantumNode = GetObject<QuantumNode>();
    if (quantumNode && quantumNode->HasAddress())
    {
        EnsureOpen();
    }
    Object::DoInitialize();
}

void
DistributeEprProtocol::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_messenger.Close();
    Object::DoDispose();
}

void
DistributeEprProtocol::EnsureOpen()
{
    if (m_messenger.IsOpen())
    {
        return;
    }
    Ptr<QuantumNode> quantumNode = GetObject<QuantumNode>();
    NS_ABORT_MSG_UNLESS(quantumNode && quantumNode->GetNode(),
                        "DistributeEprProtocol must be aggregated to a quantum node; "
                        "see QuantumNetStackHelper");
    m_messenger.Open(quantumNode->GetNode(),
                     MakeCallback(&DistributeEprProtocol::HandleMessage, this));
    m_messenger.Listen(addressUtils::ConvertToSocketAddress(quantumNode->GetAddress(), PORT));
}

void
DistributeEprProtocol::DistributeEpr(const std::string& dstOwner,
                                     const std::pair<std::string, std::string>& epr)
{
    NS_LOG_FUNCTION(this << dstOwner << epr.first << epr.second);
    EnsureOpen();
    Ptr<QuantumNode> quantumNode = GetObject<QuantumNode>();
    Ptr<QuantumPhyEntity> qphyent = quantumNode->GetQuantumPhyEntity();
    const std::string srcOwner = quantumNode->GetOwner();

    qphyent->GenerateQubitsPure(srcOwner, QNS_BELL, {epr.first, epr.second});
    // The second qubit leaves this node now and reaches the destination
    // together with the announcement.
    quantumNode->RemoveQubit(epr.second);
    // The entity is the directory that maps an owner name to an IP address.
    m_messenger.SendTo(
        QuantumMessenger::Join({srcOwner, epr.first, epr.second}),
        addressUtils::ConvertToSocketAddress(qphyent->GetQuantumNode(dstOwner)->GetAddress(),
                                             PORT));
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << srcOwner << " sent the qubit "
                      << epr.second << " to " << dstOwner);
    m_sentTrace(dstOwner, epr.first, epr.second);
}

void
DistributeEprProtocol::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    const std::vector<std::string> fields = QuantumMessenger::Split(message);
    NS_ABORT_MSG_IF(fields.size() != 3, "Malformed EPR announcement \"" << message << "\"");
    const std::string& srcOwner = fields[0];
    const std::string& qubit = fields[2];

    Ptr<QuantumNode> quantumNode = GetObject<QuantumNode>();
    quantumNode->AddQubit(qubit);
    quantumNode->GetQuantumPhyEntity()->ApplyChannelNoise(srcOwner, quantumNode->GetOwner(), qubit);
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << quantumNode->GetOwner()
                      << " received the qubit " << qubit << " from " << srcOwner);
    m_receivedTrace(srcOwner, fields[1], qubit);
}

} // namespace ns3
