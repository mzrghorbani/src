# Modify
import FWCore.ParameterSet.Config as cms

from L1Trigger.TrackerDTC.ProducerES_cff import TrackTriggerSetup
from L1Trigger.TrackerTFP.Producer_cfi import TrackerTFPProducer_params
from L1Trigger.TrackerTFP.ProducerES_cff import TrackTriggerDataFormats

TrackerTFPProducerGP = cms.EDProducer( 'trackerTFP::ProducerGP', TrackerTFPProducer_params )
TrackerTFPProducerLF = cms.EDProducer( 'trackerTFP::ProducerLF', TrackerTFPProducer_params )
