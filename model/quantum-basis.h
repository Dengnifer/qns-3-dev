/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_BASIS_H
#define QUANTUM_BASIS_H

#include <cmath>
#include <complex>
#include <string>
#include <vector>

/**
 * @file
 * @ingroup quantum
 * Vocabulary shared by the whole quantum module: tensor data, reserved names,
 * and the standard states and gates.
 */

namespace ns3
{

/**
 * @defgroup quantum Quantum Network Simulation
 * Exact, noise-aware simulation of quantum network protocols.
 */

/**
 * @ingroup quantum
 * Flat storage of the complex entries of a state vector, a density matrix, or a
 * gate matrix.
 *
 * Basis states are numbered with the first qubit as the least significant bit.
 * Matrices are stored row by row: entry (row, column) of a matrix on n qubits
 * is at position row * 2^n + column.
 */
using QuantumTensorData = std::vector<std::complex<double>>;

/**
 * @ingroup quantum
 * Tolerance below which a real number is treated as zero.
 */
constexpr double QNS_EPSILON = 1e-6;

/**
 * @ingroup quantum
 * Prefix of every name that the module reserves for itself.
 *
 * Do not give qubits, gates or applications names starting with this prefix.
 */
inline const std::string QNS_PREFIX = "QNS_";

/// Prefix of the names of the built-in gates.
inline const std::string QNS_GATE_PREFIX = QNS_PREFIX + "GATE_";
/// Prefix of the names of the ancilla qubits allocated by QuantumPhyEntity.
inline const std::string QNS_ANCILLA_PREFIX = QNS_PREFIX + "ANCILLA";

/**
 * @ingroup quantum
 * Name of the pseudo owner that may operate on every qubit, without gate noise.
 *
 * The control-flow-adapted applications use it for the operations that stand
 * in for classical communication and therefore have no physical counterpart.
 */
inline const std::string QNS_GOD_OWNER = "God";

/// @name Names of the built-in gates
/// @{
inline const std::string QNS_GATE_I = QNS_GATE_PREFIX + "I";       //!< Identity
inline const std::string QNS_GATE_PX = QNS_GATE_PREFIX + "PX";     //!< Pauli X
inline const std::string QNS_GATE_PY = QNS_GATE_PREFIX + "PY";     //!< Pauli Y
inline const std::string QNS_GATE_PZ = QNS_GATE_PREFIX + "PZ";     //!< Pauli Z
inline const std::string QNS_GATE_H = QNS_GATE_PREFIX + "H";       //!< Hadamard
inline const std::string QNS_GATE_CNOT = QNS_GATE_PREFIX + "CNOT"; //!< Controlled NOT
inline const std::string QNS_GATE_CZ = QNS_GATE_PREFIX + "CZ";     //!< Controlled Z
inline const std::string QNS_GATE_SWAP = QNS_GATE_PREFIX + "SWAP"; //!< Swap
inline const std::string QNS_GATE_TOFF = QNS_GATE_PREFIX + "TOFF"; //!< Toffoli
inline const std::string QNS_GATE_QOR = QNS_GATE_PREFIX + "QOR";   //!< Quantum OR
/// @}

/// @name Names of the protocols, used to register and to address their applications
/// @{
inline const std::string QNS_APP_TELEP = QNS_PREFIX + "telep";       //!< Teleportation
inline const std::string QNS_APP_ENT_SWAP = QNS_PREFIX + "ent_swap"; //!< Entanglement swapping
inline const std::string QNS_APP_DISTILL = QNS_PREFIX + "distill";   //!< Distillation
inline const std::string QNS_APP_DISTILL_NESTED =
    QNS_PREFIX + "distill_nested"; //!< Nested distillation
/// @}

/// @name Standard states
/// @{

/// State vector of |0>.
inline const QuantumTensorData QNS_KET_0{{1.0, 0.0}, {0.0, 0.0}};
/// State vector of |1>.
inline const QuantumTensorData QNS_KET_1{{0.0, 0.0}, {1.0, 0.0}};
/// State vector of the Bell state (|00> + |11>) / sqrt(2).
inline const QuantumTensorData QNS_BELL{
    {M_SQRT1_2, 0.0},
    {0.0, 0.0},
    {0.0, 0.0},
    {M_SQRT1_2, 0.0},
};

/// @}

/**
 * @name Standard gates
 *
 * A two-qubit gate that distinguishes its qubits (CNOT) takes them in the order
 * {target, control}. The Toffoli and QOR gates take {target, control, control}.
 */
/// @{

/// Identity.
inline const QuantumTensorData QNS_PAULI_I{{1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}};
/// Pauli X.
inline const QuantumTensorData QNS_PAULI_X{{0.0, 0.0}, {1.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}};
/// Pauli Y.
inline const QuantumTensorData QNS_PAULI_Y{{0.0, 0.0}, {0.0, -1.0}, {0.0, 1.0}, {0.0, 0.0}};
/// Pauli Z.
inline const QuantumTensorData QNS_PAULI_Z{{1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {-1.0, 0.0}};
/// Projector |0><0|.
inline const QuantumTensorData QNS_PROJECTOR_0{{1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}};
/// Projector |1><1|.
inline const QuantumTensorData QNS_PROJECTOR_1{{0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}};
/// Hadamard.
inline const QuantumTensorData QNS_HADAMARD{
    {M_SQRT1_2, 0.0},
    {M_SQRT1_2, 0.0},
    {M_SQRT1_2, 0.0},
    {-M_SQRT1_2, 0.0},
};

// clang-format off

/// Controlled NOT.
inline const QuantumTensorData QNS_CNOT{
    {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0},
};

/// Controlled Z.
inline const QuantumTensorData QNS_CZ{
    {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {-1.0, 0.0},
};

/// Swap.
inline const QuantumTensorData QNS_SWAP{
    {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0},
};

/// Toffoli: flips the target when both controls are 1.
inline const QuantumTensorData QNS_TOFFOLI{
    {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0},
};

/// Quantum OR: flips the target when at least one control is 1.
inline const QuantumTensorData QNS_QOR{
    {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0},
    {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0},
};

// clang-format on

/// @}

/**
 * @ingroup quantum
 * Look up the matrix of a built-in gate.
 *
 * @param name Name of the gate.
 * @return The matrix of the gate, or nullptr if @p name is not a built-in gate.
 */
const QuantumTensorData* FindBuiltinGate(const std::string& name);

/**
 * @ingroup quantum
 * Build the density matrix of a Werner state.
 *
 * @param fidelity Fidelity of the state with respect to the Bell state #QNS_BELL.
 * @return The 4 x 4 density matrix
 *         fidelity |Bell><Bell| + (1 - fidelity) / 3 (I - |Bell><Bell|).
 */
QuantumTensorData MakeWernerState(double fidelity);

/**
 * @ingroup quantum
 * Count the qubits that a state vector or a square matrix acts on.
 *
 * @param size Number of entries, which must be a power of two.
 * @return The base-2 logarithm of @p size.
 */
unsigned CountQubits(std::size_t size);

/**
 * @ingroup quantum
 * Split a list of qubits into halves.
 *
 * @param qubits Names of the qubits.
 * @return The first half of @p qubits; the shorter half if the size is odd.
 */
std::vector<std::string> GetFirstHalf(const std::vector<std::string>& qubits);

/**
 * @ingroup quantum
 * Split a list of qubits into halves.
 *
 * @param qubits Names of the qubits.
 * @return The second half of @p qubits; the longer half if the size is odd.
 */
std::vector<std::string> GetSecondHalf(const std::vector<std::string>& qubits);

} // namespace ns3

#endif /* QUANTUM_BASIS_H */
