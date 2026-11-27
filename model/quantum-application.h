/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_APPLICATION_H
#define QUANTUM_APPLICATION_H

#include "quantum-messenger.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <string>
#include <vector>

namespace ns3
{

class DistributeEprProtocol;
class QuantumPhyEntity;

/**
 * @ingroup quantum
 * Base class of the quantum applications.
 *
 * A quantum protocol alternates local quantum operations with classical
 * messages. This class provides the classical half: it exchanges short text
 * messages with a peer and hands each received message to HandleMessage().
 * Derived classes implement the protocol in DoStartApplication() and
 * HandleMessage(), and call NotifyFinished() when it ends.
 *
 * As in SourceApplication and SinkApplication, the peer is the socket address
 * in the Remote attribute and the application receives on the socket address
 * in the Local attribute. QuantumApplicationHelper fills in both when it
 * installs the application.
 *
 * An application that needs entanglement asks the Bell pair service of its
 * node, see GetEprProtocol().
 */
class QuantumApplication : public Application
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    QuantumApplication();
    ~QuantumApplication() override;

    /**
     * Encode fields as one message.
     *
     * @param fields The fields, none of which may contain QuantumMessenger::DELIMITER.
     * @return The message.
     */
    static std::string JoinMessage(const std::vector<std::string>& fields);

    /**
     * Decode a message into its fields.
     *
     * @param message The message.
     * @return The fields.
     */
    static std::vector<std::string> SplitMessage(const std::string& message);

    /**
     * TracedCallback signature for the end of a protocol.
     *
     * @param app The application whose protocol has finished.
     */
    typedef void (*FinishedTracedCallback)(Ptr<const QuantumApplication> app);

  protected:
    void DoDispose() override;

    /**
     * Start the protocol; called once the application can send and receive.
     */
    virtual void DoStartApplication();

    /**
     * Handle a message from a peer.
     *
     * @param message The message.
     * @param from Socket address of the sender.
     */
    virtual void HandleMessage(const std::string& message, const Address& from);

    /**
     * Send a message to the peer set by the Remote attribute.
     *
     * @param message The message; it may not contain a null character.
     */
    void SendMessage(const std::string& message);

    /**
     * Send a message to a given socket address, typically to reply to the
     * sender of a message passed to HandleMessage().
     *
     * @param message The message; it may not contain a null character.
     * @param to Socket address of the receiver.
     */
    void SendMessageTo(const std::string& message, const Address& to);

    /**
     * Report that the protocol of this application has finished.
     *
     * This fires the Finished trace source. Applications do not compute
     * results themselves: a simulation script connects to the trace source and
     * reads what it wants from QuantumPhyEntity at that moment.
     */
    void NotifyFinished();

    /**
     * Get the quantum physical entity.
     *
     * @return The quantum physical entity.
     */
    Ptr<QuantumPhyEntity> GetQuantumPhyEntity() const;

    /**
     * Get the Bell pair service of the node the application is installed on.
     *
     * @return The protocol that distributes Bell pairs from this node.
     */
    Ptr<DistributeEprProtocol> GetEprProtocol() const;

    /**
     * Get the owner name of the node the application is installed on.
     *
     * @return The owner name.
     */
    std::string GetOwner() const;

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * Log a received message and pass it to HandleMessage().
     *
     * @param message The message.
     * @param from Socket address of the sender.
     */
    void Receive(const std::string& message, const Address& from);

    Ptr<QuantumPhyEntity> m_qphyent; //!< The quantum physical entity
    Address m_local;                 //!< Socket address to receive on, if any
    Address m_remote;                //!< Socket address of the peer, if any
    uint32_t m_packetSize{0};        //!< Minimum payload size of a message, in bytes
    QuantumMessenger m_messenger;    //!< Carries the classical messages

    /// Trace fired when the protocol has finished.
    TracedCallback<Ptr<const QuantumApplication>> m_finishedTrace;
};

} // namespace ns3

#endif /* QUANTUM_APPLICATION_H */
