# The quantum module keeps the state of all qubits as a tensor network, which
# it contracts with the ExaTN library (https://github.com/ORNL-QCI/exatn).
# ExaTN is not part of ns-3. Tell CMake where it is installed with
#
#   ./ns3 configure -- -DExaTN_DIR=/path/to/exatn
#
# or with the environment variable ExaTN_DIR. The default is ~/.exatn, the
# install prefix that ExaTN itself uses by default. Without ExaTN the module
# is skipped and the rest of ns-3 builds as usual.
if(NOT ExaTN_DIR)
  if(DEFINED ENV{ExaTN_DIR})
    set(ExaTN_DIR "$ENV{ExaTN_DIR}")
  else()
    set(ExaTN_DIR "$ENV{HOME}/.exatn")
  endif()
endif()
set(quantum_exatn_dir "${ExaTN_DIR}")

# The package file of ExaTN appends compiler flags to CMAKE_CXX_FLAGS. Keep
# them out of the module, where they would disable the precompiled headers,
# and give them to the one source file that includes ExaTN headers.
set(quantum_cxx_flags "${CMAKE_CXX_FLAGS}")
find_package(ExaTN QUIET)
string(REPLACE "${quantum_cxx_flags}" "" quantum_exatn_flags "${CMAKE_CXX_FLAGS}")
separate_arguments(quantum_exatn_flags)
set(CMAKE_CXX_FLAGS "${quantum_cxx_flags}")
if(NOT ExaTN_FOUND)
  message(
    ${HIGHLIGHTED_STATUS}
    "Skipping contrib/quantum: ExaTN was not found in ${quantum_exatn_dir}; see contrib/quantum/README.md"
  )
  return()
endif()

include_directories(SYSTEM ${EXATN_INCLUDE_DIRS} ${EXATN_INCLUDE_ROOT})
set_source_files_properties(
  model/quantum-network-simulator.cc
  PROPERTIES COMPILE_OPTIONS "${quantum_exatn_flags}"
             SKIP_PRECOMPILE_HEADERS ON
)

build_lib(
  LIBNAME quantum
  SOURCE_FILES helper/distill-helper.cc
               helper/distill-nested-adapt-helper.cc
               helper/distill-nested-helper.cc
               helper/distribute-epr-helper.cc
               helper/ent-swap-adapt-helper.cc
               helper/ent-swap-adapt-local-helper.cc
               helper/ent-swap-helper.cc
               helper/quantum-application-helper.cc
               helper/quantum-net-stack-helper.cc
               helper/telep-adapt-helper.cc
               helper/telep-helper.cc
               helper/telep-lin-adapt-helper.cc
               model/distill-app.cc
               model/distill-nested-adapt-app.cc
               model/distill-nested-app.cc
               model/distribute-epr-protocol.cc
               model/ent-swap-adapt-app.cc
               model/ent-swap-adapt-local-app.cc
               model/ent-swap-app.cc
               model/quantum-application.cc
               model/quantum-basis.cc
               model/quantum-channel.cc
               model/quantum-error-model.cc
               model/quantum-memory.cc
               model/quantum-network-simulator.cc
               model/quantum-node.cc
               model/quantum-operation.cc
               model/quantum-phy-entity.cc
               model/qubit.cc
               model/telep-adapt-app.cc
               model/telep-app.cc
               model/telep-lin-adapt-app.cc
  HEADER_FILES helper/distill-helper.h
               helper/distill-nested-adapt-helper.h
               helper/distill-nested-helper.h
               helper/distribute-epr-helper.h
               helper/ent-swap-adapt-helper.h
               helper/ent-swap-adapt-local-helper.h
               helper/ent-swap-helper.h
               helper/quantum-application-helper.h
               helper/quantum-net-stack-helper.h
               helper/telep-adapt-helper.h
               helper/telep-helper.h
               helper/telep-lin-adapt-helper.h
               model/distill-app.h
               model/distill-nested-adapt-app.h
               model/distill-nested-app.h
               model/distribute-epr-protocol.h
               model/ent-swap-adapt-app.h
               model/ent-swap-adapt-local-app.h
               model/ent-swap-app.h
               model/quantum-application.h
               model/quantum-basis.h
               model/quantum-channel.h
               model/quantum-error-model.h
               model/quantum-memory.h
               model/quantum-network-simulator.h
               model/quantum-node.h
               model/quantum-operation.h
               model/quantum-phy-entity.h
               model/qubit.h
               model/telep-adapt-app.h
               model/telep-app.h
               model/telep-lin-adapt-app.h
  LIBRARIES_TO_LINK ${libinternet}
                    exatn::exatn
  TEST_SOURCES test/quantum-test-suite.cc
)
