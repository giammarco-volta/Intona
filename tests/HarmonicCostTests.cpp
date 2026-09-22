#include "tuning/HarmonicCostAdapting.h"
#include "tuning/TuningAlgorithms.h"
#include "CentsUtilities.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <numeric>

using namespace Intona::Tuning;
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
uint16_t keys(std::initializer_list<int> ks) { uint16_t m=0; for(int k:ks)m|=1<<k; return m; }
double oracle(const Config& c, uint16_t mask, const std::vector<HarmonicWeight>& h, uint16_t held = 0)
{
  std::vector<int> notes, sounding;
  for(int k=0;k<12;++k)if(mask&(1<<k))notes.push_back(c.valueForKey[k]);
  for(int k=0;k<12;++k)if((mask|held)&(1<<k))sounding.push_back(c.valueForKey[k]);
  double score=0;
  const auto d=[&](int a,int b){return std::abs(a-b);};
  for(int a:notes)for(auto old:h)score+=old.weight*d(a,old.value);
  for(size_t i=0;i<sounding.size();++i)for(size_t j=0;j<i;++j)score+=d(sounding[i],sounding[j]);
  return score;
}
}

void runHarmonicCostTests()
{
  const auto it=std::find_if(kNtetMappings.begin(),kNtetMappings.end(),[](const auto& m){return m.N==31;});
  const auto& edo=*it;
  const Config initial=relativeKeyboardConfig(0,0);
  require(harmonicDistance(-1,6)==7 && harmonicDistance(5,-2)==7
    && harmonicDistance(5,10)==5,"Every pair costs only its absolute fifth distance");
  require(harmonicGroupCost(initial,keys({0,4,7}),{})==8,
    "A triad sums all three internal pairs without division");
  require(harmonicGroupCost(initial,keys({0,4,7,10}),{})==19,
    "A seventh chord sums all six internal pairs without division");
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
  const std::vector<std::vector<HarmonicWeight>> contexts={
    {{-1,1},{3,1},{0,1}},{{-1,1},{-4,1},{0,1}},{{4,1},{8,1},{5,1}},{{4,1},{1,1},{5,1}}};
  const double germanExpected[4][2]={{46,80},{54,98},{78,82},{56,74}};
  const double frenchExpected[4][2]={{74,90},{72,112},{112,72},{90,74}};
  for(int context=0;context<4;++context)for(bool french:{false,true})
  {
    const auto mask=french?keys({0,4,6,10}):keys({0,4,7,10});
    const auto conventional=relativeKeyboardConfig(french?-1:0,0);
    const auto augmented=relativeKeyboardConfig(4,0);
    const auto& expected=french?frenchExpected:germanExpected;
    require(std::abs(harmonicGroupCost(conventional,mask,contexts[context])-expected[context][0])<1e-9,"Conventional seventh matches the independently calculated context table");
    require(std::abs(harmonicGroupCost(augmented,mask,contexts[context])-expected[context][1])<1e-9,"Augmented sixth matches the independently calculated context table");
    const auto choice=chooseHarmonicCostConfig(initial,0,edo,mask,0,contexts[context]);
    // These are numerical minima, not a requirement that every tonal context
    // resolve to the musically desired chord. Pure fifth distance also admits
    // mixed spellings and may reinterpret a root or third.
    const int frenchKeys[4]={0,4,6,10};
    const int frenchWinner[4][4]={{0,4,6,-2},{0,-8,-6,-2},{12,4,6,10},{0,4,6,-2}};
    if(french)for(int k=0;k<4;++k)
      require(choice.config.valueForKey[frenchKeys[k]]==frenchWinner[context][k],
        "Pure fifth-distance search matches the full French-context oracle");
    else require(choice.config.valueForKey[0]==0 && choice.config.valueForKey[4]==4
      && choice.config.valueForKey[7]==1 && choice.config.valueForKey[10]==-2,
      "Unnormalized German-context search matches the numerical minimum");
  }
  {
    const auto d = relativeKeyboardConfig(2, 0); // F, A, C and G#.
    const auto f = relativeKeyboardConfig(-1, 0); // F, Ab and C.
    const uint16_t arriving = keys({8}), sustained = keys({0, 5});
    const std::vector<HarmonicWeight> history = {{-1,1},{3,1},{0,1}};
    require(std::abs(harmonicGroupCost(d,arriving,history)-22)<1e-9,
      "An isolated G# has the expected external cost");
    require(std::abs(harmonicGroupCost(f,arriving,history)-14)<1e-9,
      "An isolated Ab has no additional chromatic cost");
    require(std::abs(harmonicGroupCost(d,arriving,history,sustained)-40)<1e-9,
      "Held F and C contribute to the internal cost of F G# C");
    require(std::abs(harmonicGroupCost(f,arriving,history,sustained)-22)<1e-9,
      "Held F and C favor Ab without becoming new historical comparisons");
    const auto choice=chooseHarmonicCostConfig(d,0,edo,arriving,sustained,history,{},sustained);
    require(choice.config.valueForKey[8]==-4 && choice.config.valueForKey[0]==0
      && choice.config.valueForKey[5]==-1,"F major to F minor resolves Ab while preserving held pivots");
    require(chooseHarmonicCostConfig(d,0,edo,arriving,0,history).config.valueForKey[8]==-4,
      "Released F and C contribute only historical evidence, still favoring Ab here");
    const uint16_t triad=arriving|sustained;
    require(std::abs(harmonicGroupCost(f,triad,history,sustained)
      -harmonicGroupCost(f,triad,history))<1e-9,"Overlapping new and held keys count only once");
    require(chooseHarmonicCostConfig(d,0,edo,triad,0,history).config.valueForKey[8]==-4,
      "Rearticulating all three notes still selects F minor");
    const auto noHistory=chooseHarmonicCostConfig(d,0,edo,arriving,sustained,{},{},sustained);
    require(noHistory.config.valueForKey[8]==-4,"Held harmony supports new notes even with no history weight");
  }
  // Exhaustive reference searches independently verify every competing
  // mapping, including all no-history pitch/pivot periods.
  int searches=0;
  for(bool includeHeld:{false,true})for(int seed=0;seed<72;++seed)
  {
    const auto& m=kNtetMappings[seed%kNtetMappings.size()];
    const int center=seed%19-9,anchor=seed%12;
    const auto c=relativeKeyboardConfig(center,anchor);
    const uint16_t mask=uint16_t((seed*593+37)%4095+1),pivots=seed%3==0?uint16_t(1<<(seed%12)):0;
    const std::vector<HarmonicWeight> h=seed%7==0?std::vector<HarmonicWeight>{}:std::vector<HarmonicWeight>{{seed%17-8,.3},{seed%23-11,1},{seed%13-6,2}};
    const uint16_t sustained=includeHeld?uint16_t((seed*317+65)%4095+1):0;
    const auto result=chooseHarmonicCostConfig(c,anchor,m,mask,pivots,h,{},sustained);
    double best=oracle(c,mask,h,sustained);int retuned=0,bestCenter=center;
    const int extent=std::lcm(int(m.N),12)+150;
    for(int distance=1;distance<=extent;++distance)for(int direction:{1,-1})
    {
      const int t=center+direction*distance;const auto candidate=relativeKeyboardConfig(t,anchor);
      bool allowed=true,changedGroup=false;int changes=0;
      for(int k=0;k<12;++k)
      {
        bool changed=mod(candidate.valueForKey[k]-c.valueForKey[k],m.N)!=0;
        changes+=changed;if(changed&&(pivots&(1<<k)))allowed=false;
        if(mask&(1<<k))changedGroup|=candidate.valueForKey[k]!=c.valueForKey[k];
      }
      if(!allowed||!changedGroup)continue;
      const double score=oracle(candidate,mask,h,sustained);
      if(score<best-1e-8||(std::abs(score-best)<1e-8&&changes<retuned))
      {best=score;retuned=changes;bestCenter=t;}
    }
    require(std::abs(result.cost-best)<1e-7 && result.retunedKeys==retuned && result.config.tuningCenter==bestCenter,
      "Bounded search matches exhaustive search including pivots and tie order");
    ++searches;
  }
  {
    HarmonicHistory adjustable;
    require(adjustable.windowIntervals()==16 && adjustable.windowMs()==4000,
      "Default window retains sixteen typical intervals");
    adjustable.admit({1,0,0,0,32000});
    for(int length:{1,8,16,32,128})
    {
      adjustable.setWindowIntervals(length);
      require(adjustable.windowMs()==length*250.0,"Window length scales the adaptive interval");
      for(double slope:{0.0,1.0,3.0})
        require(std::abs(adjustable.weights(32000,slope).front().weight-length/(slope+1))<1e-8,
          "Window length and slope independently control the integrated historical weight");
    }
    adjustable.setWindowIntervals(8);
    adjustable.beginGroup(32000,1);adjustable.beginGroup(32500,2);
    require(adjustable.intervalMs()==500 && adjustable.windowMs()==4000,
      "Configured window still follows the measured tempo");
    adjustable.setWindowIntervals(0);adjustable.setWindowIntervals(129);
    require(adjustable.windowIntervals()==8,"Invalid window lengths are ignored");
    adjustable.reset();
    require(adjustable.windowIntervals()==8 && adjustable.windowMs()==2000 && adjustable.size()==0,
      "Reset clears history and tempo but preserves the selected window length");
    HarmonicHistory shortWindow,longWindow;
    shortWindow.setWindowIntervals(4);longWindow.setWindowIntervals(16);
    shortWindow.admit({1,0,0,0,250});longWindow.admit({1,0,0,0,250});
    require(shortWindow.weights(2000,1).empty() && !longWindow.weights(2000,1).empty(),
      "Window selection changes which released notes remain in the context");
  }
  HarmonicHistory held,repeated,fast,slow;
  held.beginGroup(0,1);repeated.beginGroup(0,1);
  held.admit({1,0,0,0,1000});
  for(int i=0;i<4;++i){repeated.beginGroup(250*i,1);repeated.admit({uint64_t(i+1),0,0,double(250*i),double(250*(i+1))});}
  require(held.intervalMs()==repeated.intervalMs(),"Pure rearticulations do not accelerate the estimated clock");
  require(std::abs(held.weights(1000,1)[0].weight-repeated.weights(1000,1)[0].weight)<1e-9,"Held and gapless rearticulated notes have identical duration and recency weights");
  for(auto pair:{std::make_pair(&fast,1.0),std::make_pair(&slow,3.0)})
  {
    auto& h=*pair.first;const double f=pair.second;
    h.beginGroup(0,1);h.admit({1,0,0,0,200*f});h.beginGroup(250*f,2);h.admit({2,1,-5,250*f,600*f});h.beginGroup(700*f,4);
  }
  fast.setWindowIntervals(24);slow.setWindowIntervals(24);
  const auto fw=fast.weights(800,2.5),sw=slow.weights(2400,2.5);
  require(fw.size()==sw.size(),"Tempo-scaled histories retain the same events");
  for(size_t i=0;i<fw.size();++i)require(fw[i].value==sw[i].value&&std::abs(fw[i].weight-sw[i].weight)<1e-9,"Uniform tempo scaling preserves duration and recency weights");
  repeated.beginGroup(12000,4);
  require(repeated.size()==0&&repeated.intervalMs()==250,"Ten seconds of silence reset history and clock at the next valid group");
  HarmonicHistory sustaining;sustaining.beginGroup(0,1);sustaining.admit({1,0,0,0,std::nullopt});sustaining.beginGroup(12000,2);
  require(sustaining.size()==1,"A held note is not silence");
  HarmonicHistory decay;decay.admit({1,0,0,0,250});
  require(std::abs(decay.weights(250,0)[0].weight-1)<1e-9,"One typical interval has unit duration weight before decay");
  const double linear=decay.weights(1000,1)[0].weight;
  require(decay.weights(1000,2)[0].weight<linear,"Larger slope reduces older evidence faster");
  std::cout<<"PASS: "<<mapped<<" relative mappings and MIDI lifts, "<<searches<<" exhaustive harmonic searches, German/French contexts, history scaling, rearticulation and silence resets.\n";
}
