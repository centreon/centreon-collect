/*
** Copyright 2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_SE_QUERY_SPAN_HH
#define CCB_SE_QUERY_SPAN_HH

#include <opentelemetry/trace/provider.h>

CCB_BEGIN()
namespace stats_exporter {
class query_span {
  // std::shared_ptr<Span> _span;
  // std::shared_ptr<Scope> _current_scope;

 public:
  query_span(stats* const parent, const std::string& query) {
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    auto tracer = provider->GetTracer("cbd-mysql-connection");
    auto _span = tracer->StartSpan("Query");
    auto _current_scope = tracer->WithActiveSpan(_span);
    //_span->SetAttribute(
    // query.size() > 50
    //             ? fmt::format("{}...", fmt::string_view(query.data(), 50))
    //             : query;
    //    );
  }
  ~query_span() noexcept {
    //_current_scope.reset();
    //_span->End();
  }
};

}  // namespace stats_exporter

CCB_END()
#endif /* !CCB_SE_QUERY_SPAN_HH */
