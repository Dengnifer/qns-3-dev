/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef DISTRIBUTE_EPR_PROTOCOL_H
#define DISTRIBUTE_EPR_PROTOCOL_H

#include "quantum-messenger.h"

#include "ns3/object.h"
#include "ns3/traced-callback.h"

#include <string>
#include <utility>

namespace ns3
{

/**
 * @ingroup quantum
 * The link-layer service that distributes Bell pairs between nodes.
 *
 * QuantumNetStackHelper aggregates one instance to every quantum node. A
 * protocol that needs entanglement asks the instance of its own node:
 *
 * @code
 *   GetNode()->GetObject<DistributeEprProtocol>()->DistributeEpr("Bob", {"A0", "B0"});
 * @endcode
 *
 * The node creates the pair, keeps the first qubit and sends the second one.
 * The transfer is announced by a classical message. When the message arrives,
 * the instance of the destination gives the qubit to its node, applies the
 * noise of the quantum channel, and fires the EprReceived trace source.
 */
class DistributeEprProtocol : public Object
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /// UDP port on which every node receives the announcements of Bell pairs.
    static constexpr uint16_t PORT{4747};

    DistributeEprProtocol();
    ~DistributeEprProtocol() override;

    /**
     * Create a Bell pair on this node and send its second qubit to another node.
     *
     * @param dstOwner Owner name of the node that receives the second qubit.
     * @param epr Unique names of the two qubits; the first stays on this node.
     */
    void DistributeEpr(const std::string& dstOwner, const std::pair<std::string, std::string>& epr);

    /**
     * TracedCallback signature for a distributed Bell pair.
     *
     * @param peer Owner name of the node at the other end.
     * @param srcQubit Name of the qubit that stayed at the source.
     * @param dstQubit Name of the qubit that went to the destination.
     */
    typedef void (*EprTracedCallback)(const std::string& peer,
                                      const std::string& srcQubit,
                                      const std::string& dstQubit);

  protected:
    void DoInitialize() override;
    void DoDispose() override;

  private:
    /**
     * Open the messenger on the node, unless it is open already.
     */
    void EnsureOpen();

    /**
     * Take delivery of the qubit that a message announces.
     *
     * @param message The announcement: source owner and the two qubit names.
     * @param from Socket address of the sender.
     */
    void HandleMessage(const std::string& message, const Address& from);

    QuantumMessenger m_messenger; //!< Carries the announcements

    /// Trace fired at the source when a pair has been sent.
    TracedCallback<const std::string&, const std::string&, const std::string&> m_sentTrace;
    /// Trace fired at the destination when a qubit has arrived.
    TracedCallback<const std::string&, const std::string&, const std::string&> m_receivedTrace;
};

} // namespace ns3

#endif /* DISTRIBUTE_EPR_PROTOCOL_H */
