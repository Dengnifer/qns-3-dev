/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef TELEP_ADAPT_HELPER_H
#define TELEP_ADAPT_HELPER_H

#include "quantum-application-helper.h"

namespace ns3
{

/**
 * @ingroup quantum
 * Installs TelepAdaptApp on the source of a quantum channel.
 */
class TelepAdaptHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from Alice to Bob.
     */
    TelepAdaptHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

} // namespace ns3

#endif /* TELEP_ADAPT_HELPER_H */
