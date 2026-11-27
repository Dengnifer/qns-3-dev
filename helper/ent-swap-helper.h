/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef ENT_SWAP_HELPER_H
#define ENT_SWAP_HELPER_H

#include "quantum-application-helper.h"

namespace ns3
{

/**
 * @ingroup quantum
 * Installs EntSwapSrcApp on a repeater node of a swapping chain.
 */
class EntSwapSrcHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param channel The quantum channel from the repeater node to the last node of the chain.
     */
    EntSwapSrcHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel);
};

/**
 * @ingroup quantum
 * Installs EntSwapDstApp on the last node of a swapping chain.
 */
class EntSwapDstHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param node The last node of the chain.
     */
    EntSwapDstHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumNode> node);
};

} // namespace ns3

#endif /* ENT_SWAP_HELPER_H */
