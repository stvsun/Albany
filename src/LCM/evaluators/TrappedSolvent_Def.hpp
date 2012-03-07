/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
*                                                                    *
* Notice: This computer software was prepared by Sandia Corporation, *
* hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
* the Department of Energy (DOE). All rights in the computer software*
* are reserved by DOE on behalf of the United States Government and  *
* the Contractor as provided in the Contract. You are authorized to  *
* use this computer software for Governmental purposes but it is not *
* to be released or distributed to the public. NEITHER THE GOVERNMENT*
* NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
* ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Andy Salinger, agsalin@sandia.gov                  *
\********************************************************************/


#include <fstream>
#include <Sacado_MathFunctions.hpp>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"
#include "Albany_Utils.hpp"

namespace LCM {

template<typename EvalT, typename Traits>
TrappedSolvent<EvalT, Traits>::
TrappedSolvent(Teuchos::ParameterList& p) :
trappedSolvent(p.get<std::string>("Trapped Solvent Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout"))
{
  Teuchos::ParameterList* elmd_list = 
    p.get<Teuchos::ParameterList*>("Parameter List");

  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  Teuchos::RCP<ParamLib> paramLib = 
    p.get< Teuchos::RCP<ParamLib> >("Parameter Library", Teuchos::null);

  std::string type = elmd_list->get("Trapped Solvent Type", "Constant");
  if (type == "Constant") {
    is_constant = true;
    constant_value = elmd_list->get("Value", 0.0);

    // Add Trapped Solvent as a Sacado-ized parameter
    new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
	"Trapped Solvent", this, paramLib);
  }
  else if (type == "Truncated KL Expansion") {
    is_constant = false;
    PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim>
      fx(p.get<string>("QP Coordinate Vector Name"), vector_dl);
    coordVec = fx;
    this->addDependentField(coordVec);

    exp_rf_kl = 
      Teuchos::rcp(new Stokhos::KL::ExponentialRandomField<MeshScalarT>(*elmd_list));
    int num_KL = exp_rf_kl->stochasticDimension();

    // Add KL random variables as Sacado-ized parameters
    rv.resize(num_KL);
    for (int i=0; i<num_KL; i++) {
      std::string ss = Albany::strint("Trapped Solvent KL Random Variable",i);
      new Sacado::ParameterRegistration<EvalT, SPL_Traits>(ss, this, paramLib);
      rv[i] = elmd_list->get(ss, 0.0);
    }
  }
  else {
	  TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
		       "Invalid Trapped Solvent type " << type);
  } 

  if ( p.isType<string>("eqps Name") ) {
     Teuchos::RCP<PHX::DataLayout> scalar_dl =
       p.get< Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout");
     PHX::MDField<ScalarT,Cell,QuadPoint>
       tp(p.get<string>("eqps Name"), scalar_dl);
     eqps = tp;
     this->addDependentField(eqps);
     AConstant = elmd_list->get("A Constant Value", 23.30);
     new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
                                 "A Constant Value", this, paramLib);
     BConstant = elmd_list->get("B Constant Value", 2.330);
          new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
                                      "B Constant Value", this, paramLib);
     CConstant = elmd_list->get("C Constant Value", 0.0);
               new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
                                           "C Constant Value", this, paramLib);

   }
   else {
     AConstant=23.3;
     BConstant=2.33;
     CConstant=0.0;
   }

  if ( p.isType<string>("Avogadro Number Name") ) {
       Teuchos::RCP<PHX::DataLayout> scalar_dl =
         p.get< Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout");
       PHX::MDField<ScalarT,Cell,QuadPoint>
         ap(p.get<string>("Avogadro Number Name"), scalar_dl);
       avogadroNum = ap;
       this->addDependentField(avogadroNum);

  }




  this->addEvaluatedField(trappedSolvent);
  this->setName("Trapped Solvent"+PHX::TypeString<EvalT>::value);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void TrappedSolvent<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(trappedSolvent,fm);
  if (!is_constant) this->utils.setFieldData(coordVec,fm);
  this->utils.setFieldData(avogadroNum,fm);
  this->utils.setFieldData(eqps,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void TrappedSolvent<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  std::size_t numCells = workset.numCells;

  if (is_constant) {
    for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
    	  trappedSolvent(cell,qp) = constant_value;
      }
    }
  }
  else {
    for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
	Teuchos::Array<MeshScalarT> point(numDims);
	for (std::size_t i=0; i<numDims; i++)
	  point[i] = Sacado::ScalarValue<MeshScalarT>::eval(coordVec(cell,qp,i));
			trappedSolvent(cell,qp) = exp_rf_kl->evaluate(point, rv);
      }
    }
  }
  for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
    	  trappedSolvent(cell,qp) = pow(10.0, AConstant-BConstant*exp(-1.0*CConstant*eqps(cell,qp)))/avogadroNum(cell,qp);

      }
   }
}


// **********************************************************************
template<typename EvalT,typename Traits>
typename TrappedSolvent<EvalT,Traits>::ScalarT&
TrappedSolvent<EvalT,Traits>::getValue(const std::string &n)
{
  if (n == "Trapped Solvent")
    return constant_value;
  else if (n == "A Constant Value")
     return AConstant;
  else if (n == "B Constant Value")
       return BConstant;
  else if (n == "C Constant Value")
       return CConstant;
  for (int i=0; i<rv.size(); i++) {
    if (n == Albany::strint("Trapped Solvent KL Random Variable",i))
      return rv[i];
  }
  TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
		     std::endl <<
		     "Error! Logic error in getting paramter " << n
		     << " in TrappedSolvent::getValue()" << std::endl);
  return constant_value;
}

// **********************************************************************
// **********************************************************************
}
