/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef DISTILL_NESTED_HELPER_H
#define DISTILL_NESTED_HELPER_H

#include "quantum-application-helper.h"

namespace ns3
{

/**
 * @ingroup quantum
 * Installs DistillNestedSrcApp on the source of a quantum channel.
 */
class DistillNestedSrcHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from Alice to Bob.
     */
    DistillNestedSrcHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

/**
 * @ingroup quantum
 * Installs DistillNestedDstApp on the destination of a quantum channel.
 */
class DistillNestedDstHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from Alice to Bob.
     */
    DistillNestedDstHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

} // namespace ns3

#endif /* DISTILL_NESTED_HELPER_H */
