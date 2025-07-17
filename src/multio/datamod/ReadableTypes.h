/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#pragma once

#include "multio/util/TypeTraits.h"

// Additionally to `Reader` and `ReadSpec<>`, there's a template `ReadableTypes` that can be specialized to explicitly
// list readable types for interfacing (used for python usually) -- that`s because we can not generate a definite list
// of all supported convertible types from all overloads of `ReadSpec<>`.

namespace multio::datamod {

//=============================================================================

template <typename T, class = void>
struct HasReadableTypes : std::false_type {};

template <typename T>
struct HasReadableTypes<T, std::void_t<typename T::ReadableTypes>> : std::true_type {};

template <typename T>
inline constexpr bool HasReadableTypes_v = HasReadableTypes<T>::value;




// Support on interfacing -- allows specializing specific types
template <typename T>
struct ReadableTypes {
    using type = util::TypeList<T>;
};

template <typename T>
using ReadableTypes_t = typename ReadableTypes<T>::type;


// Accessor to acces by unified type definition `ReadableTypes`
template <typename T>
struct GetReadableTypes {
    using ReadableTypes = ReadableTypes_t<T>;
};


//=============================================================================

}  // namespace multio::datamod
