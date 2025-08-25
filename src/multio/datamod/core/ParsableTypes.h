/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#pragma once

#include "multio/util/TypeTraits.h"

// Additionally to `TypeParser` and `ParseType<>`, there's a template `ParsableTypes` that can be specialized to explicitly
// list readable types for interfacing (used for python usually) -- that`s because we can not generate a definite list
// of all supported convertible types from all overloads of `ParseType<>`.

namespace multio::datamod {

//=============================================================================

template <typename T, class = void>
struct HasParsableTypes : std::false_type {};

template <typename T>
struct HasParsableTypes<T, std::void_t<typename T::ParsableTypes>> : std::true_type {};

template <typename T>
inline constexpr bool HasParsableTypes_v = HasParsableTypes<T>::value;


// Support on interfacing -- allows specializing specific types
template <typename T>
struct ParsableTypes {
    using type = util::TypeList<T>;
};

template <typename T>
using ParsableTypes_t = typename ParsableTypes<T>::type;


// Accessor to acces by unified type definition `ParsableTypes`
template <typename T>
struct GetParsableTypes {
    using ParsableTypes = ParsableTypes_t<T>;
};


//=============================================================================

}  // namespace multio::datamod
