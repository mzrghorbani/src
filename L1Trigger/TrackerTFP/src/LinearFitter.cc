#include "L1Trigger/TrackerTFP/interface/LinearFitter.h"

#include <numeric>
#include <algorithm>
#include <iterator>
#include <deque>
#include <vector>
#include <set>
#include <utility>
#include <cmath>

using namespace std;
using namespace edm;
using namespace trackerDTC;

namespace trackerTFP {

  LinearFitter::LinearFitter(const ParameterSet& iConfig, const Setup* setup, const DataFormats* dataFormats, int region) :
    enableTruncation_(iConfig.getParameter<bool>("EnableTruncation")),
    setup_(setup),
    dataFormats_(dataFormats),
    qOverPt_(dataFormats_->format(Variable::qOverPt, Process::lf)),
    phiT_(dataFormats_->format(Variable::phiT, Process::lf)),
    region_(region),
    input_(dataFormats_->numChannel(Process::lf), vector<deque<StubGP*>>(dataFormats_->numChannel(Process::gp)))
  {}

  // read in and organize input product
  void LinearFitter::consume(const TTDTC::Streams& streams) {
    const int offset = region_ * dataFormats_->numChannel(Process::gp);
    auto validFrame = [](int& sum, const TTDTC::Frame& frame){ return sum += frame.first.isNonnull() ? 1 : 0; };
    int nStubsGP(0);
    for (int sector = 0; sector < dataFormats_->numChannel(Process::gp); sector++) {
      const TTDTC::Stream& stream = streams[offset + sector];
      nStubsGP += accumulate(stream.begin(), stream.end(), 0, validFrame);
    }
    stubsGP_.reserve(nStubsGP);
    for (int sector = 0; sector < dataFormats_->numChannel(Process::gp); sector++) {
      const int sectorPhi = sector % setup_->numSectorsPhi();
      const int sectorEta = sector / setup_->numSectorsPhi();
      for (const TTDTC::Frame& frame : streams[offset + sector]) {
        StubGP* stub = nullptr;
        if (frame.first.isNonnull()) {
          stubsGP_.emplace_back(frame, dataFormats_, sectorPhi, sectorEta);
          stub = &stubsGP_.back();
        }
        for (int binQoverPt = 0; binQoverPt < dataFormats_->numChannel(Process::lf); binQoverPt++)
          input_[binQoverPt][sector].push_back(stub && stub->inQoverPtBin(binQoverPt) ? stub : nullptr);
      }
    }
    // remove all gaps between end and last stub
    for(vector<deque<StubGP*>>& input : input_)
      for(deque<StubGP*>& stubs : input)
        for(auto it = stubs.end(); it != stubs.begin();)
          it = (*--it) ? stubs.begin() : stubs.erase(it);
    auto validStub = [](int& sum, StubGP* stub){ return sum += stub ? 1 : 0; };
    int nStubsHT(0);
    for (const vector<deque<StubGP*>>& binQoverPt : input_)
      for (const deque<StubGP*>& sector : binQoverPt)
        nStubsHT += accumulate(sector.begin(), sector.end(), 0, validStub);
    stubsLF_.reserve(nStubsHT);
  }

  // fill output products
  void LinearFitter::produce(TTDTC::Streams& accepted, TTDTC::Streams& lost) {
    for (int binQoverPt = 0; binQoverPt < dataFormats_->numChannel(Process::lf); binQoverPt++) {
      const int qOverPt = qOverPt_.toSigned(binQoverPt);
      deque<StubLF*> acceptedAll;
      deque<StubLF*> lostAll;
      for (deque<StubGP*>& inputSector : input_[binQoverPt]) {
        const int size = inputSector.size();
        vector<StubLF*> acceptedSector;
        vector<StubLF*> lostSector;
        acceptedSector.reserve(size);
        lostSector.reserve(size);
        // associate stubs with qOverPt and phiT bins
        fillIn(inputSector, acceptedSector, lostSector, qOverPt);
        // Process::lf collects all stubs before readout starts -> remove all gaps
        acceptedSector.erase(remove(acceptedSector.begin(), acceptedSector.end(), nullptr), acceptedSector.end());
        acceptedSector.shrink_to_fit();
        lostSector.shrink_to_fit();
        // identify tracks
        readOut(acceptedSector, lostSector, acceptedAll, lostAll);
      }
      // truncate accepted stream
      const auto limit = enableTruncation_ ? next(acceptedAll.begin(), min(setup_->numFrames(), (int)acceptedAll.size())) : acceptedAll.end();
      copy_if(limit, acceptedAll.end(), back_inserter(lostAll), [](StubLF* stub){ return stub; });
      acceptedAll.erase(limit, acceptedAll.end());
      // store found tracks
      auto put = [](const deque<StubLF*>& stubs, TTDTC::Stream& stream){
        stream.reserve(stubs.size());
        for (StubLF* stub : stubs)
          stream.emplace_back(stub ? stub->frame() : TTDTC::Frame());
      };
      const int offset = region_ * dataFormats_->numChannel(Process::lf);
      put(acceptedAll, accepted[offset + binQoverPt]);
      // store lost tracks
      put(lostAll, lost[offset + binQoverPt]);
    }
  }

  // associate stubs with qOverPt and phiT bins
  void LinearFitter::fillIn(deque<StubGP*>& inputSector, vector<StubLF*>& acceptedSector, vector<StubLF*>& lostSector, int qOverPt) {
    // fifo, used to store stubs which belongs to a second possible track
    deque<StubLF*> stack;
    // clock accurate firmware emulation, each while trip describes one clock tick, one stub in and one stub out per tick
    while (!inputSector.empty() || !stack.empty()) {
      StubLF* StubLF = nullptr;
      StubGP* stubGP = pop_front(inputSector);
      if (stubGP) {
        const double phiT = stubGP->phi() + qOverPt_.floating(qOverPt) * stubGP->r();
        const int major = phiT_.integer(phiT);
        if (phiT_.inRange(major)) {
          // major candidate has pt > threshold (3 GeV)
          stubsLF_.emplace_back(*stubGP, major, qOverPt);
          StubLF = &stubsLF_.back();
        }
        const double chi = phiT - phiT_.floating(major);
        if (abs(stubGP->r() * qOverPt_.base()) + 2. * abs(chi) >= phiT_.base()) {
          // stub belongs to two candidates
          const int minor = chi >= 0. ? major + 1 : major - 1;
          if (phiT_.inRange(minor)) {
            // second (minor) candidate has pt > threshold (3 GeV)
            stubsLF_.emplace_back(*stubGP, minor, qOverPt);
            if (enableTruncation_ && (int)stack.size() == setup_->htDepthMemory() - 1)
              // buffer overflow
              lostSector.push_back(pop_front(stack));
            // store minor stub in fifo
            stack.push_back(&stubsLF_.back());
          }
        }
      }
      // take a minor stub if no major stub available
      acceptedSector.push_back(StubLF ? StubLF : pop_front(stack));
    }
    // truncate to many input stubs
    const auto limit = enableTruncation_ ? next(acceptedSector.begin(), min(setup_->numFrames(), (int)acceptedSector.size())) : acceptedSector.end();
    copy_if(limit, acceptedSector.end(), back_inserter(lostSector), [](StubLF* stub){ return stub; });
    acceptedSector.erase(limit, acceptedSector.end());
  }

  // identify tracks
  void LinearFitter::readOut(const vector<StubLF*>& acceptedSector, const vector<StubLF*>& lostSector, deque<StubLF*>& acceptedAll, deque<StubLF*>& lostAll) const {
    // used to recognise in which order tracks are found
    TTBV patternPhiTs(0, setup_->htNumBinsPhiT());
    // hitPattern for all possible tracks, used to find tracks
    vector<TTBV> patternHits(setup_->htNumBinsPhiT(), TTBV(0, setup_->numLayers()));
    // found unsigned phiTs, ordered in time
    vector<int> binsPhiT;
    // stub container for all possible tracks
    vector<vector<StubLF*>> tracks(setup_->htNumBinsPhiT());
    for (int binPhiT = 0; binPhiT < setup_->htNumBinsPhiT(); binPhiT++) {
      const int phiT = phiT_.toSigned(binPhiT);
      auto samePhiT = [phiT](int& sum, StubLF* stub){ return sum += stub->phiT() == phiT; };
      const int numAccepted = accumulate(acceptedSector.begin(), acceptedSector.end(), 0, samePhiT);
      const int numLost = accumulate(lostSector.begin(), lostSector.end(), 0, samePhiT);
      tracks[binPhiT].reserve(numAccepted + numLost);
    }
    for (StubLF* stub : acceptedSector) {
      const int binPhiT = phiT_.toUnsigned(stub->phiT());
      TTBV& pattern = patternHits[binPhiT];
      pattern.set(stub->layer());
      tracks[binPhiT].push_back(stub);
      if (pattern.count() >= setup_->htMinLayers() && !patternPhiTs[binPhiT]) {
        // first time track found
        patternPhiTs.set(binPhiT);
        binsPhiT.push_back(binPhiT);
      }
    }
    // read out found tracks ordered as found
    for (int binPhiT : binsPhiT) {
      const vector<StubLF*>& track = tracks[binPhiT];
      acceptedAll.insert(acceptedAll.end(), track.begin(), track.end());
    }
    // look for lost tracks
    for (StubLF* stub : lostSector) {
      const int binPhiT = phiT_.toUnsigned(stub->phiT());
      if (!patternPhiTs[binPhiT])
        tracks[binPhiT].push_back(stub);
    }
    for (int binPhiT : patternPhiTs.ids(false)) {
      const vector<StubLF*>& track = tracks[binPhiT];
      set<int> layers;
      auto toLayer = [](StubLF* stub){ return stub->layer(); };
      transform(track.begin(), track.end(), inserter(layers, layers.begin()), toLayer);
      if ((int)layers.size() >= setup_->htMinLayers())
        lostAll.insert(lostAll.end(), track.begin(), track.end());
    }
  }

  // remove and return first element of deque, returns nullptr if empty
  template<class T>
  T* LinearFitter::pop_front(deque<T*>& ts) const {
    T* t = nullptr;
    if (!ts.empty()) {
      t = ts.front();
      ts.pop_front();
    }
    return t;
  }

} // namespace trackerTFP