/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-messenger.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumMessenger");

// Defined here, where Node and Socket are complete types.
QuantumMessenger::QuantumMessenger() = default;

QuantumMessenger::~QuantumMessenger()
{
    Close();
}

void
QuantumMessenger::Open(Ptr<Node> node, Handler handler)
{
    NS_LOG_FUNCTION(this << node);
    Close();
    m_node = node;
    m_handler = handler;
}

bool
QuantumMessenger::IsOpen() const
{
    return bool(m_node);
}

void
QuantumMessenger::Listen(const Address& local)
{
    NS_LOG_FUNCTION(this << local);
    NS_ABORT_MSG_UNLESS(IsOpen(), "A messenger must be opened before it can listen");
    m_recvSocket = Socket::CreateSocket(m_node, UdpSocketFactory::GetTypeId());
    NS_ABORT_MSG_IF(m_recvSocket->Bind(local) == -1, "Failed to bind to " << local);
    m_recvSocket->SetRecvCallback(MakeCallback(&QuantumMessenger::HandleRead, this));
}

void
QuantumMessenger::SendTo(const std::string& message, const Address& to, uint32_t minSize)
{
    NS_LOG_FUNCTION(this << message << to << minSize);
    NS_ABORT_MSG_UNLESS(IsOpen(), "A messenger must be opened before it can send");
    NS_ABORT_MSG_IF(message.find('\0') != std::string::npos,
                    "A message may not contain a null character");
    if (!m_sendSocket)
    {
        m_sendSocket = Socket::CreateSocket(m_node, UdpSocketFactory::GetTypeId());
        m_sendSocket->SetRecvCallback(MakeCallback(&QuantumMessenger::HandleRead, this));
    }

    // The payload is the null-terminated message, padded to the minimum size.
    std::vector<uint8_t> payload(std::max<std::size_t>(message.size() + 1, minSize), 0);
    std::copy(message.begin(), message.end(), payload.begin());
    m_sendSocket->SendTo(Create<Packet>(payload.data(), payload.size()), 0, to);
}

void
QuantumMessenger::Close()
{
    NS_LOG_FUNCTION(this);
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
    m_node = nullptr;
    m_handler = Handler();
}

void
QuantumMessenger::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        std::vector<uint8_t> payload(packet->GetSize());
        packet->CopyData(payload.data(), payload.size());
        const std::string message(payload.begin(), std::find(payload.begin(), payload.end(), 0));
        if (!m_handler.IsNull())
        {
            m_handler(message, from);
        }
    }
}

std::string
QuantumMessenger::Join(const std::vector<std::string>& fields)
{
    std::string message;
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        NS_ABORT_MSG_IF(fields[i].find(DELIMITER) != std::string::npos,
                        "The message field " << fields[i] << " contains the delimiter "
                                             << DELIMITER);
        message += (i == 0 ? "" : std::string(1, DELIMITER)) + fields[i];
    }
    return message;
}

std::vector<std::string>
QuantumMessenger::Split(const std::string& message)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t end = message.find(DELIMITER, start);
        fields.push_back(message.substr(start, end - start));
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return fields;
}

} // namespace ns3
