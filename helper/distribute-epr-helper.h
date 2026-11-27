/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef DISTRIBUTE_EPR_HELPER_H
#define DISTRIBUTE_EPR_HELPER_H

#include "quantum-application-helper.h"

namespace ns3
{

/**
 * @ingroup quantum
 * Installs the source side of the distribution of Bell pairs; see QuantumNetStackHelper.
 */
class DistributeEprSrcHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from the source to the destination.
     */
    DistributeEprSrcHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

/**
 * @ingroup quantum
 * Installs the destination side of the distribution of Bell pairs; see QuantumNetStackHelper.
 */
class DistributeEprDstHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from the source to the destination.
     */
    DistributeEprDstHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

} // namespace ns3

#endif /* DISTRIBUTE_EPR_HELPER_H */
