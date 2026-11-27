/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-channel.h"

#include "quantum-node.h"

#include "ns3/abort.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumChannel");

NS_OBJECT_ENSURE_REGISTERED(QuantumChannel);

TypeId
QuantumChannel::GetTypeId()
{
    // The owners are constructor arguments, not attributes: an attribute would
    // reset them to its initial value right after construction.
    static TypeId tid = TypeId("ns3::QuantumChannel")
                            .SetParent<Object>()
                            .SetGroupName("Quantum")
                            .AddConstructor<QuantumChannel>();
    return tid;
}

QuantumChannel::QuantumChannel(const std::string& srcOwner, const std::string& dstOwner)
    : m_srcOwner(srcOwner),
      m_dstOwner(dstOwner)
{
    NS_LOG_FUNCTION(this << srcOwner << dstOwner);
}

QuantumChannel::QuantumChannel(Ptr<QuantumNode> src, Ptr<QuantumNode> dst)
    : QuantumChannel(src->GetOwner(), dst->GetOwner())
{
}

std::string
QuantumChannel::GetSrcOwner() const
{
    NS_ABORT_MSG_IF(m_srcOwner.empty(), "The quantum channel has no source");
    return m_srcOwner;
}

std::string
QuantumChannel::GetDstOwner() const
{
    NS_ABORT_MSG_IF(m_dstOwner.empty(), "The quantum channel has no destination");
    return m_dstOwner;
}

std::pair<std::string, std::string>
QuantumChannel::GetOwners() const
{
    return {GetSrcOwner(), GetDstOwner()};
}

std::string
QuantumChannel::GetServiceName(const std::string& protocol) const
{
    return protocol + ":" + GetSrcOwner() + "->" + GetDstOwner();
}

} // namespace ns3
