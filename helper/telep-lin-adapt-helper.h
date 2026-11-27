/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef TELEP_LIN_ADAPT_HELPER_H
#define TELEP_LIN_ADAPT_HELPER_H

#include "quantum-application-helper.h"

namespace ns3
{

/**
 * @ingroup quantum
 * Installs TelepLinAdaptApp on one node of a teleportation chain.
 */
class TelepLinAdaptHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel to the next node of the chain; null for
     *                the last node, which must then be passed to Install().
     */
    TelepLinAdaptHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

} // namespace ns3

#endif /* TELEP_LIN_ADAPT_HELPER_H */
