/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-node.h"

#include "quantum-memory.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/log.h"
#include "ns3/node.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumNode");

NS_OBJECT_ENSURE_REGISTERED(QuantumNode);

TypeId
QuantumNode::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QuantumNode")
                            .SetParent<Object>()
                            .SetGroupName("Quantum")
                            .AddConstructor<QuantumNode>();
    return tid;
}

QuantumNode::QuantumNode()
    : QuantumNode(nullptr, "")
{
}

QuantumNode::QuantumNode(Ptr<QuantumPhyEntity> qphyent, const std::string& owner)
    : m_qphyent(qphyent),
      m_owner(owner),
      m_memory(CreateObject<QuantumMemory>())
{
    NS_LOG_FUNCTION(this << owner);
}

void
QuantumNode::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_qphyent = nullptr;
    m_memory = nullptr;
    Object::DoDispose();
}

std::string
QuantumNode::GetOwner() const
{
    return m_owner;
}

Ptr<Node>
QuantumNode::GetNode() const
{
    return GetObject<Node>();
}

Ptr<QuantumPhyEntity>
QuantumNode::GetQuantumPhyEntity() const
{
    return m_qphyent;
}

Ptr<QuantumMemory>
QuantumNode::GetQuantumMemory() const
{
    return m_memory;
}

void
QuantumNode::SetAddress(const Address& address)
{
    NS_LOG_FUNCTION(this << address);
    m_address = address;
}

Address
QuantumNode::GetAddress() const
{
    if (!m_address.IsInvalid())
    {
        return m_address;
    }

    Ptr<Node> node = GetNode();
    NS_ABORT_MSG_UNLESS(node, "The quantum node " << m_owner << " is not aggregated to a node");
    // Interface 0 is the loopback interface.
    if (Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>())
    {
        for (uint32_t i = 1; i < ipv4->GetNInterfaces(); ++i)
        {
            if (ipv4->GetNAddresses(i) > 0)
            {
                return ipv4->GetAddress(i, 0).GetLocal();
            }
        }
    }
    if (Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>())
    {
        for (uint32_t i = 1; i < ipv6->GetNInterfaces(); ++i)
        {
            for (uint32_t j = 0; j < ipv6->GetNAddresses(i); ++j)
            {
                const Ipv6InterfaceAddress address = ipv6->GetAddress(i, j);
                if (address.GetScope() == Ipv6InterfaceAddress::GLOBAL)
                {
                    return address.GetAddress();
                }
            }
        }
    }
    NS_ABORT_MSG("The node of " << m_owner << " has neither an IPv4 nor a global IPv6 address; "
                                << "assign addresses before installing quantum applications");
    return Address();
}

uint16_t
QuantumNode::GetServicePort(const std::string& service)
{
    NS_LOG_FUNCTION(this << service);
    auto it = m_servicePorts.find(service);
    if (it != m_servicePorts.end())
    {
        return it->second;
    }
    NS_ABORT_MSG_IF(m_nextPort == 0, "The node " << m_owner << " ran out of UDP ports");
    const uint16_t port = m_nextPort++;
    m_servicePorts[service] = port;
    return port;
}

void
QuantumNode::AddQubit(const std::string& qubit)
{
    NS_LOG_FUNCTION(this << qubit);
    m_memory->AddQubit(qubit);
}

bool
QuantumNode::RemoveQubit(const std::string& qubit)
{
    NS_LOG_FUNCTION(this << qubit);
    return m_memory->RemoveQubit(qubit);
}

bool
QuantumNode::HasQubit(const std::string& qubit) const
{
    return m_memory->HasQubit(qubit);
}

void
QuantumNode::SetTimeModel(Time coherenceTime)
{
    m_qphyent->SetTimeModel(m_owner, coherenceTime);
}

void
QuantumNode::SetDephaseModel(const std::string& gate, Time coherenceTime)
{
    m_qphyent->SetDephaseModel(m_owner, gate, coherenceTime);
}

} // namespace ns3
