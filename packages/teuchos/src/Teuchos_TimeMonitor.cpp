// @HEADER
// ***********************************************************************
//
//                    Teuchos: Common Tools Package
//                 Copyright (2004) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ***********************************************************************
// @HEADER

#include "Teuchos_TimeMonitor.hpp"
#include "Teuchos_CommHelpers.hpp"
#include "Teuchos_DefaultComm.hpp"
#include "Teuchos_TableColumn.hpp"
#include "Teuchos_TableFormat.hpp"
#include <functional>

namespace Teuchos {
  /**
   * \class MaxLoc
   \brief Teuchos version of MPI_MAXLOC.
   \author Mark Hoemmen
   
   \tparam Ordinal The template parameter of \c Comm.

   \tparam Packet A type with value semantics; the type on which to
     reduce.

   MPI_MAXLOC is a standard reduction operator provided by the MPI
   standard.  According to the standard, MPI_MAXLOC combines the
   (value, index) pairs (u,i) and (v,j) into (w,j), where \f$w =
   max(u,v)\f$, and
   \f[
     k = \begin{cases}
       i         & \text{if $u > v$}, \\
       \min(i,j) & \text{if $u = v$}, \\
       j         & \text{if $u < v$}. \\
     \end{cases}
   \f]
   This class implements the MPI_MAXLOC reduction operator for the
   Teuchos communication wrappers.
  ///
   What happens to NaN ("Not a Number")?  A NaN is neither less
   than, greater than, or equal to any floating-point number or any
   NaN.  We can alter the above definition slightly so that a
   MaxLoc reduction has a well-defined result in case the array
   contains a NaN:
   \f[
     w = \begin{cases}
       u     & \text{if $u > v$}, \\
       v     & \text{if $u < v$}. \\
       u     & \text{otherwise}. \\
     \end{cases}
   \f]
   and 
   \f[
     k = \begin{cases}
       i         & \text{if $u > v$}, \\
       j         & \text{if $u < v$}. \\
       \min(i,j) & \text{otherwise}. \\
     \end{cases}
   \f]
   Defining MaxLoc in this way ensures that for any array
   containing a NaN, the value (w) returned is the first NaN, and
   the index (k) returned is the index of the first NaN.
  */
  template<class Ordinal, class ScalarType, class IndexType>
  class MaxLoc : 
    public ValueTypeReductionOp<Ordinal, std::pair<ScalarType, IndexType> > {
  public:
    void 
    reduce (const Ordinal count,
	    const std::pair<ScalarType, IndexType> inBuffer[],
	    std::pair<ScalarType, IndexType> inoutBuffer[]) const;
  };

  template<class Ordinal>
  class MaxLoc<Ordinal, double, int> :
    public ValueTypeReductionOp<Ordinal, std::pair<double, int> > {
  public:
    void 
    reduce (const Ordinal count,
	    const std::pair<double, int> inBuffer[],
	    std::pair<double, int> inoutBuffer[]) const
    {
      for (Ordinal ind = 0; ind < count; ++ind) {
	const std::pair<double, int>& in = inBuffer[ind];
	std::pair<double, int>& inout = inoutBuffer[ind];

	if (in.first > inout.first) {
	  inout.first = in.first;
	  inout.second = in.second;
	} else if (in.first < inout.first) {
	  // Don't need to do anything; inout has the values.
	} else { // equal, or at least one is NaN.
	  inout.first = in.first;
	  inout.second = std::min (in.second, inout.second);
	}
      }
    }
  };

  /// \class MinLoc
  /// \brief Teuchos version of MPI_MINLOC.
  /// \author Mark Hoemmen
  ///
  /// \tparam Ordinal The template parameter of \c Comm.
  ///
  /// \tparam Packet A type with value semantics; the type on which to
  ///   reduce.
  ///
  /// MPI_MINLOC is a standard reduction operator provided by the MPI
  /// standard.  According to the standard, MPI_MINLOC combines the
  /// (value, index) pairs (u,i) and (v,j) into (w,j), where \f$w =
  /// min(u,v)\f$, and
  /// \f[
  ///   k = \begin{cases}
  ///     i         & \text{if $u < v$}, \\
  ///     \min(i,j) & \text{if $u = v$}, \\
  ///     j         & \text{if $u > v$}. \\
  ///   \end{cases}
  /// \f]
  /// This class implements the MPI_MINLOC reduction operator for the
  /// Teuchos communication wrappers.
  ///
  /// Refer to the note in the documentation of \c MaxLoc that
  /// explains how to adjust the above definition to produce
  /// well-defined results even if the array contains a NaN.
  template<class Ordinal, class ScalarType, class IndexType>
  class MinLoc : 
    public ValueTypeReductionOp<Ordinal, std::pair<ScalarType, IndexType> > {
  public:
    void 
    reduce (const Ordinal count,
	    const std::pair<ScalarType, IndexType> inBuffer[],
	    std::pair<ScalarType, IndexType> inoutBuffer[]) const;
  };

  template<class Ordinal>
  class MinLoc<Ordinal, double, int> :
    public ValueTypeReductionOp<Ordinal, std::pair<double, int> > {
  public:
    void 
    reduce (const Ordinal count,
	    const std::pair<double, int> inBuffer[],
	    std::pair<double, int> inoutBuffer[]) const
    {
      for (Ordinal ind = 0; ind < count; ++ind) {
	const std::pair<double, int>& in = inBuffer[ind];
	std::pair<double, int>& inout = inoutBuffer[ind];

	if (in.first < inout.first) {
	  inout.first = in.first;
	  inout.second = in.second;
	} else if (in.first > inout.first) {
	  // Don't need to do anything; inout has the values.
	} else { // equal, or at least one is NaN.
	  inout.first = in.first;
	  inout.second = std::min (in.second, inout.second);
	}
      }
    }
  };

  // Typedef used internally by TimeMonitor::summarize() and its
  // helper functions.
  typedef std::map<std::string, std::pair<double, int> > timer_map_t;

  TimeMonitor::TimeMonitor (Time& timer, bool reset) 
    : PerformanceMonitorBase<Time>(timer, reset)
  {
    if (!isRecursiveCall()) counter().start(reset);
  }

  TimeMonitor::~TimeMonitor() {
    if (!isRecursiveCall()) counter().stop();
  }

  void 
  TimeMonitor::zeroOutTimers()
  {
    const Array<RCP<Time> > timers = counters();
  
    // In debug mode, loop first to check whether any of the timers
    // are running, before resetting them.  This ensures that this
    // method satisfies the strong exception guarantee (either it
    // completes normally, or there are no side effects).
#ifdef TEUCHOS_DEBUG
    typedef Array<RCP<Time> >::size_type size_type;
    const size_type numTimers = timers.size();
    for (size_type i = 0; i < numTimers; ++i) 
      {
	Time &timer = *timers[i];
	// We throw a runtime_error rather than a logic_error, because
	// logic_error suggests a bug in the implementation of
	// TimeMonitor.  Calling zeroOutTimers() when a timer is
	// running is not TimeMonitor's fault.
	TEUCHOS_TEST_FOR_EXCEPTION(timer.isRunning(), std::runtime_error,
			   "The timer i = " << i << " with name \"" 
			   << timer.name() << "\" is currently running and may not "
			   "be reset.");
      }
#endif // TEUCHOS_DEBUG

    for (Array<RCP<Time> >::const_iterator it = timers.begin(); 
	 it != timers.end(); ++it)
      (*it)->reset ();
  }

  // An anonymous namespace is the standard way of limiting linkage of
  // its contained routines to file scope.
  namespace {
    // \brief Return an "empty" local timer datum.
    // 
    // "Empty" means the datum has zero elapsed time and zero call
    // count.  This function does not actually create a timer.
    //
    // \param name The timer's name.
    std::pair<std::string, std::pair<double, int> >
    makeEmptyTimerDatum (const std::string& name)
    {
      return std::make_pair (name, std::make_pair (double(0), int(0)));
    }

    // \brief Locally filter out timer data with zero call counts.
    //
    // \param timerData [in/out]
    void
    filterZeroData (timer_map_t& timerData)
    {
      timer_map_t newTimerData;
      for (timer_map_t::const_iterator it = timerData.begin(); 
	   it != timerData.end(); ++it)
	{
	  if (it->second.second > 0)
	    newTimerData[it->first] = it->second;
	}
      timerData.swap (newTimerData);
    }

    //
    // \brief Collect and sort local timer data by timer names.
    //
    void
    collectLocalTimerData (timer_map_t& localData,
			   const Array<RCP<Time> >& localCounters)
    {
      using std::make_pair;
      typedef timer_map_t::const_iterator const_iter_t;
      typedef timer_map_t::iterator iter_t;

      timer_map_t theLocalData;
      for (Array<RCP<Time> >::const_iterator it = localCounters.begin();
	   it != localCounters.end(); ++it)
	{
	  const std::string& name = (*it)->name();
	  const double timing = (*it)->totalElapsedTime();
	  const int numCalls = (*it)->numCalls();

	  // Merge timers with duplicate labels, by summing their
	  // total elapsed times and call counts.
	  iter_t loc = theLocalData.find (name);
	  if (loc == theLocalData.end())
	    // Use loc as an insertion location hint.
	    theLocalData.insert (loc, make_pair (name, make_pair (timing, numCalls)));
	  else
	    {
	      loc->second.first += timing;
	      loc->second.second += numCalls;
	    }
	}
      // This avoids copying the map, and also makes this method
      // satisfy the strong exception guarantee.
      localData.swap (theLocalData);
    }

    /// \brief Collect the local timer data and timer names from the timers.
    ///
    /// \param localTimerData [out] On output: timer data extracted
    ///   from localTimers.  Resized as necessary.  Contents on input
    ///   are ignored (should be empty on input).
    /// \param localTimerNames [out] On output: names of timers
    ///   extracted from localTimers.  Resized as necessary.  Contents
    ///   on input are ignored (should be empty on input).
    /// \param localTimers [in] List of local timers.
    /// \param writeZeroTimers [in] If true, filter out timers with zero
    ///   call counts.
    ///
    /// \return (local timer data, local timer names).
    void
    collectLocalTimerDataAndNames (timer_map_t& localTimerData,
				   Array<std::string>& localTimerNames,
				   ArrayView<const RCP<Time> > localTimers,
				   const bool writeZeroTimers)
    {
      // Collect and sort local timer data by timer names.
      collectLocalTimerData (localTimerData, localTimers);

      // Filter out zero data locally first.  This ensures that if we
      // are writing global stats, and if a timer name exists in the
      // set of global names, then that timer has a nonzero call count
      // on at least one MPI process.
      if (! writeZeroTimers) {
	filterZeroData (localTimerData);
      }

      // Extract the set of local timer names.  The std::map keeps
      // them sorted alphabetically.
      localTimerNames.reserve (localTimerData.size());
      for (timer_map_t::const_iterator it = localTimerData.begin(); 
	   it != localTimerData.end(); ++it) {
	localTimerNames.push_back (it->first);
      }
    }

    /// \brief Merge local timer data into global data.
    ///
    /// Call this method in summarize() only if the writeGlobalStats
    /// argument is true.
    ///
    /// \param globalTimerData [out] Result of merging localTimerData
    ///   over the processes in the given communicator.
    ///
    /// \param globalTimerNames [out] Names of the timers in
    ///   globalTimerData; same as the keys of the map.
    ///
    /// \param localTimerData [in/out] On input: the first return
    ///   value of \c collectLocalTimerDataAndNames().  On output, if
    ///   writeZeroTimers is true, data for timers with zero call
    ///   counts will be removed.
    ///
    /// \param localTimerNames [in/out] On input: the second return
    ///   value of \c collectLocalTimerDataAndNames().  On output, if
    ///   writeZeroTimers is true, names of timers with zero call
    ///   counts will be removed.
    ///
    /// \param comm [in] Communicator over which to merge.
    ///
    /// \param alwaysWriteLocal [in] If true, and if the local set of
    ///   timers differs from the global set of timers (either the
    ///   union or the intersection, depending on \c setOp), Proc 0
    ///   will create corresponding timer data in localTimerData with
    ///   zero elapsed times and call counts.  (This pads the output
    ///   so it fits in a tabular form.)
    void
    collectGlobalTimerData (timer_map_t& globalTimerData,
			    Array<std::string>& globalTimerNames,
			    timer_map_t& localTimerData,
			    Array<std::string>& localTimerNames,
			    Ptr<const Comm<int> > comm,
			    const bool alwaysWriteLocal,
			    const ECounterSetOp setOp)
    {
      // There may be some global timers that are not local timers on
      // the calling MPI process(es).  In that case, if
      // alwaysWriteLocal is true, then we need to fill in the
      // "missing" local timers.  That will ensure that both global
      // and local timer columns in the output table have the same
      // number of rows.  The collectLocalTimerDataAndNames() method
      // may have already filtered out local timers with zero call
      // counts (if its writeZeroTimers argument was false), but we
      // won't be filtering again.  Thus, any local timer data we
      // insert here won't get filtered out.
      //
      // Note that calling summarize() with writeZeroTimers == false
      // will still do what it says, even if we insert local timers
      // with zero call counts here.

      // This does the correct and inexpensive thing (just copies the
      // timer data) if numProcs == 1.  Otherwise, it initiates a
      // communication with \f$O(\log P)\f$ messages along the
      // critical path, where \f$P\f$ is the number of participating
      // processes.
      mergeCounterNames (*comm, localTimerNames, globalTimerNames, setOp);

#ifdef TEUCHOS_DEBUG
      {
	// Sanity check that all processes have the name number of
	// global timer names.
	const timer_map_t::size_type myNumGlobalNames = globalTimerNames.size();
	timer_map_t::size_type minNumGlobalNames = 0;
	timer_map_t::size_type maxNumGlobalNames = 0;
	reduceAll (*comm, REDUCE_MIN, myNumGlobalNames, 
		   outArg (minNumGlobalNames));
	reduceAll (*comm, REDUCE_MAX, myNumGlobalNames, 
		   outArg (maxNumGlobalNames));
	TEUCHOS_TEST_FOR_EXCEPTION(minNumGlobalNames != maxNumGlobalNames,
          std::logic_error, "Min # global timer names = " << minNumGlobalNames 
	  << " != max # global timer names = " << maxNumGlobalNames
	  << ".  Please report this bug to the Teuchos developers.");
	TEUCHOS_TEST_FOR_EXCEPTION(myNumGlobalNames != minNumGlobalNames,
	  std::logic_error, "My # global timer names = " << myNumGlobalNames 
	  << " != min # global timer names = " << minNumGlobalNames
	  << ".  Please report this bug to the Teuchos developers.");
      }
#endif // TEUCHOS_DEBUG

      // mergeCounterNames() just merges the counters' names, not
      // their actual data.  Now we need to fill globalTimerData with
      // this process' timer data for the timers in globalTimerNames.
      //
      // All processes need the full list of global timers, since
      // there may be some global timers that are not local timers.
      // That's why mergeCounterNames() has to be an all-reduce, not
      // just a reduction to Proc 0.
      //
      // Insertion optimization: if the iterator given to map::insert
      // points right before where we want to insert, insertion is
      // O(1).  globalTimerNames is sorted, so feeding the iterator
      // output of map::insert into the next invocation's input should
      // make the whole insertion O(N) where N is the number of
      // entries in globalTimerNames.
      timer_map_t::iterator globalMapIter = globalTimerData.begin();
      timer_map_t::iterator localMapIter;
      for (Array<string>::const_iterator it = globalTimerNames.begin(); 
	   it != globalTimerNames.end(); ++it) {
	const std::string& globalName = *it;
	localMapIter = localTimerData.find (globalName);

	if (localMapIter == localTimerData.end()) {
	  if (alwaysWriteLocal) {
	    // If there are some global timers that are not local
	    // timers, and if we want to print local timers, we insert
	    // a local timer datum with zero elapsed time and zero
	    // call count into localTimerData as well.  This will
	    // ensure that both global and local timer columns in the
	    // output table have the same number of rows.
	    //
	    // We really only need to do this on Proc 0, which is the
	    // only process that currently may print local timers.
	    // However, we do it on all processes, just in case
	    // someone later wants to modify this function to print
	    // out local timer data for some process other than Proc
	    // 0.  This extra computation won't affect the cost along
	    // the critical path, for future computations in which
	    // Proc 0 participates.
	    localMapIter = localTimerData.insert (localMapIter, makeEmptyTimerDatum (globalName));

	    // Make sure the missing global name gets added to the
	    // list of local names.  We'll re-sort the list of local
	    // names below.
	    localTimerNames.push_back (globalName);
	  }
	  // There's a global timer that's not a local timer.  Add it
	  // to our pre-merge version of the global timer data so that
	  // we can safely merge the global timer data later.
	  globalMapIter = globalTimerData.insert (globalMapIter, makeEmptyTimerDatum (globalName));
	}
	else {
	  // We have this global timer name in our local timer list.
	  // Fill in our pre-merge version of the global timer data
	  // with our local data.
	  globalMapIter = globalTimerData.insert (globalMapIter, std::make_pair (globalName, localMapIter->second));
	}
      }

      if (alwaysWriteLocal) {
	// Re-sort the list of local timer names, since we may have
	// inserted "missing" names above.
	std::sort (localTimerNames.begin(), localTimerNames.end());
      }

#ifdef TEUCHOS_DEBUG
      {
	// Sanity check that all processes have the name number of
	// global timers.
	const timer_map_t::size_type myNumGlobalTimers = globalTimerData.size();
	timer_map_t::size_type minNumGlobalTimers = 0;
	timer_map_t::size_type maxNumGlobalTimers = 0;
	reduceAll (*comm, REDUCE_MIN, myNumGlobalTimers, 
		   outArg (minNumGlobalTimers));
	reduceAll (*comm, REDUCE_MAX, myNumGlobalTimers, 
		   outArg (maxNumGlobalTimers));
	TEUCHOS_TEST_FOR_EXCEPTION(minNumGlobalTimers != maxNumGlobalTimers,
				   std::logic_error, "Min # global timers = " << minNumGlobalTimers 
				   << " != max # global timers = " << maxNumGlobalTimers
				   << ".  Please report this bug to the Teuchos developers.");
	TEUCHOS_TEST_FOR_EXCEPTION(myNumGlobalTimers != minNumGlobalTimers,
				   std::logic_error, "My # global timers = " << myNumGlobalTimers 
				   << " != min # global timers = " << minNumGlobalTimers
				   << ".  Please report this bug to the Teuchos developers.");
      }
#endif // TEUCHOS_DEBUG
    }

    /// \brief Compute global timer statistics.
    ///
    /// Currently, this function computes the "Min", "Mean", and "Max"
    /// timing for each timer.  Along with the min / max timing comes
    /// the call count of the process who had the min / max.  (If more
    /// than one process had the min / max, then the call count on the
    /// process with the smallest rank is reported.)  
    ///
    /// The "Mean" is an arithmetic mean of all timings that accounts
    /// for call counts.  Each timing is the sum over all calls.
    /// Thus, this mean equals the sum of the timing over all
    /// processes, divided by the sum of the call counts over all
    /// processes for that timing.  (We compute it a bit differently
    /// to help prevent overflow.)  Along with the mean timing comes
    /// the mean call count.  This may be fractional, and has no
    /// particular connection to the mean timing.
    ///
    /// \param statData [out] On output: Global timer statistics.  See
    ///   the \c stat_map_type typedef documentation for an explanation
    ///   of the data structure.
    ///
    /// \param statNames [out] On output: Each value in the statData
    ///   map is a vector.  That vector v has the same number of
    ///   entries as statNames.  statNames[k] is the name of the
    ///   statistic (e.g., "Min", "Mean", or "Max") stored as v[k].
    ///
    /// \param comm [in] Communicator over which to compute statistics.
    ///
    /// \param globalTimerData [in] Output with the same name of the
    ///   \c collectGlobalTimerData() function.  That function assures
    ///   that all processes have the same keys stored in this map.
    void
    computeGlobalTimerStats (stat_map_type& statData,
			     std::vector<std::string>& statNames,
			     Ptr<const Comm<int> > comm,
			     const timer_map_t& globalTimerData)
    {
      const int numTimers = static_cast<int> (globalTimerData.size());
      const int numProcs = comm->getSize();

      // Extract pre-reduction timings and call counts into a
      // sequential array.  This array will be in the same order as
      // the global timer names are in the map.
      Array<std::pair<double, int> > timingsAndCallCounts;
      timingsAndCallCounts.reserve (numTimers);
      for (timer_map_t::const_iterator it = globalTimerData.begin(); 
	   it != globalTimerData.end(); ++it) {
	timingsAndCallCounts.push_back (it->second);
      }

      // For each timer name, compute the min timing and its
      // corresponding call count.
      Array<std::pair<double, int> > minTimingsAndCallCounts (numTimers);
      if (numTimers > 0) {
	reduceAll (*comm, MinLoc<int, double, int>(), numTimers, 
		   &timingsAndCallCounts[0], &minTimingsAndCallCounts[0]);
      }

      // For each timer name, compute the max timing and its
      // corresponding call count.
      Array<std::pair<double, int> > maxTimingsAndCallCounts (numTimers);
      if (numTimers > 0) {
	reduceAll (*comm, MaxLoc<int, double, int>(), numTimers, 
		   &timingsAndCallCounts[0], &maxTimingsAndCallCounts[0]);
      }

      // For each timer name, compute the mean timing and the mean
      // call count.  The mean call count is reported as a double to
      // allow a fractional value.
      // 
      // Each local timing is really the total timing over all local
      // invocations.  The number of local invocations is the call
      // count.  Thus, the mean timing is really the sum of all the
      // timings (over all processes), divided by the sum of all the
      // call counts (over all processes).
      Array<double> meanTimings (numTimers);
      Array<double> meanCallCounts (numTimers);
      {
	// When summing, first scale by the number of processes.  This
	// avoids unnecessary overflow, and also gives us the mean
	// call count automatically.
	Array<double> scaledTimings (numTimers);
	Array<double> scaledCallCounts (numTimers);
	const double P = static_cast<double> (numProcs);
	for (int k = 0; k < numTimers; ++k) {
	  const double timing = timingsAndCallCounts[k].first;
	  const double callCount = static_cast<double> (timingsAndCallCounts[k].second);

	  scaledTimings[k] = timing / P;
	  scaledCallCounts[k] = callCount / P;
	}
	if (numTimers > 0) {
	  reduceAll (*comm, REDUCE_SUM, numTimers, &scaledTimings[0], &meanTimings[0]);
	  reduceAll (*comm, REDUCE_SUM, numTimers, &scaledCallCounts[0], &meanCallCounts[0]);
	}
	// We don't have to undo the scaling for the mean timings;
	// just divide by the scaled call count.
	for (int k = 0; k < numTimers; ++k) {
	  meanTimings[k] = meanTimings[k] / meanCallCounts[k];
	}
      }

      // Reformat the data into the map of statistics.  Be sure that
      // each value (the std::vector of (timing, call count) pairs,
      // each entry of which is a different statistic) preserves the
      // order of statNames.
      statNames.resize (3);
      statNames[0] = "Min";
      statNames[1] = "Mean";
      statNames[2] = "Max";

      stat_map_type::iterator statIter = statData.end();
      timer_map_t::const_iterator it = globalTimerData.begin();
      for (int k = 0; it != globalTimerData.end(); ++k, ++it) {
	std::vector<std::pair<double, double> > curData (3);
	curData[0] = minTimingsAndCallCounts[k];
	curData[1] = std::make_pair (meanTimings[k], meanCallCounts[k]);
	curData[2] = maxTimingsAndCallCounts[k];

	// statIter gives an insertion location hint that makes each
	// insertion O(1), since we remember the location of the last
	// insertion.
	statIter = statData.insert (statIter, std::make_pair (it->first, curData));
      }
    }


    /// \brief Get a default communicator appropriate for the environment.
    ///
    /// If Trilinos was configured with MPI support, and if MPI has
    /// been initialized, return a wrapped MPI_COMM_WORLD.  If
    /// Trilinos was configured with MPI support, and if MPI has not
    /// yet been initialized, return a serial communicator (containing
    /// one process).  If Trilinos was <i>not</i> configured with MPI
    /// support, return a serial communicator.
    ///
    /// Rationale: Callers may or may not have initialized MPI before
    /// calling this method.  Just because they built with MPI,
    /// doesn't mean they want to use MPI.  It's not my responsibility
    /// to initialize MPI for them, and I don't have the context I
    /// need in order to do so anyway.  Thus, if Trilinos was built
    /// with MPI and MPI has not yet been initialized, this method
    /// returns a "serial" communicator.
    RCP<const Comm<int> > 
    getDefaultComm ()
    {
      // The default communicator.  If Trilinos was built with MPI
      // enabled, this should be MPI_COMM_WORLD.  (If MPI has not yet
      // been initialized, it's not valid to use the communicator!)
      // Otherwise, this should be a "serial" (no MPI, one "process")
      // communicator.
      RCP<const Comm<int> > comm = DefaultComm<int>::getComm ();

#ifdef HAVE_MPI
      {
	int mpiHasBeenStarted = 0;
	MPI_Initialized (&mpiHasBeenStarted);
	if (! mpiHasBeenStarted) {
	  // Make pComm a new "serial communicator."
	  comm = rcp_implicit_cast<const Comm<int> > (rcp (new SerialComm<int> ()));
	}
      }
#endif // HAVE_MPI
      return comm;
    }

  } // namespace (anonymous)


  void
  TimeMonitor::computeGlobalTimerStatistics (stat_map_type& statData,
					     std::vector<std::string>& statNames,
					     Ptr<const Comm<int> > comm,
					     const ECounterSetOp setOp)
  {
    // Collect local timer data and names.  Filter out timers with
    // zero call counts if writeZeroTimers is false.
    timer_map_t localTimerData;
    Array<std::string> localTimerNames;
    const bool writeZeroTimers = false;
    collectLocalTimerDataAndNames (localTimerData, localTimerNames, counters(), writeZeroTimers);

    // Merge the local timer data and names into global timer data and
    // names.
    timer_map_t globalTimerData;
    Array<std::string> globalTimerNames;
    const bool alwaysWriteLocal = false;
    collectGlobalTimerData (globalTimerData, globalTimerNames, 
			    localTimerData, localTimerNames,
			    comm, alwaysWriteLocal, setOp);
    // Compute statistics on the data.
    computeGlobalTimerStats (statData, statNames, comm, globalTimerData);
  }


  void 
  TimeMonitor::summarize (Ptr<const Comm<int> > comm,
			  std::ostream& out,
			  const bool alwaysWriteLocal,
			  const bool writeGlobalStats,
			  const bool writeZeroTimers,
			  const ECounterSetOp setOp)
  {
    //
    // We can't just call computeGlobalTimerStatistics(), since
    // summarize() has different options that affect whether global
    // statistics are computed and printed.
    //
    const int numProcs = comm->getSize();
    const int myRank = comm->getRank();

    // Collect local timer data and names.  Filter out timers with
    // zero call counts if writeZeroTimers is false.
    timer_map_t localTimerData;
    Array<std::string> localTimerNames;
    collectLocalTimerDataAndNames (localTimerData, localTimerNames,
				   counters(), writeZeroTimers);

    // If we're computing global statistics, merge the local timer
    // data and names into global timer data and names, and compute
    // global timer statistics.  Otherwise, leave the global data
    // empty.
    timer_map_t globalTimerData;
    Array<std::string> globalTimerNames;
    stat_map_type statData;
    std::vector<std::string> statNames;
    if (writeGlobalStats) {
      collectGlobalTimerData (globalTimerData, globalTimerNames, 
			      localTimerData, localTimerNames,
			      comm, alwaysWriteLocal, setOp);
      // Compute statistics on the data, but only if the communicator
      // contains more than one process.  Otherwise, statistics don't
      // make sense and we don't print them (see below).
      if (numProcs > 1) { 
	computeGlobalTimerStats (statData, statNames, comm, globalTimerData);
      }
    }

    // Precision of floating-point numbers in the table.
    const int precision = format().precision();

    // All columns of the table, in order.
    Array<TableColumn> tableColumns;

    // Labels of all the columns of the table.
    // We will append to this when we add each column.
    Array<std::string> titles;

    // Widths (in number of characters) of each column.
    // We will append to this when we add each column.
    Array<int> columnWidths;

    // Table column containing all timer names.  If writeGlobalStats
    // is true, we use the global timer names, otherwise we use the
    // local timer names.  We build the table on all processes
    // redundantly, but only print on Rank 0.
    {
      titles.append ("Timer Name");

      // The column labels depend on whether we are computing global statistics.
      TableColumn nameCol (writeGlobalStats ? globalTimerNames : localTimerNames);
      tableColumns.append (nameCol);

      // Each column is as wide as it needs to be to hold both its
      // title and all of the column data.  This column's title is the
      // current last entry of the titles array.
      columnWidths.append (format().computeRequiredColumnWidth (titles.back(), nameCol));
    }

    // Table column containing local timer stats, if applicable.  We
    // only write local stats if asked, only on MPI Proc 0, and only
    // if there is more than one MPI process in the communicator
    // (otherwise local stats == global stats, so we just print the
    // global stats).  In this case, we've padded the local data on
    // Proc 0 if necessary to match the global timer list, so that the
    // columns have the same number of rows.
    if (alwaysWriteLocal && numProcs > 1 && myRank == 0) {
      titles.append ("Local time (num calls)");

      // Copy local timer data out of the array-of-structs into
      // separate arrays, for display in the table.
      Array<double> localTimings;
      Array<double> localNumCalls;
      for (timer_map_t::const_iterator it = localTimerData.begin();
	   it != localTimerData.end(); ++it) {
	localTimings.push_back (it->second.first);
	localNumCalls.push_back (static_cast<double> (it->second.second));
      }
      TableColumn timeAndCalls (localTimings, localNumCalls, precision, true);
      tableColumns.append (timeAndCalls);
      columnWidths.append (format().computeRequiredColumnWidth (titles.back(), timeAndCalls));
    }

    if (writeGlobalStats) {
      // If there's only 1 process in the communicator, don't display
      // statistics; statistics don't make sense in that case.  Just
      // display the timings and call counts.  If there's more than 1
      // process, do display statistics.
      if (numProcs == 1) {
	// Extract timings and the call counts from globalTimerData.
	Array<double> globalTimings;
	Array<double> globalNumCalls;
	for (timer_map_t::const_iterator it = globalTimerData.begin();
	     it != globalTimerData.end(); ++it) {
	  globalTimings.push_back (it->second.first);
	  globalNumCalls.push_back (static_cast<double> (it->second.second));
	}
	// Print the table column.
	titles.append ("Global time (num calls)");
	TableColumn timeAndCalls (globalTimings, globalNumCalls, precision, true);
	tableColumns.append (timeAndCalls);
	columnWidths.append (format().computeRequiredColumnWidth (titles.back(), timeAndCalls));
      }
      else { // numProcs > 1
	// Print a table column for each statistic.  statNames and
	// each value in statData use the same ordering, so we can
	// iterate over valid indices of statNames to display the
	// statistics in the right order.
	for (std::vector<std::string>::size_type statInd = 0; statInd < statNames.size(); ++statInd) {
	  // Extract lists of timings and their call counts for the
	  // current statistic.
	  Array<double> statTimings (numGlobalTimers);
	  Array<double> statCallCounts (numGlobalTimers);
	  stat_map_type::const_iterator it = statData.begin();
	  for (int k = 0; it != statData.end(); ++it, ++k) {
	    statTimings[k] = (it->second[statInd]).first;
	    statCallCounts[k] = (it->second[statInd]).second;
	  }
	  // Print the table column.
	  const std::string& statisticName = statNames[statInd];
	  const std::string titleString = statisticName + " over procs";
	  titles.append (titleString);
	  TableColumn timeAndCalls (statTimings, statCallCounts, precision, true);
	  tableColumns.append (timeAndCalls);
	  columnWidths.append (format().computeRequiredColumnWidth (titles.back(), timeAndCalls));
	}
      }
    }

    // Print the whole table to the given output stream on MPI Rank 0.
    format ().setColumnWidths (columnWidths);
    if (myRank == 0) {
      std::ostringstream theTitle;
      theTitle << "TimeMonitor results over " << numProcs << " processor" 
	       << (numProcs > 1 ? "s" : "");
      format().writeWholeTable (out, theTitle.str(), titles, tableColumns);
    }
  }

  void 
  TimeMonitor::summarize (std::ostream &out,
			  const bool alwaysWriteLocal,
			  const bool writeGlobalStats,
			  const bool writeZeroTimers,
			  const ECounterSetOp setOp)
  {
    // The default communicator.  If Trilinos was built with MPI
    // enabled, this should be MPI_COMM_WORLD.  Otherwise, this should
    // be a "serial" (no MPI, one "process") communicator.
    RCP<const Comm<int> > comm = getDefaultComm();

    summarize (comm.ptr(), out, alwaysWriteLocal, 
	       writeGlobalStats, writeZeroTimers, setOp);
  }

  void 
  TimeMonitor::computeGlobalTimerStatistics (stat_map_type& statData,
					     std::vector<std::string>& statNames,
					     const ECounterSetOp setOp)
  {
    // The default communicator.  If Trilinos was built with MPI
    // enabled, this should be MPI_COMM_WORLD.  Otherwise, this should
    // be a "serial" (no MPI, one "process") communicator.
    RCP<const Comm<int> > comm = getDefaultComm();

    computeGlobalTimerStatistics (statData, statNames, comm.ptr(), setOp);
  }


} // namespace Teuchos
