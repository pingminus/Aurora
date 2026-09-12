#include "cve_service.h"
#include "include/cef_parser.h"
#include "include/cef_task.h"
#include <functional>
#include <cmath>
namespace aurora {
namespace {
// CEF constructs a fresh C++ request wrapper for callbacks. Identity is the
// generation captured by this dedicated client, never a wrapper address.
class PageClient final : public CefURLRequestClient {
 public:
  PageClient(CefRefPtr<CveService> owner, uint64_t token) : owner_(owner), token_(token) {}
  void OnRequestComplete(CefRefPtr<CefURLRequest> request) override { owner_->complete(token_, request); }
  void OnDownloadData(CefRefPtr<CefURLRequest> request, const void* data, size_t size) override { owner_->receive(token_, request, data, size); }
  void OnUploadProgress(CefRefPtr<CefURLRequest>, int64_t, int64_t) override {}
  void OnDownloadProgress(CefRefPtr<CefURLRequest>, int64_t, int64_t) override {}
  bool GetAuthCredentials(bool, const CefString&, int, const CefString&, const CefString&, CefRefPtr<CefAuthCallback>) override { return false; }
 private:
  CefRefPtr<CveService> owner_;
  const uint64_t token_;
  IMPLEMENT_REFCOUNTING(PageClient);
};
class FeedTask final : public CefTask {
 public:
 explicit FeedTask(std::function<void()> run) : run_(std::move(run)) {}
 void Execute() override { run_(); }
 private:
 std::function<void()> run_;
 IMPLEMENT_REFCOUNTING(FeedTask);
};
CefRefPtr<CefTask> task(std::function<void()> run) { return new FeedTask(std::move(run)); }
cve::Time now() { return std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()); }
std::optional<std::string> string_field(CefRefPtr<CefDictionaryValue> d,const char* key,size_t limit) {
  if(!d||d->GetType(key)!=VTYPE_STRING)return {};
  auto s=d->GetString(key).ToString();if(s.size()>limit)return {};return s;
}
std::optional<std::vector<cve::Entry>> parse_entries(CefRefPtr<CefListValue> list) {
  if(!list||list->GetSize()>500)return {};
  std::vector<cve::Entry> entries;
  for(size_t i=0;i<list->GetSize();++i){
    if(list->GetType(i)!=VTYPE_DICTIONARY)return {};
    auto wrapper=list->GetDictionary(i);auto d=wrapper->GetDictionary("cve");if(!d)return {};
    auto status=string_field(d,"vulnStatus",100); if(!status)return {};
    if(*status=="Rejected"||*status=="REJECT")continue;
    auto id=string_field(d,"id",32),pub=string_field(d,"published",24),mod=string_field(d,"lastModified",24);
    if(!id||!pub||!mod)return {};
    auto descriptions=d->GetList("descriptions");if(!descriptions||descriptions->GetSize()>100)return {};
    std::string description;
    for(size_t j=0;j<descriptions->GetSize();++j){auto text=descriptions->GetDictionary(j);if(!text)return {};auto lang=string_field(text,"lang",16),value=string_field(text,"value",32768);if(!lang||!value)return {};if(*lang=="en"&&description.empty())description=*value;}
    if(description.empty())description="No English description is available.";
    if(description.size()>600){description.resize(600);while(!description.empty()&&(static_cast<unsigned char>(description.back())&0xc0)==0x80)description.pop_back();if(!description.empty()&&static_cast<unsigned char>(description.back())>=0xc0)description.pop_back();description+="…";}
    std::vector<cve::Metric> metrics;
    auto metric_dict=d->GetDictionary("metrics");
    if(d->HasKey("metrics")&&!metric_dict)return {};
    if(metric_dict)for(const char* key:{"cvssMetricV40","cvssMetricV31","cvssMetricV30","cvssMetricV2"}){
      if(!metric_dict->HasKey(key))continue;
      auto values=metric_dict->GetList(key);if(!values||values->GetSize()>100)return {};
      for(size_t j=0;j<values->GetSize();++j){auto value=values->GetDictionary(j);if(!value)return {};auto data=value->GetDictionary("cvssData");if(!data)return {};
        auto version=string_field(data,"version",8),source=string_field(value,"source",256),type=string_field(value,"type",32);
        if(!version||!source||!type||(data->GetType("baseScore")!=VTYPE_DOUBLE&&data->GetType("baseScore")!=VTYPE_INT))return {};
        double score=data->GetType("baseScore")==VTYPE_INT?static_cast<double>(data->GetInt("baseScore")):data->GetDouble("baseScore");if(!std::isfinite(score)||score<0||score>10)return {};
        metrics.push_back({*version,*source,*type,score});
      }
    }
    entries.push_back({*id,description,*pub,*mod,cve::select_metric(metrics)});
  }return entries;
}
}
void CveService::poll(bool refresh){CefPostTask(TID_FILE_BACKGROUND,task([self=CefRefPtr<CveService>(this),refresh]{self->begin(refresh);}));}
std::string CveService::snapshot(){std::lock_guard lock(mutex_);return snapshot_;}
void CveService::shutdown(){CefPostTask(TID_FILE_BACKGROUND,task([self=CefRefPtr<CveService>(this)]{self->stopped_=true;++self->generation_;auto request=self->request_;self->request_=nullptr;if(request)request->Cancel();}));}
void CveService::begin(bool refresh){if(stopped_)return;if(!cache_.begin(now(),refresh)){publish();return;}batch_.emplace(cve::day_start(cache_.attempted),cache_.attempted);publish();fetch();}
void CveService::fetch(){
  if(stopped_||!cache_.loading)return;
  body_.clear();const auto token=++generation_;
  auto request=CefRequest::Create();
  auto start=cve::format_time(cve::day_start(cache_.attempted)),end=cve::format_time(cache_.attempted);
  request->SetURL("https://services.nvd.nist.gov/rest/json/cves/2.0?noRejected&resultsPerPage=500&startIndex="+std::to_string(batch_->next())+"&pubStartDate="+start+"&pubEndDate="+end);
  request->SetMethod("GET");request->SetFlags(UR_FLAG_SKIP_CACHE | UR_FLAG_STOP_ON_REDIRECT);
  request->SetHeaderByName("Accept","application/json",true);
  request_=CefURLRequest::Create(request,new PageClient(this,token),nullptr);
  if(!request_){fail("Could not start the NVD request. Retry after one minute.");return;}
  CefPostDelayedTask(TID_FILE_BACKGROUND,task([self=CefRefPtr<CveService>(this),token]{if(self->generation_==token&&self->request_){auto request=self->request_;self->request_=nullptr;++self->generation_;self->fail("NVD request timed out. Retry after one minute.");request->Cancel();}}),30000);
}
void CveService::receive(uint64_t token,CefRefPtr<CefURLRequest> request,const void* data,size_t size){
  if(!request_||token!=generation_||stopped_)return;
  if(size>8*1024*1024-body_.size()){request_=nullptr;++generation_;fail("NVD response exceeded the safe size limit. Results are incomplete.");request->Cancel();return;}
  body_.append(static_cast<const char*>(data),size);
}
void CveService::fail(const std::string& error){cache_.fail(error);cache_.attempted=now();body_.clear();batch_.reset();publish();}
void CveService::complete(uint64_t token,CefRefPtr<CefURLRequest> request){
  if(stopped_||!request_||token!=generation_)return;
  request_=nullptr;++generation_;
  auto response=request->GetResponse();
  if(request->GetRequestStatus()!=UR_SUCCESS||!response||response->GetStatus()!=200){
    const int status=response?response->GetStatus():0;
    fail(status==429||status==403?"NVD access was rate-limited or denied. Retry after one minute.":"NVD could not be reached. Results were not refreshed; retry after one minute.");return;
  }
  auto value=CefParseJSON(body_,JSON_PARSER_RFC);body_.clear();
  auto root=value&&value->GetType()==VTYPE_DICTIONARY?value->GetDictionary():nullptr;
  if(!root||root->GetType("startIndex")!=VTYPE_INT||root->GetType("totalResults")!=VTYPE_INT||root->GetType("resultsPerPage")!=VTYPE_INT){fail("NVD returned an invalid page. No partial results were published.");return;}
  auto list=root->GetList("vulnerabilities");auto entries=parse_entries(list);
  if(!entries||root->GetInt("resultsPerPage")!=static_cast<int>(list->GetSize())||!batch_->append(root->GetInt("startIndex"),root->GetInt("totalResults"),static_cast<int>(list->GetSize()),std::move(*entries))){fail("NVD returned inconsistent, malformed or oversized results. No partial results were published.");return;}
  if(batch_->complete()){cache_.success(*batch_,now());batch_.reset();publish();return;}
  publish();CefPostDelayedTask(TID_FILE_BACKGROUND,task([self=CefRefPtr<CveService>(this)]{self->fetch();}),7000);
}
void CveService::publish(){
  auto d=CefDictionaryValue::Create();d->SetInt("version",1);
  const auto time=now(); const bool stale=cache_.available&&(!cache_.error.empty()||time-cache_.fetched>=std::chrono::minutes(15)||cache_.start!=cve::day_start(time));
  d->SetString("status",cache_.loading?"loading":stale?"stale":!cache_.error.empty()?"error":cache_.available?"ready":"loading");
  d->SetString("message",cache_.error.empty()?(cache_.loading?"Fetching every page from NVD…":""):cache_.error);
  d->SetString("start",cve::format_time(cache_.available?cache_.start:cve::day_start(time)));
  d->SetString("end",cve::format_time(cache_.available?cache_.end:cache_.attempted));
  d->SetString("fetched",cache_.available?cve::format_time(cache_.fetched):"");
  auto list=CefListValue::Create();size_t i=0;
  for(const auto& entry:cache_.entries){auto item=CefDictionaryValue::Create();item->SetString("id",entry.id);item->SetString("description",entry.description);item->SetString("published",entry.published);item->SetString("modified",entry.modified);
    if(entry.metric){item->SetDouble("score",entry.metric->score);item->SetString("cvssVersion",entry.metric->version);item->SetString("severity",cve::severity(*entry.metric));item->SetString("assessmentSource",entry.metric->source);}else{item->SetNull("score");item->SetString("cvssVersion","");item->SetString("severity","Unscored");item->SetString("assessmentSource","");}
    list->SetDictionary(i++,item);
  }d->SetList("entries",list);auto value=CefValue::Create();value->SetDictionary(d);auto json=CefWriteJSON(value,JSON_WRITER_DEFAULT).ToString();std::lock_guard lock(mutex_);snapshot_=std::move(json);
}
}
