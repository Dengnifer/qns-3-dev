/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef ENT_SWAP_ADAPT_LOCAL_HELPER_H
#define ENT_SWAP_ADAPT_LOCAL_HELPER_H

#include "quantum-application-helper.h"

namespace ns3
{

/**
 * @ingroup quantum
 * Installs EntSwapAdaptLocalApp; pass the last node of the chain to Install().
 */
class EntSwapAdaptLocalHelper : public QuantumApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity of the network.
     */
    EntSwapAdaptLocalHelper(Ptr<QuantumPhyEntity> qphyent);
};

} // namespace ns3

#endif /* ENT_SWAP_ADAPT_LOCAL_HELPER_H */
