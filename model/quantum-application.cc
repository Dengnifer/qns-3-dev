/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-application.h"

#include "quantum-node.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/address-utils.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <algorithm>

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
            .AddAttribute("Service",
                          "Name of the service on which the application receives messages; "
                          "empty if it only receives replies.",
                          StringValue(""),
                          MakeStringAccessor(&QuantumApplication::m_service),
                          MakeStringChecker())
            .AddAttribute("PeerOwner",
                          "Owner name of the node that messages are sent to.",
                          StringValue(""),
                          MakeStringAccessor(&QuantumApplication::m_peerOwner),
                          MakeStringChecker())
            .AddAttribute("PeerService",
                          "Name of the service that messages are sent to.",
                          StringValue(""),
                          MakeStringAccessor(&QuantumApplication::m_peerService),
                          MakeStringChecker())
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
    m_qphyent = nullptr;
    m_sendSocket = nullptr;
    m_recvSocket = nullptr;
    Application::DoDispose();
}

std::string
QuantumApplication::JoinMessage(const std::vector<std::string>& fields)
{
    std::string message;
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        NS_ABORT_MSG_IF(fields[i].find(MESSAGE_DELIMITER) != std::string::npos,
                        "The message field " << fields[i] << " contains the delimiter "
                                             << MESSAGE_DELIMITER);
        message += (i == 0 ? "" : std::string(1, MESSAGE_DELIMITER)) + fields[i];
    }
    return message;
}

std::vector<std::string>
QuantumApplication::SplitMessage(const std::string& message)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t end = message.find(MESSAGE_DELIMITER, start);
        fields.push_back(message.substr(start, end - start));
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return fields;
}

void
QuantumApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_UNLESS(m_qphyent, "The QPhyEntity attribute of a quantum application is not set");
    m_running = true;
    if (!m_service.empty())
    {
        Ptr<QuantumNode> quantumNode = GetNode()->GetObject<QuantumNode>();
        NS_ABORT_MSG_UNLESS(quantumNode,
                            "A quantum application must be installed on a quantum "
                            "node; see QuantumNetStackHelper");
        const uint16_t port = quantumNode->GetServicePort(m_service);
        const Address local = Ipv4Address::IsMatchingType(quantumNode->GetAddress())
                                  ? Address(InetSocketAddress(Ipv4Address::GetAny(), port))
                                  : Address(Inet6SocketAddress(Ipv6Address::GetAny(), port));
        m_recvSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        NS_ABORT_MSG_IF(m_recvSocket->Bind(local) == -1,
                        "Failed to bind the service " << m_service << " of " << GetOwner()
                                                      << " to port " << port);
        m_recvSocket->SetRecvCallback(MakeCallback(&QuantumApplication::HandleRead, this));
    }
    DoStartApplication();
}

void
QuantumApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);
    m_running = false;
    for (auto& socket : {m_sendSocket, m_recvSocket})
    {
        if (socket)
        {
            socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            socket->Close();
        }
    }
    m_sendSocket = nullptr;
    m_recvSocket = nullptr;
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
QuantumApplication::SendMessage(const std::string& message)
{
    NS_LOG_FUNCTION(this << message);
    NS_ABORT_MSG_IF(m_peerOwner.empty() || m_peerService.empty(),
                    "A quantum application of " << GetOwner() << " has no peer to send to");
    Ptr<QuantumNode> peer = m_qphyent->GetQuantumNode(m_peerOwner);
    SendMessageTo(message,
                  addressUtils::ConvertToSocketAddress(peer->GetAddress(),
                                                       peer->GetServicePort(m_peerService)));
}

void
QuantumApplication::SendMessageTo(const std::string& message, const Address& to)
{
    NS_LOG_FUNCTION(this << message << to);
    NS_ABORT_MSG_UNLESS(m_running,
                        "A quantum application of " << GetOwner() << " sends while not running");
    NS_ABORT_MSG_IF(message.find('\0') != std::string::npos,
                    "A message may not contain a null character");
    if (!m_sendSocket)
    {
        m_sendSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_sendSocket->SetRecvCallback(MakeCallback(&QuantumApplication::HandleRead, this));
    }

    // The payload is the null-terminated message, padded to the minimum size.
    std::vector<uint8_t> payload(std::max<std::size_t>(message.size() + 1, m_packetSize), 0);
    std::copy(message.begin(), message.end(), payload.begin());
    Ptr<Packet> packet = Create<Packet>(payload.data(), payload.size());
    m_sendSocket->SendTo(packet, 0, to);
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << GetOwner() << " sent \"" << message
                      << "\"");
}

void
QuantumApplication::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        std::vector<uint8_t> payload(packet->GetSize());
        packet->CopyData(payload.data(), payload.size());
        const std::string message(payload.begin(), std::find(payload.begin(), payload.end(), 0));
        NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << GetOwner() << " received \""
                          << message << "\"");
        HandleMessage(message, from);
    }
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
