// @HEADER
//
// ***********************************************************************
//
//        MueLu: A package for multigrid based preconditioning
//                  Copyright 2012 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
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
// Questions? Contact
//                    Jonathan Hu       (jhu@sandia.gov)
//                    Ray Tuminaro      (rstumin@sandia.gov)
//
// ***********************************************************************
//
// @HEADER
#ifndef MUELU_SETUPREGIONHIERARCHY_DEF_HPP
#define MUELU_SETUPREGIONHIERARCHY_DEF_HPP

#include <vector>
#include <iostream>

#include <Kokkos_DefaultNode.hpp>

#include <Teuchos_RCP.hpp>

#include <Xpetra_ConfigDefs.hpp>
#include <Xpetra_Export.hpp>
#include <Xpetra_Import.hpp>
#include <Xpetra_Map.hpp>
#include <Xpetra_MultiVector.hpp>
#include <Xpetra_Vector.hpp>
#include <Xpetra_CrsMatrixWrap.hpp>

#include <MueLu_CreateXpetraPreconditioner.hpp>
#include <MueLu_Utilities.hpp>

#if defined(HAVE_MUELU_ZOLTAN2) && defined(HAVE_MPI)
#include <MueLu_RepartitionFactory.hpp>
#include <MueLu_RepartitionHeuristicFactory.hpp>
#include <MueLu_Zoltan2Interface.hpp>
#endif

#include "SetupRegionVector_def.hpp"
#include "SetupRegionMatrix_def.hpp"
#include "SetupRegionSmoothers_def.hpp"


#if defined(HAVE_MUELU_TPETRA) && defined(HAVE_MUELU_AMESOS2)
#include <Amesos2_config.h>
#include <Amesos2.hpp>
#endif

using Teuchos::RCP;
using Teuchos::ArrayRCP;
using Teuchos::Array;
using Teuchos::ArrayView;
using Teuchos::ParameterList;

/*! \brief Create coarse level maps with continuous GIDs
 *
 *  The direct solver requires maps with continuous GIDs. Starting from the
 *  coarse level composite maps with discontinuous GIDs, we create a new row map
 *  and a matching column map.
 *
 *  Range and Domain map happen to correspond to the Row map, so we don't have
 *  to deal with them in particular.
 */
template <class LocalOrdinal, class GlobalOrdinal, class Node>
void createContinuousCoarseLevelMaps(const RCP<const Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > rowMap, ///< row map
                                     const RCP<const Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > colMap, ///< column map
                                     RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> >& contRowMap, ///< row map with continuous GIDs
                                     RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> >& contColMap ///< column map with continuous GIDs
                                     )
{
#include "Xpetra_UseShortNamesOrdinal.hpp"
  //   /!\ This function is pure ordinal, no scalar type is passed as input
  //       This means that use only three template paramters and that we
  //       do not use the Scalar dependent short names!

  // Create row map with continuous GIDs
  contRowMap = MapFactory::Build(rowMap->lib(),
                                 rowMap->getGlobalNumElements(),
                                 rowMap->getNodeNumElements(),
                                 rowMap->getIndexBase(),
                                 rowMap->getComm());

  return;
} // createContinuousCoarseLevelMaps


/* Reconstruct coarse-level maps (assuming fully structured grids)
 *
 * We know the regional map on the coarse levels since they are just the
 * row maps of the coarse level operators. Though, we need to re-construct
 * the quasiRegional and composite maps ourselves.
 *
 * We ultimately are only interested in the composite map on the coarsest level.
 * Intermediate levels are dealt with along the way, because we go through all
 * levels recursively.
 *
 * Assumptions:
 * - fully structured grid
 * - only on region per proc and group
 */
template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void MakeCoarseLevelMaps(const int maxRegPerGID,
                         Teuchos::ArrayView<LocalOrdinal>  compositeToRegionLIDsFinest,
                         Array<RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> > >& regProlong,
                         Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& regRowMaps,
                         Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& regColMaps,
                         Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& quasiRegRowMaps,
                         Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& quasiRegColMaps,
                         Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& compRowMaps,
                         Array<RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> > >& regRowImporters,
                         Array<Teuchos::RCP<Xpetra::MultiVector<GlobalOrdinal, LocalOrdinal, GlobalOrdinal, Node> > >& interfaceGIDs,
                         Array<Teuchos::RCP<Xpetra::MultiVector<LocalOrdinal, LocalOrdinal, GlobalOrdinal, Node> > >& regionsPerGIDWithGhosts,
                         Array<Teuchos::ArrayRCP<LocalOrdinal> >& regionMatVecLIDs,
                         Array<Teuchos::RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> > >& regionInterfaceImporter) {

#include "Xpetra_UseShortNames.hpp"

  using MT = typename Teuchos::ScalarTraits<SC>::magnitudeType;

  const GO GO_INV = Teuchos::OrdinalTraits<GO>::invalid();
  const int numLevels = regProlong.size();

  // RCP<Teuchos::FancyOStream> fancy = Teuchos::fancyOStream(Teuchos::rcpFromRef(std::cout));
  // Teuchos::FancyOStream& out = *fancy;
  const int myRank = regProlong[1]->getRowMap()->getComm()->getRank();

  Teuchos::Array<LO> coarseCompositeToRegionLIDs;
  Teuchos::ArrayView<LO> compositeToRegionLIDs = compositeToRegionLIDsFinest;
  for(int currentLevel = 1; currentLevel < numLevels; ++currentLevel) {

    // Extracting some basic information about local mesh in composite/region format
    const size_t numFineRegionNodes    = regProlong[currentLevel]->getNodeNumRows();
    const size_t numFineCompositeNodes = compositeToRegionLIDs.size();
    const size_t numFineDuplicateNodes = numFineRegionNodes - numFineCompositeNodes;

    const size_t numCoarseRegionNodes  = regProlong[currentLevel]->getColMap()->getNodeNumElements();

    // Find the regionLIDs associated with local duplicated nodes
    // This will allow us to later loop only on duplicated nodes
    size_t countComposites = 0, countDuplicates = 0;
    Array<LO> fineDuplicateLIDs(numFineDuplicateNodes);
    for(size_t regionIdx = 0; regionIdx < numFineRegionNodes; ++regionIdx) {
      if(compositeToRegionLIDs[countComposites] == static_cast<LO>(regionIdx)) {
        ++countComposites;
      } else {
        fineDuplicateLIDs[countDuplicates] = regionIdx;
        ++countDuplicates;
      }
    }

    // We gather the coarse GIDs associated with each fine point in the local composite mesh part.
    RCP<Xpetra::Vector<MT,LO,GO,NO> > coarseCompositeGIDs
      = Xpetra::VectorFactory<MT,LO,GO,NO>::Build(regRowImporters[currentLevel - 1]->getSourceMap(), false);
    Teuchos::ArrayRCP<MT> coarseCompositeGIDsData = coarseCompositeGIDs->getDataNonConst(0);

    for(size_t compositeNodeIdx = 0; compositeNodeIdx < numFineCompositeNodes; ++compositeNodeIdx) {
      ArrayView<const LO> coarseRegionLID; // Should contain a single value
      ArrayView<const SC> dummyData; // Should contain a single value
      regProlong[currentLevel]->getLocalRowView(compositeToRegionLIDs[compositeNodeIdx],
                                                   coarseRegionLID,
                                                   dummyData);
      if(coarseRegionLID.size() == 1) {
        coarseCompositeGIDsData[compositeNodeIdx] = regProlong[currentLevel]->getColMap()->getGlobalElement(coarseRegionLID[0]);
      } else {
        coarseCompositeGIDsData[compositeNodeIdx] = -1;
      }
    }

    // We communicate the above GIDs to their duplicate so that we can replace GIDs of the region
    // column map and form the quasiRegion column map.
    RCP<Xpetra::Vector<MT, LO, GO, NO> > coarseQuasiregionGIDs;
    RCP<Xpetra::Vector<MT, LO, GO, NO> > coarseRegionGIDs;
    compositeToRegional(coarseCompositeGIDs,
                        coarseQuasiregionGIDs,
                        coarseRegionGIDs,
                        regRowMaps[currentLevel - 1],
                        regRowImporters[currentLevel - 1]);

    regionsPerGIDWithGhosts[currentLevel] =
      Xpetra::MultiVectorFactory<LO, LO, GO, NO>::Build(regRowMaps[currentLevel],
                                                        maxRegPerGID,
                                                        false);
    interfaceGIDs[currentLevel] =
      Xpetra::MultiVectorFactory<GO, LO, GO, NO>::Build(regRowMaps[currentLevel],
                                                        maxRegPerGID,
                                                        false);
    Array<ArrayRCP<const LO> > regionsPerGIDWithGhostsFine(maxRegPerGID);
    Array<ArrayRCP<LO> > regionsPerGIDWithGhostsCoarse(maxRegPerGID);
    Array<ArrayRCP<GO> > interfaceGIDsCoarse(maxRegPerGID);
    for(size_t idx = 0; idx < static_cast<size_t>(maxRegPerGID); ++idx) {
      regionsPerGIDWithGhostsFine[idx]   = regionsPerGIDWithGhosts[currentLevel - 1]->getData(idx);
      regionsPerGIDWithGhostsCoarse[idx] = regionsPerGIDWithGhosts[currentLevel]->getDataNonConst(idx);
      interfaceGIDsCoarse[idx]          = interfaceGIDs[currentLevel]->getDataNonConst(idx);
      for(size_t coarseIdx = 0;
          coarseIdx < regionsPerGIDWithGhosts[currentLevel]->getLocalLength(); ++coarseIdx) {
        regionsPerGIDWithGhostsCoarse[idx][coarseIdx] = -1;
        interfaceGIDsCoarse[idx][coarseIdx] = 0;
      }
    }

    for(size_t fineIdx = 0; fineIdx < numFineRegionNodes; ++fineIdx) {
      ArrayView<const LO> coarseRegionLID; // Should contain a single value
      ArrayView<const SC> dummyData;       // Should contain a single value
      regProlong[currentLevel]->getLocalRowView(fineIdx,
                                                   coarseRegionLID,
                                                   dummyData);
      const LO coarseIdx = coarseRegionLID[0];

      // Now fill regionPerGIDWithGhostsCoarse[:][coarseRegionLID]
      // with data from regionPerGIDWithGhostsFine[:][fineIdx].
      // The problem is we might have more then maxRegPerGID on currentLevel
      // then on currentLevel-1... we might need to do a union or something?
      // I guess technically using the restriction operator here would be more
      // helpful than the prolongator, this way we could find all the fine interface
      // points easily and compute the union of the PIDs they belong too.
      // This can actually be done extracting the local matrix and compute the transpose.
      // For now let us assume that maxRegPerGID is constant and hope for the best.
      LO countFinePIDs   = 0;
      LO countCoarsePIDs = 0;
      for(LO idx = 0; idx < maxRegPerGID; ++idx) {
        if(-1 < regionsPerGIDWithGhostsFine[idx][fineIdx]) {++countFinePIDs;}
        if(-1 < regionsPerGIDWithGhostsCoarse[idx][coarseIdx]) {++countCoarsePIDs;}
      }

      if(countCoarsePIDs < countFinePIDs) {
        for(LO idx = 0; idx < countFinePIDs; ++idx) {
          regionsPerGIDWithGhostsCoarse[idx][coarseIdx] = regionsPerGIDWithGhostsFine[idx][fineIdx];
          if(regionsPerGIDWithGhostsCoarse[idx][coarseIdx] == myRank) {
            interfaceGIDsCoarse[idx][coarseIdx] = regRowMaps[currentLevel]->getGlobalElement(coarseIdx);
          }
        }
      }
    }

    Array<GO> fineRegionDuplicateCoarseLIDs(numFineDuplicateNodes);
    Array<GO> fineRegionDuplicateCoarseGIDs(numFineDuplicateNodes);
    for(size_t duplicateIdx = 0; duplicateIdx < numFineDuplicateNodes; ++duplicateIdx) {
      ArrayView<const LO> coarseRegionLID; // Should contain a single value
      ArrayView<const SC> dummyData; // Should contain a single value
      regProlong[currentLevel]->getLocalRowView(fineDuplicateLIDs[duplicateIdx],
                                                   coarseRegionLID,
                                                   dummyData);
      fineRegionDuplicateCoarseLIDs[duplicateIdx] = regProlong[currentLevel]->getColMap()->getGlobalElement(coarseRegionLID[0]);
      fineRegionDuplicateCoarseGIDs[duplicateIdx] = (coarseQuasiregionGIDs->getDataNonConst(0))[fineDuplicateLIDs[duplicateIdx]];
    }

    // Create the coarseQuasiregRowMap, it will be based on the coarseRegRowMap
    LO countCoarseComposites = 0;
    coarseCompositeToRegionLIDs.resize(numCoarseRegionNodes);
    Array<GO> coarseQuasiregRowMapData = regProlong[currentLevel]->getColMap()->getNodeElementList();
    Array<GO> coarseCompRowMapData(numCoarseRegionNodes, -1);
    for(size_t regionIdx = 0; regionIdx < numCoarseRegionNodes; ++regionIdx) {
      const GO initialValue = coarseQuasiregRowMapData[regionIdx];
      for(size_t duplicateIdx = 0; duplicateIdx < numFineDuplicateNodes; ++duplicateIdx) {
        if((initialValue == fineRegionDuplicateCoarseLIDs[duplicateIdx]) &&
           (fineRegionDuplicateCoarseGIDs[duplicateIdx] < coarseQuasiregRowMapData[regionIdx]) &&
           (-1 < fineRegionDuplicateCoarseGIDs[duplicateIdx])){
          coarseQuasiregRowMapData[regionIdx] = fineRegionDuplicateCoarseGIDs[duplicateIdx];
        }
      }
      if(initialValue == coarseQuasiregRowMapData[regionIdx]) {
        coarseCompRowMapData[countCoarseComposites] = coarseQuasiregRowMapData[regionIdx];
        coarseCompositeToRegionLIDs[countCoarseComposites] = regionIdx;
        ++countCoarseComposites;
      }
    }
    coarseCompRowMapData.resize(countCoarseComposites);
    coarseCompositeToRegionLIDs.resize(countCoarseComposites);

    // We are now ready to fill up the outputs
    regRowMaps[currentLevel] = Teuchos::rcp_const_cast<Map>(regProlong[currentLevel]->getColMap());
    regColMaps[currentLevel] = Teuchos::rcp_const_cast<Map>(regProlong[currentLevel]->getColMap());
    quasiRegRowMaps[currentLevel] = MapFactory::Build(regProlong[currentLevel]->getColMap()->lib(),
                                                         GO_INV,
                                                         coarseQuasiregRowMapData(),
                                                         regProlong[currentLevel]->getColMap()->getIndexBase(),
                                                         regProlong[currentLevel]->getColMap()->getComm());
    quasiRegColMaps[currentLevel] = quasiRegRowMaps[currentLevel];
    compRowMaps[currentLevel] = MapFactory::Build(regProlong[currentLevel]->getColMap()->lib(),
                                                  GO_INV,
                                                  coarseCompRowMapData(),
                                                  regProlong[currentLevel]->getColMap()->getIndexBase(),
                                                  regProlong[currentLevel]->getColMap()->getComm());
    regRowImporters[currentLevel] = ImportFactory::Build(compRowMaps[currentLevel], quasiRegRowMaps[currentLevel]);

    // Now generate matvec data
    SetupMatVec(interfaceGIDs[currentLevel], regionsPerGIDWithGhosts[currentLevel],
                regRowMaps[currentLevel], regRowImporters[currentLevel],
                regionMatVecLIDs[currentLevel], regionInterfaceImporter[currentLevel]);

    // Finally reset compositeToRegionLIDs
    compositeToRegionLIDs = coarseCompositeToRegionLIDs();
  } // Loop over numLevels
} // MakeCoarseLevelMaps


// Form the composite coarse level operator
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void MakeCoarseCompositeOperator(RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> >& compRowMap,
                                 RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> >& quasiRegRowMap,
                                 RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> >& quasiRegColMap,
                                 RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> >& regRowImporter,
                                 RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& regMatrix,
                                 RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType, LocalOrdinal, GlobalOrdinal, Node> >& regCoarseCoordinates,
                                 RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& coarseCompOp,
                                 RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType, LocalOrdinal, GlobalOrdinal, Node> >& compCoarseCoordinates,
                                 const bool makeCompCoords)
{
#include "Xpetra_UseShortNames.hpp"
  using CoordType = typename Teuchos::ScalarTraits<Scalar>::coordinateType;
  coarseCompOp = MatrixFactory::Build(compRowMap,
                                       // This estimate is very conservative and probably costs us lots of memory...
                                      8*regMatrix->getCrsGraph()->getNodeMaxNumRowEntries());
  regionalToComposite(regMatrix,
                      quasiRegRowMap, quasiRegColMap,
                      regRowImporter, Xpetra::ADD,
                      coarseCompOp);

  const int dofsPerNode = regMatrix->GetFixedBlockSize();
  coarseCompOp->SetFixedBlockSize(dofsPerNode);
  coarseCompOp->setObjectLabel("coarse composite operator");

  // Create coarse composite coordinates for repartitioning
  if(makeCompCoords) {
    const int check       = regMatrix->getRowMap()->getNodeNumElements() % regCoarseCoordinates->getMap()->getNodeNumElements();
    TEUCHOS_ASSERT(check == 0);

    RCP<Map> compCoordMap;
    RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> > regCoordImporter;
    if(dofsPerNode == 1) {
      compCoordMap = compRowMap;
      regCoordImporter = regRowImporter;
    } else {
      using size_type = typename Teuchos::Array<GlobalOrdinal>::size_type;
      Array<GlobalOrdinal> compCoordMapData(compRowMap->getNodeNumElements() / dofsPerNode);
      ArrayView<const GlobalOrdinal> compRowMapData = compRowMap->getNodeElementList();
      for(size_type nodeIdx = 0; nodeIdx < compCoordMapData.size(); ++nodeIdx) {
        compCoordMapData[nodeIdx] = compRowMapData[nodeIdx*dofsPerNode] / dofsPerNode;
      }
      compCoordMap = MapFactory::Build(compRowMap->lib(),
                                       compRowMap->getGlobalNumElements() / dofsPerNode,
                                       compCoordMapData(),
                                       compRowMap->getIndexBase(),
                                       compRowMap->getComm());

      RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > quasiRegCoordMap;
        Array<GlobalOrdinal> quasiRegCoordMapData(quasiRegRowMap->getNodeNumElements() / dofsPerNode);
        ArrayView<const GlobalOrdinal> quasiRegRowMapData = quasiRegRowMap->getNodeElementList();
        for(size_type nodeIdx = 0; nodeIdx < quasiRegCoordMapData.size(); ++nodeIdx) {
          quasiRegCoordMapData[nodeIdx] = quasiRegRowMapData[nodeIdx*dofsPerNode] / dofsPerNode;
        }
        quasiRegCoordMap = MapFactory::Build(quasiRegRowMap->lib(),
                                             quasiRegRowMap->getGlobalNumElements() / dofsPerNode,
                                             quasiRegCoordMapData(),
                                             quasiRegRowMap->getIndexBase(),
                                             quasiRegRowMap->getComm());
        regCoordImporter = ImportFactory::Build(compCoordMap, quasiRegCoordMap);
    }
    compCoarseCoordinates = Xpetra::MultiVectorFactory<CoordType, LocalOrdinal, GlobalOrdinal, Node>
      ::Build(compCoordMap, regCoarseCoordinates->getNumVectors());
    TEUCHOS_ASSERT(Teuchos::nonnull(compCoarseCoordinates));

    // The following looks like regionalToComposite for Vector
    // but it is a bit different since we do not want to add
    // entries in the coordinate MultiVector as we would for
    // a solution or residual vector.
    RCP<Xpetra::MultiVector<CoordType, LocalOrdinal, GlobalOrdinal, Node>> quasiRegCoarseCoordinates;
    quasiRegCoarseCoordinates = regCoarseCoordinates;
    TEUCHOS_ASSERT(Teuchos::nonnull(quasiRegCoarseCoordinates));
    quasiRegCoarseCoordinates->replaceMap(regCoordImporter->getTargetMap());
    compCoarseCoordinates->doExport(*quasiRegCoarseCoordinates, *(regCoordImporter), Xpetra::INSERT);
  }
} // MakeCoarseCompositeOperator


/* Create a direct solver for a composite operator
 *
 * Create the solver object and compute symbolic and numeric factorization.
 * Finally, the solver object will be ready to be applied during the V-cycle call.
 *
 * \note For now, we're limited to Tpetra/Amesos2. From Amesos2, we use KLU as direct solver.
 */
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
RCP<Amesos2::Solver<Tpetra::CrsMatrix<Scalar, LocalOrdinal, GlobalOrdinal, Node>, Tpetra::MultiVector<Scalar, LocalOrdinal, GlobalOrdinal, Node> > >
MakeCompositeDirectSolver(RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& compOp)
{
  using Tpetra_CrsMatrix = Tpetra::CrsMatrix<Scalar, LocalOrdinal, GlobalOrdinal, Node>;
  using Tpetra_MultiVector = Tpetra::MultiVector<Scalar, LocalOrdinal, GlobalOrdinal, Node>;
  using Utilities = MueLu::Utilities<Scalar, LocalOrdinal, GlobalOrdinal, Node>;
  using Teuchos::TimeMonitor;

  RCP<Amesos2::Solver<Tpetra_CrsMatrix, Tpetra_MultiVector> > coarseSolver;
  {
    RCP<TimeMonitor> tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("MakeCompositeDirectSolver: 1 - Setup")));

    // convert matrix to Tpetra
    RCP<Tpetra_CrsMatrix> tMat = Utilities::Op2NonConstTpetraCrs(compOp);

    // Amesos2-specific key phrase that denote smoother type
    std::string amesos2SolverName = "KLU2";
    TEUCHOS_ASSERT(Amesos2::query(amesos2SolverName));
    coarseSolver = Amesos2::create<Tpetra_CrsMatrix,Tpetra_MultiVector>(amesos2SolverName, tMat);

    Teuchos::ParameterList amesos2_params("Amesos2");
    amesos2_params.sublist(amesos2SolverName).set("IsContiguous", false, "Are GIDs Contiguous");
    coarseSolver->setParameters(Teuchos::rcpFromRef(amesos2_params));
  }

  {
    RCP<TimeMonitor> tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("MakeCompositeDirectSolver: 2 - Factorization")));

    coarseSolver->symbolicFactorization();
    coarseSolver->numericFactorization();
  }

  return coarseSolver;
} // MakeCorseCompositeDirectSolver

/* Rebalance coarse operator
 *
 */
#if defined(HAVE_MUELU_ZOLTAN2) && defined(HAVE_MPI)
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void RebalanceCoarseCompositeOperator(const int rebalanceNumPartitions,
              RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& coarseCompOp,
              RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType, LocalOrdinal, GlobalOrdinal, Node> >& compCoarseCoordinates,
              RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& rebalancedCompOp,
              RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType, LocalOrdinal, GlobalOrdinal, Node> >& rebalancedCoordinates,
              RCP<const Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> >& rebalanceImporter)
{
#include "MueLu_UseShortNames.hpp"
  using CoordType = typename Teuchos::ScalarTraits<Scalar>::coordinateType;
  using Teuchos::TimeMonitor;

  RCP<TimeMonitor> tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("RebalanceCoarseCompositeOperator: ")));

  RCP<Teuchos::FancyOStream> fos = Teuchos::fancyOStream(Teuchos::rcpFromRef(std::cout));
  fos->setOutputToRootOnly(0);
  *fos << "Rebalancing coarse composite operator." << std::endl;

  const int numPartitions = rebalanceNumPartitions;

  // We build a fake level
  Level level;
  level.SetLevelID(1);

  RCP<FactoryManagerBase> factoryHandler = rcp(new FactoryManager());
  level.SetFactoryManager(factoryHandler);

  RCP<TimeMonitor> tmLocal = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("rebalanceCoarse: Zoltan construction:")));

  RCP<Zoltan2Interface> zoltan = rcp(new Zoltan2Interface());

  level.Set<RCP<Matrix> >     ("A",                    coarseCompOp);
  level.Set<RCP<MultiVector> >("Coordinates",          compCoarseCoordinates);

  RCP<RepartitionFactory> repart = rcp(new RepartitionFactory());
  Teuchos::ParameterList paramList;
  paramList.set("repartition: remap parts",       false);
  if( numPartitions > 0 ){ // If number of coarse rebalance partitions was provided by the user.
    level.Set<int>              ("number of partitions", numPartitions);
  } else {
    Teuchos::ParameterList paramListHeuristic;
    paramListHeuristic.set("repartition: start level",       1);
    RCP<RepartitionHeuristicFactory> repartHeuristic = rcp(new RepartitionHeuristicFactory());
    repartHeuristic->SetParameterList(paramListHeuristic);
    repart->SetFactory("number of partitions", repartHeuristic );
  }
  repart->SetParameterList(paramList);
  repart->SetFactory("Partition", zoltan);

  // Build
  level.Request("Importer", repart.get());
  repart->Build(level);

  tmLocal = Teuchos::null;

  // Build importer for rebalancing
  level.Get("Importer", rebalanceImporter, repart.get());

  ParameterList XpetraList;
  XpetraList.set("Restrict Communicator", true);
  XpetraList.set("Timer Label","MueLu::RebalanceAc-for-coarseAMG");

  // Build rebalanced coarse composite operator
  rebalancedCompOp = MatrixFactory::Build(coarseCompOp, *rebalanceImporter, *rebalanceImporter, rebalanceImporter->getTargetMap(), rebalanceImporter->getTargetMap(), rcp(&XpetraList,false));
        if (!rebalancedCompOp.is_null()) {
          rebalancedCompOp->SetFixedBlockSize(coarseCompOp->GetFixedBlockSize());
        }

  // Build rebalanced coarse coordinates (The following code is borrowed from MueLu_RebalanceTransferFactory_def.hpp)
  LO blkSize = coarseCompOp->GetFixedBlockSize();
  RCP<const Import> coordImporter;
  if (blkSize == 1) {
    coordImporter = rebalanceImporter;

  } else {
    // NOTE: there is an implicit assumption here: we assume that dof any node are enumerated consequently
    // Proper fix would require using decomposition similar to how we construct importer in the
    // RepartitionFactory
    RCP<const Map> origMap   = compCoarseCoordinates->getMap();
    GO             indexBase = origMap->getIndexBase();

    ArrayView<const GO> OEntries   = rebalanceImporter->getTargetMap()->getNodeElementList();
    LO                  numEntries = OEntries.size()/blkSize;
    ArrayRCP<GO> Entries(numEntries);
    for (LO i = 0; i < numEntries; i++)
      Entries[i] = (OEntries[i*blkSize]-indexBase)/blkSize + indexBase;

    RCP<const Map> targetMap = MapFactory::Build(origMap->lib(), origMap->getGlobalNumElements(), Entries(), indexBase, origMap->getComm());
    coordImporter = ImportFactory::Build(origMap, targetMap);
  }
  rebalancedCoordinates = Xpetra::MultiVectorFactory<CoordType,LocalOrdinal,GlobalOrdinal,Node>::Build(coordImporter->getTargetMap(), compCoarseCoordinates->getNumVectors());
  rebalancedCoordinates->doImport(*compCoarseCoordinates, *coordImporter, Xpetra::INSERT);
  rebalancedCoordinates->replaceMap(rebalancedCoordinates->getMap()->removeEmptyProcesses());

  return;
} // RebalanceCoarseCompositeOperator
#endif

/* Create an AMG hierarchy for a composite operator
 *
 * Create the hierarchy object and perform the multigrid setup.
 * Finally, the hierarhcy object will be ready to be applied during the region MG V-cycle call.
 */
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
RCP<MueLu::Hierarchy<Scalar, LocalOrdinal, GlobalOrdinal, Node> >
MakeCompositeAMGHierarchy(RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& compOp,
                          const std::string& xmlFileName,
                          RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType, LocalOrdinal, GlobalOrdinal, Node> > coordinates)
{
#include "MueLu_UseShortNames.hpp"
  using coordinates_type = typename Teuchos::ScalarTraits<Scalar>::coordinateType;

  const Scalar one = Teuchos::ScalarTraits<Scalar>::one();

  RCP<Teuchos::FancyOStream> fos = Teuchos::fancyOStream(Teuchos::rcpFromRef(std::cout));
  fos->setOutputToRootOnly(0);
  *fos << "Attempting to setup AMG hierarchy for the composite coarse grid problem" << std::endl;

  // Get parameter list for AMG hierarchy
  RCP<ParameterList> mueluParams = Teuchos::rcp(new ParameterList());
  Teuchos::updateParametersFromXmlFileAndBroadcast(xmlFileName, mueluParams.ptr(),
      *compOp->getRowMap()->getComm());


  // Get the user data sublist
  const std::string userName = "user data";
  Teuchos::ParameterList& userParamList = mueluParams->sublist(userName);

  // Add nullspace information
  {
    // Compute nullspace
    RCP<MultiVector> nullspace;
    if((compOp->GetFixedBlockSize() == 1) || Teuchos::is_null(coordinates)) { // Scalar problem, constant nullspace
      nullspace = MultiVectorFactory::Build(compOp->getRowMap(), 1);
      nullspace->putScalar(one);
    } else if(compOp->GetFixedBlockSize() == 2) { // 2D Elasticity
      nullspace = MultiVectorFactory::Build(compOp->getRowMap(), 3);
      Array<ArrayRCP<SC> > nullspaceData(3);
      Array<ArrayRCP<const coordinates_type> > coordinateData(2);

      // Calculate center
      const coordinates_type cx = coordinates->getVector(0)->meanValue();
      const coordinates_type cy = coordinates->getVector(1)->meanValue();

      coordinateData[0] = coordinates->getData(0);
      coordinateData[1] = coordinates->getData(1);

      for(int vecIdx = 0; vecIdx < 3; ++vecIdx) {
        nullspaceData[vecIdx] = nullspace->getDataNonConst(vecIdx);
      }

      for(size_t nodeIdx = 0; nodeIdx < coordinates->getLocalLength(); ++nodeIdx) {
        // translations
        nullspaceData[0][2*nodeIdx + 0] = one;
        nullspaceData[1][2*nodeIdx + 1] = one;

        // rotation about z axis
        nullspaceData[2][2*nodeIdx + 0] = -(coordinateData[1][nodeIdx] - cy);
        nullspaceData[2][2*nodeIdx + 1] =  (coordinateData[0][nodeIdx] - cx);
      }

    } else if(compOp->GetFixedBlockSize() == 3) { // 3D Elasticity
      nullspace = MultiVectorFactory::Build(compOp->getRowMap(), 6);
      Array<ArrayRCP<SC> > nullspaceData(6);
      Array<ArrayRCP<const coordinates_type> > coordinateData(3);

      // Calculate center
      const coordinates_type cx = coordinates->getVector(0)->meanValue();
      const coordinates_type cy = coordinates->getVector(1)->meanValue();
      const coordinates_type cz = coordinates->getVector(2)->meanValue();

      coordinateData[0] = coordinates->getData(0);
      coordinateData[1] = coordinates->getData(1);
      coordinateData[2] = coordinates->getData(2);

      for(int vecIdx = 0; vecIdx < 6; ++vecIdx) {
        nullspaceData[vecIdx] = nullspace->getDataNonConst(vecIdx);
      }

      for(size_t nodeIdx = 0; nodeIdx < coordinates->getLocalLength(); ++nodeIdx) {
        // translations
        nullspaceData[0][3*nodeIdx + 0] = one;
        nullspaceData[1][3*nodeIdx + 1] = one;
        nullspaceData[2][3*nodeIdx + 2] = one;

        // rotation about z axis
        nullspaceData[3][3*nodeIdx + 0] = -(coordinateData[1][nodeIdx] - cy);
        nullspaceData[3][3*nodeIdx + 1] =  (coordinateData[0][nodeIdx] - cx);

        // rotation about x axis
        nullspaceData[4][3*nodeIdx + 1] = -(coordinateData[2][nodeIdx] - cz);
        nullspaceData[4][3*nodeIdx + 2] =  (coordinateData[1][nodeIdx] - cy);

        // rotation about y axis
        nullspaceData[5][3*nodeIdx + 0] =  (coordinateData[2][nodeIdx] - cz);
        nullspaceData[5][3*nodeIdx + 2] = -(coordinateData[0][nodeIdx] - cx);
      }
    }

    // Equalize norms of all vectors to that of the first one
    // We do not normalize them as a vector of ones seems nice
    Teuchos::Array<typename Teuchos::ScalarTraits<Scalar>::magnitudeType> norms2(nullspace->getNumVectors());
    nullspace->norm2(norms2);
    Teuchos::Array<Scalar> norms2scalar(nullspace->getNumVectors());
    for (size_t vectorIdx = 0; vectorIdx < nullspace->getNumVectors(); ++vectorIdx) {
      norms2scalar[vectorIdx] = norms2[0] / norms2[vectorIdx];
    }
    nullspace->scale(norms2scalar);

    // Insert into parameter list
    userParamList.set("Nullspace", nullspace);
  }

  // Add coordinate information for rebalancing
  {
    if(Teuchos::nonnull(coordinates)) {
      userParamList.set("Coordinates", coordinates);
    }
  }

  // Create an AMG hierarchy based on the composite coarse level operator from the region MG scheme
  RCP<Hierarchy> compOpHiearchy = MueLu::CreateXpetraPreconditioner(compOp, *mueluParams);

  // We will use the hiearchy as a solver
  compOpHiearchy->IsPreconditioner(false);
  compOpHiearchy->SetVerbLevel(MueLu::VERB_NONE);

  return compOpHiearchy;
} // MakeCompositeAMGHierarchy


  // Make interface scaling factors recursively
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void MakeInterfaceScalingFactors(const int numLevels,
                                 Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& compRowMaps,
                                 Array<RCP<Xpetra::Vector<Scalar, LocalOrdinal, GlobalOrdinal, Node> > >& regInterfaceScalings,
                                 Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& regRowMaps,
                                 Array<RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> > >& regRowImporters,
                                 Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& quasiRegRowMaps)
{
#include "Xpetra_UseShortNames.hpp"
  // std::cout << compRowMaps[0]->getComm()->getRank() << " | Computing interface scaling factors ..." << std::endl;

  const SC SC_ONE = Teuchos::ScalarTraits<SC>::one();

  TEUCHOS_TEST_FOR_EXCEPT_MSG(!(numLevels>0), "We require numLevel > 0. Probably, numLevel has not been set, yet.");

  for (int l = 0; l < numLevels; l++) {
    // initialize region vector with all ones.
    regInterfaceScalings[l] = VectorFactory::Build(regRowMaps[l]);
    regInterfaceScalings[l]->putScalar(SC_ONE);

    // transform to composite layout while adding interface values via the Export() combine mode
    RCP<Vector> compInterfaceScalingSum = VectorFactory::Build(compRowMaps[l], true);
    regionalToComposite(regInterfaceScalings[l], compInterfaceScalingSum, regRowImporters[l]);

    /* transform composite layout back to regional layout. Now, GIDs associated
     * with region interface should carry a scaling factor (!= 1).
     */
    RCP<Vector> quasiRegInterfaceScaling; // Is that vector really needed?
    compositeToRegional(compInterfaceScalingSum, quasiRegInterfaceScaling,
                        regInterfaceScalings[l],
                        regRowMaps[l], regRowImporters[l]);
  }
} // MakeInterfaceScalingFactors


template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void createRegionHierarchy(const int numDimensions,
                           const Array<int> lNodesPerDim,
                           const std::string aggregationRegionType,
                           RCP<Teuchos::ParameterList>& interfaceParams,
                           const std::string xmlFileName,
                           RCP<Xpetra::MultiVector<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& nullspace,
                           RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType, LocalOrdinal, GlobalOrdinal, Node> >& coordinates,
                           RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& regionMats,
                           const RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > mapComp,
                           const RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > rowMap,
                           const RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > colMap,
                           const RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > revisedRowMap,
                           const RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > revisedColMap,
                           const RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> > rowImport,
                           Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& compRowMaps,
                           Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& compColMaps,
                           Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& regRowMaps,
                           Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& regColMaps,
                           Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& quasiRegRowMaps,
                           Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > >& quasiRegColMaps,
                           Array<RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> > >& regMatrices,
                           Array<RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> > >& regProlong,
                           Array<RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> > >& regRowImporters,
                           Array<RCP<Xpetra::Vector<Scalar, LocalOrdinal, GlobalOrdinal, Node> > >& regInterfaceScalings,
                           Array<Teuchos::RCP<Xpetra::MultiVector<GlobalOrdinal, LocalOrdinal, GlobalOrdinal, Node> > >& interfaceGIDs,
                           Array<Teuchos::RCP<Xpetra::MultiVector<LocalOrdinal, LocalOrdinal, GlobalOrdinal, Node> > >& regionsPerGIDWithGhosts,
                           Array<Teuchos::ArrayRCP<LocalOrdinal> >& regionMatVecLIDs,
                           Array<Teuchos::RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> > >& regionInterfaceImporter,
                           const int maxRegPerGID,
                           ArrayView<LocalOrdinal> compositeToRegionLIDs,
                           RCP<Teuchos::ParameterList>& coarseSolverData,
                           Array<RCP<Teuchos::ParameterList> >& smootherParams,
                           RCP<Teuchos::ParameterList> hierarchyData,
                           const bool keepCoarseCoords)
{
#include "Xpetra_UseShortNames.hpp"
  using Teuchos::TimeMonitor;
  // This monitor times everything and gets the overall setting cost
  RCP<TimeMonitor> tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("createRegionHierarchy")));

  using Hierarchy = MueLu::Hierarchy<SC, LO, GO, NO>;
  using Utilities = MueLu::Utilities<SC, LO, GO, NO>;
  using DirectCoarseSolver = Amesos2::Solver<Tpetra::CrsMatrix<Scalar, LocalOrdinal, GlobalOrdinal, Node>, Tpetra::MultiVector<Scalar, LocalOrdinal, GlobalOrdinal, Node> >;

  // std::cout << mapComp->getComm()->getRank() << " | Setting up MueLu hierarchies ..." << std::endl;
  int numLevels = 0;

  // A hierarchy
  RCP<Hierarchy> regHierarchy;

  RCP<TimeMonitor> tmLocal = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("createRegionHierarchy: Hierarchy")));

  /* Set number of nodes per processor per dimension
   *
   * We don't use the number of owned nodes provided on input.
   * Use the region dimensions instead. This is the right thing to do
   * since duplication of interface nodes has added duplicated nodes to those regions
   * where inpData_ownedX/inpData_ownedY and inpData_regionX/inpData_regionY have been different on input.
   */

  // Read MueLu parameter list form xml file
  RCP<ParameterList> mueluParams = Teuchos::rcp(new ParameterList());
  Teuchos::updateParametersFromXmlFileAndBroadcast(xmlFileName, mueluParams.ptr(), *mapComp->getComm());

  // Insert region-specific data into parameter list
  const std::string userName = "user data";
  Teuchos::ParameterList& userParamList = mueluParams->sublist(userName);
  userParamList.set<int>        ("int numDimensions", numDimensions);
  userParamList.set<Array<LO> > ("Array<LO> lNodesPerDim", lNodesPerDim);
  userParamList.set<std::string>("string aggregationRegionType", aggregationRegionType);
  userParamList.set<Array<LO> > ("Array<LO> nodeOnInterface", interfaceParams->get<Array<LO> >("interfaces: interface nodes"));
  userParamList.set<Array<LO> > ("Array<LO> interfacesDimensions", interfaceParams->get<Array<LO> >("interfaces: nodes per dimensions"));
  if(Teuchos::nonnull(coordinates)) {
    userParamList.set("Coordinates", coordinates);
  }
  if(Teuchos::nonnull(nullspace)) {
    userParamList.set("Nullspace", nullspace);
  }

  // Setup hierarchy
  regHierarchy = MueLu::CreateXpetraPreconditioner(regionMats, *mueluParams);

  // std::cout << mapComp->getComm()->getRank() << " | Resize containers..." << std::endl;

  tmLocal = Teuchos::null;
  tmLocal = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("createRegionHierarchy: ExtractData")));
  // resize Arrays and vectors
  {
    // resize level containers
    numLevels = regHierarchy->GetNumLevels();
    compRowMaps.resize(numLevels);
    compColMaps.resize(numLevels);
    regRowMaps.resize(numLevels);
    regColMaps.resize(numLevels);
    quasiRegRowMaps.resize(numLevels);
    quasiRegColMaps.resize(numLevels);
    regMatrices.resize(numLevels);
    regProlong.resize(numLevels);
    regRowImporters.resize(numLevels);
    regInterfaceScalings.resize(numLevels);
    interfaceGIDs.resize(numLevels);
    regionsPerGIDWithGhosts.resize(numLevels);
    regionMatVecLIDs.resize(numLevels);
    regionInterfaceImporter.resize(numLevels);
    smootherParams.resize(numLevels);

    // resize group containers on each level
    for (int l = 0; l < numLevels; ++l) {
      // Also doing some initialization in the smootherParams
      if(l > 0) {smootherParams[l] = rcp(new Teuchos::ParameterList(*smootherParams[0]));}
    }
  }

  // std::cout << mapComp->getComm()->getRank() << " | Fill fine level containers..." << std::endl;

  // Fill fine level with our data
  {
    compRowMaps[0]     = mapComp;
    quasiRegRowMaps[0] = rowMap;
    quasiRegColMaps[0] = colMap;
    regRowMaps[0]      = revisedRowMap;
    regColMaps[0]      = revisedColMap;
    regRowImporters[0] = rowImport;
    regMatrices[0]     = regionMats;

    /* MueLu stores prolongator on coarse level, so there is no prolongator
     * on the fine level. To have level containers of the same size, let's
     * just put in dummy data
     */
    RCP<Matrix> fineLevelProlong;
    fineLevelProlong = Teuchos::null;
    regProlong[0] = fineLevelProlong;
  }

  // std::cout << mapComp->getComm()->getRank() << " | Fill coarser level containers..." << std::endl;

  /* Get coarse level matrices and prolongators from MueLu hierarchy
   * Note: fine level has been dealt with previously, so we start at level 1 here.
   */
  using real_type = typename Teuchos::ScalarTraits<Scalar>::coordinateType;
  using realvaluedmultivector_type = Xpetra::MultiVector<real_type, LocalOrdinal, GlobalOrdinal, Node>;
  RCP<realvaluedmultivector_type> regCoarseCoordinates;
  for (int l = 1; l < numLevels; ++l) { // Note: we start at level 1 (which is the first coarse level)
    RCP<MueLu::Level> level = regHierarchy->GetLevel(l);

    if(keepCoarseCoords && (l == numLevels - 1)) {
      regCoarseCoordinates = level->Get<RCP<realvaluedmultivector_type> >("Coordinates2", MueLu::NoFactory::get());
    }

    regProlong[l]  = level->Get<RCP<Matrix> >("P", MueLu::NoFactory::get());
    regMatrices[l] = level->Get<RCP<Matrix> >("A", MueLu::NoFactory::get());

    regRowMaps[l]  = Teuchos::rcp_const_cast<Xpetra::Map<LO,GO,NO> >(regMatrices[l]->getRowMap());
    regColMaps[l]  = Teuchos::rcp_const_cast<Xpetra::Map<LO,GO,NO> >(regMatrices[l]->getColMap());

    // Create residual and solution vectors and cache them for vCycle apply
    std::string levelName("level");
    levelName += std::to_string(l);
    ParameterList& levelList = hierarchyData->sublist(levelName, false, "list of data on current level");
    RCP<Vector> regRes = VectorFactory::Build(revisedRowMap, true);
    RCP<Vector> regSol = VectorFactory::Build(revisedRowMap, true);

    levelList.set<RCP<Vector> >("residual", regRes, "Cached residual vector");
    levelList.set<RCP<Vector> >("solution", regSol, "Cached solution vector");
  }

  // std::cout << mapComp->getComm()->getRank() << " | MakeCoarseLevelMaps ..." << std::endl;

  tmLocal = Teuchos::null;
  tmLocal = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("createRegionHierarchy: MakeCoarseLevel")));
  MakeCoarseLevelMaps(maxRegPerGID,
                      compositeToRegionLIDs,
                      regProlong,
                      regRowMaps,
                      regColMaps,
                      quasiRegRowMaps,
                      quasiRegColMaps,
                      compRowMaps,
                      regRowImporters,
                      interfaceGIDs,
                      regionsPerGIDWithGhosts,
                      regionMatVecLIDs,
                      regionInterfaceImporter);

  // std::cout << mapComp->getComm()->getRank() << " | MakeInterfaceScalingFactors ..." << std::endl;

  tmLocal = Teuchos::null;
  tmLocal = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("createRegionHierarchy: MakeInterfaceScaling")));
  MakeInterfaceScalingFactors(numLevels,
                              compRowMaps,
                              regInterfaceScalings,
                              regRowMaps,
                              regRowImporters,
                              quasiRegRowMaps);

  // std::cout << mapComp->getComm()->getRank() << " | Setup smoothers ..." << std::endl;

  tmLocal = Teuchos::null;
  tmLocal = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("createRegionHierarchy: SmootherSetup")));
  // Only set the smoother up to the last but one level
  // if we want to use a smoother on the coarse level
  // we will handle that separately with "coarse solver type"
  for(int levelIdx = 0; levelIdx < numLevels - 1; ++levelIdx) {
    smootherParams[levelIdx]->set("smoother: level", levelIdx);
    smootherSetup(smootherParams[levelIdx], regRowMaps[levelIdx],
                  regMatrices[levelIdx], regInterfaceScalings[levelIdx],
                  regRowImporters[levelIdx]);
  }

  // std::cout << mapComp->getComm()->getRank() << " | CreateCoarseSolver ..." << std::endl;

  tmLocal = Teuchos::null;
  tmLocal = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("createRegionHierarchy: CreateCoarseSolver")));
  const std::string coarseSolverType = coarseSolverData->get<std::string>("coarse solver type");
  if (coarseSolverType == "smoother") {
    // Set the smoother on the coarsest level.
    const std::string smootherXMLFileName = coarseSolverData->get<std::string>("smoother xml file");
    RCP<ParameterList> coarseSmootherParams = smootherParams[numLevels - 1];
    Teuchos::updateParametersFromXmlFileAndBroadcast(smootherXMLFileName, coarseSmootherParams.ptr(), *mapComp->getComm());
    coarseSmootherParams->set("smoother: level", numLevels - 1);
    coarseSmootherParams->print();
    smootherSetup(smootherParams[numLevels - 1], regRowMaps[numLevels - 1],
                  regMatrices[numLevels - 1], regInterfaceScalings[numLevels - 1],
                  regRowImporters[numLevels - 1]);
  } else if( (coarseSolverType == "direct") || (coarseSolverType == "amg") ) {
    // A composite coarse matrix is needed

    // std::cout << mapComp->getComm()->getRank() << " | MakeCoarseCompositeOperator ..." << std::endl;

    RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> > coarseCompOp;
    RCP<realvaluedmultivector_type> compCoarseCoordinates;
    MakeCoarseCompositeOperator(compRowMaps[numLevels - 1],
                                quasiRegRowMaps[numLevels - 1],
                                quasiRegColMaps[numLevels - 1],
                                regRowImporters[numLevels - 1],
                                regMatrices[numLevels - 1],
                                regCoarseCoordinates,
                                coarseCompOp,
                                compCoarseCoordinates,
                                keepCoarseCoords);

    coarseSolverData->set<RCP<const Map> >("compCoarseRowMap", coarseCompOp->getRowMap());

    // std::cout << mapComp->getComm()->getRank() << " | MakeCoarseCompositeSolver ..." << std::endl;
    if (coarseSolverType == "direct") {
      RCP<DirectCoarseSolver> coarseDirectSolver = MakeCompositeDirectSolver(coarseCompOp);
      coarseSolverData->set<RCP<DirectCoarseSolver> >("direct solver object", coarseDirectSolver);
    } else if (coarseSolverType == "amg") {
      if(keepCoarseCoords == false) {
        std::cout << "WARNING: you requested a coarse AMG solver but you did not request coarse coordinates to be kept, repartitioning is not possible!" << std::endl;
      }

      RCP<Hierarchy> coarseAMGHierarchy;
      std::string amgXmlFileName = coarseSolverData->get<std::string>("amg xml file");
#if defined(HAVE_MUELU_ZOLTAN2) && defined(HAVE_MPI)
      const bool coarseSolverRebalance = coarseSolverData->get<bool>("coarse solver rebalance");
      if(keepCoarseCoords == true && coarseSolverRebalance == true ){
        RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> > rebalancedCompOp;
        RCP<Xpetra::MultiVector<typename Teuchos::ScalarTraits<Scalar>::coordinateType, LocalOrdinal, GlobalOrdinal, Node> > rebalancedCoordinates;
        RCP<const Import> rebalanceImporter;

        const int rebalanceNumPartitions = coarseSolverData->get<int>("coarse rebalance num partitions");
        RebalanceCoarseCompositeOperator(rebalanceNumPartitions,
                                    coarseCompOp,
                                    compCoarseCoordinates,
                                    rebalancedCompOp,
                                    rebalancedCoordinates,
                                    rebalanceImporter);
        coarseSolverData->set<RCP<const Import> >("rebalanceImporter", rebalanceImporter);

        if( !rebalancedCompOp.is_null() )
          coarseAMGHierarchy = MakeCompositeAMGHierarchy(rebalancedCompOp, amgXmlFileName, rebalancedCoordinates);

      } else {
        coarseAMGHierarchy = MakeCompositeAMGHierarchy(coarseCompOp, amgXmlFileName, compCoarseCoordinates);
      }
#else
      coarseAMGHierarchy = MakeCompositeAMGHierarchy(coarseCompOp, amgXmlFileName, compCoarseCoordinates);
#endif
      coarseSolverData->set<RCP<Hierarchy> >("amg hierarchy object", coarseAMGHierarchy);
    }
  } else  {
    TEUCHOS_TEST_FOR_EXCEPT_MSG(false, "Unknown coarse solver type.");
  }

} // createRegionHierarchy


//! Recursive V-cycle in region fashion
template<class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
void vCycle(const int l, ///< ID of current level
            const int numLevels, ///< Total number of levels
            const std::string cycleType,
            RCP<Xpetra::Vector<Scalar, LocalOrdinal, GlobalOrdinal, Node> >& fineRegX, ///< solution
            RCP<Xpetra::Vector<Scalar, LocalOrdinal, GlobalOrdinal, Node> > fineRegB, ///< right hand side
            Array<RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> > > regMatrices, ///< Matrices in region layout
            Array<RCP<Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> > > regProlong, ///< Prolongators in region layout
            Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > > compRowMaps, ///< composite maps
            Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > > quasiRegRowMaps, ///< quasiRegional row maps
            Array<RCP<Xpetra::Map<LocalOrdinal, GlobalOrdinal, Node> > > regRowMaps, ///< regional row maps
            Array<RCP<Xpetra::Import<LocalOrdinal, GlobalOrdinal, Node> > > regRowImporters, ///< regional row importers
            Array<RCP<Xpetra::Vector<Scalar, LocalOrdinal, GlobalOrdinal, Node> > > regInterfaceScalings, ///< regional interface scaling factors
            Array<RCP<Teuchos::ParameterList> > smootherParams, ///< region smoother parameter list
            bool& zeroInitGuess,
            RCP<ParameterList> coarseSolverData = Teuchos::null,
            RCP<ParameterList> hierarchyData = Teuchos::null)
{
#include "MueLu_UseShortNames.hpp"
  using Teuchos::TimeMonitor;
  const Scalar SC_ZERO = Teuchos::ScalarTraits<Scalar>::zero();
  const Scalar SC_ONE = Teuchos::ScalarTraits<Scalar>::one();

  int cycleCount = 1;
  if(cycleType == "W" && l > 0) // W cycle and not on finest level
    cycleCount=2;
  if (l < numLevels - 1) { // fine or intermediate levels
    for(int cycle=0; cycle < cycleCount; cycle++){

//    std::cout << "level: " << l << std::endl;

      // extract data from hierarchy parameterlist
      std::string levelName("level" + std::to_string(l));
      ParameterList levelList;
      bool useCachedVectors = false;
      // if(Teuchos::nonnull(hierarchyData) &&  hierarchyData->isSublist(levelName)) {
      //   levelList = hierarchyData->sublist(levelName);
      //   if(levelList.isParameter("residual") && levelList.isParameter("solution")) {
      //     useCachedVectors = true;
      //   }
      // }

      RCP<TimeMonitor> tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("vCycle: 1 - pre-smoother")));

      // pre-smoothing
      smootherApply(smootherParams[l], fineRegX, fineRegB, regMatrices[l],
                    regRowMaps[l], regRowImporters[l], zeroInitGuess);

      tm = Teuchos::null;
      tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("vCycle: 2 - compute residual")));

      RCP<Vector> regRes;
      if(useCachedVectors) {
        regRes = levelList.get<RCP<Vector> >("residual");
      } else {
        regRes = VectorFactory::Build(regRowMaps[l], true);
      }
      computeResidual(regRes, fineRegX, fineRegB, regMatrices[l], *smootherParams[l]);

      tm = Teuchos::null;
      tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("vCycle: 3 - scale interface")));

      scaleInterfaceDOFs(regRes, regInterfaceScalings[l], true);

      tm = Teuchos::null;
      tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("vCycle: 4 - create coarse vectors")));

      // Transfer to coarse level
      RCP<Vector> coarseRegX;
      RCP<Vector> coarseRegB;

      {
        // Get pre-communicated communication patterns for the fast MatVec
        const ArrayRCP<LocalOrdinal> regionInterfaceLIDs = smootherParams[l+1]->get<ArrayRCP<LO>>("Fast MatVec: interface LIDs");
        const RCP<Import> regionInterfaceImporter = smootherParams[l+1]->get<RCP<Import>>("Fast MatVec: interface importer");

        coarseRegX = VectorFactory::Build(regRowMaps[l+1], true);
        coarseRegB = VectorFactory::Build(regRowMaps[l+1], true);

        ApplyMatVec(SC_ONE, regProlong[l+1], regRes, SC_ZERO, regionInterfaceImporter,
            regionInterfaceLIDs, coarseRegB, Teuchos::TRANS, true);
          // TEUCHOS_ASSERT(regProlong[l+1][j]->getRangeMap()->isSameAs(*regRes[j]->getMap()));
          // TEUCHOS_ASSERT(regProlong[l+1][j]->getDomainMap()->isSameAs(*coarseRegB[j]->getMap()));
      }

      tm = Teuchos::null;
      bool coarseZeroInitGuess = true;

      // Call V-cycle recursively
      vCycle(l+1, numLevels, cycleType,
             coarseRegX, coarseRegB, regMatrices, regProlong, compRowMaps,
             quasiRegRowMaps, regRowMaps, regRowImporters, regInterfaceScalings,
             smootherParams, coarseZeroInitGuess, coarseSolverData, hierarchyData);

      tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("vCycle: 6 - transfer coarse to fine")));

      // Transfer coarse level correction to fine level
      RCP<Vector> regCorrection;
      {
        // Get pre-communicated communication patterns for the fast MatVec
        const ArrayRCP<LocalOrdinal> regionInterfaceLIDs = smootherParams[l]->get<ArrayRCP<LO>>("Fast MatVec: interface LIDs");
        const RCP<Import> regionInterfaceImporter = smootherParams[l]->get<RCP<Import>>("Fast MatVec: interface importer");

        regCorrection = VectorFactory::Build(regRowMaps[l], true);
        ApplyMatVec(SC_ONE, regProlong[l+1], coarseRegX, SC_ZERO, regionInterfaceImporter,
            regionInterfaceLIDs, regCorrection, Teuchos::NO_TRANS, false);
          // TEUCHOS_ASSERT(regProlong[l+1][j]->getDomainMap()->isSameAs(*coarseRegX[j]->getMap()));
          // TEUCHOS_ASSERT(regProlong[l+1][j]->getRangeMap()->isSameAs(*regCorrection[j]->getMap()));
      }

      tm = Teuchos::null;
      tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("vCycle: 7 - add coarse grid correction")));

      // apply coarse grid correction
      fineRegX->update(SC_ONE, *regCorrection, SC_ONE);
      if (coarseZeroInitGuess) zeroInitGuess = true;

//    std::cout << "level: " << l << std::endl;

      tm = Teuchos::null;
      tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("vCycle: 8 - post-smoother")));

      // post-smoothing
      smootherApply(smootherParams[l], fineRegX, fineRegB, regMatrices[l],
                    regRowMaps[l], regRowImporters[l], zeroInitGuess);

      tm = Teuchos::null;

    }
  } else {

    // Coarsest grid solve

    RCP<Teuchos::FancyOStream> fos = Teuchos::fancyOStream(Teuchos::rcpFromRef(std::cout));
    fos->setOutputToRootOnly(0);

    RCP<TimeMonitor> tm = rcp(new TimeMonitor(*TimeMonitor::getNewTimer("vCycle: * - coarsest grid solve")));

    // std::cout << "Applying coarse colver" << std::endl;

    const std::string coarseSolverType = coarseSolverData->get<std::string>("coarse solver type");
    if (coarseSolverType == "smoother") {
      smootherApply(smootherParams[l], fineRegX, fineRegB, regMatrices[l],
                  regRowMaps[l], regRowImporters[l], zeroInitGuess);
    } else {
      zeroInitGuess = false;
      // First get the Xpetra vectors from region to composite format
      RCP<const Map> coarseRowMap = coarseSolverData->get<RCP<const Map> >("compCoarseRowMap");
      RCP<Vector> compX = VectorFactory::Build(coarseRowMap, true);
      RCP<Vector> compRhs = VectorFactory::Build(coarseRowMap, true);
      {
        RCP<Vector> inverseInterfaceScaling = VectorFactory::Build(regInterfaceScalings[l]->getMap());
        inverseInterfaceScaling->reciprocal(*regInterfaceScalings[l]);
        fineRegB->elementWiseMultiply(SC_ONE, *fineRegB, *inverseInterfaceScaling, SC_ZERO);

        regionalToComposite(fineRegB, compRhs, regRowImporters[l]);
      }

      if (coarseSolverType == "direct")
      {
#if defined(HAVE_MUELU_TPETRA) && defined(HAVE_MUELU_AMESOS2)

        using DirectCoarseSolver = Amesos2::Solver<Tpetra::CrsMatrix<Scalar, LocalOrdinal, GlobalOrdinal, Node>, Tpetra::MultiVector<Scalar, LocalOrdinal, GlobalOrdinal, Node> >;
        RCP<DirectCoarseSolver> coarseSolver = coarseSolverData->get<RCP<DirectCoarseSolver> >("direct solver object");

        TEUCHOS_TEST_FOR_EXCEPT_MSG(coarseRowMap->lib()!=Xpetra::UseTpetra,
            "Coarse solver requires Tpetra/Amesos2 stack.");
        TEUCHOS_ASSERT(!coarseSolver.is_null());

        // using Utilities = MueLu::Utilities<Scalar, LocalOrdinal, GlobalOrdinal, Node>;

        // From here on we switch to Tpetra for simplicity
        // we could also implement a similar Epetra branch
        using Tpetra_MultiVector = Tpetra::MultiVector<Scalar, LocalOrdinal, GlobalOrdinal, Node>;

      //    *fos << "Attempting to use Amesos2 to solve the coarse grid problem" << std::endl;
        RCP<Tpetra_MultiVector> tX = Utilities::MV2NonConstTpetraMV2(*compX);
        RCP<const Tpetra_MultiVector> tB = Utilities::MV2TpetraMV(compRhs);

        /* Solve!
         *
         * Calling solve() on the coarseSolver should just do a triangular solve, since symbolic
         * and numeric factorization are supposed to have happened during hierarchy setup.
         * Here, we just check if they're done and print message if not.
         *
         * We don't have to change the map of tX and tB since we have configured the Amesos2 solver
         * during its construction to work with non-continuous maps.
         */
        if (not coarseSolver->getStatus().symbolicFactorizationDone())
          *fos << "Symbolic factorization should have been done during hierarchy setup, "
              "but actually is missing. Anyway ... just do it right now." << std::endl;
        if (not coarseSolver->getStatus().numericFactorizationDone())
          *fos << "Numeric factorization should have been done during hierarchy setup, "
              "but actually is missing. Anyway ... just do it right now." << std::endl;
        coarseSolver->solve(tX.ptr(), tB.ptr());
#else
        *fos << "+++++++++++++++++++++++++++ WARNING +++++++++++++++++++++++++\n"
             << "+ Coarse level direct solver requires Tpetra and Amesos2.   +\n"
             << "+ Skipping the coarse level solve.                          +\n"
             << "+++++++++++++++++++++++++++ WARNING +++++++++++++++++++++++++"
             << std::endl;
#endif
      }
      else if (coarseSolverType == "amg") // use AMG as coarse level solver
      {
        const bool coarseSolverRebalance = coarseSolverData->get<bool>("coarse solver rebalance");

        // Extract the hierarchy from the coarseSolverData
        RCP<Hierarchy> amgHierarchy = coarseSolverData->get<RCP<Hierarchy>>("amg hierarchy object");

        // Run a single V-cycle
        if(coarseSolverRebalance==false){
          amgHierarchy->Iterate(*compRhs, *compX, 1, true);

        } else {
#if defined(HAVE_MUELU_ZOLTAN2) && defined(HAVE_MPI)
          RCP<const Import> rebalanceImporter = coarseSolverData->get<RCP<const Import> >("rebalanceImporter");

          // TODO: These vectors could be cached to improve performance
          RCP<Vector> rebalancedRhs = VectorFactory::Build(rebalanceImporter->getTargetMap());
          RCP<Vector> rebalancedX = VectorFactory::Build(rebalanceImporter->getTargetMap(), true);
          rebalancedRhs->doImport(*compRhs, *rebalanceImporter, Xpetra::INSERT);

          rebalancedRhs->replaceMap(rebalancedRhs->getMap()->removeEmptyProcesses());
          rebalancedX->replaceMap(rebalancedX->getMap()->removeEmptyProcesses());

          if(!amgHierarchy.is_null()){
            amgHierarchy->Iterate(*rebalancedRhs, *rebalancedX, 1, true);
          }

          rebalancedX->replaceMap(rebalanceImporter->getTargetMap());
          compX->doExport(*rebalancedX, *rebalanceImporter, Xpetra::INSERT);
#else
          amgHierarchy->Iterate(*compRhs, *compX, 1, true);
#endif
        }
      }
      else
      {
        TEUCHOS_TEST_FOR_EXCEPT_MSG(false, "Unknown coarse solver type.");
      }

      // Transform back to region format
      RCP<Vector> quasiRegX;
      compositeToRegional(compX, quasiRegX, fineRegX,
                          regRowMaps[l],
                          regRowImporters[l]);

      tm = Teuchos::null;
    }
  }

  return;
} // vCycle

#endif // MUELU_SETUPREGIONHIERARCHY_DEF_HPP
