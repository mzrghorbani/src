#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Run.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/EDGetToken.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "DataFormats/Common/interface/Handle.h"

#include "SimTracker/TrackTriggerAssociation/interface/StubAssociation.h"
#include "L1Trigger/TrackerDTC/interface/Setup.h"

#include <TProfile.h>
#include <TH1F.h>

#include <vector>
#include <set>
#include <numeric>
#include <sstream>

using namespace std;
using namespace edm;
using namespace trackerDTC;
using namespace tt;

namespace trackerTFP {

    /*! \class  trackerTFP::AnalyzerGP
     *  \brief  Class to analyze hardware like structured TTStub Collection generated by Geometric Processor
     *  \author Thomas Schuh
     *  \date   2020, Apr
     */
    class AnalyzerGP : public one::EDAnalyzer<one::WatchRuns, one::SharedResources> {
    public:
        AnalyzerGP(const ParameterSet& iConfig);
        void beginJob() override {}
        void beginRun(const Run& iEvent, const EventSetup& iSetup) override;
        void analyze(const Event& iEvent, const EventSetup& iSetup) override;
        void endRun(const Run& iEvent, const EventSetup& iSetup) override {}
        void endJob() override;

    private:
        // ED input token of stubs
        EDGetTokenT<TTDTC::Streams> edGetTokenAccepted_;
        // ED input token of lost stubs
        EDGetTokenT<TTDTC::Streams> edGetTokenLost_;
        // ED input token of TTStubRef to selected TPPtr association
        EDGetTokenT<StubAssociation> edGetTokenAss_;
        // Setup token
        ESGetToken<Setup, SetupRcd> esGetToken_;
        // stores, calculates and provides run-time constants
        const Setup* setup_;
        // enables analyze of TPs
        bool useMCTruth_;
        //
        int nEvents_;

        // Histograms

        TProfile* prof_;
        TProfile* profChannel_;
        TH1F* hisChannel_;

        // printout
        stringstream log_;
    };

    AnalyzerGP::AnalyzerGP(const ParameterSet& iConfig) : useMCTruth_(iConfig.getParameter<bool>("UseMCTruth")), nEvents_(0) {
        usesResource("TFileService");
        // book in- and output ED products
        const string& label = iConfig.getParameter<string>("LabelGP");
        const string& branchAccepted = iConfig.getParameter<string>("BranchAccepted");
        const string& branchLost = iConfig.getParameter<string>("BranchLost");
        edGetTokenAccepted_ = consumes<TTDTC::Streams>(InputTag(label, branchAccepted));
        edGetTokenLost_ = consumes<TTDTC::Streams>(InputTag(label, branchLost));
        if (useMCTruth_) {
            const auto& inputTagAss = iConfig.getParameter<InputTag>("InputTagSelection");
            edGetTokenAss_ = consumes<StubAssociation>(inputTagAss);
        }
        // book ES product
        esGetToken_ = esConsumes<Setup, SetupRcd, Transition::BeginRun>();
        setup_ = nullptr;
        // log config
        log_.setf(ios::fixed, ios::floatfield);
        log_.precision(4);
    }

    void AnalyzerGP::beginRun(const Run& iEvent, const EventSetup& iSetup) {
        // helper class to store configurations
        setup_ = &iSetup.getData(esGetToken_);
        // book histograms
        Service<TFileService> fs;
        TFileDirectory dir;
        dir = fs->mkdir("GP");
        prof_ = dir.make<TProfile>("Counts", ";", 4, 0.5, 4.5);
        prof_->GetXaxis()->SetBinLabel(1, "Stubs");
        prof_->GetXaxis()->SetBinLabel(2, "Lost Stubs");
        prof_->GetXaxis()->SetBinLabel(3, "Found TPs");
        prof_->GetXaxis()->SetBinLabel(4, "Selected TPs");
        // channel occupancy
        constexpr int maxOcc = 180;
        const int numChannels = setup_->numSectors();
        hisChannel_ = dir.make<TH1F>("His Channel Occupancy", ";", maxOcc, -.5, maxOcc - .5);
        profChannel_ = dir.make<TProfile>("Prof Channel Occupancy", ";", numChannels, -.5, numChannels - .5);
    }

    void AnalyzerGP::analyze(const Event& iEvent, const EventSetup& iSetup) {
        // read in gp products
        Handle<TTDTC::Streams> handleAccepted;
        iEvent.getByToken<TTDTC::Streams>(edGetTokenAccepted_, handleAccepted);
        Handle<TTDTC::Streams> handleLost;
        iEvent.getByToken<TTDTC::Streams>(edGetTokenLost_, handleLost);
        // read in MCTruth
        const StubAssociation* stubAssociation = nullptr;
        if (useMCTruth_) {
            Handle<StubAssociation> handleAss;
            iEvent.getByToken<StubAssociation>(edGetTokenAss_, handleAss);
            stubAssociation = handleAss.product();
            prof_->Fill(4, stubAssociation->numTPs());
        }
        // analyze gp products and find still reconstrucable TrackingParticles
        set<TPPtr> setTPPtr;
        for (int region = 0; region < setup_->numRegions(); region++) {
            int nStubs(0);
            int nLost(0);
            map<TPPtr, vector<TTStubRef>> mapTPsTTStubs;
            for (int channel = 0; channel < setup_->numSectors(); channel++) {
                const int index = region * setup_->numSectors() + channel;
                const TTDTC::Stream& accepted = handleAccepted->at(index);
                hisChannel_->Fill(accepted.size());
                profChannel_->Fill(channel, accepted.size());
                for (const TTDTC::Frame& frame : accepted) {
                    if (frame.first.isNull())
                        continue;
                    nStubs++;
                    if (!useMCTruth_)
                        continue;
                    const vector<TPPtr>& tpPtrs = stubAssociation->findTrackingParticlePtrs(frame.first);
                    for (const TPPtr& tpPtr : tpPtrs) {
                        auto it = mapTPsTTStubs.find(tpPtr);
                        if (it == mapTPsTTStubs.end()) {
                            it = mapTPsTTStubs.emplace(tpPtr, vector<TTStubRef>()).first;
                            it->second.reserve(stubAssociation->findTTStubRefs(tpPtr).size());
                        }
                        it->second.push_back(frame.first);
                    }
                }
                nLost += handleLost->at(index).size();
            }
            for (const auto& p : mapTPsTTStubs)
                if (setup_->reconstructable(p.second))
                    setTPPtr.insert(p.first);

            prof_->Fill(1, nStubs);
            prof_->Fill(2, nLost);
        }
        prof_->Fill(3, setTPPtr.size());
        nEvents_++;
    }

    void AnalyzerGP::endJob() {
        // printout GP summary
        const double numStubs = prof_->GetBinContent(1);
        const double numStubsLost = prof_->GetBinContent(2);
        const double errStubs = prof_->GetBinError(1);
        const double errStubsLost = prof_->GetBinError(2);
        const double numTPs = prof_->GetBinContent(3);
        const double totalTPs = prof_->GetBinContent(4);
        const double eff = numTPs / totalTPs;
        const double errEff = sqrt(eff * (1. - eff) / totalTPs / nEvents_);
        const vector<double> nums = {numStubs, numStubsLost};
        const vector<double> errs = {errStubs, errStubsLost};
        const int wNums = ceil(log10(*max_element(nums.begin(), nums.end()))) + 5;
        const int wErrs = ceil(log10(*max_element(errs.begin(), errs.end()))) + 5;
        log_ << "                         GP  SUMMARY                         " << endl;
        log_ << "number of stubs      per TFP = " << setw(wNums) << numStubs << " +- " << setw(wErrs) << errStubs << endl;
        log_ << "number of lost stubs per TFP = " << setw(wNums) << numStubsLost << " +- " << setw(wErrs) << errStubsLost << endl;
        log_ << "     max tracking efficiency = " << setw(wNums) << eff << " +- " << setw(wErrs) << errEff << endl;
        log_ << "=============================================================";
        LogPrint("L1Trigger/TrackerTFP") << log_.str();
    }

}  // namespace trackerTFP

DEFINE_FWK_MODULE(trackerTFP::AnalyzerGP);
