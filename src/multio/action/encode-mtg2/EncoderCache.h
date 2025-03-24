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


#include "multio/action/encode-mtg2/MultIOM.h"
#include "multio/action/encode-mtg2/Options.h"
#include "multio/datamod/ContainerInterop.h"
#include "multio/datamod/MarsMiscGeo.h"
#include "multio/util/PrehashedKey.h"


namespace multio::action {

using PrehashedMarsKeys = util::PrehashedKey<datamod::EncoderCacheMarsKeyValueSet>;

class EncoderCache {
public:
    EncoderCache(MultiOMEncoderKind kind, MultiOMDict&& options);
    MultiOMRawEncoder& getEncoder(const datamod::MarsKeyValueSet& marsKeys, MultiOMDict& marsDict);

    static EncoderCache make(const EncodeOptionsKeyValueSet& opts, const ComponentConfiguration& conf);

private:
    MultiOMEncoderKind kind_;
    MultiOMDict options_;
    std::unordered_map<PrehashedMarsKeys, MultiOMRawEncoder> cache_{};
};


//---------------------------------------------------------------------------------------------------------------------


}  // namespace multio::action
