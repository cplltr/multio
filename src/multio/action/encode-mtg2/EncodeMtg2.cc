/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include "EncodeMtg2.h"

#include <iostream>

#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"

#include "multio/LibMultio.h"
#include "multio/action/encode-mtg2/AtlasGeoSetter.h"
#include "multio/action/encode-mtg2/EncoderCache.h"
#include "multio/action/encode-mtg2/MultIOM.h"
#include "multio/action/encode-mtg2/Options.h"
#include "multio/config/PathConfiguration.h"
#include "multio/datamod/Glossary.h"
#include "multio/message/Parametrization.h"
#include "multio/util/MioGribHandle.h"
#include "multio/util/PrecisionTag.h"

namespace multio::action {

using message::Message;
using message::Peer;


EncodeMtg2::EncodeMtg2(const ComponentConfiguration& compConf) :
    ChainedAction{compConf},
    options_{datamod::read(EncodeOptionsKeySet{}, compConf.parsedConfig())},
    encoder_{MultiOMEncoder::make(options_, compConf)},
    cache_{EncoderCache::make(options_, compConf)} {}


void EncodeMtg2::executeImpl(Message msg) {
    if (msg.tag() != Message::Tag::Field) {
        executeNext(std::move(msg));
        return;
    }

    auto& md = msg.metadata();


    // TODO MIVAL : to be removed
    // std::cout << "Encoding message with metadata: " << md << std::endl;

    // TO encoding
    MultiOMDict mars{MultiOMDictKind::MARS};
    MultiOMDict par{MultiOMDictKind::Parametrization};

    {
        using namespace datamod;
        // Read and set unscoped mars keys
        auto marsKeys = read(keySet<MarsKeys>().unscoped(), md);
        write(marsKeys, mars);

        // Read scoped misc keys
        auto miscKeys = read(keySet<MiscKeys>().scoped(), md);
        // Write unscoped misc keys
        miscKeys.keySet.unscoped();
        write(miscKeys, par);

        // Handle geometry
        withScopedGeometryKeySet(marsKeys, [&](GridType gridType, std::string scope, auto geoKeySet) {
            const auto& grid = key<MarsKeys::GRID>(marsKeys);
            if (!grid.isMissing()) {
                const auto& global = message::Parametrization::instance().get();
                const auto& geoFromAtlas = key<EncodeOptions::GeoFromAtlas>(options_);
                if (geoFromAtlas.get() && (global.find(scope) == global.end())) {
                    extract::AtlasGeoSetter::handleGrid(scope, grid.get());
                }
            }

            MultiOMDict geom{([&]() {
                switch (gridType) {
                    case GridType::GG:
                        return MultiOMDictKind::ReducedGG;
                    case GridType::HEALPix:
                        return MultiOMDictKind::HEALPix;
                    case GridType::LL:
                        return MultiOMDictKind::RegularLL;
                    case GridType::SH:
                        return MultiOMDictKind::SH;
                }
                throw EncodeMtg2Exception("unkown gridType", Here());
            })()};

            auto geoKeys = read(geoKeySet, md);
            write(geoKeys.unscoped(), geom);
            par.set_geometry(std::move(geom));
        });


        // @Mirco here we get the cached raw encoder
        MultiOMRawEncoder& rawEncoder = cache_.getEncoder(marsKeys, mars);

        auto& payload = msg.payload();

        executeNext(dispatchPrecisionTag(msg.precision(), [&](auto pt) {
            using Precision = typename decltype(pt)::type;

            // @Mirco here we would call the rawEncoder
            auto rawGrib2Handle = encoder_.encode(mars, par, static_cast<const Precision*>(payload.data()),
                                                  payload.size() / sizeof(Precision));

            // Create non-owning grib handle by passing by reference
            util::MioGribHandle gribHandle{*rawGrib2Handle.get()};

            // Initialize buffer with length
            eckit::Buffer buf{gribHandle.length()};
            gribHandle.write(buf);


            return Message{Message::Header{Message::Tag::Field, Peer{msg.source().group()}, Peer{msg.destination()}},
                           std::move(buf)};
        }));
    }

    // TODO MIVAL : to be removed
    // std::cout << "Exit encoding with metadata: " << md << std::endl;
}

void EncodeMtg2::print(std::ostream& os) const {
    os << "EncodeMtg2(";
    os << "options=" << options_;
    os << ")";
}

static ActionBuilder<EncodeMtg2> EncodeMtg2Builder("encode-mtg2");

}  // namespace multio::action
