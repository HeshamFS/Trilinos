// @HEADER
// ****************************************************************************
//                Tempus: Copyright (2017) Sandia Corporation
//
// Distributed under BSD 3-clause license (See accompanying file Copyright.txt)
// ****************************************************************************
// @HEADER

#include "Teuchos_UnitTestHarness.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Teuchos_TimeMonitor.hpp"

#include "Thyra_VectorStdOps.hpp"

#include "Tempus_IntegratorBasic.hpp"
#include "Tempus_IntegratorObserverLogging.hpp"
#include "Tempus_IntegratorObserverComposite.hpp"

#include "Tempus_StepperRKObserverLogging.hpp"
#include "Tempus_StepperRKObserverComposite.hpp"

#include "../TestModels/SinCosModel.hpp"
#include "../TestModels/VanDerPolModel.hpp"
#include "../TestUtils/Tempus_ConvergenceTestUtils.hpp"

#include <vector>

namespace Tempus_Test {

using Teuchos::RCP;
using Teuchos::rcp;
using Teuchos::ParameterList;
using Teuchos::sublist;
using Teuchos::getParametersFromXmlFile;

using Tempus::IntegratorBasic;
using Tempus::SolutionHistory;
using Tempus::SolutionState;


// ************************************************************
// ************************************************************
TEUCHOS_UNIT_TEST(Observer, IntegratorObserverLogging)
{
  // Read params from .xml file
  RCP<ParameterList> pList =
    getParametersFromXmlFile("Tempus_Observer_SinCos.xml");

  // Setup the SinCosModel
  RCP<ParameterList> scm_pl = sublist(pList, "SinCosModel", true);
  RCP<SinCosModel<double> > model =
    Teuchos::rcp(new SinCosModel<double> (scm_pl));

  // Setup the Integrator and reset initial time step
  RCP<ParameterList> pl = sublist(pList, "Tempus", true);
  RCP<Tempus::IntegratorBasic<double> > integrator =
    Tempus::integratorBasic<double>(pl, model);

  RCP<Tempus::IntegratorObserverLogging<double> > loggingObs =
    Teuchos::rcp(new Tempus::IntegratorObserverLogging<double>);
  integrator->setObserver(loggingObs);

  // Integrate to timeMax
  bool integratorStatus = integrator->advanceTime();
  TEST_ASSERT(integratorStatus)

  // Test if at 'Final Time'
  double time = integrator->getTime();
  double timeFinal = pl->sublist("Demo Integrator")
    .sublist("Time Step Control").get<double>("Final Time");
  TEST_FLOATING_EQUALITY(time, timeFinal, 1.0e-14);

  // Construct the reference counter and order for comparison.
  std::map<std::string,int> refCounters;
  std::list<std::string> refOrder;

  refCounters[loggingObs->nameObserveStartIntegrator_   ] = 1;
  refCounters[loggingObs->nameObserveStartTimeStep_     ] = 10;
  refCounters[loggingObs->nameObserveNextTimeStep_      ] = 10;
  refCounters[loggingObs->nameObserveBeforeTakeStep_    ] = 10;
  refCounters[loggingObs->nameObserveAfterTakeStep_     ] = 10;
  refCounters[loggingObs->nameObserveAfterCheckTimeStep_] = 10;
  refCounters[loggingObs->nameObserveEndTimeStep_       ] = 10;
  refCounters[loggingObs->nameObserveEndIntegrator_     ] = 1;

  refOrder.push_back(loggingObs->nameObserveStartIntegrator_ );
  for (int i=0 ; i<10; ++i) {
    refOrder.push_back(loggingObs->nameObserveStartTimeStep_     );
    refOrder.push_back(loggingObs->nameObserveNextTimeStep_      );
    refOrder.push_back(loggingObs->nameObserveBeforeTakeStep_    );
    refOrder.push_back(loggingObs->nameObserveAfterTakeStep_     );
    refOrder.push_back(loggingObs->nameObserveAfterCheckTimeStep_);
    refOrder.push_back(loggingObs->nameObserveEndTimeStep_       );
  }
  refOrder.push_back(loggingObs->nameObserveEndIntegrator_       );

  const std::map<std::string,int>& counters = *(loggingObs->getCounters());
  const std::list<std::string>&    order    = *(loggingObs->getOrder());

  // Compare against reference.
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveStartIntegrator_   )->second,
    refCounters.find(loggingObs->nameObserveStartIntegrator_   )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveStartTimeStep_     )->second,
    refCounters.find(loggingObs->nameObserveStartTimeStep_     )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveNextTimeStep_      )->second,
    refCounters.find(loggingObs->nameObserveNextTimeStep_      )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveBeforeTakeStep_    )->second,
    refCounters.find(loggingObs->nameObserveBeforeTakeStep_    )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveAfterTakeStep_     )->second,
    refCounters.find(loggingObs->nameObserveAfterTakeStep_     )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveAfterCheckTimeStep_)->second,
    refCounters.find(loggingObs->nameObserveAfterCheckTimeStep_)->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveEndTimeStep_       )->second,
    refCounters.find(loggingObs->nameObserveEndTimeStep_       )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveEndIntegrator_     )->second,
    refCounters.find(loggingObs->nameObserveEndIntegrator_     )->second);

  TEUCHOS_ASSERT(order.size() == refOrder.size());
  std::list<std::string>::const_iterator orderIter = order.begin();
  std::list<std::string>::const_iterator refOrderIter = refOrder.begin();
  for ( ; orderIter != order.end(); ++orderIter,++refOrderIter) {
    //std::cout << *orderIter << std::endl;
    TEST_EQUALITY(*orderIter, *refOrderIter);
  }

  // Test the reset.
  loggingObs->resetLogCounters();
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveStartIntegrator_   )->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveStartTimeStep_     )->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveNextTimeStep_      )->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveBeforeTakeStep_    )->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveAfterTakeStep_     )->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveAfterCheckTimeStep_)->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveEndTimeStep_       )->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveEndIntegrator_     )->second, 0);
  TEST_EQUALITY(order.size(), 0);

  Teuchos::TimeMonitor::summarize();
}

TEUCHOS_UNIT_TEST( Observer, IntegratorObserverComposite) {

  // Read params from .xml file
  RCP<ParameterList> pList =
    getParametersFromXmlFile("Tempus_Observer_SinCos.xml");

  // Setup the SinCosModel
  RCP<ParameterList> scm_pl = sublist(pList, "SinCosModel", true);
  RCP<SinCosModel<double> > model =
    Teuchos::rcp(new SinCosModel<double> (scm_pl));

  // Setup the Integrator and reset initial time step
  RCP<ParameterList> pl = sublist(pList, "Tempus", true);
  RCP<Tempus::IntegratorBasic<double> > integrator =
    Tempus::integratorBasic<double>(pl, model);

  RCP<Tempus::IntegratorObserverLogging<double> > loggingObs =
    Teuchos::rcp(new Tempus::IntegratorObserverLogging<double>);

  // creating another logging observer
  RCP<Tempus::IntegratorObserverLogging<double> > loggingObs2 =
    Teuchos::rcp(new Tempus::IntegratorObserverLogging<double>);

  RCP<Tempus::IntegratorObserverComposite<double> > compObs =
      Teuchos::rcp(new Tempus::IntegratorObserverComposite<double>);

  compObs->addObserver(loggingObs);
  compObs->addObserver(loggingObs2);

  compObs->observeStartIntegrator(*integrator);
  compObs->observeStartTimeStep(*integrator);
  compObs->observeBeforeTakeStep(*integrator);
  compObs->observeAfterTakeStep(*integrator);
  compObs->observeAfterCheckTimeStep(*integrator);
  compObs->observeEndTimeStep(*integrator);


  for (int i=0 ; i<10; ++i) {
      compObs->observeStartTimeStep(*integrator);
      compObs->observeBeforeTakeStep(*integrator);
      compObs->observeAfterTakeStep(*integrator);
      compObs->observeAfterCheckTimeStep(*integrator);
      compObs->observeEndTimeStep(*integrator);
  }
  compObs->observeEndIntegrator(*integrator);

 const std::map<std::string,int>& counters = *(loggingObs->getCounters());

  TEST_EQUALITY(
        counters.find(loggingObs->nameObserveStartIntegrator_    )->second, 1);
  TEST_EQUALITY(
        counters.find(loggingObs->nameObserveStartTimeStep_      )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs->nameObserveBeforeTakeStep_     )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs->nameObserveAfterTakeStep_      )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs->nameObserveAfterCheckTimeStep_ )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs->nameObserveEndTimeStep_        )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs->nameObserveEndIntegrator_      )->second, 1);
  TEST_EQUALITY(
        counters.find(loggingObs2->nameObserveStartIntegrator_   )->second, 1);
  TEST_EQUALITY(
        counters.find(loggingObs2->nameObserveStartTimeStep_     )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs2->nameObserveBeforeTakeStep_    )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs2->nameObserveAfterTakeStep_     )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs2->nameObserveAfterCheckTimeStep_)->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs2->nameObserveEndTimeStep_       )->second,11);
  TEST_EQUALITY(
        counters.find(loggingObs2->nameObserveEndIntegrator_     )->second, 1);
}


// ************************************************************
// ************************************************************
TEUCHOS_UNIT_TEST(Observer, StepperRKObserverLogging)
{

  // number of stages for the RK method
  const int erkStageCount = 4;

  // Read params from .xml file
  RCP<ParameterList> pList =
    getParametersFromXmlFile("Tempus_Observer_SinCos.xml");

  // Setup the SinCosModel
  RCP<ParameterList> scm_pl = sublist(pList, "SinCosModel", true);
  RCP<SinCosModel<double> > model =
    Teuchos::rcp(new SinCosModel<double> (scm_pl));

  // Setup the Integrator and reset initial time step
  RCP<ParameterList> pl = sublist(pList, "Tempus", true);
  pl->sublist("Demo Integrator").set("Stepper Name", "Demo RK Stepper");
  RCP<Tempus::IntegratorBasic<double> > integrator =
    Tempus::integratorBasic<double>(pl, model);

  RCP<Tempus::StepperRKObserverLogging<double> > loggingObs =
    Teuchos::rcp(new Tempus::StepperRKObserverLogging<double>);

  const auto stepper_observer =
      Teuchos::rcp_dynamic_cast<Tempus::StepperRKObserverComposite<double> >
      (integrator->getStepper()->getObserver(), true);

  stepper_observer->clearObservers();
  stepper_observer->addObserver(loggingObs);

  // Integrate to timeMax
  bool integratorStatus = integrator->advanceTime();
  TEST_ASSERT(integratorStatus)

  // Test if at 'Final Time'
  double time = integrator->getTime();
  const int numberTimeStep = integrator->getIndex();
  double timeFinal = pl->sublist("Demo Integrator")
    .sublist("Time Step Control").get<double>("Final Time");
  TEST_FLOATING_EQUALITY(time, timeFinal, 1.0e-14);

  // Construct the reference counter and order for comparison.
  std::map<std::string,int> refCounters;
  std::list<std::string> refOrder;

  refCounters[loggingObs->nameObserveBeginTakeStep_           ] = numberTimeStep;
  refCounters[loggingObs->nameObserveBeginStage_              ] = erkStageCount * numberTimeStep;
  refCounters[loggingObs->nameObserveBeforeImplicitExplicitly_] = erkStageCount * numberTimeStep;
  refCounters[loggingObs->nameObserveBeforeSolve_             ] = erkStageCount * numberTimeStep;
  refCounters[loggingObs->nameObserveAfterSolve_              ] = erkStageCount * numberTimeStep;
  refCounters[loggingObs->nameObserveBeforeExplicit_          ] = erkStageCount * numberTimeStep;
  refCounters[loggingObs->nameObserveEndStage_                ] = erkStageCount * numberTimeStep;
  refCounters[loggingObs->nameObserveEndTakeStep_             ] = numberTimeStep;

  for (int i=0 ; i< numberTimeStep; ++i) {
    refOrder.push_back(loggingObs->nameObserveBeginTakeStep_ );
    for (int j=0; j<erkStageCount; ++j){
      refOrder.push_back(loggingObs->nameObserveBeginStage_ );
      refOrder.push_back(loggingObs->nameObserveBeforeImplicitExplicitly_ );
      refOrder.push_back(loggingObs->nameObserveBeforeSolve_ );
      refOrder.push_back(loggingObs->nameObserveAfterSolve_ );
      refOrder.push_back(loggingObs->nameObserveBeforeExplicit_ );
      refOrder.push_back(loggingObs->nameObserveEndStage_ );
    }
    refOrder.push_back(loggingObs->nameObserveEndTakeStep_ );
  }

  const std::map<std::string,int>& counters = *(loggingObs->getCounters());
  const std::list<std::string>&    order    = *(loggingObs->getOrder());

  // Compare against reference.
  TEST_EQUALITY(
      counters.find(loggingObs->nameObserveBeginTakeStep_    )->second,
      refCounters.find(loggingObs->nameObserveBeginTakeStep_ )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveBeginStage_     )->second,
    refCounters.find(loggingObs->nameObserveBeginStage_     )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveBeforeImplicitExplicitly_ )->second,
    refCounters.find(loggingObs->nameObserveBeforeImplicitExplicitly_ )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveBeforeSolve_ )->second,
    refCounters.find(loggingObs->nameObserveBeforeSolve_ )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveAfterSolve_ )->second,
    refCounters.find(loggingObs->nameObserveAfterSolve_ )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveBeforeExplicit_ )->second,
    refCounters.find(loggingObs->nameObserveBeforeExplicit_ )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveEndStage_ )->second,
    refCounters.find(loggingObs->nameObserveEndStage_ )->second);
  TEST_EQUALITY(
       counters.find(loggingObs->nameObserveEndTakeStep_ )->second,
    refCounters.find(loggingObs->nameObserveEndTakeStep_ )->second);

  TEUCHOS_ASSERT(order.size() == refOrder.size());

  std::list<std::string>::const_iterator orderIter = order.begin();
  std::list<std::string>::const_iterator refOrderIter = refOrder.begin();

  for ( ; orderIter != order.end(); ++orderIter,++refOrderIter) {
    TEST_EQUALITY(*orderIter, *refOrderIter);
  }

  // Test the reset.
  loggingObs->resetLogCounters();
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveBeginTakeStep_)->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveBeginStage_)->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveBeforeImplicitExplicitly_)->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveBeforeSolve_)->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveAfterSolve_)->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveBeforeExplicit_)->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveEndStage_)->second, 0);
  TEST_EQUALITY(
    counters.find(loggingObs->nameObserveEndTakeStep_)->second, 0);
  TEST_EQUALITY(order.size(), 0);


  Teuchos::TimeMonitor::summarize();
}


} // namespace Tempus_Test
