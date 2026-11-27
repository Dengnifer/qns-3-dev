/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-network-simulator.h"

#include "quantum-operation.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exatn.hpp> // after the ns-3 headers: TAL-SH defines macros that break them
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <talshxx.hpp>
#include <thread>
#include <tuple>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumNetworkSimulator");

namespace
{

/// True between the initialization and the finalization of ExaTN.
bool g_exatnAlive = false;

/// Number of engines created so far in this process.
unsigned g_engineCount = 0;

/**
 * Start ExaTN on first use and arrange for it to be shut down at exit.
 *
 * ExaTN is started once per process, so that several engines can exist one
 * after another, as in a test suite.
 */
void
EnsureExatnInitialized()
{
    static bool initialized = false;
    if (initialized)
    {
        return;
    }
    initialized = true;
    // ExaTN runs every contraction on all cores by default. The tensors of a
    // network simulation are tiny, so starting the threads costs far more than
    // the contraction itself. The thread count can only be set from outside.
    if (!std::getenv("OMP_NUM_THREADS") && std::thread::hardware_concurrency() > 4)
    {
        std::cerr << "quantum: OMP_NUM_THREADS is not set, so ExaTN uses all "
                  << std::thread::hardware_concurrency()
                  << " cores; small tensor networks contract much faster with "
                     "OMP_NUM_THREADS=1"
                  << std::endl;
    }
#ifndef NO_GPU
    // A GPU build of ExaTN reserves most of the memory of every GPU it can see.
    if (!std::getenv("CUDA_VISIBLE_DEVICES"))
    {
        std::cerr << "quantum: CUDA_VISIBLE_DEVICES is not set, so ExaTN reserves memory on "
                     "every GPU of this machine; set it to the GPU to use"
                  << std::endl;
    }
#endif
    exatn::initialize();
    g_exatnAlive = true;
    std::atexit([]() {
        g_exatnAlive = false;
        exatn::finalize();
    });
}

/**
 * Join names for logging.
 *
 * @param names The names.
 * @return The names separated by commas.
 */
std::string
Join(const std::vector<std::string>& names)
{
    std::ostringstream oss;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        oss << (i == 0 ? "" : ", ") << names[i];
    }
    return oss.str();
}

/**
 * Format a density matrix for logging.
 *
 * @param data The density matrix.
 * @return One line of text per row of the matrix.
 */
std::string
FormatMatrix(const QuantumTensorData& data)
{
    const std::size_t dim = std::size_t{1} << (CountQubits(data.size()) / 2);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    for (std::size_t row = 0; row < dim; ++row)
    {
        oss << "\n ";
        for (std::size_t col = 0; col < dim; ++col)
        {
            const std::complex<double>& entry = data[row * dim + col];
            oss << " (" << entry.real() << (entry.imag() < 0 ? "" : "+") << entry.imag() << "i)";
        }
    }
    return oss.str();
}

/// A leg of a tensor in the network.
struct Leg
{
    unsigned tensor{0}; //!< Id of the tensor
    unsigned index{0};  //!< Position of the leg in the tensor
};

/**
 * The shape of a tensor network, for choosing a contraction order.
 *
 * Only the sizes matter here, not the entries. Sizes are base-2 logarithms,
 * so a tensor with n qubit legs has size n.
 */
class ContractionGraph
{
  public:
    /**
     * Read the shape of a network.
     * @param network The network.
     */
    explicit ContractionGraph(const exatn::TensorNetwork& network)
    {
        for (auto it = network.cbegin(); it != network.cend(); ++it)
        {
            if (it->first == 0)
            {
                continue; // the output tensor
            }
            Node& node = m_nodes[it->first];
            const std::vector<exatn::TensorLeg>& legs = it->second.getTensorLegs();
            for (unsigned i = 0; i < legs.size(); ++i)
            {
                const double extent = std::log2(static_cast<double>(it->second.getDimExtent(i)));
                node.size += extent;
                if (legs[i].getTensorId() != 0)
                {
                    node.bonds[legs[i].getTensorId()] += extent;
                }
            }
        }
    }

    /**
     * Get the tensors that are left.
     * @return The id and the size of each tensor.
     */
    std::vector<std::pair<unsigned, double>> GetTensors() const
    {
        std::vector<std::pair<unsigned, double>> tensors;
        tensors.reserve(m_nodes.size());
        for (const auto& [id, node] : m_nodes)
        {
            tensors.emplace_back(id, node.size);
        }
        return tensors;
    }

    /**
     * Get the tensors that share a leg with a tensor.
     * @param id Id of the tensor.
     * @return The ids of its neighbors.
     */
    std::vector<unsigned> GetNeighbors(unsigned id) const
    {
        std::vector<unsigned> neighbors;
        for (const auto& [other, extent] : m_nodes.at(id).bonds)
        {
            neighbors.push_back(other);
        }
        return neighbors;
    }

    /**
     * Check whether a tensor is still in the graph.
     * @param id Id of the tensor.
     * @return True if the tensor has not been contracted yet.
     */
    bool Has(unsigned id) const
    {
        return m_nodes.count(id) > 0;
    }

    /// What a greedy search minimizes at each step.
    enum class Rule
    {
        GROWTH, //!< Entries of the result minus entries of the two operands
        SIZE,   //!< Entries of the result
    };

    /**
     * Rate the contraction of two tensors; lower is better.
     *
     * @param rule What to minimize.
     * @param left Id of one tensor.
     * @param right Id of the other tensor.
     * @return The score of the contraction.
     */
    double Score(Rule rule, unsigned left, unsigned right) const
    {
        const Node& a = m_nodes.at(left);
        const Node& b = m_nodes.at(right);
        const double result = a.size + b.size - 2 * Shared(a, right);
        return rule == Rule::SIZE ? result
                                  : std::exp2(result) - std::exp2(a.size) - std::exp2(b.size);
    }

    /**
     * Replace two tensors by their contraction.
     *
     * @param result Id of the new tensor.
     * @param left Id of one tensor.
     * @param right Id of the other tensor.
     */
    void Contract(unsigned result, unsigned left, unsigned right)
    {
        const Node a = m_nodes.at(left);
        const Node b = m_nodes.at(right);
        const double shared = Shared(a, right);
        m_cost += std::exp2(a.size + b.size - shared);
        m_nodes.erase(left);
        m_nodes.erase(right);

        Node merged;
        merged.size = a.size + b.size - 2 * shared;
        for (const Node* operand : {&a, &b})
        {
            for (const auto& [other, extent] : operand->bonds)
            {
                if (other != left && other != right)
                {
                    merged.bonds[other] += extent;
                }
            }
        }
        for (const auto& [other, extent] : merged.bonds)
        {
            Node& neighbor = m_nodes.at(other);
            neighbor.bonds.erase(left);
            neighbor.bonds.erase(right);
            neighbor.bonds[result] = extent;
        }
        m_largest = std::max(m_largest, merged.size);
        m_nodes[result] = std::move(merged);
    }

    /**
     * Get the work done by the contractions so far.
     * @return The number of multiplications.
     */
    double GetCost() const
    {
        return m_cost;
    }

    /**
     * Get the size of the largest tensor produced so far.
     * @return The base-2 logarithm of its number of entries.
     */
    double GetLargest() const
    {
        return m_largest;
    }

  private:
    /// A tensor of the network.
    struct Node
    {
        double size{0};                   //!< Size of the tensor
        std::map<unsigned, double> bonds; //!< Size of the legs shared with each neighbor
    };

    /**
     * Get the size of the legs that a tensor shares with another one.
     *
     * @param node The tensor.
     * @param other Id of the other tensor.
     * @return The size of the shared legs, 0 if there are none.
     */
    static double Shared(const Node& node, unsigned other)
    {
        const auto it = node.bonds.find(other);
        return it == node.bonds.end() ? 0.0 : it->second;
    }

    std::map<unsigned, Node> m_nodes; //!< The tensors that are left
    double m_cost{0};                 //!< Multiplications so far
    double m_largest{0};              //!< Size of the largest tensor produced
};

} // namespace

/**
 * State of the engine that depends on ExaTN.
 */
class QuantumNetworkSimulator::Impl
{
  public:
    Impl();
    ~Impl();

    /**
     * Allocate a name for a new tensor.
     * @return A name that no other tensor of this process uses.
     */
    std::string AllocTensorName();

    /**
     * Register a tensor with ExaTN.
     *
     * @param extents Dimension of each leg.
     * @param data Entries of the tensor, first leg varying fastest.
     * @return The name of the tensor.
     */
    std::string CreateTensor(const std::vector<std::size_t>& extents,
                             const QuantumTensorData& data);

    /**
     * Get the tensor of a gate, registering it on first use.
     *
     * @param gate Name of the gate.
     * @param data Matrix of the gate; ignored for a built-in gate.
     * @return The name of the tensor.
     */
    std::string GetGateTensor(const std::string& gate, const QuantumTensorData& data);

    /**
     * Get the open mode of the network on the ket side of a qubit.
     *
     * @param network The main network or a copy of it.
     * @param qubit Name of the qubit.
     * @return The position of the mode in the output of the network.
     */
    unsigned KetMode(exatn::TensorNetwork& network, const std::string& qubit) const;

    /**
     * Get the open mode of the network on the bra side of a qubit.
     *
     * @param network The main network or a copy of it.
     * @param qubit Name of the qubit.
     * @return The position of the mode in the output of the network.
     */
    unsigned BraMode(exatn::TensorNetwork& network, const std::string& qubit) const;

    /**
     * Append a gate tensor to both sides of the main network.
     *
     * @param tensorName Name of the gate tensor.
     * @param qubits Names of the qubits the gate acts on.
     */
    void AppendGate(const std::string& tensorName, const std::vector<std::string>& qubits);

    /**
     * Connect the ket side and the bra side of a qubit.
     *
     * @param network The main network or a copy of it.
     * @param nextId Id to give to the appended tensor; incremented.
     * @param qubit Name of the qubit.
     */
    void TraceOut(exatn::TensorNetwork& network, unsigned& nextId, const std::string& qubit);

    /**
     * Compute the reduced density matrix of some qubits.
     *
     * @param qubits Names of the qubits.
     * @return The density matrix.
     */
    QuantumTensorData Reduce(const std::vector<std::string>& qubits);

    /**
     * Compute the probability that measuring a qubit yields 0.
     *
     * @param qubit Name of the qubit.
     * @return The probability, in [0, 1].
     */
    double ProbabilityOfZero(const std::string& qubit);

    /**
     * Project a qubit onto a measurement outcome and renormalize the state.
     *
     * @param qubit Name of the qubit.
     * @param outcome The outcome, 0 or 1.
     * @param probability Probability of the outcome; it must be positive.
     */
    void Project(const std::string& qubit, unsigned outcome, double probability);

    /**
     * Contract a network.
     *
     * @param network The network.
     * @param optimizer Name of the contraction order optimizer.
     * @return The entries of the resulting tensor.
     */
    QuantumTensorData Evaluate(exatn::TensorNetwork& network, const std::string& optimizer);

    /**
     * Build the contraction order of the "auto" optimizer.
     *
     * It builds several orders and keeps the one that needs the fewest
     * multiplications: the order of creation and two greedy orders.
     *
     * @param network The network.
     * @return The contraction sequence, in the format of ExaTN.
     */
    static std::vector<unsigned> MakeAutoSequence(exatn::TensorNetwork& network);

    /**
     * Build a greedy contraction order.
     *
     * At each step it contracts the two connected tensors with the best score.
     *
     * @param network The network.
     * @param rule What the score minimizes.
     * @return The contraction sequence, in the format of ExaTN.
     */
    static std::vector<unsigned> MakeGreedySequence(exatn::TensorNetwork& network,
                                                    ContractionGraph::Rule rule);

    /**
     * Compute the cost of a contraction order.
     *
     * @param network The network.
     * @param sequence The contraction sequence, in the format of ExaTN.
     * @return The number of multiplications, and the base-2 logarithm of the
     *         number of entries of the largest tensor.
     */
    static std::pair<double, double> RateSequence(const exatn::TensorNetwork& network,
                                                  const std::vector<unsigned>& sequence);

    /**
     * Build the contraction order of the "ascend" optimizer.
     *
     * @param network The network.
     * @return The contraction sequence, in the format of ExaTN.
     */
    static std::vector<unsigned> MakeAscendingSequence(exatn::TensorNetwork& network);

    /**
     * Abort unless qubits are valid.
     * @param qubits Names of the qubits.
     */
    void RequireValid(const std::vector<std::string>& qubits) const;

    exatn::TensorNetwork m_network;                  //!< Density matrix of all qubits
    unsigned m_nextTensorId{1};                      //!< Id of the next tensor of the network
    std::vector<std::string> m_allQubits;            //!< Every qubit created, in order
    std::set<std::string> m_validQubits;             //!< Qubits not traced out
    std::map<std::string, Leg> m_ket;                //!< Open ket leg of each qubit
    std::map<std::string, Leg> m_bra;                //!< Open bra leg of each qubit
    std::string m_namePrefix;                        //!< Prefix of the tensor names of this engine
    unsigned m_nameCount{0};                         //!< Number of tensor names allocated
    std::vector<std::string> m_tensors;              //!< Tensors registered with ExaTN
    std::map<std::string, std::string> m_gateTensor; //!< Tensor of each gate used so far
    std::map<std::string, QuantumTensorData> m_gateData; //!< Matrix of each custom gate
};

QuantumNetworkSimulator::Impl::Impl()
{
    EnsureExatnInitialized();
    m_namePrefix = QNS_PREFIX + "E" + std::to_string(g_engineCount++) + "_T";
    m_network.rename(AllocTensorName());
}

QuantumNetworkSimulator::Impl::~Impl()
{
    if (!g_exatnAlive)
    {
        return;
    }
    for (const auto& name : m_tensors)
    {
        exatn::destroyTensorSync(name);
    }
}

std::string
QuantumNetworkSimulator::Impl::AllocTensorName()
{
    return m_namePrefix + std::to_string(m_nameCount++);
}

std::string
QuantumNetworkSimulator::Impl::CreateTensor(const std::vector<std::size_t>& extents,
                                            const QuantumTensorData& data)
{
    const std::string name = AllocTensorName();
    bool ok = exatn::createTensor(name, exatn::TensorElementType::COMPLEX64, extents);
    NS_ABORT_MSG_UNLESS(ok, "ExaTN failed to create the tensor " << name);
    ok = exatn::initTensorData(name, data);
    NS_ABORT_MSG_UNLESS(ok, "ExaTN failed to initialize the tensor " << name);
    m_tensors.push_back(name);
    return name;
}

std::string
QuantumNetworkSimulator::Impl::GetGateTensor(const std::string& gate, const QuantumTensorData& data)
{
    const QuantumTensorData* builtin = FindBuiltinGate(gate);
    auto it = m_gateTensor.find(gate);
    if (it != m_gateTensor.end())
    {
        NS_ABORT_MSG_IF(!builtin && !data.empty() && data != m_gateData[gate],
                        "The gate " << gate << " was applied before with a different matrix");
        return it->second;
    }

    const QuantumTensorData& matrix = builtin ? *builtin : data;
    NS_ABORT_MSG_IF(matrix.empty(), "No matrix given for the unknown gate " << gate);
    const unsigned nLegs = CountQubits(matrix.size());
    NS_ABORT_MSG_IF(nLegs % 2 != 0, "The matrix of the gate " << gate << " is not square");
    NS_LOG_LOGIC("Registering the gate " << gate);
    const std::string name = CreateTensor(std::vector<std::size_t>(nLegs, 2), matrix);
    m_gateTensor[gate] = name;
    if (!builtin)
    {
        m_gateData[gate] = matrix;
    }
    return name;
}

unsigned
QuantumNetworkSimulator::Impl::KetMode(exatn::TensorNetwork& network,
                                       const std::string& qubit) const
{
    const Leg& leg = m_ket.at(qubit);
    return network.getTensorConn(leg.tensor)->getTensorLeg(leg.index).getDimensionId();
}

unsigned
QuantumNetworkSimulator::Impl::BraMode(exatn::TensorNetwork& network,
                                       const std::string& qubit) const
{
    const Leg& leg = m_bra.at(qubit);
    return network.getTensorConn(leg.tensor)->getTensorLeg(leg.index).getDimensionId();
}

void
QuantumNetworkSimulator::Impl::RequireValid(const std::vector<std::string>& qubits) const
{
    for (const auto& qubit : qubits)
    {
        NS_ABORT_MSG_UNLESS(m_validQubits.count(qubit),
                            "The qubit " << qubit << " does not exist or was traced out");
    }
}

void
QuantumNetworkSimulator::Impl::AppendGate(const std::string& tensorName,
                                          const std::vector<std::string>& qubits)
{
    const unsigned n = qubits.size();
    NS_ABORT_MSG_IF(exatn::getTensor(tensorName)->getRank() != 2 * n,
                    "A gate on " << exatn::getTensor(tensorName)->getRank() / 2
                                 << " qubit(s) cannot act on " << n << " qubit(s)");

    // The gate has the legs (in_0 .. in_{n-1}, out_0 .. out_{n-1}).
    std::vector<exatn::LegDirection> ketDirections(n, exatn::LegDirection::INWARD);
    ketDirections.insert(ketDirections.end(), n, exatn::LegDirection::OUTWARD);
    std::vector<exatn::LegDirection> braDirections(n, exatn::LegDirection::OUTWARD);
    braDirections.insert(braDirections.end(), n, exatn::LegDirection::INWARD);

    std::vector<std::pair<unsigned, unsigned>> pairing;
    pairing.reserve(n);
    for (unsigned i = 0; i < n; ++i)
    {
        pairing.emplace_back(KetMode(m_network, qubits[i]), i);
    }
    const unsigned ketId = m_nextTensorId++;
    m_network.appendTensor(ketId, exatn::getTensor(tensorName), pairing, ketDirections, false);
    for (unsigned i = 0; i < n; ++i)
    {
        m_ket[qubits[i]] = {ketId, n + i};
    }

    // Appending the ket tensor renumbered the open modes, so query them again.
    pairing.clear();
    for (unsigned i = 0; i < n; ++i)
    {
        pairing.emplace_back(BraMode(m_network, qubits[i]), i);
    }
    const unsigned braId = m_nextTensorId++;
    m_network.appendTensor(braId, exatn::getTensor(tensorName), pairing, braDirections, true);
    for (unsigned i = 0; i < n; ++i)
    {
        m_bra[qubits[i]] = {braId, n + i};
    }
}

void
QuantumNetworkSimulator::Impl::TraceOut(exatn::TensorNetwork& network,
                                        unsigned& nextId,
                                        const std::string& qubit)
{
    const std::string identity = GetGateTensor(QNS_GATE_I, {});
    network.appendTensor(nextId++,
                         exatn::getTensor(identity),
                         {{KetMode(network, qubit), 0}, {BraMode(network, qubit), 1}},
                         {exatn::LegDirection::INWARD, exatn::LegDirection::OUTWARD},
                         false);
}

QuantumTensorData
QuantumNetworkSimulator::Impl::Reduce(const std::vector<std::string>& qubits)
{
    RequireValid(qubits);

    exatn::TensorNetwork network = m_network;
    network.rename(AllocTensorName());
    unsigned nextId = m_nextTensorId;
    for (const auto& qubit : m_allQubits)
    {
        if (m_validQubits.count(qubit) &&
            std::find(qubits.begin(), qubits.end(), qubit) == qubits.end())
        {
            TraceOut(network, nextId, qubit);
        }
    }

    // ExaTN stores tensors with the first mode varying fastest. Listing the bra
    // modes first therefore yields the density matrix row by row, a row being
    // a ket index.
    std::vector<unsigned> order;
    order.reserve(qubits.size());
    for (const auto& qubit : qubits)
    {
        order.push_back(BraMode(network, qubit));
    }
    for (const auto& qubit : qubits)
    {
        order.push_back(KetMode(network, qubit));
    }
    network.reorderOutputModes(order);

    return Evaluate(network, "auto");
}

std::vector<unsigned>
QuantumNetworkSimulator::Impl::MakeGreedySequence(exatn::TensorNetwork& network,
                                                  ContractionGraph::Rule rule)
{
    /// A pair of tensors that may be contracted next.
    using Candidate = std::tuple<double, unsigned, unsigned>; // score, left, right
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> candidates;

    ContractionGraph graph(network);
    for (const auto& [id, size] : graph.GetTensors())
    {
        for (const unsigned other : graph.GetNeighbors(id))
        {
            if (id < other)
            {
                candidates.emplace(graph.Score(rule, id, other), id, other);
            }
        }
    }

    // Each triple reads (result, left, right). Intermediate results take the
    // ids after the largest id of the network.
    unsigned nextId = network.getMaxTensorId() + 1;
    std::vector<unsigned> sequence;
    sequence.reserve(3 * network.getNumTensors());
    auto contract = [&](unsigned left, unsigned right) {
        graph.Contract(nextId, left, right);
        sequence.insert(sequence.end(), {nextId, left, right});
        return nextId++;
    };

    while (!candidates.empty())
    {
        const auto [score, left, right] = candidates.top();
        candidates.pop();
        // A contraction never changes the score of the other pairs, so a
        // candidate is outdated only if one of its tensors is gone.
        if (!graph.Has(left) || !graph.Has(right))
        {
            continue;
        }
        const unsigned result = contract(left, right);
        for (const unsigned other : graph.GetNeighbors(result))
        {
            candidates.emplace(graph.Score(rule, other, result), other, result);
        }
    }

    // The tensors that are left share no leg: multiply them, smallest first.
    using Rest = std::pair<double, unsigned>; // size, id
    std::priority_queue<Rest, std::vector<Rest>, std::greater<>> rest;
    for (const auto& [id, size] : graph.GetTensors())
    {
        rest.emplace(size, id);
    }
    while (rest.size() > 1)
    {
        const Rest left = rest.top();
        rest.pop();
        const Rest right = rest.top();
        rest.pop();
        rest.emplace(left.first + right.first, contract(left.second, right.second));
    }

    // The last result is the output tensor, which has id 0.
    sequence[sequence.size() - 3] = 0;
    return sequence;
}

std::pair<double, double>
QuantumNetworkSimulator::Impl::RateSequence(const exatn::TensorNetwork& network,
                                            const std::vector<unsigned>& sequence)
{
    ContractionGraph graph(network);
    for (std::size_t i = 0; i + 2 < sequence.size(); i += 3)
    {
        // The last result has id 0, which no other tensor of the graph uses.
        graph.Contract(sequence[i], sequence[i + 1], sequence[i + 2]);
    }
    return {graph.GetCost(), graph.GetLargest()};
}

std::vector<unsigned>
QuantumNetworkSimulator::Impl::MakeAutoSequence(exatn::TensorNetwork& network)
{
    using Rule = ContractionGraph::Rule;
    std::vector<unsigned> best = MakeAscendingSequence(network);
    std::pair<double, double> bestCost = RateSequence(network, best);
    NS_LOG_DEBUG("Order of creation: " << bestCost.first << " multiplications");
    for (const Rule rule : {Rule::GROWTH, Rule::SIZE})
    {
        std::vector<unsigned> sequence = MakeGreedySequence(network, rule);
        const std::pair<double, double> cost = RateSequence(network, sequence);
        NS_LOG_DEBUG("Greedy order " << static_cast<int>(rule) << ": " << cost.first
                                     << " multiplications");
        if (cost < bestCost)
        {
            best = std::move(sequence);
            bestCost = cost;
        }
    }
    return best;
}

std::vector<unsigned>
QuantumNetworkSimulator::Impl::MakeAscendingSequence(exatn::TensorNetwork& network)
{
    // Each triple reads (result, left, right). Intermediate results take the
    // ids after the largest id of the network; the final result has id 0.
    const unsigned nTensors = network.getNumTensors();
    unsigned nextId = network.getMaxTensorId() + 1;
    std::vector<unsigned> sequence{nextId++, 1, 2};
    for (unsigned id = 3; id < nTensors; ++id, ++nextId)
    {
        sequence.insert(sequence.end(), {nextId, nextId - 1, id});
    }
    sequence.insert(sequence.end(), {0, nextId - 1, nTensors});
    return sequence;
}

QuantumTensorData
QuantumNetworkSimulator::Impl::Evaluate(exatn::TensorNetwork& network, const std::string& optimizer)
{
    static const std::set<std::string> exatnOptimizers{"dummy", "heuro", "greed", "metis", "cutnn"};
    static const std::set<std::string> ownOptimizers{"auto", "ascend"};
    NS_ABORT_MSG_UNLESS(exatnOptimizers.count(optimizer) || ownOptimizers.count(optimizer),
                        "Unknown contraction order optimizer " << optimizer);

    NS_LOG_INFO("Contracting " << network.getNumTensors() << " tensors with the " << optimizer
                               << " optimizer");
    const double start = exatn::Timer::timeInSecHR();
    network.collapseIsometries();
    if (exatnOptimizers.count(optimizer))
    {
        exatn::resetContrSeqOptimizer(optimizer);
    }
    else if (network.getMaxTensorId() < 3)
    {
        // The orders of this module need at least three tensors.
        exatn::resetContrSeqOptimizer("greed");
    }
    else
    {
        // ExaTN skips its own search when it is given an order.
        const std::vector<unsigned> sequence =
            optimizer == "auto" ? MakeAutoSequence(network) : MakeAscendingSequence(network);
        if (g_log.IsEnabled(LOG_INFO))
        {
            const std::pair<double, double> cost = RateSequence(network, sequence);
            NS_LOG_INFO("The order needs " << cost.first
                                           << " multiplications; its largest tensor has 2^"
                                           << cost.second << " entries");
        }
        network.importContractionSequence(sequence);
    }
    const bool ok = exatn::evaluateSync(network);
    NS_ABORT_MSG_UNLESS(ok, "ExaTN failed to contract the tensor network");
    NS_LOG_INFO("Contraction took " << exatn::Timer::timeInSecHR(start) << " s");

    const std::string resultName = network.getTensor(0)->getName();
    QuantumTensorData data;
    {
        auto result = exatn::getLocalTensor(resultName);
        const std::complex<double>* body = nullptr;
        NS_ABORT_MSG_UNLESS(result && result->getDataAccessHostConst(&body),
                            "ExaTN did not return the contracted tensor");
        data.assign(body, body + result->getVolume());
    }
    exatn::destroyTensorSync(resultName);
    return data;
}

QuantumNetworkSimulator::QuantumNetworkSimulator()
    : m_impl(std::make_unique<Impl>())
{
    NS_LOG_FUNCTION(this);
}

QuantumNetworkSimulator::~QuantumNetworkSimulator()
{
    NS_LOG_FUNCTION(this);
}

void
QuantumNetworkSimulator::GenerateQubitsPure(const std::string& owner,
                                            const QuantumTensorData& data,
                                            const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << owner << qubits.size());
    const unsigned n = CountQubits(data.size());
    NS_ABORT_MSG_IF(n != qubits.size(),
                    "A state vector of " << n << " qubit(s) given for " << qubits.size()
                                         << " name(s)");
    for (const auto& qubit : qubits)
    {
        NS_ABORT_MSG_IF(m_impl->m_ket.count(qubit), "The qubit " << qubit << " already exists");
    }
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << owner << " generates "
                      << Join(qubits));

    const std::string name = m_impl->CreateTensor(std::vector<std::size_t>(n, 2), data);
    const unsigned ketId = m_impl->m_nextTensorId++;
    m_impl->m_network.appendTensor(
        ketId,
        exatn::getTensor(name),
        {},
        std::vector<exatn::LegDirection>(n, exatn::LegDirection::OUTWARD),
        false);
    const unsigned braId = m_impl->m_nextTensorId++;
    m_impl->m_network.appendTensor(braId,
                                   exatn::getTensor(name),
                                   {},
                                   std::vector<exatn::LegDirection>(n, exatn::LegDirection::INWARD),
                                   true);
    for (unsigned i = 0; i < n; ++i)
    {
        m_impl->m_allQubits.push_back(qubits[i]);
        m_impl->m_validQubits.insert(qubits[i]);
        m_impl->m_ket[qubits[i]] = {ketId, i};
        m_impl->m_bra[qubits[i]] = {braId, i};
    }
}

void
QuantumNetworkSimulator::GenerateQubitsMixed(const std::string& owner,
                                             const QuantumTensorData& data,
                                             const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << owner << qubits.size());
    const unsigned nLegs = CountQubits(data.size());
    const unsigned n = nLegs / 2;
    NS_ABORT_MSG_IF(nLegs % 2 != 0 || n != qubits.size(),
                    "A density matrix with " << data.size() << " entries given for "
                                             << qubits.size() << " name(s)");
    for (const auto& qubit : qubits)
    {
        NS_ABORT_MSG_IF(m_impl->m_ket.count(qubit), "The qubit " << qubit << " already exists");
    }
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << owner << " generates "
                      << Join(qubits));

    // The tensor has the legs (bra_0 .. bra_{n-1}, ket_0 .. ket_{n-1}).
    const std::string name = m_impl->CreateTensor(std::vector<std::size_t>(nLegs, 2), data);
    std::vector<exatn::LegDirection> directions(n, exatn::LegDirection::INWARD);
    directions.insert(directions.end(), n, exatn::LegDirection::OUTWARD);
    const unsigned id = m_impl->m_nextTensorId++;
    m_impl->m_network.appendTensor(id, exatn::getTensor(name), {}, directions, false);
    for (unsigned i = 0; i < n; ++i)
    {
        m_impl->m_allQubits.push_back(qubits[i]);
        m_impl->m_validQubits.insert(qubits[i]);
        m_impl->m_ket[qubits[i]] = {id, n + i};
        m_impl->m_bra[qubits[i]] = {id, i};
    }
}

void
QuantumNetworkSimulator::ApplyGate(const std::string& owner,
                                   const std::string& gate,
                                   const QuantumTensorData& data,
                                   const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << owner << gate << qubits.size());
    m_impl->RequireValid(qubits);
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << owner << " applies " << gate
                      << " to " << Join(qubits));
    m_impl->AppendGate(m_impl->GetGateTensor(gate, data), qubits);
}

void
QuantumNetworkSimulator::ApplyOperation(const QuantumOperation& operation,
                                        const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << qubits.size());
    m_impl->RequireValid(qubits);
    const unsigned n = qubits.size();
    NS_ABORT_MSG_IF(operation.GetNQubits() != n,
                    "An operation on " << operation.GetNQubits() << " qubit(s) cannot act on " << n
                                       << " qubit(s)");
    NS_LOG_LOGIC("At " << Simulator::Now().As(Time::S) << " a quantum operation acts on "
                       << Join(qubits));

    // The tensor has the legs (in_0 .. in_{n-1}, out_0 .. out_{n-1}, kraus).
    // The Kraus leg joins the ket copy and the bra copy of the tensor.
    std::vector<std::size_t> extents(2 * n, 2);
    extents.push_back(operation.GetSize());
    QuantumTensorData flat;
    for (const auto& kraus : operation.GetKrausOperators())
    {
        flat.insert(flat.end(), kraus.begin(), kraus.end());
    }
    const std::string name = m_impl->CreateTensor(extents, flat);

    std::vector<exatn::LegDirection> ketDirections(n, exatn::LegDirection::INWARD);
    ketDirections.insert(ketDirections.end(), n + 1, exatn::LegDirection::OUTWARD);
    std::vector<exatn::LegDirection> braDirections(n, exatn::LegDirection::OUTWARD);
    braDirections.insert(braDirections.end(), n + 1, exatn::LegDirection::INWARD);

    exatn::TensorNetwork& network = m_impl->m_network;
    std::vector<std::pair<unsigned, unsigned>> pairing;
    pairing.reserve(n);
    for (unsigned i = 0; i < n; ++i)
    {
        pairing.emplace_back(m_impl->KetMode(network, qubits[i]), i);
    }
    const unsigned ketId = m_impl->m_nextTensorId++;
    network.appendTensor(ketId, exatn::getTensor(name), pairing, ketDirections, false);
    for (unsigned i = 0; i < n; ++i)
    {
        m_impl->m_ket[qubits[i]] = {ketId, n + i};
    }

    pairing.clear();
    pairing.emplace_back(network.getTensorConn(ketId)->getTensorLeg(2 * n).getDimensionId(), 2 * n);
    for (unsigned i = 0; i < n; ++i)
    {
        pairing.emplace_back(m_impl->BraMode(network, qubits[i]), i);
    }
    const unsigned braId = m_impl->m_nextTensorId++;
    network.appendTensor(braId, exatn::getTensor(name), pairing, braDirections, true);
    for (unsigned i = 0; i < n; ++i)
    {
        m_impl->m_bra[qubits[i]] = {braId, n + i};
    }
}

double
QuantumNetworkSimulator::Impl::ProbabilityOfZero(const std::string& qubit)
{
    RequireValid({qubit});

    // The probability of outcome 0 is the trace of P0 rho P0.
    exatn::TensorNetwork network = m_network;
    network.rename(AllocTensorName());
    unsigned nextId = m_nextTensorId;
    const std::string projector = GetGateTensor(QNS_GATE_PREFIX + "M0", QNS_PROJECTOR_0);
    const unsigned ketId = nextId++;
    network.appendTensor(ketId,
                         exatn::getTensor(projector),
                         {{KetMode(network, qubit), 0}},
                         {exatn::LegDirection::INWARD, exatn::LegDirection::OUTWARD},
                         false);
    network.appendTensor(nextId++,
                         exatn::getTensor(projector),
                         {{BraMode(network, qubit), 0},
                          {network.getTensorConn(ketId)->getTensorLeg(1).getDimensionId(), 1}},
                         {exatn::LegDirection::OUTWARD, exatn::LegDirection::INWARD},
                         true);
    for (const auto& other : m_allQubits)
    {
        if (other != qubit && m_validQubits.count(other))
        {
            TraceOut(network, nextId, other);
        }
    }
    const QuantumTensorData scalar = Evaluate(network, "auto");
    NS_ABORT_MSG_IF(scalar.size() != 1 || std::abs(scalar[0].imag()) > QNS_EPSILON,
                    "The probability of a measurement outcome is not a real number");
    return std::clamp(scalar[0].real(), 0.0, 1.0);
}

void
QuantumNetworkSimulator::Impl::Project(const std::string& qubit,
                                       unsigned outcome,
                                       double probability)
{
    NS_ABORT_MSG_IF(probability <= 0.0,
                    "Cannot project the qubit " << qubit << " onto the impossible outcome "
                                                << outcome);
    QuantumTensorData projection = outcome ? QNS_PROJECTOR_1 : QNS_PROJECTOR_0;
    const double scale = 1 / std::sqrt(probability);
    for (auto& entry : projection)
    {
        entry *= scale;
    }
    AppendGate(CreateTensor({2, 2}, projection), {qubit});
}

QuantumMeasurement
QuantumNetworkSimulator::Measure(const std::string& owner, const std::string& qubit, double sample)
{
    NS_LOG_FUNCTION(this << owner << qubit << sample);
    const double probability0 = m_impl->ProbabilityOfZero(qubit);
    QuantumMeasurement result;
    result.distribution = {probability0, 1 - probability0};
    result.outcome = sample < probability0 ? 0 : 1;
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << owner << " measures " << qubit
                      << " and gets " << result.outcome << " (probability of 0: " << probability0
                      << ")");
    m_impl->Project(qubit, result.outcome, result.distribution[result.outcome]);
    return result;
}

double
QuantumNetworkSimulator::PostSelect(const std::string& owner,
                                    const std::string& qubit,
                                    unsigned outcome)
{
    NS_LOG_FUNCTION(this << owner << qubit << outcome);
    NS_ABORT_MSG_IF(outcome > 1, "A measurement outcome is 0 or 1, got " << outcome);
    const double probability0 = m_impl->ProbabilityOfZero(qubit);
    const double probability = outcome ? 1 - probability0 : probability0;
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " " << owner << " post-selects " << qubit
                      << " on " << outcome << " (probability " << probability << ")");
    m_impl->Project(qubit, outcome, probability);
    return probability;
}

QuantumTensorData
QuantumNetworkSimulator::PeekDensityMatrix(const std::string& owner,
                                           const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << owner << qubits.size());
    const QuantumTensorData densityMatrix = m_impl->Reduce(qubits);
    NS_LOG_INFO(
        "At " << Simulator::Now().As(Time::S) << " " << owner << " peeks the density matrix of "
              << Join(qubits) << ":"
              << (qubits.size() < 4 ? FormatMatrix(densityMatrix) : " (too large to print)"));
    return densityMatrix;
}

void
QuantumNetworkSimulator::PartialTrace(const std::vector<std::string>& qubits)
{
    NS_LOG_FUNCTION(this << qubits.size());
    m_impl->RequireValid(qubits);
    NS_LOG_INFO("At " << Simulator::Now().As(Time::S) << " tracing out " << Join(qubits));
    for (const auto& qubit : qubits)
    {
        m_impl->TraceOut(m_impl->m_network, m_impl->m_nextTensorId, qubit);
    }
    for (const auto& qubit : qubits)
    {
        m_impl->m_validQubits.erase(qubit);
    }
}

QuantumTensorData
QuantumNetworkSimulator::Contract(const std::string& optimizer)
{
    NS_LOG_FUNCTION(this << optimizer);
    exatn::TensorNetwork& network = m_impl->m_network;
    const unsigned rank = network.getRank();
    NS_ABORT_MSG_IF(rank != 2 * m_impl->m_validQubits.size(),
                    "The tensor network has " << rank << " open legs for "
                                              << m_impl->m_validQubits.size() << " qubit(s)");

    // The contracted tensor keeps the open modes of the network in order.
    std::vector<exatn::LegDirection> directions(rank, exatn::LegDirection::UNDIRECT);
    for (const auto& qubit : m_impl->m_validQubits)
    {
        const unsigned ketMode = m_impl->KetMode(network, qubit);
        const unsigned braMode = m_impl->BraMode(network, qubit);
        m_impl->m_ket[qubit] = {1, ketMode};
        m_impl->m_bra[qubit] = {1, braMode};
        directions[ketMode] = exatn::LegDirection::OUTWARD;
        directions[braMode] = exatn::LegDirection::INWARD;
    }

    const QuantumTensorData data = m_impl->Evaluate(network, optimizer);
    const std::string name = m_impl->CreateTensor(std::vector<std::size_t>(rank, 2), data);

    network = exatn::TensorNetwork();
    network.rename(m_impl->AllocTensorName());
    m_impl->m_nextTensorId = 1;
    network.appendTensor(m_impl->m_nextTensorId++, exatn::getTensor(name), {}, directions, false);
    return data;
}

double
QuantumNetworkSimulator::CalculateFidelity(const std::pair<std::string, std::string>& epr)
{
    NS_LOG_FUNCTION(this << epr.first << epr.second);
    const QuantumTensorData rho = PeekDensityMatrix(QNS_GOD_OWNER, {epr.first, epr.second});
    // <Bell| rho |Bell> only involves the entries between |00> and |11>.
    const std::complex<double> fidelity = (rho[0] + rho[3] + rho[12] + rho[15]) / 2.0;
    NS_ABORT_MSG_IF(std::abs(fidelity.imag()) > QNS_EPSILON, "The fidelity is not a real number");
    NS_LOG_INFO("The fidelity of (" << epr.first << ", " << epr.second << ") is "
                                    << fidelity.real());
    return fidelity.real();
}

bool
QuantumNetworkSimulator::AreValid(const std::vector<std::string>& qubits) const
{
    return std::all_of(qubits.begin(), qubits.end(), [this](const std::string& qubit) {
        return m_impl->m_validQubits.count(qubit) > 0;
    });
}

std::vector<std::string>
QuantumNetworkSimulator::GetValidQubits() const
{
    std::vector<std::string> valid;
    for (const auto& qubit : m_impl->m_allQubits)
    {
        if (m_impl->m_validQubits.count(qubit))
        {
            valid.push_back(qubit);
        }
    }
    return valid;
}

std::size_t
QuantumNetworkSimulator::GetNTensors() const
{
    return m_impl->m_network.getNumTensors();
}

} // namespace ns3
