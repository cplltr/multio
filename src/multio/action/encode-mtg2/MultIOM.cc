/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include "MultIOM.h"

#include <iostream>

#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"

#include "multio/LibMultio.h"
#include "multio/action/encode-mtg2/Options.h"
#include "multio/config/PathConfiguration.h"
#include "multio/datamod/Glossary.h"
#include "multio/util/MioGribHandle.h"
#include "multio/util/PrecisionTag.h"

namespace multio::action {


std::string multiOMDictKindString(MultiOMDictKind kind) {
    switch (kind) {
        case MultiOMDictKind::Options:
            return "options";
        case MultiOMDictKind::MARS:
            return "mars";
        case MultiOMDictKind::Parametrization:
            return "parametrization";
        case MultiOMDictKind::ReducedGG:
            return "reduced-gg";
        case MultiOMDictKind::RegularLL:
            return "regular-ll";
        case MultiOMDictKind::SH:
            return "sh";
        case MultiOMDictKind::HEALPix:
            return "HEALPix";
        default:
            NOTIMP;
    }
}


std::string multiOMEncoderKindString(MultiOMEncoderKind kind) {
    switch (kind) {
        case MultiOMEncoderKind::Simple:
            return "simple";
        case MultiOMEncoderKind::Cached:
            return "cached";
        default:
            NOTIMP;
    }
}

MultiOMDict::MultiOMDict(MultiOMDictKind kind) : kind_{kind} {
    std::string kindStr = multiOMDictKindString(kind);
    void* dict = NULL;
    if (multio_grib2_dict_create(&dict, kindStr.data()) != 0) {
        throw EncodeMtg2Exception(std::string("Can not create dict kind ") + kindStr, Here());
    }

    if (kind == MultiOMDictKind::Options) {
        ASSERT(multio_grib2_init_options(&dict) == 0);
    }
    dict_.reset(static_cast<ForeignDictType*>(dict));
}

void MultiOMDict::toYAML(const std::string& file) {
    multio_grib2_dict_to_yaml(get(), "stdout");
}

void MultiOMDict::set(const char* key, const char* val) {
    if (multio_grib2_dict_set(get(), key, val) != 0) {
        throw EncodeMtg2Exception(
            std::string("Can not set key ") + std::string(key) + std::string(" with value ") + std::string(val),
            Here());
    }
}

void MultiOMDict::set(const std::string& key, const std::string& val) {
    set(key.c_str(), val.c_str());
}

void MultiOMDict::set_geometry(MultiOMDict&& geom) {
    ASSERT(kind_ == MultiOMDictKind::Parametrization);
    switch (geom.kind_) {
        case MultiOMDictKind::HEALPix:
        case MultiOMDictKind::ReducedGG:
        case MultiOMDictKind::RegularLL:
        case MultiOMDictKind::SH:
            geom_ = std::make_unique<MultiOMDict>(std::move(geom));
            ASSERT(multio_grib2_dict_set_geometry(get(), geom_->get()) == 0);
            break;
        default:
            throw EncodeMtg2Exception("Passed dict is not a geometry dict", Here());
    }
}


void MultiOMDict::set(const std::string& key, std::int64_t val) {
    if (multio_grib2_dict_set_int64(get(), key.c_str(), val) != 0) {
        throw EncodeMtg2Exception(std::string("Can not set key ") + std::string(key) + std::string(" with int64 value ")
                                      + std::to_string(val),
                                  Here());
    }
}
void MultiOMDict::set(const std::string& key, bool val) {
    set(key, (std::int64_t)val);
}
void MultiOMDict::set(const std::string& key, double val) {
    if (multio_grib2_dict_set_double(get(), key.c_str(), val) != 0) {
        throw EncodeMtg2Exception(std::string("Can not set key ") + std::string(key)
                                      + std::string(" with double value ") + std::to_string(val),
                                  Here());
    }
}
void MultiOMDict::set(const std::string& key, const std::int64_t* val, std::size_t len) {
    if (multio_grib2_dict_set_int64_array(get(), key.c_str(), val, len) != 0) {
        throw EncodeMtg2Exception(std::string("Can not set key ") + std::string(key) + std::string(" with int64 array"),
                                  Here());
    }
}
void MultiOMDict::set(const std::string& key, const double* val, std::size_t len) {
    if (multio_grib2_dict_set_double_array(get(), key.c_str(), val, len) != 0) {
        throw EncodeMtg2Exception(
            std::string("Can not set key ") + std::string(key) + std::string(" with double array"), Here());
    }
}
void MultiOMDict::set(const std::string& key, const std::vector<std::int64_t>& val) {
    set(key, val.data(), val.size());
}
void MultiOMDict::set(const std::string& key, const std::vector<double>& val) {
    set(key, val.data(), val.size());
}


void* MultiOMDict::get() {
    return static_cast<void*>(dict_.get());
}


MultiOMRawEncoder::MultiOMRawEncoder(MultiOMEncoderKind kind, MultiOMDict& options, MultiOMDict& mars) : kind_{kind} {
    std::string kindStr = multiOMEncoderKindString(kind);
    void* encoder = NULL;
    // @Mirco TO BE DONE
    // if (multio_grib2_encoder_create(&encoder, kindStr.data()) != 0) {
    //     throw EncodeMtg2Exception(std::string("Can not create encoder kind ") + kindStr, Here());
    // }

    encoder_.reset(static_cast<ForeignEncoderType*>(encoder));
}

void* MultiOMRawEncoder::get() {
    return static_cast<void*>(encoder_.get());
}


MultiOMEncoder MultiOMEncoder::make(const EncodeOptionsKeyValueSet& opts, const ComponentConfiguration& conf) {
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
    return MultiOMEncoder(optDict);
}


MultiOMEncoder::MultiOMEncoder(MultiOMDict& options) {
    ASSERT(multio_grib2_encoder_open(options.get(), &encoder_) == 0);
}

std::unique_ptr<codes_handle> MultiOMEncoder::encode(MultiOMDict& mars, MultiOMDict& par, const double* data,
                                                     std::size_t len) {
    codes_handle* rawOutputCodesHandle = nullptr;
    ASSERT(multio_grib2_encoder_encode64(encoder_, mars.get(), par.get(), data, len, (void**)&rawOutputCodesHandle)
           == 0);
    return std::unique_ptr<codes_handle>{rawOutputCodesHandle};
}

std::unique_ptr<codes_handle> MultiOMEncoder::encode(MultiOMDict& mars, MultiOMDict& par, const float* data,
                                                     std::size_t len) {
    codes_handle* rawOutputCodesHandle = nullptr;
    ASSERT(multio_grib2_encoder_encode32(encoder_, mars.get(), par.get(), data, len, (void**)&rawOutputCodesHandle)
           == 0);
    return std::unique_ptr<codes_handle>{rawOutputCodesHandle};
}


MultiOMEncoder::~MultiOMEncoder() {
    ASSERT(multio_grib2_encoder_close(&encoder_) == 0);
}


}  // namespace multio::action
