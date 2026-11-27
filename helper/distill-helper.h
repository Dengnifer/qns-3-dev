/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef DISTILL_HELPER_H
#define DISTILL_HELPER_H

#include "quantum-application-helper.h"

namespace ns3
{

/**
 * @ingroup quantum
 * Installs DistillSrcApp on the source of a quantum channel.
 */
class DistillSrcHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from Alice to Bob.
     */
    DistillSrcHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

/**
 * @ingroup quantum
 * Installs DistillDstApp on the destination of a quantum channel.
 */
class DistillDstHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from Alice to Bob.
     */
    DistillDstHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

} // namespace ns3

#endif /* DISTILL_HELPER_H */
