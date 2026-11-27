/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef TELEP_HELPER_H
#define TELEP_HELPER_H

#include "quantum-application-helper.h"

namespace ns3
{

/**
 * @ingroup quantum
 * Installs TelepSrcApp on the source of a quantum channel.
 */
class TelepSrcHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from Alice to Bob.
     */
    TelepSrcHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

/**
 * @ingroup quantum
 * Installs TelepDstApp on the destination of a quantum channel.
 */
class TelepDstHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from Alice to Bob.
     */
    TelepDstHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

} // namespace ns3

#endif /* TELEP_HELPER_H */
