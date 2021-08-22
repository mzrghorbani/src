#include "L1Trigger/TrackerTFP/interface/LinearRegression.h"

using namespace std;
using namespace edm;
using namespace trackerDTC;

#include <numeric>
#include <vector>
#include <iterator>
#include <algorithm>

namespace trackerTFP {

  LinearRegression::LinearRegression(const ParameterSet& iConfig, const Setup* setup, const DataFormats* dataFormats, int region) :
    setup_(setup),
    dataFormats_(dataFormats),
    region_(region),
    input_(dataFormats_->numChannel(Process::gp)) {}

  void LinearRegression::consume(const TTDTC::Streams& streams) {
    auto valid = [](int& sum, const TTDTC::Frame& frame){ return sum += (frame.first.isNonnull() ? 1 : 0); };
    int nStubsGP(0);
    for (int channel = 0; channel < dataFormats_->numChannel(Process::gp); channel++) {
      const TTDTC::Stream& stream = streams[region_ * dataFormats_->numChannel(Process::gp) + channel];
      nStubsGP += accumulate(stream.begin(), stream.end(), 0, valid);
    }
    stubsGP_.reserve(nStubsGP);
    stubsLR_.reserve(nStubsGP);
    for (int channel = 0; channel < dataFormats_->numChannel(Process::gp); channel++) {
      const TTDTC::Stream& stream = streams[region_ * dataFormats_->numChannel(Process::gp) + channel];
      vector<StubGP*>& input = input_[channel];
      input.reserve(stream.size());
      for (const TTDTC::Frame& frame : stream) {
        StubGP* stub = nullptr;
        if (frame.first.isNonnull()) {
          stubsGP_.emplace_back(frame, dataFormats_);
          stub = & stubsGP_.back();
        }
        input.push_back(stub);
      }
    }
  }

  void LinearRegression::produce(TTDTC::Streams& stubs, TTDTC::Streams& tracks) {
    for (int channel = 0; channel < dataFormats_->numChannel(Process::gp); channel++) {
      const vector<StubGP*>& input = input_[channel];
      TTDTC::Stream& streamStubs = stubs[region_ * dataFormats_->numChannel(Process::gp) + channel];
      TTDTC::Stream& streamTracks = tracks[region_ * dataFormats_->numChannel(Process::gp) + channel];
      streamStubs.reserve(input.size());
      streamTracks.reserve(input.size());
      for (auto it = input.begin(); it != input.end();) {
        if (!*it) {
          streamStubs.emplace_back(TTDTC::Frame());
          streamTracks.emplace_back(TTDTC::Frame());
          it++;
          continue;
        }
        const auto start = it;
        const int id = (*it)->trackId();
        auto different = [id](StubGP* stub){ return !stub || id != stub->trackId(); };
        it = find_if(it, input.end(), different);
        vector<StubGP*> track;
        StubLR* stub = nullptr;
        track.reserve(distance(start, it));
        copy(start, it, back_inserter(track));
        fit(track, stub);
        for (StubGP* stub : track)
          streamStubs.emplace_back(stub ? stub->frame() : TTDTC::Frame());
        streamTracks.insert(streamTracks.end(), track.size(), stub ? stub->frame() : TTDTC::Frame());
      }
    }
  }

  // fit track
  void LinearRegression::fit(vector<StubGP*>& stubs, StubLR* track) {
    std::cout << "processign LR fit function..." << std::endl;
  }

} // namespace trackerTFP