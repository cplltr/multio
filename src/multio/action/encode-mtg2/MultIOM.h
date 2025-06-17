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


#include <memory>
#include <string>

#include "eccodes.h"
#include "metkit/codes/CodesHandleDeleter.h"
#include "multio/action/encode-mtg2/Options.h"
#include "multiom/api/c/api.h"

namespace multio::action {
struct ForeignDictType;
}

template <>
class std::default_delete<multio::action::ForeignDictType> {
public:
    void operator()(multio::action::ForeignDictType* ptr) const {
        void* p = static_cast<void*>(ptr);
        ASSERT(multio_grib2_dict_destroy(&p) == 0);
    }
};


namespace multio::action {
struct ForeignEncoderType;
}

template <>
class std::default_delete<multio::action::ForeignEncoderType> {
public:
    void operator()(multio::action::ForeignEncoderType* ptr) const {
        void* p = static_cast<void*>(ptr);
        // TODO uncomment with new function
        // ASSERT(multio_grib2_encoder_close(&p) == 0);
    }
};


namespace multio::action {

enum class MultiOMEncoderKind : unsigned long
{
    Simple,
    Cached,
};

std::string multiOMEncoderKindString(MultiOMEncoderKind kind);

enum class MultiOMDictKind : unsigned long
{
    Options,
    MARS,
    Parametrization,
    // Geometry dicts
    ReducedGG,
    RegularLL,
    SH,
    HEALPix,
};

std::string multiOMDictKindString(MultiOMDictKind kind);

struct MultiOMDict {
    MultiOMDict(MultiOMDictKind kind);
    ~MultiOMDict() = default;

    MultiOMDict(MultiOMDict&&) noexcept = default;
    MultiOMDict& operator=(MultiOMDict&&) noexcept = default;

    void toYAML(const std::string& file = "stdout");

    void set(const char* key, const char* val);
    void set(const std::string& key, const std::string& val);

    // Typed setters
    void set(const std::string& key, std::int64_t val);
    void set(const std::string& key, double val);
    void set(const std::string& key, bool val);
    void set(const std::string& key, const std::int64_t* val, std::size_t len);
    void set(const std::string& key, const double* val, std::size_t len);
    void set(const std::string& key, const std::vector<std::int64_t>& val);
    void set(const std::string& key, const std::vector<double>& val);

    // Set geoemtry on parametrization
    void set_geometry(MultiOMDict&& geom);

    void* get();

    MultiOMDictKind kind_;
    std::unique_ptr<ForeignDictType> dict_;
    std::unique_ptr<MultiOMDict> geom_;
};

}  // namespace multio::action


namespace multio {

template <>
struct datamod::KeyValueWriter<action::MultiOMDict> {
    template <typename KVD, typename KV_,
              std::enable_if_t<(IsKeyValueDescription_v<std::decay_t<KVD>> && IsKeyValue_v<std::decay_t<KV_>>), bool>
              = true>
    static void set(const KVD& kvd, KV_&& kv, action::MultiOMDict& md) {
        using KV = std::decay_t<KV_>;
        std::forward<KV_>(kv).visit(eckit::Overloaded{
            [&](MissingValue v) {},
            [&](auto&& v) {
                md.set(kvd.key, KVD::template write<action::MultiOMDict>(std::forward<decltype(v)>(v)));
            }});
    }
};

}  // namespace multio


namespace multio::action {

struct MultiOMEncoder {
    MultiOMEncoder(MultiOMDict& options);

    std::unique_ptr<codes_handle> encode(MultiOMDict& mars, MultiOMDict& par, const double* data, std::size_t len);
    std::unique_ptr<codes_handle> encode(MultiOMDict& mars, MultiOMDict& par, const float* data, std::size_t len);

    ~MultiOMEncoder();

    void* encoder_ = nullptr;

    static MultiOMEncoder make(const EncodeOptionsKeyValueSet& opts, const ComponentConfiguration& conf);
};


// New encoder for caching
struct MultiOMRawEncoder {
    MultiOMRawEncoder(MultiOMEncoderKind kind, MultiOMDict& options, MultiOMDict& mars);
    ~MultiOMRawEncoder() = default;

    MultiOMRawEncoder(MultiOMRawEncoder&&) noexcept = default;
    MultiOMRawEncoder& operator=(MultiOMRawEncoder&&) noexcept = default;

    void* get();

    MultiOMEncoderKind kind_;
    std::unique_ptr<ForeignEncoderType> encoder_;
};


//---------------------------------------------------------------------------------------------------------------------


}  // namespace multio::action
