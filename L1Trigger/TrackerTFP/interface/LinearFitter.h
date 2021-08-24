#ifndef L1Trigger_TrackerTFP_LinearFitter_h
#define L1Trigger_TrackerTFP_LinearFitter_h

#include "L1Trigger/TrackerDTC/interface/Setup.h"
#include "L1Trigger/TrackerTFP/interface/DataFormats.h"

#include <vector>
#include <set>
#include <deque>

namespace trackerTFP {

  // Class to find initial rough candidates in r-phi in a region
  class LinearFitter {
  public:
    LinearFitter(const edm::ParameterSet& iConfig, const trackerDTC::Setup* setup, const DataFormats* dataFormats, int region);
    ~LinearFitter(){}

    // read in and organize input product
    void consume(const TTDTC::Streams& streams);
    // fill output products
    void produce(TTDTC::Streams& accepted, TTDTC::Streams& lost);

  private:
    // remove and return first element of deque, returns nullptr if empty
    template<class T>
    T* pop_front(std::deque<T*>& ts) const;
    // associate stubs with qOverPt and phiT bins
    void fillIn(std::deque<StubGP*>& inputSector, std::vector<StubLF*>& acceptedSector, std::vector<StubLF*>& lostSector, int qOverPt);
    // identify tracks
    void readOut(const std::vector<StubLF*>& acceptedSector, const std::vector<StubLF*>& lostSector, std::deque<StubLF*>& acceptedAll, std::deque<StubLF*>& lostAll) const;
    // identify lost tracks
    void analyze();
    // store tracks
    void put() const;

    //
    bool enableTruncation_;
    // 
    const trackerDTC::Setup* setup_;
    //
    const DataFormats* dataFormats_;
    //
    DataFormat qOverPt_;
    //
    DataFormat phiT_;
    //
    int region_;
    //
    std::vector<StubGP> stubsGP_;
    //
    std::vector<StubLF> stubsLF_;
    //
    std::vector<std::vector<std::deque<StubGP*>>> input_;
  };

}

#endif