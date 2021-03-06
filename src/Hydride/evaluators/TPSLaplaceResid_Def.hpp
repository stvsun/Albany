//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "Sacado.hpp"
#include "Sacado_Traits.hpp"

namespace PHAL {

//**********************************************************************
template<typename EvalT, typename Traits>
TPSLaplaceResid<EvalT, Traits>::
TPSLaplaceResid(const Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl) :
  solnVec(p.get<std::string> ("Solution Vector Name"), dl->node_vector),
  cubature(p.get<Teuchos::RCP <Intrepid::Cubature<RealType> > >("Cubature")),
  cellType(p.get<Teuchos::RCP <shards::CellTopology> > ("Cell Type")),
  intrepidBasis(p.get<Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > > ("Intrepid Basis")),
  solnResidual(p.get<std::string> ("Residual Name"), dl->node_vector) {


  this->addDependentField(solnVec);
  this->addEvaluatedField(solnResidual);

  std::vector<PHX::DataLayout::size_type> dims;
  dl->node_qp_vector->dimensions(dims);
  worksetSize = dims[0];
  numNodes = dims[1];
  numQPs  = dims[2];
  numDims = dims[3];

  // Allocate Temporary FieldContainers
  grad_at_cub_points.resize(numNodes, numQPs, numDims);
  refPoints.resize(numQPs, numDims);
  refWeights.resize(numQPs);
  jacobian.resize(worksetSize, numQPs, numDims, numDims);
  jacobian_det.resize(worksetSize, numQPs);

  // Pre-Calculate reference element quantitites
  cubature->getCubature(refPoints, refWeights);
  intrepidBasis->getValues(grad_at_cub_points, refPoints, Intrepid::OPERATOR_GRAD);

  this->setName("LaplaceResid" + PHX::TypeString<EvalT>::value);

}

//**********************************************************************
template<typename EvalT, typename Traits>
void TPSLaplaceResid<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm) {
  this->utils.setFieldData(solnVec, fm);
  this->utils.setFieldData(solnResidual, fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void TPSLaplaceResid<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset) {

  // Note that all the integration operations are ScalarT! We need the partials of the Jacobian to show up in the
  // system Jacobian as the solution is a function of coordinates (it IS the coordinates!).

  // This adds significant time to the compile

  Intrepid::CellTools<ScalarT>::setJacobian(jacobian, refPoints, solnVec, *cellType);
  Intrepid::CellTools<ScalarT>::setJacobianDet(jacobian_det, jacobian);

   // Straight Laplace's equation evaluation for the nodal coord solution

    for(std::size_t cell = 0; cell < workset.numCells; ++cell) {
      for(std::size_t node_a = 0; node_a < numNodes; ++node_a) {

        for(std::size_t eq = 0; eq < numDims; eq++)  {

          solnResidual(cell, node_a, eq) = 0.0;

        }

        for(std::size_t qp = 0; qp < numQPs; ++qp) {
          for(std::size_t node_b = 0; node_b < numNodes; ++node_b) {

            ScalarT kk = 0.0;

            for(std::size_t i = 0; i < numDims; i++) {

              kk += grad_at_cub_points(node_a, qp, i) * grad_at_cub_points(node_b, qp, i);

            }

            for(std::size_t eq = 0; eq < numDims; eq++) {

              solnResidual(cell, node_a, eq) +=
                kk * solnVec(cell, node_b, eq) * jacobian_det(cell, qp) * refWeights(qp);

            }
          }
        }
      }
    }
}

//**********************************************************************
}

