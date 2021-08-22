#ifndef L1Trigger_TrackerTFP_LinearRegression_h
#define L1Trigger_TrackerTFP_LinearRegression_h

#include "L1Trigger/TrackerDTC/interface/Setup.h"
#include "L1Trigger/TrackerTFP/interface/DataFormats.h"

namespace trackerTFP {

  // Class to find in a region finer rough candidates in r-phi
  class LinearRegression {
  public:
    LinearRegression(const edm::ParameterSet& iConfig, const trackerDTC::Setup* setup, const DataFormats* dataFormats, int region);
    ~LinearRegression() {}

    // read in and organize input product
    void consume(const TTDTC::Streams& streams);
    
    // fill output products
    void produce(TTDTC::Streams& stubs, TTDTC::Streams& tracks);

  private:
    // fit track
    void fit(std::vector<StubGP*>& stubs, StubLR* track);
    // 
    const trackerDTC::Setup* setup_;
    //
    const DataFormats* dataFormats_;
    //
    int region_;
    // 
    std::vector<StubGP> stubsGP_;
    // 
    std::vector<StubLR> stubsLR_;
    //
    std::vector<std::vector<StubGP*>> input_;
  };

}

#endif