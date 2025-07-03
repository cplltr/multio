/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */


#pragma once

#include "multio/action/encode-mtg2/EncoderConf.h"
#include "multio/datamod/DataModelling.h"


namespace multio::action::rules {

// First id_ is the key to be set. idx is the path to it
struct NoOp {
    void operator()(EncoderSections& conf) const {}
};


// First id_ is the key to be set. idx is the path to it
template <auto id_, auto... idx>
struct SetKey {
    datamod::KeyValue<id_> value;

    void operator()(EncoderSections& conf) const {
        auto& k = datamod::alteredKeyPath<idx..., id_>(conf);
        k = value;
        alter(k);
    }
};


template <typename... Setters>
struct SetAll {
    std::tuple<Setters...> setters;

    void operator()(EncoderSections& conf) const {
        std::apply([&](const auto&... setter) { (setter(conf), ...); }, setters);
    }
};

template <typename... Setters>
auto setAll(Setters&&... setters) {
    return SetAll<std::decay_t<Setters>...>{std::make_tuple(std::forward<Setters>(setters)...)};
}


}  // namespace multio::action::rules

