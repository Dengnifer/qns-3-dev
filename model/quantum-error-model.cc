/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-error-model.h"

#include "ns3/double.h"
#include "ns3/log.h"

#include <cmath>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumErrorModel");

namespace
{

/**
 * Build the dephasing accumulated in a memory.
 *
 * @param duration Time spent in the memory.
 * @param coherenceTime Coherence time of the memory.
 * @return A Pauli Z error with probability (1 - exp(-duration / coherenceTime)) / 2.
 */
QuantumOperation
MakeDephasing(Time duration, Time coherenceTime)
{
    const double probability =
        (1 - std::exp(-duration.GetSeconds() / coherenceTime.GetSeconds())) / 2;
    return {{"I", "PZ"}, {QNS_PAULI_I, QNS_PAULI_Z}, {1 - probability, probability}};
}

} // namespace

NS_OBJECT_ENSURE_REGISTERED(QuantumErrorModel);

TypeId
QuantumErrorModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::QuantumErrorModel").SetParent<Object>().SetGroupName("Quantum");
    return tid;
}

NS_OBJECT_ENSURE_REGISTERED(TimeModel);

TypeId
TimeModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TimeModel")
            .SetParent<QuantumErrorModel>()
            .SetGroupName("Quantum")
            .AddConstructor<TimeModel>()
            .AddAttribute("CoherenceTime",
                          "Coherence time of the memory; a longer time means less noise.",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&TimeModel::m_coherenceTime),
                          MakeTimeChecker(NanoSeconds(1)));
    return tid;
}

QuantumOperation
TimeModel::GetOperation(Time elapsed) const
{
    NS_LOG_FUNCTION(this << elapsed);
    return MakeDephasing(elapsed, m_coherenceTime);
}

std::string
TimeModel::GetDescription() const
{
    std::ostringstream oss;
    oss << "TimeModel with coherence time " << m_coherenceTime.As(Time::S);
    return oss.str();
}

NS_OBJECT_ENSURE_REGISTERED(DephaseModel);

TypeId
DephaseModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DephaseModel")
            .SetParent<QuantumErrorModel>()
            .SetGroupName("Quantum")
            .AddConstructor<DephaseModel>()
            .AddAttribute("CoherenceTime",
                          "Coherence time of the memory in which the gate is modeled to wait.",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&DephaseModel::m_coherenceTime),
                          MakeTimeChecker(NanoSeconds(1)))
            .AddAttribute("GateDuration",
                          "Duration of the wait that models the gate.",
                          TimeValue(MicroSeconds(200)),
                          MakeTimeAccessor(&DephaseModel::m_gateDuration),
                          MakeTimeChecker(Time(0)));
    return tid;
}

QuantumOperation
DephaseModel::GetOperation(Time elapsed) const
{
    NS_LOG_FUNCTION(this << elapsed);
    return MakeDephasing(m_gateDuration, m_coherenceTime);
}

std::string
DephaseModel::GetDescription() const
{
    std::ostringstream oss;
    oss << "DephaseModel with coherence time " << m_coherenceTime.As(Time::S)
        << " and gate duration " << m_gateDuration.As(Time::S);
    return oss.str();
}

NS_OBJECT_ENSURE_REGISTERED(DepolarModel);

TypeId
DepolarModel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DepolarModel")
                            .SetParent<QuantumErrorModel>()
                            .SetGroupName("Quantum")
                            .AddConstructor<DepolarModel>()
                            .AddAttribute("Fidelity",
                                          "Probability that the qubit is left alone.",
                                          DoubleValue(0.95),
                                          MakeDoubleAccessor(&DepolarModel::m_fidelity),
                                          MakeDoubleChecker<double>(0.0, 1.0));
    return tid;
}

QuantumOperation
DepolarModel::GetOperation(Time elapsed) const
{
    NS_LOG_FUNCTION(this << elapsed);
    const double error = (1 - m_fidelity) / 3;
    return {{"I", "PX", "PY", "PZ"},
            {QNS_PAULI_I, QNS_PAULI_X, QNS_PAULI_Y, QNS_PAULI_Z},
            {m_fidelity, error, error, error}};
}

std::string
DepolarModel::GetDescription() const
{
    std::ostringstream oss;
    oss << "DepolarModel with fidelity " << m_fidelity;
    return oss.str();
}

double
DepolarModel::GetFidelity() const
{
    return m_fidelity;
}

} // namespace ns3
