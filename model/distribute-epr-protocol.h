/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef DISTRIBUTE_EPR_PROTOCOL_H
#define DISTRIBUTE_EPR_PROTOCOL_H

#include "quantum-application.h"

#include <string>
#include <utility>

namespace ns3
{

class QuantumChannel;

/**
 * @ingroup quantum
 * Source side of the distribution of Bell pairs over a quantum channel.
 *
 * The source creates a Bell pair, keeps the first qubit and sends the second
 * one to the destination. The transfer is announced by a classical message
 * carrying the names of both qubits; the qubit changes owner when the message
 * arrives.
 *
 * QuantumNetStackHelper installs one source and one destination per ordered
 * pair of nodes. Other applications find them with
 * QuantumPhyEntity::GetConn2Apps() under the key #QNS_APP_DIST_EPR.
 */
class DistributeEprSrcProtocol : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Create a Bell pair and send its second qubit to the destination.
     *
     * @param epr Unique names of the two qubits; the first stays at the source.
     */
    void GenerateAndDistributeEpr(const std::pair<std::string, std::string>& epr);

  protected:
    void DoDispose() override;

  private:
    Ptr<QuantumChannel> m_channel; //!< The quantum channel to the destination
};

/**
 * @ingroup quantum
 * Destination side of the distribution of Bell pairs over a quantum channel.
 *
 * On each announcement the destination takes ownership of the arriving qubit
 * and applies the noise of the channel to it.
 */
class DistributeEprDstProtocol : public QuantumApplication
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
    void HandleMessage(const std::string& message, const Address& from) override;

    Ptr<QuantumChannel> m_channel; //!< The quantum channel from the source
};

} // namespace ns3

#endif /* DISTRIBUTE_EPR_PROTOCOL_H */
