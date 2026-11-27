/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_APPLICATION_H
#define QUANTUM_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <string>
#include <vector>

namespace ns3
{

class QuantumPhyEntity;
class Socket;

/**
 * @ingroup quantum
 * Base class of the quantum applications.
 *
 * A quantum protocol alternates local quantum operations with classical
 * messages. This class provides the classical half: it exchanges short text
 * messages with a peer over UDP, on IPv4 or IPv6, and hands each received
 * message to HandleMessage(). Derived classes implement the protocol in
 * DoStartApplication() and HandleMessage().
 *
 * Applications address each other by owner name and service name rather than
 * by IP address and port. The names are resolved when a message is sent, see
 * QuantumNode::GetAddress() and QuantumNode::GetServicePort(), so applications
 * can be installed in any order, before or after addresses are assigned.
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

    /// Character that separates the fields of a message.
    static constexpr char MESSAGE_DELIMITER{'.'};

    /**
     * Encode fields as one message.
     *
     * @param fields The fields, none of which may contain #MESSAGE_DELIMITER.
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
     * Report that the protocol of this application has finished.
     *
     * This fires the Finished trace source. Applications do not compute
     * results themselves: a simulation script connects to the trace source and
     * reads what it wants from QuantumPhyEntity at that moment.
     */
    void NotifyFinished();

    /**
     * Start the protocol; called once the sockets are open.
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
     * Send a message to the peer set by the PeerOwner and PeerService attributes.
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
     * Get the quantum physical entity.
     *
     * @return The quantum physical entity.
     */
    Ptr<QuantumPhyEntity> GetQuantumPhyEntity() const;

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
     * Read the packets waiting on the receiving socket.
     *
     * @param socket The receiving socket.
     */
    void HandleRead(Ptr<Socket> socket);

    Ptr<QuantumPhyEntity> m_qphyent; //!< The quantum physical entity
    std::string m_service;           //!< Service on which messages are received, if any
    std::string m_peerOwner;         //!< Owner name of the peer
    std::string m_peerService;       //!< Service of the peer that messages are sent to
    uint32_t m_packetSize{0};        //!< Minimum payload size of a message, in bytes
    bool m_running{false};           //!< Whether the application has started and not stopped
    Ptr<Socket> m_sendSocket;        //!< Socket to send messages and to receive the replies
    /// Trace fired when the protocol has finished.
    TracedCallback<Ptr<const QuantumApplication>> m_finishedTrace;
    Ptr<Socket> m_recvSocket; //!< Socket to receive messages on the service port
};

} // namespace ns3

#endif /* QUANTUM_APPLICATION_H */
