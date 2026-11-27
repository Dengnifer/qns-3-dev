/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distribute-epr-protocol.h"

#include "quantum-channel.h"
#include "quantum-node.h"
#include "quantum-phy-entity.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/pointer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DistributeEprProtocol");

NS_OBJECT_ENSURE_REGISTERED(DistributeEprSrcProtocol);

TypeId
DistributeEprSrcProtocol::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DistributeEprSrcProtocol")
                            .SetParent<QuantumApplication>()
                            .SetGroupName("Quantum")
                            .AddConstructor<DistributeEprSrcProtocol>()
                            .AddAttribute("QChannel",
                                          "The quantum channel to the destination.",
                                          PointerValue(),
                                          MakePointerAccessor(&DistributeEprSrcProtocol::m_channel),
                                          MakePointerChecker<QuantumChannel>());
    return tid;
}

void
DistributeEprSrcProtocol::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    QuantumApplication::DoDispose();
}

void
DistributeEprSrcProtocol::GenerateAndDistributeEpr(const std::pair<std::string, std::string>& epr)
{
    NS_LOG_FUNCTION(this << epr.first << epr.second);
    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    qphyent->GenerateEpr(m_channel, epr);

    // The second qubit leaves the source now and reaches the destination
    // together with the announcement.
    qphyent->GetQuantumNode(m_channel->GetSrcOwner())->RemoveQubit(epr.second);
    SendMessage(JoinMessage({epr.first, epr.second}));
}

NS_OBJECT_ENSURE_REGISTERED(DistributeEprDstProtocol);

TypeId
DistributeEprDstProtocol::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DistributeEprDstProtocol")
                            .SetParent<QuantumApplication>()
                            .SetGroupName("Quantum")
                            .AddConstructor<DistributeEprDstProtocol>()
                            .AddAttribute("QChannel",
                                          "The quantum channel from the source.",
                                          PointerValue(),
                                          MakePointerAccessor(&DistributeEprDstProtocol::m_channel),
                                          MakePointerChecker<QuantumChannel>());
    return tid;
}

void
DistributeEprDstProtocol::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    QuantumApplication::DoDispose();
}

void
DistributeEprDstProtocol::HandleMessage(const std::string& message, const Address& from)
{
    NS_LOG_FUNCTION(this << message << from);
    const std::vector<std::string> epr = SplitMessage(message);
    NS_ABORT_MSG_IF(epr.size() != 2, "Malformed EPR announcement \"" << message << "\"");

    Ptr<QuantumPhyEntity> qphyent = GetQuantumPhyEntity();
    qphyent->GetQuantumNode(m_channel->GetDstOwner())->AddQubit(epr[1]);
    qphyent->ApplyChannelNoise(m_channel, epr[1]);
}

} // namespace ns3
