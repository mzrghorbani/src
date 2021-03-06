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
#include "L1Trigger/TrackerTFP/interface/DataFormats.h"

#include <TProfile.h>
#include <TH1F.h>

#include <vector>
#include <deque>
#include <set>
#include <cmath>
#include <numeric>
#include <sstream>

using namespace std;
using namespace edm;
using namespace trackerDTC;
using namespace tt;

namespace trackerTFP {

  /*! \class  trackerTFP::ProducerLF
   *  \brief  L1TrackTrigger Linear Fitter emulator
   *  \author Thomas Schuh, development Maziar Ghorbani
   *  \date   2020, August
   */

  class AnalyzerLF : public one::EDAnalyzer<one::WatchRuns, one::SharedResources> {
  public:
    AnalyzerLF(const ParameterSet& iConfig);
    void beginJob() override {}
    void beginRun(const Run& iEvent, const EventSetup& iSetup) override;
    void analyze(const Event& iEvent, const EventSetup& iSetup) override;
    void endRun(const Run& iEvent, const EventSetup& iSetup) override {}
    void endJob() override;

  private:
    //
    void formTracks(const TTDTC::Stream& stream, vector<vector<TTStubRef>>& tracks, int qOverPt) const;
    //
    void associate(const vector<vector<TTStubRef>>& tracks, const StubAssociation* ass, set<TPPtr>& tps, int& sum) const;

    // ED input token of stubs
    EDGetTokenT<TTDTC::Streams> edGetTokenAccepted_;
    // ED input token of lost stubs
    EDGetTokenT<TTDTC::Streams> edGetTokenLost_;
    // ED input token of TTStubRef to selected TPPtr association
    EDGetTokenT<StubAssociation> edGetTokenSelection_;
    // ED input token of TTStubRef to recontructable TPPtr association
    EDGetTokenT<StubAssociation> edGetTokenReconstructable_;
    // Setup token
    ESGetToken<Setup, SetupRcd> esGetTokenSetup_;
    // DataFormats token
    ESGetToken<DataFormats, DataFormatsRcd> esGetTokenDataFormats_;
    // stores, calculates and provides run-time constants
    const Setup* setup_;
    // helper class to extract structured data from TTDTC::Frames
    const DataFormats* dataFormats_;
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

  AnalyzerLF::AnalyzerLF(const ParameterSet& iConfig) : useMCTruth_(iConfig.getParameter<bool>("UseMCTruth")), nEvents_(0) {
    usesResource("TFileService");
    // book in- and output ED products
    const string& label = iConfig.getParameter<string>("LabelLF");
    const string& branchAccepted = iConfig.getParameter<string>("BranchAccepted");
    const string& branchLost = iConfig.getParameter<string>("BranchLost");
    edGetTokenAccepted_ = consumes<TTDTC::Streams>(InputTag(label, branchAccepted));
    edGetTokenLost_ = consumes<TTDTC::Streams>(InputTag(label, branchLost));
    if (useMCTruth_) {
      const auto& inputTagSelecttion = iConfig.getParameter<InputTag>("InputTagSelection");
      const auto& inputTagReconstructable = iConfig.getParameter<InputTag>("InputTagReconstructable");
      edGetTokenSelection_ = consumes<StubAssociation>(inputTagSelecttion);
      edGetTokenReconstructable_ = consumes<StubAssociation>(inputTagReconstructable);
    }
    // book ES products
    esGetTokenSetup_ = esConsumes<Setup, SetupRcd, Transition::BeginRun>();
    esGetTokenDataFormats_ = esConsumes<DataFormats, DataFormatsRcd, Transition::BeginRun>();
    // initial ES products
    setup_ = nullptr;
    dataFormats_ = nullptr;
    // log config
    log_.setf(ios::fixed, ios::floatfield);
    log_.precision(4);
  }

  void AnalyzerLF::beginRun(const Run& iEvent, const EventSetup& iSetup) {
    // helper class to store configurations
    setup_ = &iSetup.getData(esGetTokenSetup_);
    // helper class to extract structured data from TTDTC::Frames
    dataFormats_ = &iSetup.getData(esGetTokenDataFormats_);
    // book histograms
    Service<TFileService> fs;
    TFileDirectory dir;
    dir = fs->mkdir("LF");
    prof_ = dir.make<TProfile>("Counts", ";", 9, 0.5, 9.5);
    prof_->GetXaxis()->SetBinLabel(1, "Stubs");
    prof_->GetXaxis()->SetBinLabel(2, "Tracks");
    prof_->GetXaxis()->SetBinLabel(3, "Lost Tracks");
    prof_->GetXaxis()->SetBinLabel(4, "Matched Tracks");
    prof_->GetXaxis()->SetBinLabel(5, "All Tracks");
    prof_->GetXaxis()->SetBinLabel(6, "Found TPs");
    prof_->GetXaxis()->SetBinLabel(7, "Found selected TPs");
    prof_->GetXaxis()->SetBinLabel(8, "Lost TPs");
    prof_->GetXaxis()->SetBinLabel(9, "All TPs");
    // binQoverPt occupancy
    constexpr int maxOcc = 180;
    const int numChannel = dataFormats_->numChannel(Process::lf);
    hisChannel_ = dir.make<TH1F>("His binQoverPt Occupancy", ";", maxOcc, -.5, maxOcc - .5);
    profChannel_ = dir.make<TProfile>("Prof binQoverPt Occupancy", ";", numChannel, -.5, numChannel - .5);
  }

  void AnalyzerLF::analyze(const Event& iEvent, const EventSetup& iSetup) {
    // read in lf products
    Handle<TTDTC::Streams> handleAccepted;
    iEvent.getByToken<TTDTC::Streams>(edGetTokenAccepted_, handleAccepted);
    Handle<TTDTC::Streams> handleLost;
    iEvent.getByToken<TTDTC::Streams>(edGetTokenLost_, handleLost);
    // read in MCTruth
    const StubAssociation* selection = nullptr;
    const StubAssociation* reconstructable = nullptr;
    if (useMCTruth_) {
      Handle<StubAssociation> handleSelection;
      iEvent.getByToken<StubAssociation>(edGetTokenSelection_, handleSelection);
      selection = handleSelection.product();
      prof_->Fill(9, selection->numTPs());
      Handle<StubAssociation> handleReconstructable;
      iEvent.getByToken<StubAssociation>(edGetTokenReconstructable_, handleReconstructable);
      reconstructable = handleReconstructable.product();
    }
    // analyze lf products and associate found tracks with reconstrucable TrackingParticles
    set<TPPtr> tpPtrs;
    set<TPPtr> tpPtrsSelection;
    set<TPPtr> tpPtrsLost;
    int allMatched(0);
    int allTracks(0);
    for (int region = 0; region < setup_->numRegions(); region++) {
      int nStubs(0);
      int nTracks(0);
      int nLost(0);
      for (int channel = 0; channel < dataFormats_->numChannel(Process::lf); channel++) {
        const int qOverPt = dataFormats_->format(Variable::qOverPt, Process::lf).toSigned(channel);
        const int index = region * dataFormats_->numChannel(Process::lf) + channel;
        const TTDTC::Stream& accepted = handleAccepted->at(index);
        hisChannel_->Fill(accepted.size());
        profChannel_->Fill(channel, accepted.size());
        nStubs += accepted.size();
        vector<vector<TTStubRef>> tracks;
        vector<vector<TTStubRef>> lost;
        formTracks(accepted, tracks, qOverPt);
        formTracks(handleLost->at(index), lost, qOverPt);
        nTracks += tracks.size();
        allTracks += tracks.size();
        nLost += lost.size();
        if (!useMCTruth_)
          continue;
        int tmp(0);
        associate(tracks, selection, tpPtrsSelection, tmp);
        associate(lost, selection, tpPtrsLost, tmp);
        associate(tracks, reconstructable, tpPtrs, allMatched);
      }
      prof_->Fill(1, nStubs);
      prof_->Fill(2, nTracks);
      prof_->Fill(3, nLost);
    }
    vector<TPPtr> recovered;
    recovered.reserve(tpPtrsLost.size());
    set_intersection(tpPtrsLost.begin(), tpPtrsLost.end(), tpPtrs.begin(), tpPtrs.end(), back_inserter(recovered));
    for(const TPPtr& tpPtr : recovered)
      tpPtrsLost.erase(tpPtr);
    prof_->Fill(4, allMatched);
    prof_->Fill(5, allTracks);
    prof_->Fill(6, tpPtrs.size());
    prof_->Fill(7, tpPtrsSelection.size());
    prof_->Fill(8, tpPtrsLost.size());
    nEvents_++;
  }

  void AnalyzerLF::endJob() {
    // printout LF summary
    const double totalTPs = prof_->GetBinContent(9);
    const double numStubs = prof_->GetBinContent(1);
    const double numTracks = prof_->GetBinContent(2);
    const double numTracksLost = prof_->GetBinContent(3);
    const double totalTracks = prof_->GetBinContent(5);
    const double numTracksMatched = prof_->GetBinContent(4);
    const double numTPsAll = prof_->GetBinContent(6);
    const double numTPsEff = prof_->GetBinContent(7);
    const double numTPsLost = prof_->GetBinContent(8);
    const double errStubs = prof_->GetBinError(1);
    const double errTracks = prof_->GetBinError(2);
    const double errTracksLost = prof_->GetBinError(3);
    const double fracFake = (totalTracks - numTracksMatched) / totalTracks;
    const double fracDup = (numTracksMatched - numTPsAll) / totalTracks;
    const double eff = numTPsEff / totalTPs;
    const double errEff = sqrt(eff * (1. - eff) / totalTPs / nEvents_);
    const double effLoss = numTPsLost / totalTPs;
    const double errEffLoss = sqrt(effLoss * (1. - effLoss) / totalTPs / nEvents_);
    const vector<double> nums = {numStubs, numTracks, numTracksLost};
    const vector<double> errs = {errStubs, errTracks, errTracksLost};
    const int wNums = ceil(log10(*max_element(nums.begin(), nums.end()))) + 5;
    const int wErrs = ceil(log10(*max_element(errs.begin(), errs.end()))) + 5;
    log_ << "                         LF  SUMMARY                         " << endl;
    log_ << "number of stubs       per TFP = " << setw(wNums) << numStubs << " +- " << setw(wErrs) << errStubs << endl;
    log_ << "number of tracks      per TFP = " << setw(wNums) << numTracks << " +- " << setw(wErrs) << errTracks << endl;
    log_ << "number of lost tracks per TFP = " << setw(wNums) << numTracksLost << " +- " << setw(wErrs) << errTracksLost << endl;
    log_ << "          tracking efficiency = " << setw(wNums) << eff << " +- " << setw(wErrs) << errEff << endl;
    log_ << "     lost tracking efficiency = " << setw(wNums) << effLoss << " +- " << setw(wErrs) << errEffLoss << endl;
    log_ << "                    fake rate = " << setw(wNums) << fracFake << endl;
    log_ << "               duplicate rate = " << setw(wNums) << fracDup << endl;
    log_ << "=============================================================";
    LogPrint("L1Trigger/TrackerTFP") << log_.str();
  }

  //
  void AnalyzerLF::formTracks(const TTDTC::Stream& stream, vector<vector<TTStubRef>>& tracks, int qOverPt) const {
    vector<StubLF> stubs;
    stubs.reserve(stream.size());
    for (const TTDTC::Frame& frame : stream)
      stubs.emplace_back(frame, dataFormats_, qOverPt);
    for (auto it = stubs.begin(); it != stubs.end();) {
      const auto start = it;
      const int id = it->trackId();
      auto different = [id](const StubLF& stub){ return id != stub.trackId(); };
      it = find_if(it, stubs.end(), different);
      vector<TTStubRef> ttStubRefs;
      ttStubRefs.reserve(distance(start, it));
      transform(start, it, back_inserter(ttStubRefs), [](const StubLF& stub){ return stub.ttStubRef(); });
      tracks.push_back(ttStubRefs);
    }
  }

  //
  void AnalyzerLF::associate(const vector<vector<TTStubRef>>& tracks, const StubAssociation* ass, set<TPPtr>& tps, int& sum) const {
    for (const vector<TTStubRef>& ttStubRefs : tracks) {
      const vector<TPPtr>& tpPtrs = ass->associate(ttStubRefs);
      if (tpPtrs.empty())
        continue;
      sum++;
      copy(tpPtrs.begin(), tpPtrs.end(), inserter(tps, tps.begin()));
    }
  }

}  // namespace trackerTFP

DEFINE_FWK_MODULE(trackerTFP::AnalyzerLF);