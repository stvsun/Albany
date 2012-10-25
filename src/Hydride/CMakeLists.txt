##*****************************************************************//
##    Albany 2.0:  Copyright 2012 Sandia Corporation               //
##    This Software is released under the BSD license detailed     //
##    in the file "license.txt" in the top-level Albany directory  //
##*****************************************************************//
# HYDRIDE

# problems
  SET(SOURCES 
    problems/HydrideProblem.cpp
    )
  SET(HEADERS 
    problems/HydrideProblem.hpp
    )

IF(ALBANY_MPI)
  SET(SOURCES ${SOURCES}
    problems/MesoScaleLinkProblem.cpp
  )
  SET(HEADERS ${HEADERS}
    problems/MesoScaleLinkProblem.hpp
  )
ENDIF()

# evaluators
  SET(SOURCES ${SOURCES}
    evaluators/HydrideCResid.cpp
    evaluators/HydrideWResid.cpp
    evaluators/HydrideChemTerm.cpp
    evaluators/HydrideStress.cpp
    evaluators/HydrideStressTerm.cpp
    )
  SET(HEADERS ${HEADERS} 
    evaluators/HydrideCResid.hpp
    evaluators/HydrideCResid_Def.hpp
    evaluators/HydrideWResid.hpp
    evaluators/HydrideWResid_Def.hpp
    evaluators/HydrideChemTerm.hpp
    evaluators/HydrideChemTerm_Def.hpp
    evaluators/HydrideStress.hpp
    evaluators/HydrideStress_Def.hpp
    evaluators/HydrideStressTerm.hpp
    evaluators/HydrideStressTerm_Def.hpp
    )

IF(ALBANY_MPI)
  SET(SOURCES ${SOURCES}
    evaluators/MultiScaleStress.cpp
  )
  SET(HEADERS ${HEADERS}
    evaluators/MultiScaleStress.hpp
    evaluators/MultiScaleStress_Def.hpp
  )
ENDIF()


include_directories (${Trilinos_INCLUDE_DIRS}  ${Trilinos_TPL_INCLUDE_DIRS}
                     ${Albany_SOURCE_DIR}/src ${Albany_SOURCE_DIR}/src/evaluators 
                     ${Albany_SOURCE_DIR}/src/problems ${Albany_SOURCE_DIR}/src/responses 
                     ${Albany_SOURCE_DIR}/src/stk ${Albany_SOURCE_DIR}/src/LCM/ 
                     ${Albany_SOURCE_DIR}/src/LCM/evaluators ${Albany_SOURCE_DIR}/src/LCM/utils
                     ./problems ./evaluators
                    )

add_library(hydride ${Albany_LIBRARY_TYPE} ${SOURCES} ${HEADERS})