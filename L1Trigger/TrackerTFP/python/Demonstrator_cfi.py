# Modify
import FWCore.ParameterSet.Config as cms

TrackerTFPDemonstrator_params = cms.PSet(

  LabelInput   = cms.string( "TrackerTFPProducerGP"            ),
  LabelOutput  = cms.string( "TrackerTFPProducerLF"            ),
  BranchStubs  = cms.string( "StubAccepted"                    ),
  BranchTracks = cms.string( "TrackAccepted"                   ),
  DirIPBB      = cms.string( "/afs/cern.ch/user/m/mghorban/workspace/cmssw/CMSSW_11_1_0_pre8/src/lf" ),
  RunTime      = cms.double( 2.0 )
)
