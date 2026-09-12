#include "core/cve_feed.h"
#include <iostream>
#include <limits>
using namespace aurora::cve;
int main(){int failures=0;
#define CHECK(x) do { if(!(x)){std::cerr<<"Failed line "<<__LINE__<<"\n";++failures;} }while(false)
const auto monday=*parse_time("2026-09-07T00:00:00Z"), sunday=*parse_time("2026-09-13T23:59:59Z");
CHECK(day_start(sunday)==monday+std::chrono::days(6));CHECK(day_start(monday)==monday);CHECK(day_start(monday-std::chrono::seconds(1))==monday-std::chrono::days(1));
CHECK(format_time(day_start(*parse_time("2026-01-01T12:00:00Z")))=="2026-01-01T00:00:00Z");
CHECK(!parse_time("2026-02-30T00:00:00Z"));CHECK(!parse_time("2026-09-07T24:00:00Z"));CHECK(!parse_time("garbage"));CHECK(parse_time("2024-02-29T12:00:00.123"));CHECK(!parse_time("2026-09-07T00:00:00junk"));
CHECK(valid_id("CVE-2026-12345"));CHECK(!valid_id("CVE-2026-<bad>"));
std::vector<Metric> metrics{{"3.1","nvd@nist.gov","Primary",9.8},{"4.0","vendor","Secondary",2.1},{"4.0","z","Primary",4.5},{"4.0","nvd@nist.gov","Primary",3.5}};
CHECK(select_metric(metrics)->score==3.5);CHECK(!select_metric({{"3.1","x","Primary",std::numeric_limits<double>::quiet_NaN()},{"3.1","x","Primary",11}}));
CHECK(select_metric({{"3.1","x","Primary",0}})->score==0);CHECK(severity({"2.0","","",9.9})=="High");
auto make=[](std::string id,std::string pub,std::optional<Metric> metric=std::nullopt){return Entry{id,"Description",pub,"2026-09-14T00:00:00Z",metric};};
auto a=make("CVE-2026-1000","2026-09-07T00:00:00Z",Metric{"3.1","x","Primary",9.8});
auto b=make("CVE-2026-1001","2026-09-10T00:00:00Z",Metric{"4.0","x","Primary",2.1});
auto c=make("CVE-2026-1002","2026-09-10T00:00:00Z");
std::vector<Entry> entries{c,a,b};sort_entries(entries);CHECK(entries[0].id==a.id&&entries[1].id==b.id&&entries[2].id==c.id);
auto zero=make("CVE-2026-1003","2026-09-10T00:00:00Z",Metric{"3.1","x","Primary",0});
entries={c,zero,b,a};sort_entries(entries);CHECK(entries[0].id==a.id&&entries[1].id==b.id&&entries[2].id==zero.id&&entries[3].id==c.id);
Batch batch(monday,sunday);CHECK(batch.append(0,3,2,{a,b}));CHECK(!batch.complete());CHECK(!batch.append(1,3,1,{c}));CHECK(!batch.append(2,4,1,{c}));CHECK(!batch.append(2,3,1,{a}));CHECK(batch.append(2,3,1,{c}));CHECK(batch.complete());
Batch filtered(monday,sunday);CHECK(filtered.append(0,2,2,{make("CVE-2026-2000","2026-09-06T23:59:59Z"),make("CVE-2026-2001","2026-09-14T00:00:00Z")}));CHECK(filtered.entries().empty());
Batch empty(monday,sunday);CHECK(empty.append(0,0,0,{}));CHECK(empty.complete());Batch bad(monday,sunday);CHECK(!bad.append(0,2,0,{}));CHECK(!bad.append(0,10001,1,{a}));CHECK(!bad.append(0,1,1,{make("bad","bad")}));
Cache cache;CHECK(cache.begin((sunday-std::chrono::hours(12)),false));CHECK(!cache.begin((sunday-std::chrono::hours(12)),true));cache.success(batch,(sunday-std::chrono::hours(12)));CHECK(cache.available&&cache.entries.size()==3);CHECK(!cache.begin((sunday-std::chrono::hours(12))+std::chrono::seconds(20),true));CHECK(cache.begin((sunday-std::chrono::hours(12))+std::chrono::seconds(61),true));cache.fail("offline");CHECK(cache.available&&cache.entries.size()==3&&!cache.error.empty());CHECK(!cache.begin((sunday-std::chrono::hours(12))+std::chrono::seconds(62),true));
CHECK(cache.begin((sunday-std::chrono::hours(12))+std::chrono::days(1),false));CHECK(!cache.available&&cache.entries.empty());
Batch today(monday,monday+std::chrono::hours(12));
CHECK(today.append(0,3,3,{make("CVE-2026-3000","2026-09-06T23:59:59Z"),make("CVE-2026-3001","2026-09-07T00:00:00Z"),make("CVE-2026-3002","2026-09-07T12:00:01Z")}));
CHECK(today.entries().size()==1&&today.entries()[0].id=="CVE-2026-3001");
Cache daily;CHECK(daily.begin(monday+std::chrono::hours(12),false));daily.success(today,monday+std::chrono::hours(12));
CHECK(daily.begin(monday+std::chrono::days(1),false));CHECK(!daily.available&&daily.entries.empty());
return failures?1:0;}
