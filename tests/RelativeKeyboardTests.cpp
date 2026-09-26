#include "tuning/RelativeKeyboard.h"
#include "tuning/TuningAlgorithms.h"
#include "CentsUtilities.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace Intona::Tuning;
namespace { void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); } }
void runRelativeKeyboardTests()
{
  const auto& edo=*std::find_if(kNtetMappings.begin(), kNtetMappings.end(), [](const auto& m){return m.N==31;});
  int mapped=0;
  for(const auto& mapping:kNtetMappings)for(int anchor=0;anchor<12;++anchor)
    for(int center=-2*mapping.N;center<=2*mapping.N;++center)
    {
      const auto c=relativeKeyboardConfig(center,anchor);
      const auto next=relativeKeyboardConfig(center+1,anchor);
      int changes=0;for(int k=0;k<12;++k)changes+=c.valueForKey[k]!=next.valueForKey[k];
      require(changes==1,"One fifth must replace exactly one of twelve assignments");
      require(relativeKeyboardAnchor(c)==anchor,"Relative keyboard anchor is recovered without canonicalizing the center");
      const auto twelve=relativeKeyboardConfig(center+12,anchor);
      for(int k=0;k<12;++k)require(twelve.valueForKey[k]==c.valueForKey[k]+12,"Twelve fifths translate the entire group");
      const auto offset=findGlobalOffsetCents(c,mapping,0);
      require(offset.has_value(),"Relative mappings remain MIDI-encodable across octave-lift boundaries");
      const auto detune=computeDetuneTable(mapping.N,mapping.fifthStep,c,*offset);
      for(int k=0;k<12;++k)
      {
        const double cents=100*k+detune[k]+*offset;
        const double pitch=1200.0*mod(c.valueForKey[k]*mapping.fifthStep,mapping.N)/mapping.N;
        require(std::abs(std::remainder(cents-pitch,1200))<1e-7,"Encoded MIDI pitch matches the exact EDO step");
        if(k)require(100*k+detune[k]>100*(k-1)+detune[k-1],"Relative MIDI mapping preserves pitch order");
      }
      ++mapped;
    }
  for(const auto& mapping:kNtetMappings)
  {
    auto before=relativeKeyboardConfig(0,0);
    double offset=*findGlobalOffsetCents(before,mapping,0);
    for(int center=1;center<=12*mapping.N;++center)
    {
      const auto after=relativeKeyboardConfig(center,0);
      const auto nextOffset=findGlobalOffsetCents(after,mapping,offset);
      if(!nextOffset)
      {
        require(std::abs(offset)>6100,"Only the MIDI coarse-tuning limit may end a continuous relative path");
        break;
      }
      const auto oldDetune=computeDetuneTable(mapping.N,mapping.fifthStep,before,offset);
      const auto newDetune=computeDetuneTable(mapping.N,mapping.fifthStep,after,*nextOffset);
      for(int key=0;key<12;++key)if(before.valueForKey[key]==after.valueForKey[key])
        require(std::abs(oldDetune[key]+offset-newDetune[key]-*nextOffset)<1e-7,
          "Eleven common notes retain absolute pitch, without an octave jump at lift boundaries");
      before=after;offset=*nextOffset;
    }
  }
  require(relativeKeyboardConfig(31,0).valueForKey[1]==31,"One 31-EDO cycle returns the pitch on the next physical key");
  for(int center:{-5000,-128,5000})
  {
    const auto c=relativeKeyboardConfig(center,3);
    require(c.tuningCenter!=Config::invalid && relativeKeyboardAnchor(c)==3,"Wide fifth coordinates include -128 as a valid center");
    require(findGlobalOffsetCents(c,edo,0).has_value(),"Wide centers remain encodable");
  }
  require(!findGlobalOffsetCents(relativeKeyboardConfig(0,0),edo,1000000),
    "An unencodable relative preset offset is rejected rather than overflowing MIDI data");
  std::cout << "PASS: " << mapped << " relative mappings, common-note continuity and MIDI encoding.\n";
}
