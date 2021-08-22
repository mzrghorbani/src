import FWCore.ParameterSet.Config as cms

TrackerTFPDemonstrator_params = cms.PSet(

    LabelInput=cms.string("TrackerTFPProducerMHT"),
    # LabelOutput  = cms.string( "TrackerTFPProducerLR"),
    LabelOutput=cms.string("TrackerTFPProducerLRHLS"),
    BranchStubs=cms.string("StubAccepted"),
    BranchTracks=cms.string("TrackAccepted"),
    # DirIPBB = cms.string( /heplnw039/tschuh/work/proj/lr/"),
    DirIPBB=cms.string("/home/mghorbani/workspace/cpp/cmssw"),
    RunTime=cms.double(2.0)
)
