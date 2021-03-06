//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef TLPOROPLASTICITYPROBLEM_HPP
#define TLPOROPLASTICITYPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_ResponseUtilities.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "PHAL_AlbanyTraits.hpp"

namespace Albany {

  /*!
   * \brief Problem definition for total lagrangian Poroplasticity
   */
  class TLPoroPlasticityProblem : public Albany::AbstractProblem {
  public:
  
    //! Default constructor
    TLPoroPlasticityProblem(const Teuchos::RCP<Teuchos::ParameterList>& params,
			  const Teuchos::RCP<ParamLib>& paramLib,
			  const int numEq);

    //! Destructor
    virtual ~TLPoroPlasticityProblem();
    
    //! Return number of spatial dimensions
    virtual int spatialDimension() const { return numDim; }

    //! Build the PDE instantiations, boundary conditions, and initial solution
    virtual void buildProblem(
      Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
      StateManager& stateMgr);

    // Build evaluators
    virtual Teuchos::Array< Teuchos::RCP<const PHX::FieldTag> >
    buildEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    //! Each problem must generate it's list of valid parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

    void getAllocatedStates(
         Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > oldState_,
	 Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > newState_
	 ) const;

  private:

    //! Private to prohibit copying
    TLPoroPlasticityProblem(const TLPoroPlasticityProblem&);
    
    //! Private to prohibit copying
    TLPoroPlasticityProblem& operator=(const TLPoroPlasticityProblem&);

  public:

    //! Main problem setup routine. Not directly called, but indirectly by following functions
    template <typename EvalT> 
    Teuchos::RCP<const PHX::FieldTag>
    constructEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    void constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs);

  protected:

    //! Boundary conditions on source term
    bool haveSource;
    int T_offset;  //Position of T unknown in nodal DOFs
    int X_offset;  //Position of X unknown in nodal DOFs, followed by Y,Z
    int numDim;    //Number of spatial dimensions and displacement variable 

    std::string matModel;

    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > oldState;
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > newState;
  };
}

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_AlbanyTraits.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_ResponseUtilities.hpp"
#include "Albany_EvaluatorUtils.hpp"

#include "Time.hpp"
#include "Strain.hpp"
#include "DefGrad.hpp"
#include "PHAL_SaveStateField.hpp"
#include "Porosity.hpp"
#include "BiotCoefficient.hpp"
#include "BiotModulus.hpp"
#include "PHAL_ThermalConductivity.hpp"
#include "KCPermeability.hpp"
#include "ElasticModulus.hpp"
#include "ShearModulus.hpp"
#include "PoissonsRatio.hpp"
#include "GradientElementLength.hpp"

#include "PHAL_Source.hpp"
#include "TLPoroPlasticityResidMass.hpp"
#include "TLPoroPlasticityResidMomentum.hpp"
#include "PHAL_NSMaterialProperty.hpp"

#include "J2Stress.hpp"
#include "Neohookean.hpp"
#include "PisdWdF.hpp"
#include "HardeningModulus.hpp"
#include "YieldStrength.hpp"
#include "SaturationModulus.hpp"
#include "SaturationExponent.hpp"
#include "DislocationDensity.hpp"
#include "TLPoroStress.hpp"

#include "SurfaceBasis.hpp"
#include "SurfaceVectorJump.hpp"
#include "SurfaceVectorGradient.hpp"
#include "SurfaceVectorResidual.hpp"
#include "CurrentCoords.hpp"

template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
Albany::TLPoroPlasticityProblem::constructEvaluators(
  PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
  const Albany::MeshSpecsStruct& meshSpecs,
  Albany::StateManager& stateMgr,
  Albany::FieldManagerChoice fieldManagerChoice,
  const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
   using Teuchos::RCP;
   using Teuchos::rcp;
   using Teuchos::ParameterList;
   using PHX::DataLayout;
   using PHX::MDALayout;
   using std::vector;
   using PHAL::AlbanyTraits;

   // get the name of the current element block
   std::string elementBlockName = meshSpecs.ebName;

   RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&meshSpecs.ctd));
   RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
     intrepidBasis = Albany::getIntrepidBasis(meshSpecs.ctd);

   const int numNodes = intrepidBasis->getCardinality();
   const int worksetSize = meshSpecs.worksetSize;

   Intrepid::DefaultCubatureFactory<RealType> cubFactory;
   RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, meshSpecs.cubatureDegree);

   const int numQPts = cubature->getNumPoints();
   const int numVertices = cellType->getNodeCount();

   *out << "Field Dimensions: Workset=" << worksetSize 
        << ", Vertices= " << numVertices
        << ", Nodes= " << numNodes
        << ", QuadPts= " << numQPts
        << ", Dim= " << numDim << std::endl;


   // Construct standard FEM evaluators with standard field names                              
   RCP<Albany::Layouts> dl = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim));
   TEUCHOS_TEST_FOR_EXCEPTION(dl->vectorAndGradientLayoutsAreEquivalent==false, std::logic_error,
                              "Data Layout Usage in Mechanics problems assume vecDim = numDim");
   Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);
   std::string scatterName="Scatter PoreFluid";


   // ----------------------setup the solution field ---------------//

   // Displacement Variable
   Teuchos::ArrayRCP<std::string> dof_names(1);
   dof_names[0] = "Displacement";
   Teuchos::ArrayRCP<std::string> resid_names(1);
   resid_names[0] = dof_names[0]+" Residual";

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructDOFVecInterpolationEvaluator(dof_names[0], X_offset));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructDOFVecGradInterpolationEvaluator(dof_names[0], X_offset));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructGatherSolutionEvaluator_noTransient(true, dof_names, X_offset));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructScatterResidualEvaluator(true, resid_names, X_offset));

   // Pore Pressure Variable
   Teuchos::ArrayRCP<std::string> tdof_names(1);
   tdof_names[0] = "Pore Pressure";
   Teuchos::ArrayRCP<std::string> tdof_names_dot(1);
   tdof_names_dot[0] = tdof_names[0]+"_dot";
   Teuchos::ArrayRCP<std::string> tresid_names(1);
   tresid_names[0] = tdof_names[0]+" Residual";

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructDOFInterpolationEvaluator(tdof_names[0], T_offset));

   (evalUtils.constructDOFInterpolationEvaluator(tdof_names_dot[0], T_offset));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructDOFGradInterpolationEvaluator(tdof_names[0], T_offset));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructGatherSolutionEvaluator(false, tdof_names, tdof_names_dot, T_offset));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructScatterResidualEvaluator(false, tresid_names, T_offset, scatterName));

   // ----------------------setup the solution field ---------------//

   // General FEM stuff
   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructGatherCoordinateVectorEvaluator());

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructMapToPhysicalFrameEvaluator(cellType, cubature));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructComputeBasisFunctionsEvaluator(cellType, intrepidBasis, cubature));

   // Temporary variable used numerous times below
   Teuchos::RCP<PHX::Evaluator<AlbanyTraits> > ev;

   { // Time
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("Time Name", "Time");
     p->set<std::string>("Delta Time Name", " Delta Time");
     p->set< RCP<DataLayout> >("Workset Scalar Data Layout", dl->workset_scalar);
     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     p->set<bool>("Disable Transient", true);

     ev = rcp(new LCM::Time<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Time",dl->workset_scalar, dl->dummy, elementBlockName, "scalar", 0.0, true);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   // { // Constant Stabilization Parameter
   //   RCP<ParameterList> p = rcp(new ParameterList);

   //   p->set<std::string>("Material Property Name", "Stabilization Parameter");
   //   p->set< RCP<DataLayout> >("Data Layout", dl->qp_scalar);
   //   p->set<std::string>("Coordinate Vector Name", "Coord Vec");
   //   p->set< RCP<DataLayout> >("Coordinate Vector Data Layout", dl->qp_vector);
   //   p->set<RCP<ParamLib> >("Parameter Library", paramLib);
   //   Teuchos::ParameterList& paramList = params->sublist("Stabilization Parameter");
   //   p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

   //   ev = rcp(new PHAL::NSMaterialProperty<EvalT,AlbanyTraits>(*p));
   //   fm0.template registerEvaluator<EvalT>(ev);
   // }

   { // Element length in the direction of solution gradient
     RCP<ParameterList> p = rcp(new ParameterList("Gradient Element Length"));

     //Input
     p->set<std::string>("Unit Gradient QP Variable Name", "Pore Pressure Gradient");
     p->set<std::string>("Gradient BF Name", "Grad BF");

     //Output
     p->set<std::string>("Element Length Name", "Gradient Element Length");

     ev = rcp(new LCM::GradientElementLength<EvalT,AlbanyTraits>(*p,dl));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Gradient Element Length",dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 1.0);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }


   { // Strain
     RCP<ParameterList> p = rcp(new ParameterList("Strain"));

     //Input
     p->set<std::string>("Gradient QP Variable Name", "Displacement Gradient");

     //Output
     p->set<std::string>("Strain Name", "Strain"); //dl->qp_tensor also

     ev = rcp(new LCM::Strain<EvalT,AlbanyTraits>(*p,dl));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Strain",dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0,true);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   {  // Porosity
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("Porosity Name", "Porosity");
     p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Porosity");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);
     double initPorosity = paramList.get("Value", 0.0);

     // Setting this turns on dependence of strain and pore pressure)
     p->set<std::string>("Strain Name", "Strain");

     // porosity update based on Coussy's poromechanics (see p.79)
     p->set<std::string>("QP Pore Pressure Name", "Pore Pressure");
     p->set<std::string>("Biot Coefficient Name", "Biot Coefficient");

     ev = rcp(new LCM::Porosity<EvalT,AlbanyTraits>(*p,dl));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Porosity",dl->qp_scalar, dl->dummy,
    		                                                     elementBlockName, "scalar",
    		                                                     initPorosity, true);

     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }



   { // Biot Coefficient
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("Biot Coefficient Name", "Biot Coefficient");
     p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Biot Coefficient");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

     ev = rcp(new LCM::BiotCoefficient<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Biot Coefficient",dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 1.0);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Biot Modulus
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("Biot Modulus Name", "Biot Modulus");
     p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Biot Modulus");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

     // Setting this turns on linear dependence on porosity and Biot's coeffcient
     p->set<std::string>("Porosity Name", "Porosity");
     p->set<std::string>("Biot Coefficient Name", "Biot Coefficient");

     ev = rcp(new LCM::BiotModulus<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Biot Modulus",dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 1.0e20);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Thermal conductivity
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("QP Variable Name", "Thermal Conductivity");
     p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Thermal Conductivity");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

     ev = rcp(new PHAL::ThermalConductivity<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Kozeny-Carman Permeaiblity
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("Kozeny-Carman Permeability Name", "Kozeny-Carman Permeability");
     p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Kozeny-Carman Permeability");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

     // Setting this turns on Kozeny-Carman relation
     p->set<std::string>("Porosity Name", "Porosity");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

     ev = rcp(new LCM::KCPermeability<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Kozeny-Carman Permeability",dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 0.0);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   // Skeleton parameter

   { // Elastic Modulus
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("QP Variable Name", "Elastic Modulus");
     p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Elastic Modulus");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

     p->set<std::string>("Porosity Name", "Porosity"); // porosity is defined at Cubature points
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

     ev = rcp(new LCM::ElasticModulus<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Shear Modulus
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("QP Variable Name", "Shear Modulus");
     p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Shear Modulus");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

     ev = rcp(new LCM::ShearModulus<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Poissons Ratio 
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("QP Variable Name", "Poissons Ratio");
     p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
     p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Poissons Ratio");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

     // Setting this turns on linear dependence of nu on T, nu = nu_ + dnudT*T)
     //p->set<std::string>("QP Pore Pressure Name", "Pore Pressure");

     ev = rcp(new LCM::PoissonsRatio<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   if (matModel == "Neohookean")
   {
     { // Stress
       RCP<ParameterList> p = rcp(new ParameterList("Stress"));

       //Input
       p->set<std::string>("DefGrad Name", "Deformation Gradient");
       p->set<std::string>("Elastic Modulus Name", "Elastic Modulus");
       p->set<std::string>("Poissons Ratio Name", "Poissons Ratio");
       p->set<std::string>("DetDefGrad Name", "Jacobian");

       //Output
       p->set<std::string>("Stress Name", matModel);

       ev = rcp(new LCM::Neohookean<EvalT,AlbanyTraits>(*p,dl));
       fm0.template registerEvaluator<EvalT>(ev);
       p = stateMgr.registerStateVariable(matModel,dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
       ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
     }
   }
   else if (matModel == "Neohookean AD")
   {
     RCP<ParameterList> p = rcp(new ParameterList("Stress"));

     //Input
     p->set<std::string>("Elastic Modulus Name", "Elastic Modulus");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set<std::string>("Poissons Ratio Name", "Poissons Ratio");  // dl->qp_scalar also

     p->set<std::string>("DefGrad Name", "Deformation Gradient");
     p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

     //Output
     p->set<std::string>("Stress Name", matModel); //dl->qp_tensor also

     ev = rcp(new LCM::PisdWdF<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable(matModel,dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }
   else if (matModel == "J2")
   {
     { // Hardening Modulus
       RCP<ParameterList> p = rcp(new ParameterList);

       p->set<std::string>("QP Variable Name", "Hardening Modulus");
       p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
       p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
       p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
       p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

       p->set<RCP<ParamLib> >("Parameter Library", paramLib);
       Teuchos::ParameterList& paramList = params->sublist("Hardening Modulus");
       p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

       ev = rcp(new LCM::HardeningModulus<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
     }

     { // Yield Strength
       RCP<ParameterList> p = rcp(new ParameterList);

       p->set<std::string>("QP Variable Name", "Yield Strength");
       p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
       p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
       p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
       p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

       p->set<RCP<ParamLib> >("Parameter Library", paramLib);
       Teuchos::ParameterList& paramList = params->sublist("Yield Strength");
       p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

       ev = rcp(new LCM::YieldStrength<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
     }

     { // Saturation Modulus
       RCP<ParameterList> p = rcp(new ParameterList);

       p->set<std::string>("Saturation Modulus Name", "Saturation Modulus");
       p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
       p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
       p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
       p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

       p->set<RCP<ParamLib> >("Parameter Library", paramLib);
       Teuchos::ParameterList& paramList = params->sublist("Saturation Modulus");
       p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

       ev = rcp(new LCM::SaturationModulus<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
     }

     { // Saturation Exponent
       RCP<ParameterList> p = rcp(new ParameterList);

       p->set<std::string>("Saturation Exponent Name", "Saturation Exponent");
       p->set<std::string>("QP Coordinate Vector Name", "Coord Vec");
       p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
       p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
       p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

       p->set<RCP<ParamLib> >("Parameter Library", paramLib);
       Teuchos::ParameterList& paramList = params->sublist("Saturation Exponent");
       p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

       ev = rcp(new LCM::SaturationExponent<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
     }

     if ( numDim == 3 && params->get("Compute Dislocation Density Tensor", false) )
     { // Dislocation Density Tensor
       RCP<ParameterList> p = rcp(new ParameterList("Dislocation Density"));

       //Input
       p->set<std::string>("Fp Name", "Fp");
       p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
       p->set<std::string>("BF Name", "BF");
       p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);
       p->set<std::string>("Gradient BF Name", "Grad BF");
       p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

       //Output
       p->set<std::string>("Dislocation Density Name", "G"); //dl->qp_tensor also

       //Declare what state data will need to be saved (name, layout, init_type)
       ev = rcp(new LCM::DislocationDensity<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
       p = stateMgr.registerStateVariable("G",dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
       ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
     }

     {// Stress
       RCP<ParameterList> p = rcp(new ParameterList("Stress"));

       //Input
       p->set<std::string>("DefGrad Name", "Deformation Gradient");
       p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

       p->set<std::string>("Elastic Modulus Name", "Elastic Modulus");
       p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

       p->set<std::string>("Poissons Ratio Name", "Poissons Ratio");  // dl->qp_scalar also
       p->set<std::string>("Hardening Modulus Name", "Hardening Modulus"); // dl->qp_scalar also
       p->set<std::string>("Saturation Modulus Name", "Saturation Modulus"); // dl->qp_scalar also
       p->set<std::string>("Saturation Exponent Name", "Saturation Exponent"); // dl->qp_scalar also
       p->set<std::string>("Yield Strength Name", "Yield Strength"); // dl->qp_scalar also
       p->set<std::string>("DetDefGrad Name", "Jacobian");  // dl->qp_scalar also

       //Output
       p->set<std::string>("Stress Name", matModel); //dl->qp_tensor also
       p->set<std::string>("Fp Name", "Fp");  // dl->qp_tensor also
       p->set<std::string>("Eqps Name", "eqps");  // dl->qp_scalar also

       //Declare what state data will need to be saved (name, layout, init_type)

       ev = rcp(new LCM::J2Stress<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
       p = stateMgr.registerStateVariable(matModel,dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
       ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
       p = stateMgr.registerStateVariable("Fp",dl->qp_tensor, dl->dummy, elementBlockName, "identity", 1.0, true);
       ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
       p = stateMgr.registerStateVariable("eqps",dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 0.0, true);
       ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
       fm0.template registerEvaluator<EvalT>(ev);
     }
   }
   //   else
   //     TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
   // 			       "Unrecognized Material Name: " << matModel
   // 			       << "  Recognized names are : Neohookean and J2");


   { // Total Stress
     RCP<ParameterList> p = rcp(new ParameterList("Total Stress"));

     //Input
     p->set<std::string>("Stress Name", matModel);
     p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

     p->set<std::string>("DefGrad Name", "Deformation Gradient");
     p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);


     p->set<std::string>("Biot Coefficient Name", "Biot Coefficient");  // dl->qp_scalar also
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

     p->set<std::string>("QP Variable Name", "Pore Pressure");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);


     p->set<std::string>("DetDefGrad Name", "Jacobian");  // dl->qp_scalar also

     //Output
     p->set<std::string>("Total Stress Name", "Total Stress"); //dl->qp_tensor also


     ev = rcp(new LCM::TLPoroStress<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Total Stress",dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Pore Pressure",dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 0.0, true);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);


   }

   if (haveSource) { // Source
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<std::string>("Source Name", "Source");
     p->set<std::string>("QP Variable Name", "Pore Pressure");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     Teuchos::ParameterList& paramList = params->sublist("Source Functions");
     p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

     ev = rcp(new PHAL::Source<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Deformation Gradient
     RCP<ParameterList> p = rcp(new ParameterList("Deformation Gradient"));

     //Inputs: flags, weights, GradU
     const bool avgJ = params->get("avgJ", false);
     p->set<bool>("avgJ Name", avgJ);
     const bool volavgJ = params->get("volavgJ", false);
     p->set<bool>("volavgJ Name", volavgJ);
     const bool weighted_Volume_Averaged_J = params->get("weighted_Volume_Averaged_J", false);
     p->set<bool>("weighted_Volume_Averaged_J Name", weighted_Volume_Averaged_J);
     p->set<std::string>("Weights Name","Weights");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
     p->set<std::string>("Gradient QP Variable Name", "Displacement Gradient");
     p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

     //Outputs: F, J
     p->set<std::string>("DefGrad Name", "Deformation Gradient"); //dl->qp_tensor also
     p->set<std::string>("DetDefGrad Name", "Jacobian");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

     ev = rcp(new LCM::DefGrad<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Displacement Gradient",dl->qp_tensor,
                                        dl->dummy, elementBlockName, "identity",true);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Jacobian",
                                        dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 1.0,true);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Displacement Resid
     RCP<ParameterList> p = rcp(new ParameterList("Displacement Resid"));

     //Input
     p->set<std::string>("Total Stress Name", "Total Stress");
     p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

     p->set<std::string>("DefGrad Name", "Deformation Gradient");
     p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

     p->set<std::string>("DetDefGrad Name", "Jacobian");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

     p->set<std::string>("Weighted BF Name", "wBF");
     p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);


     p->set<std::string>("Weighted Gradient BF Name", "wGrad BF");
     p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

     p->set<bool>("Disable Transient", true);


     //Output
     p->set<std::string>("Residual Name", "Displacement Residual");
     p->set< RCP<DataLayout> >("Node Vector Data Layout", dl->node_vector);

     ev = rcp(new LCM::TLPoroPlasticityResidMomentum<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }



   { // Pore Pressure Resid
     RCP<ParameterList> p = rcp(new ParameterList("Pore Pressure Resid"));

     //Input

     // Input from nodal points
     p->set<std::string>("Weighted BF Name", "wBF");
     p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);

     p->set<bool>("Have Source", false);
     p->set<std::string>("Source Name", "Source");

     p->set<bool>("Have Absorption", false);

     p->set<bool>("Have Mechanics", true);

     // Input from cubature points
     p->set<std::string>("Element Length Name", "Gradient Element Length");
     p->set<std::string>("QP Pore Pressure Name", "Pore Pressure");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

     p->set<std::string>("QP Time Derivative Variable Name", "Pore Pressure");

     //p->set<std::string>("Material Property Name", "Stabilization Parameter");
     RealType stab_param(0.0);
     if ( params->isType<RealType>("Stabilization Parameter") ) {
       stab_param = params->get<RealType>("Stabilization Parameter");
     }
     p->set<RealType>("Stabilization Parameter", stab_param);
     p->set<std::string>("Thermal Conductivity Name", "Thermal Conductivity");
     p->set<std::string>("Porosity Name", "Porosity");
     p->set<std::string>("Kozeny-Carman Permeability Name", "Kozeny-Carman Permeability");
     p->set<std::string>("Biot Coefficient Name", "Biot Coefficient");
     p->set<std::string>("Biot Modulus Name", "Biot Modulus");

     p->set<std::string>("Gradient QP Variable Name", "Pore Pressure Gradient");
     p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

     p->set<std::string>("Weighted Gradient BF Name", "wGrad BF");
     p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

     // Inputs: X, Y at nodes, Cubature, and Basis
     p->set<std::string>("Coordinate Vector Name","Coord Vec");
     p->set< RCP<DataLayout> >("Coordinate Data Layout", dl->vertices_vector);
     p->set< RCP<Intrepid::Cubature<RealType> > >("Cubature", cubature);
     p->set<RCP<shards::CellTopology> >("Cell Type", cellType);

     p->set<std::string>("Weights Name","Weights");

     p->set<std::string>("Delta Time Name", " Delta Time");
     p->set< RCP<DataLayout> >("Workset Scalar Data Layout", dl->workset_scalar);

     p->set<std::string>("DefGrad Name", "Deformation Gradient");
     p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

     p->set<std::string>("DetDefGrad Name", "Jacobian");
     p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

     //Output
     p->set<std::string>("Residual Name", "Pore Pressure Residual");
     p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

     ev = rcp(new LCM::TLPoroPlasticityResidMass<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
     PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter", dl->dummy);
     fm0.requireField<EvalT>(res_tag);

     PHX::Tag<typename EvalT::ScalarT> res_tag2(scatterName, dl->dummy);
     fm0.requireField<EvalT>(res_tag2);

     return res_tag.clone();
   }
   else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM) {
     Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl);
     return respUtils.constructResponses(fm0, *responseList, stateMgr);
   }

   return Teuchos::null;
}

#endif // TLPOROPLASTICITYPROBLEM_HPP
