/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_MESSENGER_H
#define QUANTUM_MESSENGER_H

#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/ptr.h"

#include <string>
#include <vector>

namespace ns3
{

class Node;
class Socket;

/**
 * @ingroup quantum
 * Exchanges short text messages over UDP, on IPv4 or IPv6.
 *
 * The quantum protocols use a messenger for their classical messages. A
 * message is a string without null characters. It travels as the
 * null-terminated payload of one UDP packet.
 */
class QuantumMessenger
{
  public:
    /// Called for each received message, with the socket address of its sender.
    using Handler = Callback<void, const std::string&, const Address&>;

    /// Character that separates the fields of a message.
    static constexpr char DELIMITER{'.'};

    QuantumMessenger();
    ~QuantumMessenger();

    // A messenger owns sockets whose callbacks point to it.
    QuantumMessenger(const QuantumMessenger&) = delete;
    QuantumMessenger& operator=(const QuantumMessenger&) = delete;

    /**
     * Attach the messenger to a node.
     *
     * @param node The node whose network stack carries the messages.
     * @param handler Called for each received message.
     */
    void Open(Ptr<Node> node, Handler handler);

    /**
     * Check whether the messenger is attached to a node.
     *
     * @return True between Open() and Close().
     */
    bool IsOpen() const;

    /**
     * Receive the messages sent to a socket address of the node.
     *
     * @param local The socket address: an IP address of the node and a UDP port.
     */
    void Listen(const Address& local);

    /**
     * Send a message. A reply to it is passed to the handler.
     *
     * @param message The message; it may not contain a null character.
     * @param to Socket address of the receiver.
     * @param minSize Minimum payload size in bytes; a shorter message is
     *                padded with zeros.
     */
    void SendTo(const std::string& message, const Address& to, uint32_t minSize = 0);

    /**
     * Close the sockets and detach from the node.
     */
    void Close();

    /**
     * Encode fields as one message.
     *
     * @param fields The fields, none of which may contain #DELIMITER.
     * @return The message.
     */
    static std::string Join(const std::vector<std::string>& fields);

    /**
     * Decode a message into its fields.
     *
     * @param message The message.
     * @return The fields.
     */
    static std::vector<std::string> Split(const std::string& message);

  private:
    /**
     * Read the packets waiting on a socket.
     *
     * @param socket The socket.
     */
    void HandleRead(Ptr<Socket> socket);

    Ptr<Node> m_node;         //!< The node the messenger is attached to
    Handler m_handler;        //!< Called for each received message
    Ptr<Socket> m_sendSocket; //!< Sends messages and receives the replies
    Ptr<Socket> m_recvSocket; //!< Receives the messages sent to the listening port
};

} // namespace ns3

#endif /* QUANTUM_MESSENGER_H */
