/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

/// @author Philipp Geier

/// @date Oct 2025

#pragma once


#include <optional>
#include <string>

#include "multio/action/Action.h"
#include "multio/datamod/ContainerInterop.h"
#include "multio/datamod/DataModelling.h"

#include "eckit/exception/Exceptions.h"
#include "multio/LibMultio.h"


namespace multio {

namespace action {
enum class EncodeOptions : std::uint64_t
{
    KnowledgeRoot,
    SamplesPath,
    EncodingRules,
    MappingRules,
    GeoFromAtlas
};
}

namespace datamod {

using action::EncodeOptions;

MULTIO_KEY_SET_DESCRIPTION(EncodeOptions,                                                                       //
                           "encode-mtg2",                                                                       //
                                                                                                                //
                           KeyDef<EncodeOptions::KnowledgeRoot, std::string>{"knowledge-root"}.tagDefaulted(),  //
                           KeyDef<EncodeOptions::SamplesPath, std::string>{"samples-path"}.tagDefaulted(),      //
                           KeyDef<EncodeOptions::EncodingRules, std::string>{"encoding-rules"}.tagDefaulted(),  //
                           KeyDef<EncodeOptions::MappingRules, std::string>{"mapping-rules"}.tagDefaulted(),    //
                           KeyDef<EncodeOptions::GeoFromAtlas, bool>{"geo-from-atlas"}.withDefault(false))


template <>
struct KeySetAlter<KeySet<EncodeOptions>> {
    static void alter(KeyValueSet<KeySet<EncodeOptions>>& opts) {
        using namespace datamod;
        const auto& root = key<EncodeOptions::KnowledgeRoot>(opts)
                               .withDefault([]() { return multio::LibMultio::instance().libraryHome(); })
                               .get();

        key<EncodeOptions::SamplesPath>(opts).withDefault([&]() { return root + "/share/multiom/samples"; });
        key<EncodeOptions::MappingRules>(opts).withDefault(
            [&]() { return root + "/share/multiom/mappings/mapping-rules.yaml"; });
        key<EncodeOptions::EncodingRules>(opts).withDefault(
            [&]() { return root + "/share/multiom/encodings/encoding-rules.yaml"; });

        acquire(opts);
    }
};

};  // namespace datamod


namespace action {

using EncodeOptionsKeySet = datamod::KeySet<EncodeOptions>;
using EncodeOptionsKeyValueSet = datamod::KeyValueSet<EncodeOptionsKeySet>;

//---------------------------------------------------------------------------------------------------------------------

class EncodeMtg2Exception : public eckit::Exception {
public:
    EncodeMtg2Exception(const std::string& reason, const eckit::CodeLocation& location = eckit::CodeLocation());
};

//---------------------------------------------------------------------------------------------------------------------


}  // namespace action
}  // namespace multio
