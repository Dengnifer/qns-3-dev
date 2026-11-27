/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_APPLICATION_HELPER_H
#define QUANTUM_APPLICATION_HELPER_H

#include "ns3/application-helper.h"
#include "ns3/ptr.h"

#include <string>

namespace ns3
{

class QuantumChannel;
class QuantumNode;
class QuantumPhyEntity;

/**
 * @ingroup quantum
 * Base class of the helpers that install quantum applications.
 *
 * On top of ApplicationHelper, the class sets the QPhyEntity attribute, knows
 * on which node an application bound to a quantum channel belongs, and wires
 * the classical messages between the two ends of a channel.
 */
class QuantumApplicationHelper : public ApplicationHelper
{
  public:
    /**
     * Constructor.
     *
     * @param typeId TypeId of the application to install.
     * @param qphyent The quantum physical entity of the network.
     */
    QuantumApplicationHelper(TypeId typeId, Ptr<QuantumPhyEntity> qphyent);

    using ApplicationHelper::Install;

    /**
     * Install the application on a quantum node.
     *
     * @param node The quantum node.
     * @return The installed application.
     */
    ApplicationContainer Install(Ptr<QuantumNode> node);

    /**
     * Install the application on the node it belongs to: the end of the
     * quantum channel that the helper was constructed for.
     *
     * @return The installed application.
     */
    ApplicationContainer Install();

  protected:
    /**
     * Declare on which node Install() without argument installs.
     *
     * @param owner Owner name of the node.
     */
    void SetHome(const std::string& owner);

    /**
     * Make the application send its messages to a service of a node.
     *
     * @param owner Owner name of the node.
     * @param service Name of the service.
     */
    void SetPeer(const std::string& owner, const std::string& service);

    /**
     * Make the application receive messages on a service of its node.
     *
     * @param service Name of the service.
     */
    void SetService(const std::string& service);

  private:
    Ptr<QuantumPhyEntity> m_qphyent; //!< The quantum physical entity
    std::string m_home;              //!< Owner name of the node Install() installs on
};

} // namespace ns3

#endif /* QUANTUM_APPLICATION_HELPER_H */
