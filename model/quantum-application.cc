/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-application.h"

#include "distribute-epr-protocol.h"
#include "quantum-node.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumApplication");

NS_OBJECT_ENSURE_REGISTERED(QuantumApplication);

TypeId
QuantumApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::QuantumApplication")
            .SetParent<Application>()
            .SetGroupName("Quantum")
            .AddAttribute("QPhyEntity",
                          "The quantum physical entity of the network.",
                          PointerValue(),
                          MakePointerAccessor(&QuantumApplication::m_qphyent),
                          MakePointerChecker<QuantumPhyEntity>())
            .AddAttribute("Local",
                          "The address on which the application receives messages, made of "
                          "an IP address of its node and a UDP port. If not set, the "
                          "application only receives replies to its own messages.",
                          AddressValue(),
                          MakeAddressAccessor(&QuantumApplication::m_local),
                          MakeAddressChecker())
            .AddAttribute("Remote",
                          "The address of the peer, made of its IP address and its UDP port.",
                          AddressValue(),
                          MakeAddressAccessor(&QuantumApplication::m_remote),
                          MakeAddressChecker())
            .AddAttribute("PacketSize",
                          "Minimum payload size of a message in bytes; shorter messages "
                          "are padded with zeros.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&QuantumApplication::m_packetSize),
                          MakeUintegerChecker<uint32_t>())
            .AddTraceSource("Finished",
                            "The protocol of the application has finished.",
                            MakeTraceSourceAccessor(&QuantumApplication::m_finishedTrace),
                            "ns3::QuantumApplication::FinishedTracedCallback");
    return tid;
}

QuantumApplication::QuantumApplication()
{
    NS_LOG_FUNCTION(this);
}

QuantumApplication::~QuantumApplication()
{
    NS_LOG_FUNCTION(this);
}

void
QuantumApplication::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_messenger.Close();
    m_qphyent = nullptr;
    Application::DoDispose();
}

std::string
QuantumApplication::JoinMessage(const std::vector<std::string>& fields)
{
    return QuantumMessenger::Join(fields);
}

std::vector<std::string>
QuantumApplication::SplitMessage(const std::string& message)
{
    return QuantumMessenger::Split(message);
}

void
QuantumApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_UNLESS(m_qphyent, "The QPhyEntity attribute of a quantum application is not set");
    Ptr<QuantumNode> quantumNode = GetNode()->GetObject<QuantumNode>();
    NS_ABORT_MSG_UNLESS(quantumNode,
                        "A quantum application must be installed on a quantum node; "
                        "see QuantumNetStackHelper");
    m_messenger.Open(GetNode(), MakeCallback(&QuantumApplication::Receive, this));
    if (!m_local.IsInvalid())
    {
        m_messenger.Listen(m_local);
    }
    DoStartApplication();
}

void
QuantumApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);
    m_messenger.Close();
}

void
QuantumApplication::DoStartApplication()
{
    NS_LOG_FUNCTION(this);
}

void
QuantumApplication::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
}

void
QuantumApplication::Receive(const std::string& message, const Address& from)
{
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << GetOwner() << " received \""
                      << message << "\"");
    HandleMessage(message, from);
}

void
QuantumApplication::SendMessage(const std::string& message)
{
    NS_LOG_FUNCTION(this << message);
    NS_ABORT_MSG_IF(m_remote.IsInvalid(),
                    "The Remote attribute of a quantum application of " << GetOwner()
                                                                        << " is not set");
    SendMessageTo(message, m_remote);
}

void
QuantumApplication::SendMessageTo(const std::string& message, const Address& to)
{
    NS_LOG_FUNCTION(this << message << to);
    NS_ABORT_MSG_UNLESS(m_messenger.IsOpen(),
                        "A quantum application of " << GetOwner() << " sends while not running");
    m_messenger.SendTo(message, to, m_packetSize);
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << GetOwner() << " sent \"" << message
                      << "\"");
}

void
QuantumApplication::NotifyFinished()
{
    NS_LOG_FUNCTION(this);
    m_finishedTrace(this);
}

Ptr<QuantumPhyEntity>
QuantumApplication::GetQuantumPhyEntity() const
{
    return m_qphyent;
}

Ptr<DistributeEprProtocol>
QuantumApplication::GetEprProtocol() const
{
    Ptr<DistributeEprProtocol> protocol = GetNode()->GetObject<DistributeEprProtocol>();
    NS_ABORT_MSG_UNLESS(protocol,
                        "The node of " << GetOwner() << " has no Bell pair service; "
                                       << "install the quantum stack with QuantumNetStackHelper");
    return protocol;
}

std::string
QuantumApplication::GetOwner() const
{
    Ptr<QuantumNode> quantumNode = GetNode() ? GetNode()->GetObject<QuantumNode>() : nullptr;
    NS_ABORT_MSG_UNLESS(quantumNode,
                        "A quantum application must be installed on a quantum node; "
                        "see QuantumNetStackHelper");
    return quantumNode->GetOwner();
}

} // namespace ns3
