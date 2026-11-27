/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_CHANNEL_H
#define QUANTUM_CHANNEL_H

#include "ns3/object.h"
#include "ns3/ptr.h"

#include <string>
#include <utility>

namespace ns3
{

class QuantumNode;

/**
 * @ingroup quantum
 * A directed quantum link from a source node to a destination node.
 *
 * The channel identifies the two ends of a link. It does not move packets:
 * the hand-over of a qubit is announced by a message over the classical
 * network, and the noise of the link is the DepolarModel that
 * QuantumPhyEntity holds for it.
 */
class QuantumChannel : public Object
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    QuantumChannel() = default;

    /**
     * Constructor.
     *
     * @param srcOwner Owner name of the source node.
     * @param dstOwner Owner name of the destination node.
     */
    QuantumChannel(const std::string& srcOwner, const std::string& dstOwner);

    /**
     * Constructor.
     *
     * @param src The source node.
     * @param dst The destination node.
     */
    QuantumChannel(Ptr<QuantumNode> src, Ptr<QuantumNode> dst);

    /**
     * Get the source.
     *
     * @return The owner name of the source node.
     */
    std::string GetSrcOwner() const;

    /**
     * Get the destination.
     *
     * @return The owner name of the destination node.
     */
    std::string GetDstOwner() const;

    /**
     * Get both ends.
     *
     * @return The owner names of the source and of the destination.
     */
    std::pair<std::string, std::string> GetOwners() const;

    /**
     * Name the service that a protocol offers over this channel.
     *
     * @param protocol Name of the protocol.
     * @return A name unique to the protocol and the channel, for
     *         QuantumNode::GetServicePort().
     */
    std::string GetServiceName(const std::string& protocol) const;

  private:
    std::string m_srcOwner; //!< Owner name of the source node
    std::string m_dstOwner; //!< Owner name of the destination node
};

} // namespace ns3

#endif /* QUANTUM_CHANNEL_H */
