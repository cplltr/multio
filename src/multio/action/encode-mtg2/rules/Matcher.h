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

#include "multio/action/encode-mtg2/EncodeMtg2Exception.h"
#include "multio/datamod/ContainerInterop.h"
#include "multio/datamod/DataModelling.h"
#include "multio/datamod/MarsMiscGeo.h"

#include <functional>
#include <tuple>
#include <unordered_set>


namespace multio::action::rules {

// Check if the value of a field matches any of the set of values given
// Returns false if the field is not given
template <auto Id_>
struct OneOf {
    using ValueType = datamod::KeyDefValueType_t<Id_>;

    std::unordered_set<ValueType> values;

    bool operator()(const datamod::KeyValueSet<datamod::KeySet<decltype(Id_)>>& keys) const {
        const auto& kv = datamod::key<Id_>(keys);

        return (kv.has() && (values.find(kv.get()) != values.end()));
    }
};

// Check if the value of a field is not in a given exclusion list
// Returns false if the field is not given
template <auto Id_>
struct NoneOf {
    using ValueType = datamod::KeyDefValueType_t<Id_>;

    std::unordered_set<ValueType> values;

    bool operator()(const datamod::KeyValueSet<datamod::KeySet<decltype(Id_)>>& keys) const {
        const auto& kv = datamod::key<Id_>(keys);

        return (kv.has() && (values.find(kv.get()) == values.end()));
    }
};

// Checks if a field is given
template <auto Id_>
struct Has {
    bool operator()(const datamod::KeyValueSet<datamod::KeySet<decltype(Id_)>>& keys) const {
        const auto& kv = datamod::key<Id_>(keys);
        return kv.has();
    }
};


// Checks if a field is missing
template <auto Id_>
struct Missing {
    bool operator()(const datamod::KeyValueSet<datamod::KeySet<decltype(Id_)>>& keys) const {
        const auto& kv = datamod::key<Id_>(keys);
        return kv.isMissing();
    }
};


// Match a binary operation like >, >=, <, <=
template <auto Id_, typename OpFunctor>
struct MatchOp {
    using ValueType = datamod::KeyDefValueType_t<Id_>;

    ValueType value;

    bool operator()(const datamod::KeyValueSet<datamod::KeySet<decltype(Id_)>>& keys) const {
        const auto& kv = datamod::key<Id_>(keys);

        return (kv.has() && (OpFunctor{}(kv.get(), value)));
    }
};

template <auto Id_>
using GreaterThan = MatchOp<Id_, std::greater<>>;
template <auto Id_>
using GreaterEqual = MatchOp<Id_, std::greater_equal<>>;
template <auto Id_>
using LessThan = MatchOp<Id_, std::less<>>;
template <auto Id_>
using LessEqual = MatchOp<Id_, std::less_equal<>>;


// Compose a set of matchers with a fold AND expression
template <typename... Matchers>
struct All {
    std::tuple<Matchers...> matchers;

    template <typename KVSet, std::enable_if_t<datamod::IsKeyValueSet_v<std::decay_t<KVSet>>, bool> = true>
    bool operator()(const KVSet& keys) const {
        return std::apply([&](const auto&... mx) { return (mx(keys) && ... && true); }, matchers);
    }
};

template <typename... Matchers>
All(Matchers&&... matchers) -> All<std::decay_t<Matchers>...>;


// Compose a set of matchers with a fold OR expression
template <typename... Matchers>
struct Any {
    std::tuple<Matchers...> matchers;

    template <typename KVSet, std::enable_if_t<datamod::IsKeyValueSet_v<std::decay_t<KVSet>>, bool> = true>
    bool operator()(const KVSet& keys) const {
        return std::apply([&](const auto&... mx) { return (mx(keys) || ... || false); }, matchers);
    }
};

template <typename... Matchers>
Any(Matchers&&... matchers) -> Any<std::decay_t<Matchers>...>;


}  // namespace multio::action::rules

