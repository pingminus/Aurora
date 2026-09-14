#include "cve_feed.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <tuple>
namespace opengod::cve {
std::optional<Time> parse_time(const std::string& s) {
  if (s.size() < 19 || s.size() > 24 || s[4]!='-' || s[7]!='-' || s[10]!='T' || s[13]!=':' || s[16]!=':') return {};
  for (int i : {0,1,2,3,5,6,8,9,11,12,14,15,17,18}) if (s[i]<'0'||s[i]>'9') return {};
  if (s.size()!=19 && s.substr(19)!="Z") {
    auto suffix=s.substr(19); if (suffix.back()=='Z') suffix.pop_back();
    if (suffix.size()!=4 || suffix[0]!='.' || !std::all_of(suffix.begin()+1,suffix.end(),[](char c){return c>='0'&&c<='9';})) return {};
  }
  const int y=std::stoi(s.substr(0,4)),m=std::stoi(s.substr(5,2)),d=std::stoi(s.substr(8,2));
  const int h=std::stoi(s.substr(11,2)),mi=std::stoi(s.substr(14,2)),se=std::stoi(s.substr(17,2));
  std::chrono::year_month_day date{std::chrono::year(y),std::chrono::month(static_cast<unsigned>(m)),std::chrono::day(static_cast<unsigned>(d))};
  if(!date.ok()||y<1999||h>23||mi>59||se>59) return {};
  return std::chrono::sys_days(date)+std::chrono::hours(h)+std::chrono::minutes(mi)+std::chrono::seconds(se);
}
std::string format_time(Time value) {
  const auto days=std::chrono::floor<std::chrono::days>(value);
  const std::chrono::year_month_day date{days}; const std::chrono::hh_mm_ss clock{value-days};
  char text[32]{};
  std::snprintf(text,sizeof(text),"%04d-%02u-%02uT%02lld:%02lld:%02lldZ",int(date.year()),unsigned(date.month()),unsigned(date.day()),static_cast<long long>(clock.hours().count()),static_cast<long long>(clock.minutes().count()),static_cast<long long>(clock.seconds().count())); return text;
}
Time day_start(Time now) { return std::chrono::floor<std::chrono::days>(now); }
bool valid_id(const std::string& s) {
  if(s.size()<13||s.size()>32||!s.starts_with("CVE-")||s[8]!='-')return false;
  for(size_t i=4;i<s.size();++i) if(i!=8 && (s[i]<'0'||s[i]>'9'))return false;
  return true;
}
std::optional<Metric> select_metric(const std::vector<Metric>& metrics) {
  auto rank=[](const Metric& m){int v=m.version=="4.0"?0:m.version=="3.1"?1:m.version=="3.0"?2:m.version=="2.0"?3:4;return std::tuple(v,m.type=="Primary"?0:1,m.source=="nvd@nist.gov"?0:1,m.source,m.score);};
  std::optional<Metric> best;
  for(const auto& m:metrics)if(std::isfinite(m.score)&&m.score>=0&&m.score<=10&&std::get<0>(rank(m))<4&&(!best||rank(m)<rank(*best)))best=m;
  return best;
}
std::string severity(const Metric& m) { if(m.version=="2.0")return m.score<4?"Low":m.score<7?"Medium":"High"; return m.score==0?"None":m.score<4?"Low":m.score<7?"Medium":m.score<9?"High":"Critical"; }
void sort_entries(std::vector<Entry>& entries) { std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return std::tuple(!a.metric,a.metric?-a.metric->score:0,a.id)<std::tuple(!b.metric,b.metric?-b.metric->score:0,b.id);}); }
bool Batch::append(int offset,int total,int count,std::vector<Entry> entries) {
  if(offset!=next_||total<0||total>10000||count<0||count>500||offset+count>total||(total_>=0&&total!=total_)||(count==0&&offset!=total))return false;
  auto ids=ids_; std::vector<Entry> accepted;
  for(auto& e:entries){auto pub=parse_time(e.published);if(!valid_id(e.id)||!pub||!parse_time(e.modified)||e.description.empty()||e.description.size()>4096||!ids.insert(e.id).second)return false;if(*pub>=start_&&*pub<=end_)accepted.push_back(std::move(e));}
  if(entries.size()>static_cast<size_t>(count))return false;
  total_=total;next_+=count;ids_=std::move(ids);entries_.insert(entries_.end(),std::make_move_iterator(accepted.begin()),std::make_move_iterator(accepted.end()));return true;
}
bool Cache::begin(Time now,bool refresh) {
  if(loading|| (attempted!=Time{} && now-attempted<std::chrono::seconds(60)))return false;
  if(!refresh&&available&&start==day_start(now)&&now-fetched<std::chrono::minutes(15))return false;
  if(available&&start!=day_start(now)){available=false;entries.clear();}
  attempted=now;loading=true;error.clear();return true;
}
void Cache::success(const Batch& batch,Time now){entries=batch.entries();sort_entries(entries);start=day_start(attempted);end=attempted;fetched=now;available=true;loading=false;error.clear();}
}
