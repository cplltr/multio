/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

#include "multio/action/encode-mtg2/EncoderCache.h"
#include "multio/action/encode-mtg2/Options.h"
#include "multio/datamod/MarsMiscGeo.h"

namespace multio::action {

EncoderCache EncoderCache::make(const EncodeOptionsKeyValueSet& opts, const ComponentConfiguration& conf) {
    MultiOMDict optDict(MultiOMDictKind::Options);

    using namespace datamod;

    // Select a subset of the options (excluding knowledge-root) and set them to the opt dict
    write(read(keySet<EncodeOptions::SamplesPath, EncodeOptions::MappingRules, EncodeOptions::EncodingRules>(), opts),
          optDict);

    // TODO -- this hack will just be removed through
    const auto& knowledgeRoot = key<EncodeOptions::KnowledgeRoot>(opts);
    if (!knowledgeRoot.isMissing()) {
        setenv("IFS_INSTALL_DIR", knowledgeRoot.get().c_str(), 0);
    }


    // TODO read kind from options???
    return EncoderCache(MultiOMEncoderKind::Cached, std::move(optDict));
}

EncoderCache::EncoderCache(MultiOMEncoderKind kind, MultiOMDict&& options) :
    kind_{kind}, options_{std::move(options)} {}

MultiOMRawEncoder& EncoderCache::getEncoder(const datamod::MarsKeyValueSet& marsKeys, MultiOMDict& mars) {
    using namespace multio::datamod;
    // Select caching keys and prehash
    PrehashedMarsKeys cacheKeySet = read(EncoderCacheMarsKeySet{}, marsKeys);

    if (auto search = cache_.find(cacheKeySet); search != cache_.end()) {
        return search->second;
    }

    return cache_.emplace(std::move(cacheKeySet), MultiOMRawEncoder(kind_, options_, mars)).first->second;
}

}  // namespace multio::action
