/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_PHY_ENTITY_H
#define QUANTUM_PHY_ENTITY_H

#include "quantum-basis.h"
#include "quantum-network-simulator.h"

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

class Application;
class Node;
class QuantumChannel;
class QuantumErrorModel;
class QuantumMemory;
class QuantumNode;
class QuantumOperation;
class UniformRandomVariable;

/**
 * @ingroup quantum
 * The physical layer shared by all nodes of a quantum network.
 *
 * Entangled qubits have no state of their own, so the state of all qubits of
 * the network lives in this one object rather than in the nodes. On top of the
 * tensor-network engine, QuantumNetworkSimulator, the entity adds what makes
 * the state physical:
 *
 * - ownership: a node may only act on the qubits in its own memory;
 * - memory noise: before a qubit is touched, it suffers the noise accumulated
 *   since it was last touched (TimeModel by default);
 * - gate noise: a gate applied by a node is followed by noise on its qubits
 *   (DephaseModel by default);
 * - channel noise: a qubit that arrives over a quantum channel suffers the
 *   noise of that channel (DepolarModel by default).
 *
 * The owner #QNS_GOD_OWNER is exempt from the ownership check and from gate
 * noise.
 */
class QuantumPhyEntity : public Object
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    QuantumPhyEntity();
    ~QuantumPhyEntity() override;

    /// @name Nodes
    /// @{

    /**
     * Make a node part of the quantum network.
     *
     * @param node The node.
     * @param owner Unique owner name for the node.
     * @return The QuantumNode created and aggregated to @p node.
     */
    Ptr<QuantumNode> AddNode(Ptr<Node> node, const std::string& owner);

    /**
     * Find a node by owner name.
     *
     * @param owner Owner name of the node.
     * @return The quantum node.
     */
    Ptr<QuantumNode> GetQuantumNode(const std::string& owner) const;

    /**
     * Get the owner names of all nodes.
     *
     * @return The owner names, in the order the nodes were added.
     */
    std::vector<std::string> GetOwners() const;

    /// @}

    /// @name Operations on qubits
    /// @{

    /**
     * Create qubits in a pure state and give them to an owner.
     *
     * @param owner Owner that creates the qubits.
     * @param data State vector with 2^n entries, for n qubits.
     * @param qubits Unique names of the n qubits.
     */
    void GenerateQubitsPure(const std::string& owner,
                            const QuantumTensorData& data,
                            const std::vector<std::string>& qubits);

    /**
     * Create qubits in a mixed state and give them to an owner.
     *
     * @param owner Owner that creates the qubits.
     * @param data Density matrix with 2^n * 2^n entries, for n qubits.
     * @param qubits Unique names of the n qubits.
     */
    void GenerateQubitsMixed(const std::string& owner,
                             const QuantumTensorData& data,
                             const std::vector<std::string>& qubits);

    /**
     * Create a Bell pair at the source of a quantum channel.
     *
     * @param channel The channel whose source creates the pair.
     * @param epr Unique names of the two qubits.
     */
    void GenerateEpr(Ptr<QuantumChannel> channel, const std::pair<std::string, std::string>& epr);

    /**
     * Apply a gate, with memory noise before it and gate noise after it.
     *
     * @param owner Owner that applies the gate; it must own the qubits.
     * @param gate Name of the gate.
     * @param data Matrix of the gate; ignored for a built-in gate.
     * @param qubits Names of the qubits the gate acts on.
     */
    void ApplyGate(const std::string& owner,
                   const std::string& gate,
                   const QuantumTensorData& data,
                   const std::vector<std::string>& qubits);

    /**
     * Apply a quantum operation without any additional noise.
     *
     * @param operation The operation.
     * @param qubits Names of the qubits the operation acts on.
     */
    void ApplyOperation(const QuantumOperation& operation, const std::vector<std::string>& qubits);

    /**
     * Apply a controlled gate that stands in for a classically controlled gate.
     *
     * Control flow adaptation replaces "measure the control qubits, send the
     * outcomes, apply the original gate to the target qubits if told so" by a
     * single controlled gate. The noise is that of the original protocol:
     * memory noise before the gate, then the noise of the original gate on the
     * target qubits.
     *
     * @param origOwner Owner that would apply the original gate.
     * @param origGate Name of the original gate.
     * @param gate Name of the controlled gate.
     * @param data Matrix of the controlled gate; ignored for a built-in gate.
     * @param controlQubits Names of the control qubits.
     * @param targetQubits Names of the target qubits.
     */
    void ApplyControlledOperation(const std::string& origOwner,
                                  const std::string& origGate,
                                  const std::string& gate,
                                  const QuantumTensorData& data,
                                  const std::vector<std::string>& controlQubits,
                                  const std::vector<std::string>& targetQubits);

    /**
     * Measure one qubit in the computational basis.
     *
     * @param owner Owner that measures; it must own the qubit.
     * @param qubit Name of the qubit.
     * @return The outcome and the probabilities of both outcomes.
     */
    QuantumMeasurement Measure(const std::string& owner, const std::string& qubit);

    /**
     * Condition the state on a measurement outcome of one qubit, without sampling.
     *
     * @param owner Owner that post-selects; it must own the qubit.
     * @param qubit Name of the qubit.
     * @param outcome The outcome to keep, 0 or 1; it must have a positive probability.
     * @return The probability that a measurement would have given @p outcome.
     */
    double PostSelect(const std::string& owner, const std::string& qubit, unsigned outcome);

    /**
     * Compute the reduced density matrix of some qubits without disturbing them.
     *
     * @param owner Owner that asks; unless it is #QNS_GOD_OWNER, it must own the qubits.
     * @param qubits Names of the n qubits.
     * @return The density matrix, with 2^n * 2^n entries.
     */
    QuantumTensorData PeekDensityMatrix(const std::string& owner,
                                        const std::vector<std::string>& qubits);

    /**
     * Discard qubits, after applying their memory noise.
     *
     * @param qubits Names of the qubits to trace out.
     */
    void PartialTrace(const std::vector<std::string>& qubits);

    /**
     * Contract the tensor network into a single tensor.
     *
     * @param optimizer Name of the contraction order optimizer; see
     *                  QuantumNetworkSimulator::Contract().
     * @return The density matrix of all remaining qubits.
     */
    QuantumTensorData Contract(const std::string& optimizer = "greed");

    /**
     * Compute the fidelity of two qubits with respect to the Bell state #QNS_BELL.
     *
     * @param epr Names of the two qubits.
     * @return The fidelity.
     */
    double CalculateFidelity(const std::pair<std::string, std::string>& epr);

    /**
     * Mark the end of a subcircuit; see QuantumNetworkSimulator::Checkpoint().
     */
    void Checkpoint();

    /**
     * Allocate a name for an ancilla qubit.
     *
     * @return A name that no other qubit of this entity uses.
     */
    std::string AllocAncillaQubit();

    /// @}

    /// @name Noise
    /// @{

    /**
     * Set the memory noise of one qubit.
     *
     * @param model The noise model.
     * @param qubit Name of the qubit.
     */
    void SetErrorModel(Ptr<QuantumErrorModel> model, const std::string& qubit);

    /**
     * Set the memory noise of the qubits that an owner generates from now on.
     *
     * @param owner Owner name.
     * @param coherenceTime Coherence time of the memory; see TimeModel.
     */
    void SetTimeModel(const std::string& owner, Time coherenceTime);

    /**
     * Set the noise of a gate applied by an owner.
     *
     * @param owner Owner name.
     * @param gate Name of the gate.
     * @param coherenceTime Coherence time of the gate; see DephaseModel.
     */
    void SetDephaseModel(const std::string& owner, const std::string& gate, Time coherenceTime);

    /**
     * Set the noise of a quantum channel.
     *
     * @param channel The channel.
     * @param fidelity Fidelity of a Bell pair distributed over the channel; see DepolarModel.
     */
    void SetDepolarModel(Ptr<QuantumChannel> channel, double fidelity);

    /**
     * Apply the noise of a quantum channel to a qubit that arrives over it.
     *
     * @param channel The channel.
     * @param qubit Name of the qubit that crossed the channel.
     */
    void ApplyChannelNoise(Ptr<QuantumChannel> channel, const std::string& qubit);

    /// @}

    /// @name Checks
    /// @{

    /**
     * Check whether qubits exist and have not been traced out.
     *
     * @param qubits Names of the qubits.
     * @return True if all of the qubits are valid.
     */
    bool CheckValid(const std::vector<std::string>& qubits) const;

    /**
     * Check whether an owner owns qubits.
     *
     * @param owner Owner name.
     * @param qubits Names of the qubits.
     * @return True if @p owner is #QNS_GOD_OWNER or owns all of the qubits.
     */
    bool CheckOwned(const std::string& owner, const std::vector<std::string>& qubits) const;

    /// @}

    /// @name Applications of a connection
    /// @{

    /**
     * Record the pair of applications that run a protocol over a channel.
     *
     * @param channel The channel.
     * @param protocol Name of the protocol.
     * @param apps The applications at the source and at the destination.
     */
    void AddConn2Apps(Ptr<QuantumChannel> channel,
                      const std::string& protocol,
                      const std::pair<Ptr<Application>, Ptr<Application>>& apps);

    /**
     * Check whether applications are recorded for a protocol over a channel.
     *
     * @param channel The channel.
     * @param protocol Name of the protocol.
     * @return True if AddConn2Apps() was called for the channel and the protocol.
     */
    bool HasConn2Apps(Ptr<QuantumChannel> channel, const std::string& protocol) const;

    /**
     * Find the pair of applications that run a protocol over a channel.
     *
     * @param channel The channel.
     * @param protocol Name of the protocol.
     * @return The applications at the source and at the destination.
     */
    std::pair<Ptr<Application>, Ptr<Application>> GetConn2Apps(Ptr<QuantumChannel> channel,
                                                               const std::string& protocol) const;

    /// @}

    /**
     * Assign a fixed random variable stream number to the random variable
     * that picks measurement outcomes.
     *
     * @param stream First stream index to use.
     * @return The number of stream indices assigned.
     */
    int64_t AssignStreams(int64_t stream);

    /**
     * TracedCallback signature for a measurement.
     *
     * @param owner Owner that measured.
     * @param qubit Name of the measured qubit.
     * @param outcome The outcome, 0 or 1.
     * @param probability Probability that the outcome had.
     */
    typedef void (*MeasureTracedCallback)(const std::string& owner,
                                          const std::string& qubit,
                                          unsigned outcome,
                                          double probability);

  protected:
    void DoDispose() override;
    void NotifyConstructionCompleted() override;

  private:
    /**
     * Get the memory of an owner.
     *
     * @param owner Owner name, possibly #QNS_GOD_OWNER.
     * @return The memory holding the qubits of the owner.
     */
    Ptr<QuantumMemory> GetMemory(const std::string& owner) const;

    /**
     * Do the bookkeeping for newly created qubits.
     *
     * @param owner Owner that created the qubits.
     * @param qubits Names of the qubits.
     */
    void RegisterQubits(const std::string& owner, const std::vector<std::string>& qubits);

    /**
     * Apply to a qubit the memory noise accumulated since it was last touched.
     *
     * @param qubit Name of the qubit.
     */
    void ApplyMemoryNoise(const std::string& qubit);

    /**
     * Apply to qubits the noise of a gate.
     *
     * @param owner Owner that applied the gate.
     * @param gate Name of the gate.
     * @param qubits Names of the qubits the gate acted on.
     */
    void ApplyGateNoise(const std::string& owner,
                        const std::string& gate,
                        const std::vector<std::string>& qubits);

    std::unique_ptr<QuantumNetworkSimulator> m_engine; //!< Joint state of all qubits

    std::vector<std::string> m_owners;               //!< Owner names, in order of addition
    std::map<std::string, Ptr<QuantumNode>> m_nodes; //!< Node of each owner
    Ptr<QuantumMemory> m_godMemory;                  //!< Qubits created by QNS_GOD_OWNER
    uint32_t m_ancillaCount{0};                      //!< Number of ancilla names allocated

    Ptr<QuantumErrorModel> m_defaultTimeModel;    //!< Memory noise unless set otherwise
    Ptr<QuantumErrorModel> m_defaultDephaseModel; //!< Gate noise unless set otherwise
    Ptr<QuantumErrorModel> m_defaultDepolarModel; //!< Channel noise unless set otherwise

    std::map<std::string, Time> m_qubitTime; //!< When each qubit was last touched
    std::map<std::string, Ptr<QuantumErrorModel>> m_qubitModel; //!< Memory noise of each qubit
    std::map<std::string, Ptr<QuantumErrorModel>>
        m_ownerModel; //!< Memory noise of new qubits, per owner
    /// Gate noise, per owner and gate.
    std::map<std::pair<std::string, std::string>, Ptr<QuantumErrorModel>> m_gateModel;
    /// Channel noise, per source and destination.
    std::map<std::pair<std::string, std::string>, Ptr<QuantumErrorModel>> m_channelModel;

    /// Applications per channel (source, destination) and protocol.
    std::map<std::pair<std::pair<std::string, std::string>, std::string>,
             std::pair<Ptr<Application>, Ptr<Application>>>
        m_apps;

    Ptr<UniformRandomVariable> m_random; //!< Picks measurement outcomes

    /// Trace fired at each measurement.
    TracedCallback<const std::string&, const std::string&, unsigned, double> m_measureTrace;
};

} // namespace ns3

#endif /* QUANTUM_PHY_ENTITY_H */
