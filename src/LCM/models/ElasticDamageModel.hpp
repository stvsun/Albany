//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#if !defined(LCM_ElasticDamageModel_hpp)
#define LCM_ElasticDamageModel_hpp

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Albany_Layouts.hpp"
#include "LCM/models/ConstitutiveModel.hpp"

namespace LCM
{

//! \brief Constitutive Model Base Class
template<typename EvalT, typename Traits>
class ElasticDamageModel: public LCM::ConstitutiveModel<EvalT, Traits>
{
public:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  using ConstitutiveModel<EvalT, Traits>::num_dims_;
  using ConstitutiveModel<EvalT, Traits>::num_pts_;
  using ConstitutiveModel<EvalT, Traits>::field_name_map_;

  ///
  /// Constructor
  ///
  ElasticDamageModel(Teuchos::ParameterList* p,
      const Teuchos::RCP<Albany::Layouts>& dl);

  ///
  /// Virtual Destructorstructor
  ///
  virtual
  ~ElasticDamageModel()
  {};

  ///
  /// Method to compute the state (e.g. energy, stress, tangent)
  ///
  virtual
  void
  computeState(typename Traits::EvalData workset,
      std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > dep_fields,
      std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > eval_fields);

private:

  ///
  /// Private to prohibit copying
  ///
  ElasticDamageModel(const ElasticDamageModel&);

  ///
  /// Private to prohibit copying
  ///
  ElasticDamageModel& operator=(const ElasticDamageModel&);

  ///
  /// Damage parameters
  ///
  RealType max_damage_, saturation_;

};
}

#endif
