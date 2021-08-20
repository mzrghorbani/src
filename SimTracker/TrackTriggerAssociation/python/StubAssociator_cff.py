import FWCore.ParameterSet.Config as cms

from L1Trigger.TrackerDTC.ProducerES_cff import TrackTriggerSetup
from SimTracker.TrackTriggerAssociation.StubAssociator_cfi import StubAssociator_params

StubAssociator = cms.EDProducer('tt::StubAssociator', StubAssociator_params)