#include "MLTreeMaker.h"


// Tracks
#include "TrkTrack/Track.h"
#include "TrkParameters/TrackParameters.h"
#include "TrkExInterfaces/IExtrapolator.h"
#include "xAODTracking/VertexAuxContainer.h"

//Jets
#include "xAODJet/JetTypes.h"

// Extrapolation to the calo
#include "TrkCaloExtension/CaloExtension.h"
#include "TrkCaloExtension/CaloExtensionCollection.h"
#include "TrkParametersIdentificationHelpers/TrackParametersIdHelper.h"
#include "CaloDetDescr/CaloDepthTool.h"
#include "Identifier/IdentifierHash.h"

// Calo and cell information
#include "TileEvent/TileContainer.h"
#include "TileIdentifier/TileTBID.h"
#include "CaloEvent/CaloCellContainer.h"
#include "TrkSurfaces/DiscSurface.h"
#include "GeoPrimitives/GeoPrimitives.h"
#include "CaloUtils/CaloClusterSignalState.h"
#include "CaloEvent/CaloClusterCellLinkContainer.h"
#include "xAODCaloEvent/CaloClusterChangeSignalState.h"
// Other xAOD incudes
#include "xAODTruth/TruthEventContainer.h"
#include "AthContainers/AuxElement.h"

#include <string>
#include <vector>
#include <cmath>
#include <utility>
#include <limits>
#include <map>

MLTreeMaker::MLTreeMaker(const std::string &name, ISvcLocator *pSvcLocator) : AthHistogramAlgorithm(name, pSvcLocator),
                                                                              m_trackParametersIdHelper(std::make_unique<Trk::TrackParametersIdHelper>())
{}

MLTreeMaker::~MLTreeMaker() {}

StatusCode MLTreeMaker::initialize()
{
  ATH_MSG_INFO("Initializing " << name() << "...");  

  ATH_CHECK(m_theTrackExtrapolatorTool.retrieve());
  ATH_CHECK(m_trkSelectionTool.retrieve());

  //Initialize the ReadHandle keys
  ATH_CHECK(m_chargedFlowElementReadHandleKey.initialize());
  ATH_CHECK(m_truthParticleReadHandleKey.initialize());
  ATH_CHECK(m_vxReadHandleKey.initialize());
  ATH_CHECK(m_trackParticleReadHandleKey.initialize());
  ATH_CHECK(m_caloClusterReadHandleKey.initialize());
  ATH_CHECK(m_eventInfoReadHandleKey.initialize());
  if (!m_lcTopoEventShapeReadHandleKey.empty()) ATH_CHECK(m_lcTopoEventShapeReadHandleKey.initialize());
  if (!m_emTopoEventShapeReadHandleKey.empty()) ATH_CHECK(m_emTopoEventShapeReadHandleKey.initialize());
  ATH_CHECK(m_truthEventReadHandleKey.initialize());
  ATH_CHECK(m_jetReadHandleKeyArray.initialize());
  ATH_CHECK(m_CalibrationHitContainerKeys.initialize());
  ATH_CHECK(m_caloClusterCalibHitsDecorHandleKey.initialize());
  ATH_CHECK(m_caloCellReadHandleKey.initialize());

  // Setup the event level TTree and its branches
  CHECK(book(TTree("EventTree", "EventTree")));
  m_eventTree = tree("EventTree");

  // Event info
  m_eventTree->Branch("runNumber", &m_runNumber, "runNumber/I");
  m_eventTree->Branch("eventNumber", &m_eventNumber, "eventNumber/LI");
  m_eventTree->Branch("lumiBlock", &m_lumiBlock, "lumiBlock/I");
  m_eventTree->Branch("coreFlags", &m_coreFlags, "coreFlags/i");
  
  m_eventTree->Branch("mcEventNumber", &m_mcEventNumber, "mcEventNumber/I");
  m_eventTree->Branch("mcChannelNumber", &m_mcChannelNumber, "mcChannelNumber/I");
  m_eventTree->Branch("mcEventWeight", &m_mcEventWeight, "mcEventWeight/F");
  
  if (m_doEventCleaning)
  {
    m_eventTree->Branch("timeStamp", &m_timeStamp, "timeStamp/i");
    m_eventTree->Branch("timeStampNSOffset", &m_timeStampNSOffset, "timeStampNSOffset/i");
    m_eventTree->Branch("TileError", &m_TileError, "TileError/O");
    m_eventTree->Branch("SCTError", &m_SCTError, "SCTError/O");
    m_eventTree->Branch("LArError", &m_LArError, "LArError/O");
    m_eventTree->Branch("TileFlags", &m_TileFlags, "TileFlags/i");
    m_eventTree->Branch("SCTFlags", &m_SCTFlags, "SCTFlags/i");
    m_eventTree->Branch("LArFlags", &m_LArFlags, "LArFlags/i");
  }
  if (m_doPileup)
  {
    m_eventTree->Branch("NPV", &m_npv, "NPV/I");
    m_eventTree->Branch("actualInteractionsPerCrossing", &m_actualMu, "actualInteractionsPerCrossing/F");
    m_eventTree->Branch("averageInteractionsPerCrossing", &m_averageMu, "averageInteractionsPerCrossing/F");
    m_eventTree->Branch("weight_pileup", &m_weight_pileup, "weight_pileup/F");
   
    m_eventTree->Branch("correct_mu", &m_correct_mu, "correct_mu/F");
    m_eventTree->Branch("rand_run_nr", &m_rand_run_nr, "rand_run_nr/I");
    m_eventTree->Branch("rand_lumiblock_nr", &m_rand_lumiblock_nr, "rand_lumiblock_nr/I");
   
  }
  if (m_doShapeEM)
    m_eventTree->Branch("rhoEM", &m_rhoEM, "rhoEM/D");
  if (m_doShapeLC)
    m_eventTree->Branch("rhoLC", &m_rhoLC, "rhoLC/D");
  if (m_doEventTruth )
  {
    m_eventTree->Branch("pdgId1", &m_pdgId1, "pdgId1/I");
    m_eventTree->Branch("pdgId2", &m_pdgId2, "pdgId2/I");
    m_eventTree->Branch("pdfId1", &m_pdfId1, "pdfId1/I");
    m_eventTree->Branch("pdfId2", &m_pdfId2, "pdfId2/I");
    m_eventTree->Branch("x1", &m_x1, "x1/F");
    m_eventTree->Branch("x2", &m_x2, "x2/F");
   
    m_eventTree->Branch("xf1", &m_xf1, "xf1/F");
    m_eventTree->Branch("xf2", &m_xf2, "xf2/F");
  }

  // Truth particles
  
  if (m_doTruthParticles || m_doCalibHits || m_doTruthParticlesPerCell || m_doTrackTruthMatching) m_eventTree->Branch("nTruthPart", &m_nTruthPart, "nTruthPart/I");

  if (m_doCalibHits || m_doTruthParticles) m_eventTree->Branch("truthPartBarcode", &m_truthPartBarcode);

  if (m_doTrackTruthMatching || m_doTruthParticles) m_eventTree->Branch("truthPartPdgId", &m_truthPartPdgId);

  if (m_doTruthParticles)
  {
    m_eventTree->Branch("G4PreCalo_n_EM", &m_G4PreCalo_n_EM);
    m_eventTree->Branch("G4PreCalo_E_EM", &m_G4PreCalo_E_EM);
    m_eventTree->Branch("G4PreCalo_n_Had", &m_G4PreCalo_n_Had);
    m_eventTree->Branch("G4PreCalo_E_Had", &m_G4PreCalo_E_Had);
    m_eventTree->Branch("truthVertexX", &m_truthVertexX);
    m_eventTree->Branch("truthVertexY", &m_truthVertexY);
    m_eventTree->Branch("truthVertexZ", &m_truthVertexZ);
    m_eventTree->Branch("truthPartStatus", &m_truthPartStatus);
    m_eventTree->Branch("truthPartPt", &m_truthPartPt);
    m_eventTree->Branch("truthPartE", &m_truthPartE);
    m_eventTree->Branch("truthPartMass", &m_truthPartMass);
    m_eventTree->Branch("truthPartEta", &m_truthPartEta);
    m_eventTree->Branch("truthPartPhi", &m_truthPartPhi);
  }

  // Track variables
  if (m_doTracking)
  {
    m_eventTree->Branch("nTrack", &m_nTrack, "nTrack/I");
    m_eventTree->Branch("trackPt", &m_trackPt);
    m_eventTree->Branch("trackP", &m_trackP);
    m_eventTree->Branch("trackMass", &m_trackMass);
    m_eventTree->Branch("trackEta", &m_trackEta);
    m_eventTree->Branch("trackPhi", &m_trackPhi);  
    m_eventTree->Branch("trackTruthParticleIndex", &m_trackTruthParticleIndex);
    m_eventTree->Branch("trackVisibleCalHitCaloEnergy", &m_trackVisibleCalHitCaloEnergy);
    m_eventTree->Branch("trackFullCalHitCaloEnergy", &m_trackFullCalHitCaloEnergy);
    m_eventTree->Branch("trackSubtractedCaloEnergy", &m_trackSubtractedCaloEnergy);

    // Track quality variables
    if (m_doDetailedTracking){
      m_eventTree->Branch("trackNumberOfPixelHits", &m_trackNumberOfPixelHits);
      m_eventTree->Branch("trackNumberOfSCTHits", &m_trackNumberOfSCTHits);
      m_eventTree->Branch("trackNumberOfPixelDeadSensors", &m_trackNumberOfPixelDeadSensors);
      m_eventTree->Branch("trackNumberOfSCTDeadSensors", &m_trackNumberOfSCTDeadSensors);
      m_eventTree->Branch("trackNumberOfPixelSharedHits", &m_trackNumberOfPixelSharedHits);
      m_eventTree->Branch("trackNumberOfSCTSharedHits", &m_trackNumberOfSCTSharedHits);
      m_eventTree->Branch("trackNumberOfPixelHoles", &m_trackNumberOfPixelHoles);
      m_eventTree->Branch("trackNumberOfSCTHoles", &m_trackNumberOfSCTHoles);
      m_eventTree->Branch("trackNumberOfInnermostPixelLayerHits", &m_trackNumberOfInnermostPixelLayerHits);
      m_eventTree->Branch("trackNumberOfNextToInnermostPixelLayerHits", &m_trackNumberOfNextToInnermostPixelLayerHits);
      m_eventTree->Branch("trackExpectInnermostPixelLayerHit", &m_trackExpectInnermostPixelLayerHit);
      m_eventTree->Branch("trackExpectNextToInnermostPixelLayerHit", &m_trackExpectNextToInnermostPixelLayerHit);
      m_eventTree->Branch("trackNumberOfTRTHits", &m_trackNumberOfTRTHits);
      m_eventTree->Branch("trackNumberOfTRTOutliers", &m_trackNumberOfTRTOutliers);
      m_eventTree->Branch("trackChiSquared", &m_trackChiSquared);
      m_eventTree->Branch("trackNumberDOF", &m_trackNumberDOF);
      m_eventTree->Branch("trackD0", &m_trackD0);
      m_eventTree->Branch("trackZ0", &m_trackZ0);
    }

    // Track extrapolation
    // Presampler
    m_eventTree->Branch("trackEta_PreSamplerB", &m_trackEta_PreSamplerB);
    m_eventTree->Branch("trackPhi_PreSamplerB", &m_trackPhi_PreSamplerB);
    m_eventTree->Branch("trackEta_PreSamplerE", &m_trackEta_PreSamplerE);
    m_eventTree->Branch("trackPhi_PreSamplerE", &m_trackPhi_PreSamplerE);
    // LAr EM Barrel layers
    m_eventTree->Branch("trackEta_EMB1", &m_trackEta_EMB1);
    m_eventTree->Branch("trackPhi_EMB1", &m_trackPhi_EMB1);
    m_eventTree->Branch("trackEta_EMB2", &m_trackEta_EMB2);
    m_eventTree->Branch("trackPhi_EMB2", &m_trackPhi_EMB2);
    m_eventTree->Branch("trackEta_EMB3", &m_trackEta_EMB3);
    m_eventTree->Branch("trackPhi_EMB3", &m_trackPhi_EMB3);
    // LAr EM Endcap layers
    m_eventTree->Branch("trackEta_EME1", &m_trackEta_EME1);
    m_eventTree->Branch("trackPhi_EME1", &m_trackPhi_EME1);
    m_eventTree->Branch("trackEta_EME2", &m_trackEta_EME2);
    m_eventTree->Branch("trackPhi_EME2", &m_trackPhi_EME2);
    m_eventTree->Branch("trackEta_EME3", &m_trackEta_EME3);
    m_eventTree->Branch("trackPhi_EME3", &m_trackPhi_EME3);
    // Hadronic Endcap layers
    m_eventTree->Branch("trackEta_HEC0", &m_trackEta_HEC0);
    m_eventTree->Branch("trackPhi_HEC0", &m_trackPhi_HEC0);
    m_eventTree->Branch("trackEta_HEC1", &m_trackEta_HEC1);
    m_eventTree->Branch("trackPhi_HEC1", &m_trackPhi_HEC1);
    m_eventTree->Branch("trackEta_HEC2", &m_trackEta_HEC2);
    m_eventTree->Branch("trackPhi_HEC2", &m_trackPhi_HEC2);
    m_eventTree->Branch("trackEta_HEC3", &m_trackEta_HEC3);
    m_eventTree->Branch("trackPhi_HEC3", &m_trackPhi_HEC3);
    // Tile Barrel layers
    m_eventTree->Branch("trackEta_TileBar0", &m_trackEta_TileBar0);
    m_eventTree->Branch("trackPhi_TileBar0", &m_trackPhi_TileBar0);
    m_eventTree->Branch("trackEta_TileBar1", &m_trackEta_TileBar1);
    m_eventTree->Branch("trackPhi_TileBar1", &m_trackPhi_TileBar1);
    m_eventTree->Branch("trackEta_TileBar2", &m_trackEta_TileBar2);
    m_eventTree->Branch("trackPhi_TileBar2", &m_trackPhi_TileBar2);
    // Tile Gap layers
    m_eventTree->Branch("trackEta_TileGap1", &m_trackEta_TileGap1);
    m_eventTree->Branch("trackPhi_TileGap1", &m_trackPhi_TileGap1);
    m_eventTree->Branch("trackEta_TileGap2", &m_trackEta_TileGap2);
    m_eventTree->Branch("trackPhi_TileGap2", &m_trackPhi_TileGap2);
    m_eventTree->Branch("trackEta_TileGap3", &m_trackEta_TileGap3);
    m_eventTree->Branch("trackPhi_TileGap3", &m_trackPhi_TileGap3);
    // Tile Extended Barrel layers
    m_eventTree->Branch("trackEta_TileExt0", &m_trackEta_TileExt0);
    m_eventTree->Branch("trackPhi_TileExt0", &m_trackPhi_TileExt0);
    m_eventTree->Branch("trackEta_TileExt1", &m_trackEta_TileExt1);
    m_eventTree->Branch("trackPhi_TileExt1", &m_trackPhi_TileExt1);
    m_eventTree->Branch("trackEta_TileExt2", &m_trackEta_TileExt2);
    m_eventTree->Branch("trackPhi_TileExt2", &m_trackPhi_TileExt2);
  }

  if (m_doJets)
  {
    unsigned int nJetColl = m_jetReadHandleKeyArray.size();

    m_jet_pt.clear();
    m_jet_eta.clear();
    m_jet_phi.clear();
    m_jet_E.clear();
    m_jet_flavor.clear();

    m_jet_pt.assign(nJetColl, std::vector<float>());
    m_jet_eta.assign(nJetColl, std::vector<float>());
    m_jet_phi.assign(nJetColl, std::vector<float>());
    m_jet_E.assign(nJetColl, std::vector<float>());
    m_jet_flavor.assign(nJetColl, std::vector<int>());

    unsigned int iColl = 0;
    for (auto jetKey : m_jetReadHandleKeyArray)
    {
      std::string jet_name = jetKey.key();
      std::stringstream ss;
      ss << jet_name << "Pt";
      m_eventTree->Branch(ss.str().c_str(), &(m_jet_pt[iColl]));

      ss.str("");
      ss << jet_name << "Eta";
      m_eventTree->Branch(ss.str().c_str(), &(m_jet_eta[iColl]));

      ss.str("");
      ss << jet_name << "Phi";
      m_eventTree->Branch(ss.str().c_str(), &(m_jet_phi[iColl]));

      ss.str("");
      ss << jet_name << "E";
      m_eventTree->Branch(ss.str().c_str(), &(m_jet_E[iColl]));

      if (jet_name.find("Truth") != std::string::npos)
      {
        ss.str("");
        ss << jet_name << "Flavor";
        m_eventTree->Branch(ss.str().c_str(), &(m_jet_flavor[iColl]));
      }
      iColl++;
    }
  }
  if (m_doClusters)
  {
    // Clusters
    m_eventTree->Branch("nCluster", &m_nCluster, "nCluster/I");
    m_eventTree->Branch("cluster_E", &m_cluster_E);
    m_eventTree->Branch("cluster_E_LCCalib", &m_cluster_E_LCCalib);
    m_eventTree->Branch("cluster_Pt", &m_cluster_Pt);
    m_eventTree->Branch("cluster_Eta", &m_cluster_Eta);
    m_eventTree->Branch("cluster_Phi", &m_cluster_Phi);
    m_eventTree->Branch("cluster_nCells", &m_cluster_nCells);
    if (m_doClusterMoments)
    {
      m_eventTree->Branch("cluster_ENG_CALIB_TOT", &m_cluster_ENG_CALIB_TOT);
      m_eventTree->Branch("cluster_ENG_CALIB_OUT_T", &m_cluster_ENG_CALIB_OUT_T);
      m_eventTree->Branch("cluster_ENG_CALIB_DEAD_TOT", &m_cluster_ENG_CALIB_DEAD_TOT);
      m_eventTree->Branch("cluster_EM_PROBABILITY", &m_cluster_EM_PROBABILITY);
      m_eventTree->Branch("cluster_HAD_WEIGHT", &m_cluster_HAD_WEIGHT);
      m_eventTree->Branch("cluster_OOC_WEIGHT", &m_cluster_OOC_WEIGHT);
      m_eventTree->Branch("cluster_DM_WEIGHT", &m_cluster_DM_WEIGHT);
      m_eventTree->Branch("cluster_CENTER_MAG", &m_cluster_CENTER_MAG);
      m_eventTree->Branch("cluster_FIRST_ENG_DENS", &m_cluster_FIRST_ENG_DENS);
      m_eventTree->Branch("cluster_CENTER_LAMBDA", &m_cluster_CENTER_LAMBDA);
      m_eventTree->Branch("cluster_ISOLATION", &m_cluster_ISOLATION);
      m_eventTree->Branch("cluster_ENERGY_DigiHSTruth", &m_cluster_ENERGY_DigiHSTruth);
    }

    if (m_doClusterCells)
    {
      m_eventTree->Branch("cluster_cell_ID", &m_cluster_cell_ID);
      m_eventTree->Branch("cluster_cell_E", &m_cluster_cell_E);
      if (m_doCalibHits)
      {
        if (m_doCalibHitsPerCell)
        {
          m_eventTree->Branch("cluster_cell_hitsE_EM", &m_cluster_cell_hitsE_EM);
          m_eventTree->Branch("cluster_cell_hitsE_nonEM", &m_cluster_cell_hitsE_nonEM);
          m_eventTree->Branch("cluster_cell_hitsE_Invisible", &m_cluster_cell_hitsE_Invisible);
          m_eventTree->Branch("cluster_cell_hitsE_Escaped", &m_cluster_cell_hitsE_Escaped);
        }
        if (m_doTruthParticles)
        {
          m_eventTree->Branch("cluster_fullHitsTruthIndex", &m_cluster_fullHitsTruthIndex);
          m_eventTree->Branch("cluster_fullHitsTruthE", &m_cluster_fullHitsTruthE);
          m_eventTree->Branch("cluster_visibleHitsTruthIndex", &m_cluster_visibleHitsTruthIndex);
          m_eventTree->Branch("cluster_visibleHitsTruthE", &m_cluster_visibleHitsTruthE);
        }
        if (m_doTruthParticlesPerCell)
        {
          m_eventTree->Branch("cluster_cell_hitsTruthIndex",&m_cluster_cell_hitsTruthIndex);
          m_eventTree->Branch("cluster_cell_hitsTruthE",&m_cluster_cell_hitsTruthE);
        }
      }
    }
  }

  if (m_doAllCells){
    m_eventTree->Branch("nAllCells", &m_nCells);
    m_eventTree->Branch("cell_E", &m_cell_E);
    m_eventTree->Branch("cell_ID", &m_cell_ID);
    m_eventTree->Branch("cell_Time", &m_cell_Time);
    m_eventTree->Branch("cell_Quality", &m_cell_Quality);
  }

  return StatusCode::SUCCESS;
}

StatusCode MLTreeMaker::execute()
{

  // Clear all variables from previous event
  m_runNumber = m_eventNumber = m_mcEventNumber = m_mcChannelNumber = m_bcid = m_lumiBlock = 0;
  m_coreFlags = 0;
  // event cleaning
  m_LArError = false;
  m_TileError = false;
  m_SCTError = false;
  m_LArFlags = 0;
  m_TileFlags = 0;
  m_SCTFlags = 0;
  m_mcEventWeight = 1.;
  m_prescale_DataWeight = 1.;
  m_weight_pileup = 1.;
  m_timeStamp = -999;
  m_timeStampNSOffset = -999;
  // pileup
  m_npv = -999;
  m_actualMu = m_averageMu = -999;
  // shapeEM
  m_rhoEM = -999;
  // shapeLC
  m_rhoLC = -999;
  // truth
  m_pdgId1 = m_pdgId2 = m_pdfId1 = m_pdfId2 = -999;
  m_x1 = m_x2 = -999;
  m_xf1 = m_xf2 = -999;

  m_truthPartPdgId.clear();
  m_truthPartStatus.clear();
  m_truthPartBarcode.clear();
  m_truthPartPt.clear();
  m_truthPartE.clear();
  m_truthPartMass.clear();
  m_truthPartEta.clear();
  m_truthPartPhi.clear();

  m_trackPt.clear();
  m_trackP.clear();
  m_trackMass.clear();
  m_trackEta.clear();
  m_trackPhi.clear();
  m_trackTruthParticleIndex.clear();
  m_trackVisibleCalHitCaloEnergy.clear();
  m_trackFullCalHitCaloEnergy.clear();
  m_trackSubtractedCaloEnergy.clear();

  m_trackNumberOfPixelHits.clear();
  m_trackNumberOfSCTHits.clear();
  m_trackNumberOfPixelDeadSensors.clear();
  m_trackNumberOfSCTDeadSensors.clear();
  m_trackNumberOfPixelSharedHits.clear();
  m_trackNumberOfSCTSharedHits.clear();
  m_trackNumberOfPixelHoles.clear();
  m_trackNumberOfSCTHoles.clear();
  m_trackNumberOfInnermostPixelLayerHits.clear();
  m_trackNumberOfNextToInnermostPixelLayerHits.clear();
  m_trackExpectInnermostPixelLayerHit.clear();
  m_trackExpectNextToInnermostPixelLayerHit.clear();
  m_trackNumberOfTRTHits.clear();
  m_trackNumberOfTRTOutliers.clear();
  m_trackChiSquared.clear();
  m_trackNumberDOF.clear();
  m_trackD0.clear();
  m_trackZ0.clear();

  m_trackEta_PreSamplerB.clear();
  m_trackPhi_PreSamplerB.clear();
  m_trackEta_PreSamplerE.clear();
  m_trackPhi_PreSamplerE.clear();

  m_trackEta_EMB1.clear();
  m_trackPhi_EMB1.clear();
  m_trackEta_EMB2.clear();
  m_trackPhi_EMB2.clear();
  m_trackEta_EMB3.clear();
  m_trackPhi_EMB3.clear();

  m_trackEta_EME1.clear();
  m_trackPhi_EME1.clear();
  m_trackEta_EME2.clear();
  m_trackPhi_EME2.clear();
  m_trackEta_EME3.clear();
  m_trackPhi_EME3.clear();

  m_trackEta_HEC0.clear();
  m_trackPhi_HEC0.clear();
  m_trackEta_HEC1.clear();
  m_trackPhi_HEC1.clear();
  m_trackEta_HEC2.clear();
  m_trackPhi_HEC2.clear();
  m_trackEta_HEC3.clear();
  m_trackPhi_HEC3.clear();

  m_trackEta_TileBar0.clear();
  m_trackPhi_TileBar0.clear();
  m_trackEta_TileBar1.clear();
  m_trackPhi_TileBar1.clear();
  m_trackEta_TileBar2.clear();
  m_trackPhi_TileBar2.clear();

  m_trackEta_TileGap1.clear();
  m_trackPhi_TileGap1.clear();
  m_trackEta_TileGap2.clear();
  m_trackPhi_TileGap2.clear();
  m_trackEta_TileGap3.clear();
  m_trackPhi_TileGap3.clear();

  m_trackEta_TileExt0.clear();
  m_trackPhi_TileExt0.clear();
  m_trackEta_TileExt1.clear();
  m_trackPhi_TileExt1.clear();
  m_trackEta_TileExt2.clear();
  m_trackPhi_TileExt2.clear();

  // General event information
  
  SG::ReadHandle<xAOD::EventInfo> eventInfoReadHandle(m_eventInfoReadHandleKey);
  
  if (!eventInfoReadHandle.isValid())
  {
    ATH_MSG_WARNING("Invalid ReadHandle to EventInfo with key " << eventInfoReadHandle.key());
    return StatusCode::SUCCESS;
  }
  
  m_runNumber = eventInfoReadHandle->runNumber();
  m_eventNumber = eventInfoReadHandle->eventNumber();
  m_lumiBlock = eventInfoReadHandle->lumiBlock();
  m_coreFlags = eventInfoReadHandle->eventFlags(xAOD::EventInfo::Core);
  m_mcEventNumber = eventInfoReadHandle->mcEventNumber();
  m_mcChannelNumber = eventInfoReadHandle->mcChannelNumber();
  m_mcEventWeight = eventInfoReadHandle->mcEventWeight();

  if (m_doEventCleaning)
  {

    if (eventInfoReadHandle->errorState(xAOD::EventInfo::LAr) == xAOD::EventInfo::Error)
      m_LArError = true;
    else
      m_LArError = false;
    m_LArFlags = eventInfoReadHandle->eventFlags(xAOD::EventInfo::LAr);

    if (eventInfoReadHandle->errorState(xAOD::EventInfo::Tile) == xAOD::EventInfo::Error)
      m_TileError = true;
    else
      m_TileError = false;
    m_TileFlags = eventInfoReadHandle->eventFlags(xAOD::EventInfo::Tile);

    if (eventInfoReadHandle->errorState(xAOD::EventInfo::SCT) == xAOD::EventInfo::Error)
      m_SCTError = true;
    else
      m_SCTError = false;
    m_SCTFlags = eventInfoReadHandle->eventFlags(xAOD::EventInfo::SCT);

    m_timeStamp = eventInfoReadHandle->timeStamp();
    m_timeStampNSOffset = eventInfoReadHandle->timeStampNSOffset();
  }
  
  if (m_doPileup)
  {

    SG::ReadHandle<xAOD::VertexContainer> vxReadHandle(m_vxReadHandleKey);
    if (!vxReadHandle.isValid())
    {
      ATH_MSG_WARNING("Invalid ReadHandle to VertexContainer with key " << vxReadHandle.key());
      return StatusCode::SUCCESS;
    }

    m_npv = 0;
    unsigned int Ntracks = 2;
    for (auto vertex : *vxReadHandle)
    {
      if (vertex->vertexType() == xAOD::VxType::NoVtx)
        continue;
      if (vertex->nTrackParticles() < Ntracks)
        continue;
      m_npv++;
    }
    m_actualMu = eventInfoReadHandle->actualInteractionsPerCrossing();
    m_averageMu = eventInfoReadHandle->averageInteractionsPerCrossing();
    
    static SG::AuxElement::ConstAccessor<float> weight_pileup("PileupWeight");
    static SG::AuxElement::ConstAccessor<float> correct_mu("corrected_averageInteractionsPerCrossing");
    static SG::AuxElement::ConstAccessor<unsigned int> rand_run_nr("RandomRunNumber");
    static SG::AuxElement::ConstAccessor<unsigned int> rand_lumiblock_nr("RandomLumiBlockNumber");

    if (weight_pileup.isAvailable(*eventInfoReadHandle))
    {
      m_weight_pileup = weight_pileup(*eventInfoReadHandle);
    }
    else
    {
      m_weight_pileup = 1.0;
    }
    if (correct_mu.isAvailable(*eventInfoReadHandle))
    {
      m_correct_mu = correct_mu(*eventInfoReadHandle);
    }
    else
    {
      m_correct_mu = -1.0;
    }
    if (rand_run_nr.isAvailable(*eventInfoReadHandle))
    {
      m_rand_run_nr = rand_run_nr(*eventInfoReadHandle);
    }
    else
    {
      m_rand_run_nr = 900000;
    }
    if (rand_lumiblock_nr.isAvailable(*eventInfoReadHandle))
    {
      m_rand_lumiblock_nr = rand_lumiblock_nr(*eventInfoReadHandle);
    }
    else
    {
      m_rand_lumiblock_nr = 0;
    }
  }

  if (m_doShapeLC)
  {
    SG::ReadHandle<xAOD::EventShape> lcTopoEventShapeReadHandle(m_lcTopoEventShapeReadHandleKey);
    if (!lcTopoEventShapeReadHandle.isValid())
    {
      ATH_MSG_WARNING("Invalid ReadHandle to EventShape with key " << lcTopoEventShapeReadHandle.key());
      return StatusCode::SUCCESS;
    }

    if (!lcTopoEventShapeReadHandle->getDensity(xAOD::EventShape::Density, m_rhoLC))
    {
      Info("FillEvent()", "Could not retrieve xAOD::EventShape::Density from xAOD::EventShape");
      m_rhoLC = -999;
    }
  }
  
  if (m_doShapeEM)
  {
    SG::ReadHandle<xAOD::EventShape> emTopoEventShapeReadHandle(m_emTopoEventShapeReadHandleKey);
    if (!emTopoEventShapeReadHandle.isValid())
    {
      ATH_MSG_WARNING("Invalid ReadHandle to EventShape with key " << emTopoEventShapeReadHandle.key());
      return StatusCode::SUCCESS;
    }

    if (!emTopoEventShapeReadHandle->getDensity(xAOD::EventShape::Density, m_rhoEM))
    {
      Info("FillEvent()", "Could not retrieve xAOD::EventShape::Density from xAOD::EventShape");
      m_rhoEM = -999;
    }
  }
  if (m_doEventTruth)
  {
    SG::ReadHandle<xAOD::TruthEventContainer> truthEventReadHandle(m_truthEventReadHandleKey);
    if (!truthEventReadHandle.isValid())
    {
      ATH_MSG_WARNING("Invalid ReadHandle to TruthEventContainer with key " << truthEventReadHandle.key());
      return StatusCode::SUCCESS;
    }
    else
    {
      const xAOD::TruthEvent *truthEventContainervent = truthEventReadHandle->at(0);

      truthEventContainervent->pdfInfoParameter(m_pdgId1, xAOD::TruthEvent::PDGID1);
      truthEventContainervent->pdfInfoParameter(m_pdgId2, xAOD::TruthEvent::PDGID2);
      truthEventContainervent->pdfInfoParameter(m_pdfId1, xAOD::TruthEvent::PDFID1);
      truthEventContainervent->pdfInfoParameter(m_pdfId2, xAOD::TruthEvent::PDFID2);
      truthEventContainervent->pdfInfoParameter(m_x1, xAOD::TruthEvent::X1);
      truthEventContainervent->pdfInfoParameter(m_x2, xAOD::TruthEvent::X2);
      truthEventContainervent->pdfInfoParameter(m_xf1, xAOD::TruthEvent::XF1);
      truthEventContainervent->pdfInfoParameter(m_xf2, xAOD::TruthEvent::XF2);
    }
  }

  std::vector<const CaloCalibrationHitContainer *> v_calibHitContainer;
  if (m_doClusterCells && m_doCalibHits)
  {
    for (auto calibHitKey : m_CalibrationHitContainerKeys)
    {
      SG::ReadHandle<CaloCalibrationHitContainer> caloCalibHitReadHandle(calibHitKey);
      if (!caloCalibHitReadHandle.isValid())
      {
        ATH_MSG_WARNING("Invalid ReadHandle to CalibrationHitContainer with key " << caloCalibHitReadHandle.key());
        return StatusCode::SUCCESS;
      }
      v_calibHitContainer.push_back(caloCalibHitReadHandle.cptr());
    }
  }
  // Truth particles

  //calibration hits are associated with truth particles via barcode
  //construct mapping between barcode and index in output particle list for lookup later
  //note this index is not necessarily the same as the index in the truthContainer
  //e.g. you filter some of the particles
  std::map<int, unsigned int> truthBarcodeMap;

  if (m_doTruthParticles || m_doCalibHits || m_doTruthParticlesPerCell || m_doTrackTruthMatching)
  {

    SG::ReadHandle<xAOD::TruthParticleContainer> truthParticleReadHandle(m_truthParticleReadHandleKey);
    if (!truthParticleReadHandle.isValid())
    {
      ATH_MSG_WARNING("Invalid ReadHandle to TruthParticles with key " << truthParticleReadHandle.key());
      return StatusCode::SUCCESS;
    }

    m_nTruthPart = 0;
    m_G4PreCalo_n_EM = 0;
    m_G4PreCalo_E_EM = 0;
    m_G4PreCalo_n_Had = 0;
    m_G4PreCalo_E_Had = 0;
    m_truthVertexX = 0;
    m_truthVertexY = 0;
    m_truthVertexZ = 0;
    //
    for (auto truth : *truthParticleReadHandle)
    {

      if (truth->hasProdVtx())
      {
        auto prodVtx = truth->prodVtx();
        if (prodVtx->barcode() == -1)
        {
          m_truthVertexX = prodVtx->x();
          m_truthVertexY = prodVtx->y();
          m_truthVertexZ = prodVtx->z();
        }
      }
      if (truth->status() != 1 && m_keepOnlyStableTruthParticles)
        continue;
      if (truth->barcode() > m_G4BarcodeOffset)
      {
        //compute event-level observable for early showers
        //assume G4 particles in list are produced before calorimeter
        //thus particles w/o decay vertex hit the calorimeter
        //skip neutrinos, also skip nuclear fragments produced in hadronic showers
        //these are typically low E and should produce large ionization ~immediately
        if (!truth->hasDecayVtx())
        {
          //since many of these particles will be low energy hadrons
          //use kinetic energy = E-M, since mass energy unlikely to be recovered
          //also, w/o this modification, the "energy" can be greater than incident particle E
          float truthE = (truth->e() - truth->m()) * 1e-3;
          if ((truth->isHadron()))
          {
            m_G4PreCalo_E_Had += truthE;
            m_G4PreCalo_n_Had++;
          }
          else if (!truth->isNeutrino())
          {
            m_G4PreCalo_E_EM += truthE;
            m_G4PreCalo_n_EM++;
          }
        }
        if (!m_keepG4TruthParticles)
          continue;
      }

      if (m_doTruthParticles || m_doCalibHits) m_truthPartBarcode.push_back(truth->barcode());

      if (m_doTruthParticles || m_doTrackTruthMatching) m_truthPartPdgId.push_back(truth->pdgId());

      if (m_doTruthParticles){
        m_truthPartStatus.push_back(truth->status());
        m_truthPartPt.push_back(truth->pt() * 1e-3);
        m_truthPartE.push_back(truth->e() * 1e-3);
        m_truthPartMass.push_back(truth->m() * 1e-3);
        m_truthPartEta.push_back(truth->eta());
        m_truthPartPhi.push_back(truth->phi());
      }
      
      if (m_doCalibHits)
        truthBarcodeMap.emplace_hint(truthBarcodeMap.end(), truth->barcode(), m_nTruthPart);
      m_nTruthPart++;
    }
  }

  //Create maps between the truth particle matched to each track
  //(using its barcode) and the sum of the visible/full calibration hit 
  //energy for that truth particle found in topoclusters
  std::map<int, float> truthVisibleCalHitCaloEnergyMap;
  std::map<int, float> truthFullCalHitCaloEnergyMap;

  if (m_doTracking)
  {

    //We need to get the charged FlowElements such that we can find for any xAOD::TrackParticle
    //used as input to particle flow (eflowRec) how much energy was subtracted from matched xAOD::CaloCluster
    SG::ReadHandle<xAOD::FlowElementContainer> chargedFlowElementReadHandle(m_chargedFlowElementReadHandleKey);
    if (!chargedFlowElementReadHandle.isValid())
    {
      ATH_MSG_WARNING("Invalid ReadHandle for xAOD::FlowElementContainer with key: " << chargedFlowElementReadHandle.key());
      return StatusCode::SUCCESS;
    }

    //Make a map between TrackParticle pointer and subtractedEnergy for fast lookup in the track loop later on
    std::map<const xAOD::TrackParticle *, float> mapTrackSubtractedEnergy;

    //Define a function for summing up matched calocluster energies
    auto accumulateSubtractedEnergy = [](int accumulator, std::pair<const xAOD::IParticle *, float> map)
    { return accumulator + map.second; };

    for (auto thisFE : *chargedFlowElementReadHandle)
    {
      //Get the list of matched CaloCluster, along with the energy subtracted from each CaloCluster
      std::vector<std::pair<const xAOD::IParticle *, float>> clusterEnergies = thisFE->otherObjectsAndWeights();
      //get the track that this FlowElement represents
      const xAOD::TrackParticle *thisTrack = dynamic_cast<const xAOD::TrackParticle *>(thisFE->chargedObject(0));
      //Put the sum of the subtractd energy into the map between tracks and that sum.
      //Note that zero, the third argument, is the initial value of the sum
      mapTrackSubtractedEnergy[thisTrack] = std::accumulate(clusterEnergies.begin(), clusterEnergies.end(), 0, accumulateSubtractedEnergy);
    }

    // Tracks
    SG::ReadHandle<xAOD::TrackParticleContainer> trackParticleReadHandle(m_trackParticleReadHandleKey);
    if (!trackParticleReadHandle.isValid())
    {
      ATH_MSG_WARNING("Invalid ReadHandle for xAOD::TrackParticleContainer with key: " << trackParticleReadHandle.key());
      return StatusCode::SUCCESS;
    }    

    m_nTrack = 0;
    for (auto track : *trackParticleReadHandle)
    {

      if (!m_trkSelectionTool->accept(track))
        continue;

      m_trackPt.push_back(track->pt() * 1e-3);
      m_trackP.push_back(TMath::Abs(1. / track->qOverP()) * 1e-3);
      m_trackMass.push_back(track->m() * 1e-3);
      m_trackEta.push_back(track->eta());
      m_trackPhi.push_back(track->phi());
      //initialise the value of the calibration hit sums - this is filled in later on in the cluster loop      
      m_trackVisibleCalHitCaloEnergy.push_back(0.0);
      m_trackFullCalHitCaloEnergy.push_back(0.0);

      //get truth particle link for track
      const xAOD::TruthParticle *linkedTruthParticle = nullptr;
      if (track->isAvailable<ElementLink<xAOD::TruthParticleContainer>>("truthParticleLink"))
      {
        ElementLink<xAOD::TruthParticleContainer> truthLink = track->auxdata<ElementLink<xAOD::TruthParticleContainer>>("truthParticleLink");
        if (truthLink.isValid())
          linkedTruthParticle = *truthLink;
      }

      //get truth particle barcode for track
      if (linkedTruthParticle){
        int barcode = linkedTruthParticle->barcode();
        unsigned int truthParticleIndex = truthBarcodeMap[barcode];
        m_trackTruthParticleIndex.push_back(truthParticleIndex);
        truthVisibleCalHitCaloEnergyMap[barcode] = m_trackVisibleCalHitCaloEnergy[m_nTrack];
        truthFullCalHitCaloEnergyMap[barcode] = m_trackFullCalHitCaloEnergy[m_nTrack];
      }
      else m_trackTruthParticleIndex.push_back(-1);

      if (mapTrackSubtractedEnergy.find(track) != mapTrackSubtractedEnergy.end()){
        m_trackSubtractedCaloEnergy.push_back(mapTrackSubtractedEnergy[track]);	
      }
      else
        m_trackSubtractedCaloEnergy.push_back(-999.);

      if (m_doDetailedTracking){
        // Load track quality variables
        track->summaryValue(m_numberOfPixelHits, xAOD::numberOfPixelHits);
        track->summaryValue(m_numberOfSCTHits, xAOD::numberOfSCTHits);
        track->summaryValue(m_numberOfPixelDeadSensors, xAOD::numberOfPixelDeadSensors);
        track->summaryValue(m_numberOfSCTDeadSensors, xAOD::numberOfSCTDeadSensors);
        track->summaryValue(m_numberOfPixelDeadSensors, xAOD::numberOfPixelDeadSensors);
        track->summaryValue(m_numberOfSCTDeadSensors, xAOD::numberOfSCTDeadSensors);
        track->summaryValue(m_numberOfPixelHoles, xAOD::numberOfPixelHoles);
        track->summaryValue(m_numberOfSCTHoles, xAOD::numberOfSCTHoles);
        track->summaryValue(m_numberOfInnermostPixelLayerHits, xAOD::numberOfInnermostPixelLayerHits);
        track->summaryValue(m_numberOfNextToInnermostPixelLayerHits, xAOD::numberOfNextToInnermostPixelLayerHits);
        track->summaryValue(m_expectInnermostPixelLayerHit, xAOD::expectInnermostPixelLayerHit);
        track->summaryValue(m_expectNextToInnermostPixelLayerHit, xAOD::expectNextToInnermostPixelLayerHit);
        track->summaryValue(m_numberOfTRTHits, xAOD::numberOfTRTHits);
        track->summaryValue(m_numberOfTRTOutliers, xAOD::numberOfTRTOutliers);

        m_trackNumberOfPixelHits.push_back(m_numberOfPixelHits);
        m_trackNumberOfSCTHits.push_back(m_numberOfSCTHits);
        m_trackNumberOfPixelDeadSensors.push_back(m_numberOfPixelDeadSensors);
        m_trackNumberOfSCTDeadSensors.push_back(m_numberOfSCTDeadSensors);
        m_trackNumberOfPixelSharedHits.push_back(m_numberOfPixelSharedHits);
        m_trackNumberOfSCTSharedHits.push_back(m_numberOfSCTSharedHits);
        m_trackNumberOfPixelHoles.push_back(m_numberOfPixelHoles);
        m_trackNumberOfSCTHoles.push_back(m_numberOfSCTHoles);
        m_trackNumberOfInnermostPixelLayerHits.push_back(m_numberOfInnermostPixelLayerHits);
        m_trackNumberOfNextToInnermostPixelLayerHits.push_back(m_numberOfNextToInnermostPixelLayerHits);
        m_trackExpectInnermostPixelLayerHit.push_back(m_expectInnermostPixelLayerHit);
        m_trackExpectNextToInnermostPixelLayerHit.push_back(m_expectNextToInnermostPixelLayerHit);
        m_trackNumberOfTRTHits.push_back(m_numberOfTRTHits);
        m_trackNumberOfTRTOutliers.push_back(m_numberOfTRTOutliers);
        m_trackChiSquared.push_back(track->chiSquared());
        m_trackNumberDOF.push_back(track->numberDoF());
        m_trackD0.push_back(track->definingParameters()[0]);
        m_trackZ0.push_back(track->definingParameters()[1]);
      }

      // A map to store the track parameters (eta,phi) associated with the different layers of the calorimeter system
      std::map<CaloCell_ID::CaloSample, std::pair<double, double>> parametersMap;

      std::unique_ptr<Trk::CaloExtension> extension = m_theTrackExtrapolatorTool->caloExtension(Gaudi::Hive::currentContext(), *track);
      if (extension)
      {

        // Extract the CurvilinearParameters per each layer-track intersection
        const std::vector<Trk::CurvilinearParameters> &clParametersVector = extension->caloLayerIntersections();

        for (auto clParameter : clParametersVector)
        {

          unsigned int parametersIdentifier = clParameter.cIdentifier();
          CaloCell_ID::CaloSample intLayer;

          if (!m_trackParametersIdHelper->isValid(parametersIdentifier))
          {
            std::cout << "Invalid Track Identifier" << std::endl;
            intLayer = CaloCell_ID::CaloSample::Unknown;
          }
          else
          {
            intLayer = m_trackParametersIdHelper->caloSample(parametersIdentifier);
          }
         
          if (m_trackParametersIdHelper->isEntryToVolume(clParameter.cIdentifier())){
            parametersMap[intLayer] = std::make_pair<double, double>(clParameter.position().eta(), clParameter.position().phi());
          }
        }
      }
      else
      {
        ATH_MSG_WARNING("TrackExtension failed for track with pt and eta " << track->pt() << " and " << track->eta());
      }
      
      //  ---------Calo Sample layer Variables---------
      //  PreSamplerB=0, EMB1, EMB2, EMB3, // LAr barrel
      //  PreSamplerE, EME1, EME2, EME3,   // LAr EM endcap
      //  HEC0, HEC1, HEC2, HEC3,          // Hadronic end cap cal.
      //  TileBar0, TileBar1, TileBar2,    // Tile barrel
      //  TileGap1, TileGap2, TileGap3,    // Tile gap (ITC & scint)
      //  TileExt0, TileExt1, TileExt2,    // Tile extended barrel
      //  FCAL0, FCAL1, FCAL2,             // Forward EM endcap (excluded)
      //  Unknown

      auto getTrackEtaPhi = [](std::map<CaloCell_ID::CaloSample, std::pair<double, double>> &parametersMap, CaloCell_ID::CaloSample layer, std::vector<float> &trackEta, std::vector<float> &trackPhi) {
        if (parametersMap.find(layer) != parametersMap.end())
        {
          trackEta.push_back(parametersMap[layer].first);
          trackPhi.push_back(parametersMap[layer].second);
        }
        else {
          float defaultValue = -999999999;
          trackEta.push_back(defaultValue);
          trackPhi.push_back(defaultValue);
        }
      };

      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::PreSamplerB, m_trackEta_PreSamplerB, m_trackPhi_PreSamplerB);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::PreSamplerE, m_trackEta_PreSamplerE, m_trackPhi_PreSamplerE);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::EMB1, m_trackEta_EMB1, m_trackPhi_EMB1);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::EMB2, m_trackEta_EMB2, m_trackPhi_EMB2);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::EMB3, m_trackEta_EMB3, m_trackPhi_EMB3);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::EME1, m_trackEta_EME1, m_trackPhi_EME1);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::EME2, m_trackEta_EME2, m_trackPhi_EME2);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::EME3, m_trackEta_EME3, m_trackPhi_EME3);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::HEC0, m_trackEta_HEC0, m_trackPhi_HEC0);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::HEC1, m_trackEta_HEC1, m_trackPhi_HEC1);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::HEC2, m_trackEta_HEC2, m_trackPhi_HEC2);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::HEC3, m_trackEta_HEC3, m_trackPhi_HEC3);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileBar0, m_trackEta_TileBar0, m_trackPhi_TileBar0);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileBar1, m_trackEta_TileBar1, m_trackPhi_TileBar1);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileBar2, m_trackEta_TileBar2, m_trackPhi_TileBar2);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileGap1, m_trackEta_TileGap1, m_trackPhi_TileGap1);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileGap2, m_trackEta_TileGap2, m_trackPhi_TileGap2);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileGap3, m_trackEta_TileGap3, m_trackPhi_TileGap3);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileExt0, m_trackEta_TileExt0, m_trackPhi_TileExt0);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileExt1, m_trackEta_TileExt1, m_trackPhi_TileExt1);
      getTrackEtaPhi(parametersMap, CaloCell_ID::CaloSample::TileExt2, m_trackEta_TileExt2, m_trackPhi_TileExt2);
      
      m_nTrack++;
    }
  }
  if (m_doJets)
  {
    unsigned int iColl = 0;
    for (auto jetKey : m_jetReadHandleKeyArray)
    {
      std::string jetCollName = jetKey.key();
      bool isTruthJetColl = (jetCollName.find("Truth") != std::string::npos);
      SG::ReadHandle<xAOD::JetContainer> jetReadHandle(jetKey);
      if (!jetReadHandle.isValid())
      {
        ATH_MSG_WARNING("Invalid ReadHandle for xAOD::JetContainer with key: " << jetReadHandle.key());
        return StatusCode::SUCCESS;
      }

      std::vector<float> &v_pt = m_jet_pt.at(iColl);
      std::vector<float> &v_eta = m_jet_eta.at(iColl);
      std::vector<float> &v_phi = m_jet_phi.at(iColl);
      std::vector<float> &v_E = m_jet_E.at(iColl);
      std::vector<int> &v_flavor = m_jet_flavor.at(iColl);
      v_pt.clear();
      v_eta.clear();
      v_phi.clear();
      v_E.clear();
      v_pt.reserve(jetReadHandle->size());
      v_eta.reserve(jetReadHandle->size());
      v_phi.reserve(jetReadHandle->size());
      v_E.reserve(jetReadHandle->size());

      if (isTruthJetColl)
      {
        v_flavor.clear();
        v_flavor.reserve(jetReadHandle->size());
      }
      for (auto jet : *jetReadHandle)
      {
        xAOD::JetFourMom_t jet_p4;
        if (isTruthJetColl)
        {
          jet_p4 = jet->jetP4();
          int jetFlavor = -1;
          bool status = jet->getAttribute<int>("PartonTruthLabelID", jetFlavor);
          if (status)
            jet->getAttribute("PartonTruthLabelID", jetFlavor);
          v_flavor.push_back(jetFlavor);
        }
        else
          jet_p4 = jet->jetP4(xAOD::JetConstitScaleMomentum);
        v_pt.push_back(jet_p4.Pt() * 1e-3);
        v_eta.push_back(jet_p4.Eta());
        v_phi.push_back(jet_p4.Phi());
        v_E.push_back(jet_p4.E() * 1e-3);
      }
      iColl++;
    }
  }
  // Calo clusters
  if (m_doClusters)
  {

    SG::ReadHandle<xAOD::CaloClusterContainer> caloClusterReadHandle(m_caloClusterReadHandleKey);

    if (!caloClusterReadHandle.isValid())
    {
      ATH_MSG_WARNING("Invalid ReadHandle for xAOD::CaloClusterContainer with key: " << caloClusterReadHandle.key());
      return StatusCode::SUCCESS;
    }

    //first sort the clusters by EM scale energy
    //fill a (multi)map with key = energy and value = index of cluster in list
    std::multimap<float, unsigned int, std::greater<float>> clusterRanks;
    unsigned int iCluster = 0;
    for (auto calibratedCluster : *caloClusterReadHandle)
    {
      iCluster++;
      auto cluster = calibratedCluster;
      if (m_doUncalibratedClusters)
      {
        auto sisterCluster = calibratedCluster->getSisterCluster();
        if (sisterCluster)
          cluster = sisterCluster;
        else
        {
          ATH_MSG_ERROR("Sister cluster returns nullptr");
          return StatusCode::FAILURE;
        }
      }
      float clusterE = cluster->e() * 1e-3;
      float clusterEta = cluster->eta();
      if (clusterE < m_clusterE_min ||
          clusterE > m_clusterE_max ||
          std::abs(clusterEta) > m_clusterEtaAbs_max)
        continue;

      clusterRanks.emplace_hint(clusterRanks.end(), clusterE, iCluster - 1);
    }
    m_nCluster = clusterRanks.size();
    //
    m_cluster_nCells.clear();
    m_cluster_E.clear();
    m_cluster_E_LCCalib.clear();
    m_cluster_Pt.clear();
    m_cluster_Eta.clear();
    m_cluster_Phi.clear();
    m_cluster_ENG_CALIB_TOT.clear();
    m_cluster_ENG_CALIB_OUT_T.clear();
    m_cluster_ENG_CALIB_DEAD_TOT.clear();
    m_cluster_EM_PROBABILITY.clear();
    m_cluster_HAD_WEIGHT.clear();
    m_cluster_OOC_WEIGHT.clear();
    m_cluster_DM_WEIGHT.clear();
    m_cluster_CENTER_MAG.clear();
    m_cluster_FIRST_ENG_DENS.clear();
    m_cluster_CENTER_LAMBDA.clear();
    m_cluster_ISOLATION.clear();
    m_cluster_ENERGY_DigiHSTruth.clear();

    m_cluster_nCells.reserve(m_nCluster);
    m_cluster_E.reserve(m_nCluster);
    m_cluster_E_LCCalib.reserve(m_nCluster);
    m_cluster_Pt.reserve(m_nCluster);
    m_cluster_Eta.reserve(m_nCluster);
    m_cluster_Phi.reserve(m_nCluster);
    m_cluster_ENG_CALIB_TOT.reserve(m_nCluster);
    m_cluster_ENG_CALIB_OUT_T.reserve(m_nCluster);
    m_cluster_ENG_CALIB_DEAD_TOT.reserve(m_nCluster);
    m_cluster_EM_PROBABILITY.reserve(m_nCluster);
    m_cluster_HAD_WEIGHT.reserve(m_nCluster);
    m_cluster_OOC_WEIGHT.reserve(m_nCluster);
    m_cluster_DM_WEIGHT.reserve(m_nCluster);
    m_cluster_CENTER_MAG.reserve(m_nCluster);
    m_cluster_FIRST_ENG_DENS.reserve(m_nCluster);
    m_cluster_CENTER_LAMBDA.reserve(m_nCluster);
    m_cluster_ISOLATION.reserve(m_nCluster);
    m_cluster_ENERGY_DigiHSTruth.reserve(m_nCluster);

    if (m_doClusterCells)
    {
      m_cluster_cell_ID.clear();
      m_cluster_cell_E.clear();
      m_cluster_cell_hitsE_EM.clear();

      m_cluster_cell_ID.assign(m_nCluster, std::vector<size_t>());
      m_cluster_cell_E.assign(m_nCluster, std::vector<float>());
      m_cluster_cell_hitsE_EM.assign(m_nCluster, std::vector<float>());
      if (m_doCalibHits)
      {
        if (m_doCalibHitsPerCell)
        {
          m_cluster_cell_hitsE_nonEM.clear();
          m_cluster_cell_hitsE_Invisible.clear();
          m_cluster_cell_hitsE_Escaped.clear();

          m_cluster_cell_hitsE_nonEM.assign(m_nCluster, std::vector<float>());
          m_cluster_cell_hitsE_Invisible.assign(m_nCluster, std::vector<float>());
          m_cluster_cell_hitsE_Escaped.assign(m_nCluster, std::vector<float>());
        }
        if (m_doTruthParticles)
        {
          m_cluster_fullHitsTruthIndex.clear();
          m_cluster_fullHitsTruthE.clear();

          m_cluster_fullHitsTruthIndex.assign(m_nCluster, std::vector<int>());
          m_cluster_fullHitsTruthE.assign(m_nCluster, std::vector<float>());

          m_cluster_visibleHitsTruthIndex.clear();
          m_cluster_visibleHitsTruthE.clear();

          m_cluster_visibleHitsTruthIndex.assign(m_nCluster, std::vector<int>());
          m_cluster_visibleHitsTruthE.assign(m_nCluster, std::vector<float>());


        }
        if (m_doTruthParticlesPerCell)
        {
          m_cluster_cell_hitsTruthIndex.clear();
          m_cluster_cell_hitsTruthE.clear();

          m_cluster_cell_hitsTruthIndex.assign(m_nCluster, std::vector<std::vector<int>>());
          m_cluster_cell_hitsTruthE.assign(m_nCluster, std::vector<std::vector<float>>());
        }
      }
    }
    //loop over clusters in order of their energies
    //clusters failing E or eta cut are not included in loop
    unsigned int jCluster = 0;
    for (auto mpair : clusterRanks)
    {
      auto calibratedCluster = caloClusterReadHandle->at(mpair.second);
      auto cluster = calibratedCluster;

      if (m_doUncalibratedClusters)
        cluster = calibratedCluster->getSisterCluster();

      m_cluster_nCells.push_back(cluster->size());
      m_cluster_E.push_back(cluster->e() * 1e-3);
      m_cluster_E_LCCalib.push_back(calibratedCluster->e() * 1e-3);
      m_cluster_Pt.push_back(cluster->pt() * 1e-3);
      m_cluster_Eta.push_back(cluster->eta());
      m_cluster_Phi.push_back(cluster->phi());

      if (m_doClusterMoments)
      {

        auto getMoment = [](const xAOD::CaloCluster& theCluster, const xAOD::CaloCluster::MomentType& momentType, std::vector<float>& momentVector, const double& defaultValue, bool scale){
          double moment = defaultValue;
          if (!theCluster.retrieveMoment(momentType, moment)) momentVector.push_back(moment);
          else {
            if (scale) momentVector.push_back(moment*1e-3);
            else momentVector.push_back(moment);
          }
        };

        getMoment(*cluster, xAOD::CaloCluster::ENG_CALIB_TOT, m_cluster_ENG_CALIB_TOT,-1,true);
        getMoment(*cluster, xAOD::CaloCluster::ENG_CALIB_OUT_T, m_cluster_ENG_CALIB_OUT_T,-1,true);
        getMoment(*cluster, xAOD::CaloCluster::ENG_CALIB_DEAD_TOT, m_cluster_ENG_CALIB_DEAD_TOT,-1,true);
        getMoment(*cluster, xAOD::CaloCluster::CENTER_MAG, m_cluster_CENTER_MAG,-1,false);
        getMoment(*cluster, xAOD::CaloCluster::FIRST_ENG_DENS, m_cluster_FIRST_ENG_DENS,-1,true);
        getMoment(*cluster, xAOD::CaloCluster::CENTER_LAMBDA, m_cluster_CENTER_LAMBDA,-1,false);
        getMoment(*cluster, xAOD::CaloCluster::ISOLATION, m_cluster_ISOLATION,-1,false);
        //for moments related to the calibration, use calibratedCluster or they will be undefined
        getMoment(*calibratedCluster, xAOD::CaloCluster::EM_PROBABILITY, m_cluster_EM_PROBABILITY,-1,false);
        getMoment(*calibratedCluster, xAOD::CaloCluster::HAD_WEIGHT, m_cluster_HAD_WEIGHT,-1,false);
        getMoment(*calibratedCluster, xAOD::CaloCluster::OOC_WEIGHT, m_cluster_OOC_WEIGHT,-1,false);
        getMoment(*calibratedCluster, xAOD::CaloCluster::DM_WEIGHT, m_cluster_DM_WEIGHT,-1,false);
        getMoment(*calibratedCluster, xAOD::CaloCluster::ENERGY_DigiHSTruth, m_cluster_ENERGY_DigiHSTruth,-999,false);

      }
      if (m_doClusterCells)
      {

        std::vector<size_t> &cluster_cell_ID = m_cluster_cell_ID[jCluster];
        std::vector<float> &cluster_cell_E = m_cluster_cell_E[jCluster];
        std::vector<float> &cluster_cell_hitsE_EM = m_cluster_cell_hitsE_EM[jCluster];
        std::vector<float> &cluster_cell_hitsE_nonEM = m_cluster_cell_hitsE_nonEM[jCluster];
        std::vector<float> &cluster_cell_hitsE_Invisible = m_cluster_cell_hitsE_Invisible[jCluster];
        std::vector<float> &cluster_cell_hitsE_Escaped = m_cluster_cell_hitsE_Escaped[jCluster];
        std::vector<int> &cluster_fullHitsTruthIndex = m_cluster_fullHitsTruthIndex[jCluster];
        std::vector<float> &cluster_fullHitsTruthE = m_cluster_fullHitsTruthE[jCluster];
        std::vector<int> &cluster_visibleHitsTruthIndex = m_cluster_visibleHitsTruthIndex[jCluster];
        std::vector<float> &cluster_visibleHitsTruthE = m_cluster_visibleHitsTruthE[jCluster];

        auto nCells_cl = cluster->size();
        cluster_cell_ID.reserve(nCells_cl);
        cluster_cell_E.reserve(nCells_cl);

        std::vector<std::vector<int>>* cluster_cell_hitsTruthIndex = nullptr;
        if (m_doTruthParticlesPerCell) {
          cluster_cell_hitsTruthIndex = &m_cluster_cell_hitsTruthIndex[jCluster];
          cluster_cell_hitsTruthIndex->assign(nCells_cl, std::vector<int>());
        }

        std::vector<std::vector<float>>* cluster_cell_hitsTruthE = nullptr;
        if (m_doTruthParticlesPerCell){
          cluster_cell_hitsTruthE = &m_cluster_cell_hitsTruthE[jCluster];
          cluster_cell_hitsTruthE->assign(nCells_cl, std::vector<float>());          
        }

        if (m_doCalibHits && m_doCalibHitsPerCell)
        {
          cluster_cell_hitsE_EM.reserve(nCells_cl);
          cluster_cell_hitsE_nonEM.reserve(nCells_cl);
          cluster_cell_hitsE_Invisible.reserve(nCells_cl);
          cluster_cell_hitsE_Escaped.reserve(nCells_cl);
        }

        unsigned int jCell = 0;
        for (CaloClusterCellLink::const_iterator it_cell = cluster->cell_begin(); it_cell != cluster->cell_end(); it_cell++)
        {
          // store truth particle index, energy depostit per cell
          // ultimately these end up in the vectors, but first we have to calculate a map
          // map will be indexed by particle index, loooked up via barcode
          // map value will be total hits energy
          std::map<unsigned int, float > truthIndexEnergyMapPerCell;
          std::vector<int>* hitsTruthIndex;
          std::vector<float>* hitsTruthE;
          if (m_doTruthParticlesPerCell) {
            hitsTruthIndex = &cluster_cell_hitsTruthIndex->at(jCell);
            hitsTruthE = &cluster_cell_hitsTruthE->at(jCell);
          }

          const CaloCell *cell = (*it_cell);
          float cellE = cell->e() * (it_cell.weight()) * 1e-3;
          if (cellE < m_cellE_thres)
            continue;

          cluster_cell_ID.push_back(cell->ID().get_identifier32().get_compact());
          cluster_cell_E.push_back(cellE);
          if (m_doCalibHits)
          {
            float energy_EM = 0.;
            float energy_nonEM = 0.;
            float energy_Invisible = 0.;
            float energy_Escaped = 0.;

            for (auto hit_container : v_calibHitContainer)
            {
              for (auto ch : *hit_container)
              {
                if (ch->cellID() != cell->ID())
                  continue;
                energy_EM += ch->energyEM();
                energy_nonEM += ch->energyNonEM();
                energy_Invisible += ch->energyInvisible();
                energy_Escaped += ch->energyEscaped();

                if (m_doTruthParticlesPerCell)
                {
                  // add to the map that is indexed by the particle index, and contains the vector of particle energies
                  unsigned int barcode = ch->particleID();
                  const auto mapItr=truthBarcodeMap.find(barcode);
                  // safety check that the truth particle exists 
                  if(mapItr!=truthBarcodeMap.end()) 
                  {
                    // check that there is an entry for this truth particle
                    const auto energyMapItr = truthIndexEnergyMapPerCell.find(mapItr->second);
                    if (energyMapItr==truthIndexEnergyMapPerCell.end())
                    {
                      truthIndexEnergyMapPerCell.insert( std::pair<int, float>(mapItr->second, ch->energyTotal()) );        
                    }
                    else // if the entry exists, then just add the energy
                    {
                      energyMapItr->second += ch->energyTotal();
                    }
                  }
                }

              }
            } //end loop on calib hits containers
            if (m_doCalibHitsPerCell)
            {
              cluster_cell_hitsE_EM.push_back(energy_EM * 1e-3);
              cluster_cell_hitsE_nonEM.push_back(energy_nonEM * 1e-3);
              cluster_cell_hitsE_Invisible.push_back(energy_Invisible * 1e-3);
              cluster_cell_hitsE_Escaped.push_back(energy_Escaped * 1e-3);
            }
            if (m_doTruthParticlesPerCell)
            { 
              // now we have to construct the vectors from the map
              std::vector<std::pair<int, float>> sortedPairs;
              for (auto& it : truthIndexEnergyMapPerCell)
              {
                sortedPairs.push_back(it);
              }

              // now we sort
              std::sort(sortedPairs.begin(), sortedPairs.end(), cmp);

              // now we copy to actual vectors, up to the limit
              for(unsigned int i = 0; i < sortedPairs.size() && i < m_truthParticlesPerLimitLimit; i++)
              {
                hitsTruthIndex->push_back(sortedPairs[i].first);
                hitsTruthE->push_back(sortedPairs[i].second);
              }

            }
          } //end m_doCalibHits
          jCell++;
        }   //end cell loop
        if (m_doCalibHits)
        {

          //now get visible energy contributions via calo cluster decorations
          //these are calculated via https://acode-browser1.usatlas.bnl.gov/lxr/source/athena/Calorimeter/CaloCalibHitRec/src/CaloCalibClusterDecoratorAlgorithm.cxx
          //We have run one calculation using the full calibration hit energy and one accounting only for visible calibration hit energy          

          const SG::AuxElement::Accessor<std::vector<std::pair<unsigned int, double> > > clusterTruthDecorationsAccessor("calclus_NLeadingTruthParticleBarcodeEnergyPairs_Visible");
          std::vector<std::pair<unsigned int, double > > clusterTruthDecorations = clusterTruthDecorationsAccessor(*cluster);

          for (auto &thePair : clusterTruthDecorations)
          {
            unsigned int barcode = thePair.first;
            const auto mapItr = truthBarcodeMap.find(barcode);
            if (mapItr != truthBarcodeMap.end())
            {
              if (m_doTruthParticles) cluster_visibleHitsTruthIndex.push_back(mapItr->second);
              float calibrationHitEnergy = thePair.second * 1e-3;
              if (m_doTruthParticles) cluster_visibleHitsTruthE.push_back(calibrationHitEnergy);
              if (m_doTracking) truthVisibleCalHitCaloEnergyMap[barcode] += calibrationHitEnergy;
            }//if have valid truth particle entry in map
          }//end loop on clusterTruthDecorations

          const SG::AuxElement::Accessor<std::vector<std::pair<unsigned int, double> > > clusterTruthDecorationsAccessor_Full("calclus_NLeadingTruthParticleBarcodeEnergyPairs_Full");
          std::vector<std::pair<unsigned int, double > > clusterTruthDecorations_Full = clusterTruthDecorationsAccessor_Full(*cluster);

          for (auto &thePair : clusterTruthDecorations_Full)
          {
            unsigned int barcode = thePair.first;
            const auto mapItr = truthBarcodeMap.find(barcode);
            if (mapItr != truthBarcodeMap.end())
            {
              if (m_doTruthParticles) cluster_fullHitsTruthIndex.push_back(mapItr->second);
              float calibrationHitEnergy = thePair.second * 1e-3;
              if (m_doTruthParticles) cluster_fullHitsTruthE.push_back(calibrationHitEnergy);
              if (m_doTracking) truthFullCalHitCaloEnergyMap[barcode] += calibrationHitEnergy;
            }//if have valid truth particle entry in map
          }//end loop on clusterTruthDecorations_Full

        } //end m_doTruthParticles

      } //end m_doClusterCells
      jCluster++;
    } //end cluster loop
  }//end m_doCaloClusters

  if (m_doTracking){
    //loop over track truth match indices and fill cal hit variables, if a match                                                                                                                                                                                                
    unsigned int trackIndex = 0;
    for (auto thisIndex : m_trackTruthParticleIndex){
      if (thisIndex < 0) {
	trackIndex++;
        continue;
      }
      m_trackVisibleCalHitCaloEnergy[trackIndex] = truthVisibleCalHitCaloEnergyMap[m_truthPartBarcode[thisIndex]];
      m_trackFullCalHitCaloEnergy[trackIndex] = truthFullCalHitCaloEnergyMap[m_truthPartBarcode[thisIndex]];
      trackIndex++;
    }
  }

  //Cells
  if (m_doAllCells){

    //clear all the vectors
    m_cell_E.clear();
    m_cell_ID.clear();
    m_cell_Time.clear();
    m_cell_Quality.clear();

    SG::ReadHandle<CaloCellContainer> cellContainerHandle(m_caloCellReadHandleKey);
    if (!cellContainerHandle.isValid())
    {
      ATH_MSG_ERROR("Could not retrieve cell container " << m_caloCellReadHandleKey.key());
      return StatusCode::FAILURE;
    }

    //fill cell branches
    for (auto thisCaloCell : *cellContainerHandle){
      m_cell_E.push_back(thisCaloCell->e()*1e-3);
      m_cell_ID.push_back(thisCaloCell->ID().get_identifier32().get_compact());
      m_cell_Time.push_back(thisCaloCell->time());
      m_cell_Quality.push_back(thisCaloCell->quality());
    }//end loop on cells

  }

  m_eventTree->Fill();
  return StatusCode::SUCCESS;
}

StatusCode MLTreeMaker::finalize()
{
  ATH_MSG_INFO("Finalizing " << name() << "...");

  return StatusCode::SUCCESS;
}
