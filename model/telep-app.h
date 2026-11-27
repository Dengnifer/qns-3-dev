/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef TELEP_APP_H
#define TELEP_APP_H

#include "quantum-application.h"

#include "ns3/nstime.h"

#include <string>
#include <utility>

namespace ns3
{

class QuantumChannel;
class Qubit;

/**
 * @ingroup quantum
 * Source side (Alice) of quantum teleportation.
 *
 * Alice shares a Bell pair with Bob, measures her qubit together with her half
 * of the pair, and sends the two outcomes to Bob.
 */
class TelepSrcApp : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

  protected:
    void DoDispose() override;

  private:
    void DoStartApplication() override;

    /**
     * Measure the two qubits of Alice and send the outcomes to Bob.
     */
    void MeasureAndSend();

    Ptr<QuantumChannel> m_channel; //!< The quantum channel from Alice to Bob
    std::pair<std::string, std::string>
        m_qubits;        //!< Alice's qubit to teleport and her half of the pair
    std::string m_qubit; //!< Bob's half of the pair
    Ptr<Qubit> m_input;  //!< State to prepare Alice's qubit in, if any
    Time m_delay;        //!< Wait between sharing the pair and measuring
};

/**
 * @ingroup quantum
 * Destination side (Bob) of quantum teleportation.
 *
 * Bob applies the Pauli corrections selected by the outcomes Alice sends.
 */
class TelepDstApp : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

  private:
    void HandleMessage(const std::string& message, const Address& from) override;

    std::string m_qubit; //!< Bob's half of the pair
};

} // namespace ns3

#endif /* TELEP_APP_H */
