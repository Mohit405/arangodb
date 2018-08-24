////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "QueryRegistryFeature.h"

#include "Aql/Query.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

aql::QueryRegistry* QueryRegistryFeature::QUERY_REGISTRY = nullptr;

QueryRegistryFeature::QueryRegistryFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "QueryRegistry"),
      _trackSlowQueries(true),
      _trackBindVars(true),
      _failOnWarning(false),
      _queryMemoryLimit(0),
      _maxQueryPlans(128),
      _slowQueryThreshold(10.0),
      _queryCacheMode("off"),
      _queryCacheEntries(128),
      _queryRegistryTTL(DefaultQueryTTL) {
  setOptional(false);
  startsAfter("V8Phase");
}

void QueryRegistryFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("query", "Configure queries");
  
  options->addOldOption("database.query-cache-mode", "query.cache-mode");
  options->addOldOption("database.query-cache-max-results", "query.cache-entries");
  options->addOldOption("database.disable-query-tracking", "query.tracking");

  options->addOption("--query.memory-limit", "memory threshold for AQL queries (in bytes)",
                     new UInt64Parameter(&_queryMemoryLimit));

  options->addOption("--query.tracking", "whether to track slow AQL queries",
                     new BooleanParameter(&_trackSlowQueries));
  
  options->addOption("--query.tracking-with-bindvars", "whether to track bind vars with AQL queries",
                     new BooleanParameter(&_trackBindVars));
  
  options->addOption("--query.fail-on-warning", "whether AQL queries should fail with errors even for recoverable warnings",
                     new BooleanParameter(&_failOnWarning));
  
  options->addOption("--query.slow-threshold", "threshold for slow AQL queries (in seconds)",
                     new DoubleParameter(&_slowQueryThreshold));

  options->addOption("--query.cache-mode",
                     "mode for the AQL query result cache (on, off, demand)",
                     new StringParameter(&_queryCacheMode));

  options->addOption("--query.cache-entries",
                     "maximum number of results in query result cache per database",
                     new UInt64Parameter(&_queryCacheEntries));
  
  options->addOption("--query.optimizer-max-plans", "maximum number of query plans to create for a query",
                     new UInt64Parameter(&_maxQueryPlans));

  options->addHiddenOption("--query.registry-ttl", "Default time-to-live of query snippets (in seconds)",
                           new DoubleParameter(&_queryRegistryTTL));
}

void QueryRegistryFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (_maxQueryPlans == 0) {
    LOG_TOPIC(FATAL, Logger::AQL) << "invalid value for `--query.optimizer-max-plans`. expecting at least 1";
    FATAL_ERROR_EXIT();
  }

  // cap the value somehow. creating this many plans really does not make sense
  _maxQueryPlans = std::min(_maxQueryPlans, decltype(_maxQueryPlans)(1024));
}

void QueryRegistryFeature::prepare() {
  // configure the query cache
  std::pair<std::string, size_t> cacheProperties{_queryCacheMode,
                                                 _queryCacheEntries};
  arangodb::aql::QueryCache::instance()->setProperties(cacheProperties);

  if (_queryRegistryTTL <= 0) {
    _queryRegistryTTL = DefaultQueryTTL;
  }

  // create the query registery
  _queryRegistry.reset(new aql::QueryRegistry(_queryRegistryTTL));
  QUERY_REGISTRY = _queryRegistry.get();
}

void QueryRegistryFeature::start() {}

void QueryRegistryFeature::unprepare() {
  // clear the query registery
  QUERY_REGISTRY = nullptr;
}

} // arangodb
