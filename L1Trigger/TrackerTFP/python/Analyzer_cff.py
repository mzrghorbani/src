# Modify
import FWCore.ParameterSet.Config as cms

from L1Trigger.TrackerTFP.Analyzer_cfi import TrackerTFPAnalyzer_params
from L1Trigger.TrackerTFP.Producer_cfi import TrackerTFPProducer_params

TrackerTFPAnalyzerGP = cms.EDAnalyzer( 'trackerTFP::AnalyzerGP', TrackerTFPAnalyzer_params, TrackerTFPProducer_params )
TrackerTFPAnalyzerLF = cms.EDAnalyzer( 'trackerTFP::AnalyzerLF', TrackerTFPAnalyzer_params, TrackerTFPProducer_params )
