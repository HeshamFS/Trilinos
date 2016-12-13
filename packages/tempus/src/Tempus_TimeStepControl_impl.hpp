#ifndef Tempus_TimeStepControl_impl_hpp
#define Tempus_TimeStepControl_impl_hpp

// Teuchos
#include "Teuchos_ScalarTraits.hpp"
#include "Teuchos_StandardParameterEntryValidators.hpp"
#include "Teuchos_VerboseObjectParameterListHelpers.hpp"
#include "Teuchos_TimeMonitor.hpp"


namespace {

  //static std::string timeMin_name    = "Minimum Simulation Time";
  //static double      timeMin_default = 0.0;

  //static std::string timeMax_name    = "Maximum Simulation Time";
  //static double      timeMax_default = std::numeric_limits<double>::max();

  //static std::string dtMin_name    = "Minimum Time Step";
  //static double      dtMin_default = std::numeric_limits<double>::epsilon();

  //static std::string dtMax_name    = "Maximum Time Step";
  //static double      dtMax_default = std::numeric_limits<double>::max();

  //static std::string iStepMin_name    = "Minimum Time Step Index";
  //static int         iStepMin_default = 0;

  //static std::string iStepMax_name    = "Maximum Time Step Index";
  //static int         iStepMax_default = 1000000;

  //static std::string errorMaxAbs_name    = "Maximum Absolute Error";
  //static double      errorMaxAbs_default = 1.0e-08;

  //static std::string errorMaxRel_name    = "Maximum Relative Error";
  //static double      errorMaxRel_default = 1.0e-08;

  //static std::string orderMin_name    = "Minimum Time Integration Order";
  //static int         orderMin_default = 1;

  //static std::string orderMax_name    = "Maximum Time Integration Order";
  //static int         orderMax_default = 4;

  //static std::string Constant_name    = "Constant";
  //static std::string Variable_name    = "Variable";
  //static std::string stepType_name    = "Integrator Step Type";
  //static std::string stepType_default = Variable_name;

  //std::vector<std::string> stepType_names = {Constant_name, Variable_name};

  //const Teuchos::RCP<Teuchos::StringToIntegralParameterEntryValidator<Tempus::StepType> >
  //  stepTypeValidator = Teuchos::rcp(
  //      new Teuchos::StringToIntegralParameterEntryValidator<Tempus::StepType>(
  //        stepType_names,
  //        Teuchos::tuple<Tempus::StepType>(
  //          Tempus::CONSTANT_STEP_SIZE,
  //          Tempus::VARIABLE_STEP_SIZE),
  //        stepType_name));

  //static std::string outputTimeList_name         = "Output Time List";
  //static std::string outputTimeList_default      = "";
  //static std::string outputIndexList_name        = "Output Index List";
  //static std::string outputIndexList_default     = "";
  //static std::string outputTimeInterval_name     = "Output Time Interval";
  //static double      outputTimeInterval_default  =
  //  std::numeric_limits<double>::max();
  //static std::string outputIndexInterval_name    = "Output Index Interval";
  //static int         outputIndexInterval_default = 1000000;

  //static std::string nFailuresMax_name    =
  //  "Maximum Number of Stepper Failures";
  //static int         nFailuresMax_default = 10.0;
  //static std::string nConsecutiveFailuresMax_name    =
  //  "Maximum Number of Consecutive Stepper Failures";
  //static int         nConsecutiveFailuresMax_default = 5;

} // namespace


namespace Tempus {

// TimeStepControl definitions:
template<class Scalar>
TimeStepControl<Scalar>::TimeStepControl()
{
  pList_->validateParametersAndSetDefaults(*this->getValidParameters());
  this->setParameterList(pList_);
}

template<class Scalar>
TimeStepControl<Scalar>::TimeStepControl(
  Teuchos::RCP<Teuchos::ParameterList> pList)
{
  this->setParameterList(pList);
}

template<class Scalar>
TimeStepControl<Scalar>::TimeStepControl(const TimeStepControl<Scalar>& tsc_)
  : timeMin_      (tsc_.timeMin_    ),
    timeMax_      (tsc_.timeMax_    ),
    dtMin_        (tsc_.dtMin_      ),
    dtInit_       (tsc_.dtInit_     ),
    dtMax_        (tsc_.dtMax_      ),
    iStepMin_     (tsc_.iStepMin_   ),
    iStepMax_     (tsc_.iStepMax_   ),
    errorMaxAbs_  (tsc_.errorMaxAbs_),
    errorMaxRel_  (tsc_.errorMaxRel_),
    orderMin_     (tsc_.orderMin_   ),
    orderInit_    (tsc_.orderInit_  ),
    orderMax_     (tsc_.orderMax_   ),
    stepType_     (tsc_.stepType_   ),
    outputIndices_(tsc_.outputIndices_),
    outputTimes_  (tsc_.outputTimes_),
    nFailuresMax_ (tsc_.nFailuresMax_),
    nConsecutiveFailuresMax_(tsc_.nConsecutiveFailuresMax_),
    pList_        (tsc_.pList_      )
{}


template<class Scalar>
void TimeStepControl<Scalar>::getNextTimeStep(
  const Teuchos::RCP<SolutionHistory<Scalar> > & solutionHistory,
  Status & integratorStatus) const
{
  using Teuchos::RCP;

  TEMPUS_FUNC_TIME_MONITOR("Tempus::TimeStepControl::getNextTimeStep()");
  {
    RCP<SolutionState<Scalar> > workingState=solutionHistory->getWorkingState();
    RCP<SolutionStateMetaData<Scalar> > metaData_ = workingState->metaData_;
    const Scalar time = metaData_->getTime();
    const int iStep = metaData_->getIStep();
    const Scalar errorAbs = metaData_->getErrorAbs();
    const Scalar errorRel = metaData_->getErrorRel();
    int order = metaData_->getOrder();
    Scalar dt = metaData_->getDt();
    bool output = metaData_->getOutput();

    RCP<StepperState<Scalar> > stepperState = workingState->stepperState_;

    output = false;

    if (dt < dtMin_) {
      RCP<Teuchos::FancyOStream> out = this->getOStream();
      Teuchos::OSTab ostab(out,1,"getNextTimeStep");
      *out << "Warning - Time step size (=" << dt << ") is less than\n"
           << "  minimum time step size (=" << dtMin_ << ")\n."
           << "  Resetting to minimum time step size." << std::endl;
      dt = dtMin_;
    }

    if (stepType_ == "Constant") {

      // Stepper failure
      if (stepperState->stepperStatus_ == Status::FAILED) {
        if (order+1 <= orderMax_) {
          order++;
          RCP<Teuchos::FancyOStream> out = this->getOStream();
          Teuchos::OSTab ostab(out,1,"getNextTimeStep");
          *out << "Warning - Stepper failure with constant time step.\n"
               << "  Try increasing order.  order = " << order << std::endl;
        } else {
          RCP<Teuchos::FancyOStream> out = this->getOStream();
          Teuchos::OSTab ostab(out,1,"getNextTimeStep");
          *out << "Failure - Stepper failed and can not change time step size "
               << "or order!\n"
               << "    Time step type == CONSTANT_STEP_SIZE\n"
               << "    order = " << order << std::endl;
          integratorStatus = FAILED;
          return;
        }
      }

      // Absolute error failure
      if (errorAbs > errorMaxAbs_) {
        if (order+1 <= orderMax_) {
          order++;
          RCP<Teuchos::FancyOStream> out = this->getOStream();
          Teuchos::OSTab ostab(out,1,"getNextTimeStep");
          *out
            <<"Warning - Absolute error is too large with constant time step.\n"
            <<"  (errorAbs ="<<errorAbs<<") > (errorMaxAbs ="<<errorMaxAbs_<<")"
            <<"  Try increasing order.  order = " << order << std::endl;
        } else {
          RCP<Teuchos::FancyOStream> out = this->getOStream();
          Teuchos::OSTab ostab(out,1,"getNextTimeStep");
          *out
            <<"Failure - Absolute error failed and can not change time step "
            <<"size or order!\n"
            <<"  Time step type == CONSTANT_STEP_SIZE\n"
            <<"  order = " << order
            <<"  (errorAbs ="<<errorAbs<<") > (errorMaxAbs ="<<errorMaxAbs_<<")"
            << std::endl;
          integratorStatus = FAILED;
          return;
        }
      }

      // Relative error failure
      if (errorRel > errorMaxRel_) {
        if (order+1 <= orderMax_) {
          order++;
          RCP<Teuchos::FancyOStream> out = this->getOStream();
          Teuchos::OSTab ostab(out,1,"getNextTimeStep");
          *out
            <<"Warning - Relative error is too large with constant time step.\n"
            <<"  (errorRel ="<<errorRel<<") > (errorMaxRel ="<<errorMaxRel_<<")"
            <<"  Try increasing order.  order = " << order << std::endl;
        } else {
          RCP<Teuchos::FancyOStream> out = this->getOStream();
          Teuchos::OSTab ostab(out,1,"getNextTimeStep");
          *out
            <<"Failure - Relative error failed and can not change time step "
            <<"size or order!\n"
            <<"  Time step type == CONSTANT_STEP_SIZE\n"
            <<"  order = " << order
            <<"  (errorRel ="<<errorRel<<") > (errorMaxRel ="<<errorMaxRel_<<")"
            << std::endl;
          integratorStatus = FAILED;
          return;
        }
      }

      const Scalar relTol = 1.0e-14;
      if (time+dt < timeMin_*(1.0-relTol) || time+dt > timeMax_*(1.0+relTol)) {
        RCP<Teuchos::FancyOStream> out = this->getOStream();
        Teuchos::OSTab ostab(out,1,"getNextTimeStep");
        *out <<"Warning - Time step moves time outside desired time range.\n"
             <<"  [timeMin, timeMax] = [" << timeMin_ <<", "<< timeMax_ << "]\n"
             <<"  T + dt = "<< time <<" + "<< dt<<" = "<< time + dt <<"\n";
        output = true;
      }

      // Consistency checks
      TEUCHOS_TEST_FOR_EXCEPTION(
        (dt != dtInit_), std::out_of_range,
        "Error - ( dt = "<< dt <<") != ( dtInit = "<< dtInit_ <<" )!\n");

      TEUCHOS_TEST_FOR_EXCEPTION(
        (order < orderMin_ || order > orderMax_), std::out_of_range,
        "Error - Solution order is out of range and can not change "
        "time step size!\n"
        "    Time step type == CONSTANT_STEP_SIZE\n"
        "    [order_min, order_max] = [" <<orderMin_<< ", " <<orderMax_<< "]\n"
        "    order = " << order << "\n");

    } else { // VARIABLE_STEP_SIZE

      // \todo The following controls should be generalized to plugable options.
      if (stepperState->stepperStatus_ == Status::FAILED) dt /=2;
      if (errorAbs > errorMaxAbs_) dt /= 2;
      if (errorRel > errorMaxRel_) dt /= 2;
      if (order < orderMin_) dt *= 2;
      if (order > orderMax_) dt /= 2;

      if (dt < dtMin_) dt = dtMin_;
      if (dt > dtMax_) dt = dtMax_;

      if (time + dt > timeMax_ ) dt = timeMax_ - time;

      // Consistency checks
      TEUCHOS_TEST_FOR_EXCEPTION(
        (time + dt < timeMin_), std::out_of_range,
        "Error - Time step does not move time INTO time range.\n"
        "    [timeMin, timeMax] = [" << timeMin_ << ", " << timeMax_ << "]\n"
        "    T + dt = " << time <<" + "<< dt <<" = " << time + dt << "\n");

      TEUCHOS_TEST_FOR_EXCEPTION(
        (time + dt > timeMax_), std::out_of_range,
        "Error - Time step move time OUT OF time range.\n"
        "    [timeMin, timeMax] = [" << timeMin_ << ", " << timeMax_ << "]\n"
        "    T + dt = " << time <<" + "<< dt <<" = " << time + dt << "\n");
    }

    // Check if we need to output this step
    std::vector<int>::const_iterator it =
      std::find(outputIndices_.begin(), outputIndices_.end(), iStep+1);
    if (it != outputIndices_.end()) output = true;

    if (!output) {
      for (size_t i=0; i < outputTimes_.size(); ++i) {
        if (time < outputTimes_[i] && outputTimes_[i] <= time + dt) {
          output = true;
          if (stepType_ == "Variable") dt = outputTimes_[i] - time;
          break;
        }
      }
    }

    metaData_->setOrder(order);
    metaData_->setDt(dt);
    metaData_->setOutput(output);
  }
  return;
}


/// Test if time is within range: include timeMin and exclude timeMax.
template<class Scalar>
bool TimeStepControl<Scalar>::timeInRange(const Scalar time) const{
  const Scalar relTol = 1.0e-14;
  bool tir = (timeMin_*(1.0-relTol) <= time and time < timeMax_*(1.0-relTol));
  return tir;
}


template<class Scalar>
bool TimeStepControl<Scalar>::indexInRange(const int iStep) const{
  bool iir = (iStepMin_ <= iStep and iStep < iStepMax_);
  return iir;
}


template<class Scalar>
std::string TimeStepControl<Scalar>::description() const
{
  std::string name = "Tempus::TimeStepControl";
  return(name);
}


template<class Scalar>
void TimeStepControl<Scalar>::describe(
   Teuchos::FancyOStream               &out,
   const Teuchos::EVerbosityLevel      verbLevel) const
{
  if (verbLevel == Teuchos::VERB_EXTREME) {
    out << description() << "::describe:" << std::endl
        << "timeMin      = " << timeMin_      << std::endl
        << "timeMax      = " << timeMax_      << std::endl
        << "dtMin        = " << dtMin_        << std::endl
        << "dtInit       = " << dtInit_       << std::endl
        << "dtMax        = " << dtMax_        << std::endl
        << "iStepMin     = " << iStepMin_     << std::endl
        << "iStepMax     = " << iStepMax_     << std::endl
        << "orderMin     = " << orderMin_     << std::endl
        << "orderInit    = " << orderInit_    << std::endl
        << "orderMax     = " << orderMax_     << std::endl
        << "errorMaxAbs  = " << errorMaxAbs_  << std::endl
        << "errorMaxRel  = " << errorMaxRel_  << std::endl
        << "stepType     = " << stepType_     << std::endl
        << "nFailuresMax = " << nFailuresMax_ << std::endl
        << "nConsecutiveFailuresMax = " << nConsecutiveFailuresMax_ << std::endl
        << "pList        = " << pList_        << std::endl;
  }
}


template <class Scalar>
void TimeStepControl<Scalar>::setParameterList(
  Teuchos::RCP<Teuchos::ParameterList> const& pList)
{
  TEUCHOS_TEST_FOR_EXCEPT(is_null(pList));
  pList->validateParameters(*this->getValidParameters());
  pList->validateParametersAndSetDefaults(*this->getValidParameters());
  pList_ = pList;

  Teuchos::readVerboseObjectSublist(&*pList_,this);

  timeMin_     = pList_->get<double>("Initial Time");
  timeMax_     = pList_->get<double>("Final Time");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (timeMin_ > timeMax_ ), std::logic_error,
    "Error - Inconsistent time range.\n"
    "    (timeMin = "<<timeMin_<<") > (timeMax = "<<timeMax_<<")\n");

  dtMin_       = pList_->get<double>("Minimum Time Step");
  dtInit_      = pList_->get<double>("Initial Time Step");
  dtMax_       = pList_->get<double>("Maximum Time Step");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (dtMin_ < Teuchos::ScalarTraits<Scalar>::zero() ), std::logic_error,
    "Error - Negative minimum time step.  dtMin = "<<dtMin_<<")\n");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (dtMax_ < Teuchos::ScalarTraits<Scalar>::zero() ), std::logic_error,
    "Error - Negative maximum time step.  dtMax = "<<dtMax_<<")\n");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (dtMin_ > dtMax_ ), std::logic_error,
    "Error - Inconsistent time step range.\n"
    "    (dtMin = "<<dtMin_<<") > (dtMax = "<<dtMax_<<")\n");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (dtInit_ < Teuchos::ScalarTraits<Scalar>::zero() ), std::logic_error,
    "Error - Negative initial time step.  dtInit = "<<dtInit_<<")\n");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (dtInit_ < dtMin_ || dtInit_ > dtMax_ ),
    std::out_of_range,
    "Error - Initial time step is out of range.\n"
    << "    [dtMin, dtMax] = [" << dtMin_ << ", " << dtMax_ << "]\n"
    << "    dtInit = " << dtInit_ << "\n");

  iStepMin_    = pList_->get<int>("Initial Time Index");
  iStepMax_    = pList_->get<int>("Final Time Index");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (iStepMin_ > iStepMax_ ), std::logic_error,
    "Error - Inconsistent time index range.\n"
    "    (iStepMin = "<<iStepMin_<<") > (iStepMax = "<<iStepMax_<<")\n");

  errorMaxAbs_ = pList_->get<double>("Maximum Absolute Error");
  errorMaxRel_ = pList_->get<double>("Maximum Relative Error");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (errorMaxAbs_ < Teuchos::ScalarTraits<Scalar>::zero() ), std::logic_error,
    "Error - Negative maximum time step.  errorMaxAbs = "<<errorMaxAbs_<<")\n");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (errorMaxRel_ < Teuchos::ScalarTraits<Scalar>::zero() ), std::logic_error,
    "Error - Negative maximum time step.  errorMaxRel = "<<errorMaxRel_<<")\n");

  orderMin_    = pList_->get<int>("Minimum Order");
  orderInit_   = pList_->get<int>("Initial Order");
  orderMax_    = pList_->get<int>("Maximum Order");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (orderMin_ < Teuchos::ScalarTraits<Scalar>::zero() ), std::logic_error,
    "Error - Negative minimum order.  orderMin = "<<orderMin_<<")\n");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (orderMax_ < Teuchos::ScalarTraits<Scalar>::zero() ), std::logic_error,
    "Error - Negative maximum order.  orderMax = "<<orderMax_<<")\n");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (orderMin_ > orderMax_ ), std::logic_error,
    "Error - Inconsistent order range.\n"
    "    (orderMin = "<<orderMin_<<") > (orderMax = "<<orderMax_<<")\n");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (orderInit_ < orderMin_ || orderInit_ > orderMax_), std::out_of_range,
    "Error - Initial order is out of range.\n"
    << "    [orderMin, orderMax] = [" << orderMin_ << ", " << orderMax_ << "]\n"
    << "    order = " << orderInit_  << "\n");

  stepType_ = pList_->get<std::string>("Integrator Step Type", "Variable");
  TEUCHOS_TEST_FOR_EXCEPTION(
    (stepType_ != "Constant" and stepType_ != "Variable"), std::out_of_range,
      "Error - 'Integrator Step Type' does not equal none of these:\n"
    << "  'Constant' - Integrator will take constant time step sizes.\n"
    << "  'Variable' - Integrator will allow changes to the time step size.\n"
    << "  stepType = " << stepType_  << "\n");

  // Parse output times
  {
    outputTimes_.clear();
    std::string str = pList_->get<std::string>("Output Time List");
    std::string delimiters(",");
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
    while ((pos != std::string::npos) || (lastPos != std::string::npos)) {
      std::string token = str.substr(lastPos,pos-lastPos);
      outputTimes_.push_back(Scalar(std::stod(token)));
      if(pos==std::string::npos)
        break;

      lastPos = str.find_first_not_of(delimiters, pos);
      pos = str.find_first_of(delimiters, lastPos);
    }

    Scalar outputTimeInterval = pList_->get<double>("Output Time Interval");
    Scalar output_t = timeMin_;
    while (output_t <= timeMax_) {
      outputTimes_.push_back(output_t);
      output_t += outputTimeInterval;
    }

    // order output times
    std::sort(outputTimes_.begin(),outputTimes_.end());
  }

  // Parse output indices
  {
    outputIndices_.clear();
    std::string str = pList_->get<std::string>("Output Index List");
    std::string delimiters(",");
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
    while ((pos != std::string::npos) || (lastPos != std::string::npos)) {
      std::string token = str.substr(lastPos,pos-lastPos);
      outputIndices_.push_back(int(std::stoi(token)));
      if(pos==std::string::npos)
        break;

      lastPos = str.find_first_not_of(delimiters, pos);
      pos = str.find_first_of(delimiters, lastPos);
    }

    Scalar outputIndexInterval = pList_->get<int>("Output Index Interval");
    Scalar output_i = iStepMin_;
    while (output_i <= iStepMax_) {
      outputIndices_.push_back(output_i);
      output_i += outputIndexInterval;
    }

    // order output indices
    std::sort(outputIndices_.begin(),outputIndices_.end());
  }

  nFailuresMax_ = pList_->get<int>("Maximum Number of Stepper Failures");
  nConsecutiveFailuresMax_ = pList_->get<int>(
    "Maximum Number of Consecutive Stepper Failures");
  return;
}


template<class Scalar>
Teuchos::RCP<const Teuchos::ParameterList>
TimeStepControl<Scalar>::getValidParameters() const
{
  static Teuchos::RCP<Teuchos::ParameterList> validPL;

  if (is_null(validPL)) {

    Teuchos::RCP<Teuchos::ParameterList> pl = Teuchos::parameterList();
    Teuchos::setupVerboseObjectSublist(&*pl);

    const double stdMin = std::numeric_limits<double>::epsilon();
    const double stdMax = std::numeric_limits<double>::max();
    pl->set("Initial Time"      , 0.0    , "Initial time");
    pl->set("Final Time"        , stdMax , "Final time");
    pl->set("Initial Time Index", 0      , "Initial time index");
    pl->set("Final Time Index"  , 1000000, "Final time index");
    pl->set("Minimum Time Step" , stdMin , "Minimum time step size");
    pl->set("Initial Time Step" , stdMin , "Initial time step size");
    pl->set("Maximum Time Step" , stdMax , "Maximum time step size");
    pl->set("Maximum Absolute Error", 1.0e-08, "Maximum absolute error");
    pl->set("Maximum Relative Error", 1.0e-08, "Maximum relative error");
    pl->set("Minimum Order", 0,
      "Minimum time-integration order.  If set to zero (default), the\n"
      "Stepper minimum order is used.");
    pl->set("Initial Order", 0,
      "Initial time-integration order.  If set to zero (default), the\n"
      "Stepper minimum order is used.");
    pl->set("Maximum Order", 0,
      "Maximum time-integration order.  If set to zero (default), the\n"
      "Stepper maximum order is used.");

    pl->set("Integrator Step Type", "Variable",
      "'Integrator Step Type' indicates whether the Integrator will allow "
      "the time step to be modified.\n"
      "  'Constant' - Integrator will take constant time step sizes.\n"
      "  'Variable' - Integrator will allow changes to the time step size.\n");

    pl->set("Output Time List", "", "Comma deliminated list of output times");
    pl->set("Output Index List","", "Comma deliminated list of output indices");
    pl->set("Output Time Interval", stdMax, "Output time interval");
    pl->set("Output Index Interval", 1000000, "Output index interval");

    pl->set("Maximum Number of Stepper Failures", 10,
      "Maximum number of Stepper failures");
    pl->set("Maximum Number of Consecutive Stepper Failures", 5,
      "Maximum number of consecutive Stepper failures");

    validPL = pl;

  }
  return validPL;
}


template <class Scalar>
Teuchos::RCP<Teuchos::ParameterList>
TimeStepControl<Scalar>::getNonconstParameterList()
{
  return(pList_);
}


template <class Scalar>
Teuchos::RCP<Teuchos::ParameterList>
TimeStepControl<Scalar>::unsetParameterList()
{
  Teuchos::RCP<Teuchos::ParameterList> temp_plist = pList_;
  pList_ = Teuchos::null;
  return(temp_plist);
}


} // namespace Tempus
#endif // Tempus_TimeStepControl_impl_hpp
