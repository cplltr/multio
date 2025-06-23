/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

/// @author Philipp Geier

/// @date March 2025

#pragma once

#include <tuple>
#include <type_traits>
#include "eckit/utils/Overloaded.h"
#include "multio/datamod/DataModellingException.h"
#include "multio/datamod/ReaderWriter.h"
#include "multio/util/Hash.h"
#include "multio/util/PrehashedKey.h"
#include "multio/util/TypeTraits.h"

// TLDR:
// Use enum tagged tuples instead of struct/classes to describe product types for incoming metadata model or
// configuration to generate parsers/validators, emitters, printers etc.....
//
// # Why: C++ is lacking compile time reflective features.
//
// MultIO is a lot of glue code that needs to handle various set of keys (data models) for
//   * validating and specifying expected metadata
//   * configuration for a lot of actions
//   * interfacing metadata & configuration to different libraries
//
// *Metadata*: Subset of mars keys, grib specific keys & custom data models to sort things ous - unfortunately even
// after years we can not define a limited set of classes that are reused among packages...
//
// *Configuration*: In the end
// a big YAML file is describing a plan. It should be possible to describe and verify the plan "easily". Often
// configuration contains metadata keys or content for third-party configuration/metadata
//
// Advantage:
//  * The mechanism implies some constraints and requirements on what we need to describe. Then we generate a lot of
//  code.
//  * Exchanging interfacing types (i.e. removing `eckit::LocalConfiguration` of other YAML parser) is simple
//  * Possibility to generate more detailed documentation and python code (i.e. via json-schema)
//  * In future we can to more analysis on metadata requirements on a pipeline of actions
// Disadvantage:
//  * Some error messages can be ugly.
//  * Need to use some of the features here
//
// Personal comment (Philipp Geier): After two years of resisting going that way, I'm bored of writing the same glue
// code again and again.
//   With generated code, validation and error messages are much more consistent.
//   On the long run it should allow focussing more on essential things in the actions (execution & handling state...)
//
//
// # Essentials & How to migrate away from this:
//
// The keysets described through this mechanism already contain the most important things:
//   * keys (stringified)
//   * types
//   * defaults
//   * mappers
//   * initialization
//
// Type mappers & initialization can be natually described through `struct` and `classes` - but may require creating
// more types for specific keys. Defaults can also be implied explicitly in the code (it's just not organized then). For
// interfacing with other containers (metadata, configuration, third-party...) specialized code has to be written,
// mostly because all members need to be iterated or accessed. This is the most errorprone & heavyweighted part when a
// lot of keys are involved, because it involves a lot of repetition. Suggestion: At least provide a `forEach` function
// that iterates all members with a descriptive type.
//
//
// # Content
//
// This code aims to provide a mechanims to effectively describe a set of key value pairs  with
//   * a type
//   * name (string representation)
//   * possible mappers (to do conversions or checks)
//   * tags (if a key is required, defaulted or optional)
//   * scope (for a whole key set)
//
// The advantage of this mechanism is that we get a behaviour similar to complie-time reflexion - meaning we get
//   * en/decoding/parsing/reading & writing calls to interact with other containers
//   * hashing functionality
//   * very few repetition of code (no need to repeat tho whole list of keys in various places)
//   * doc strings as constexpr string_view (which mean they are not compiled into binary unless used - we can split
//   json-schema generating binaries...)
//
// Fundamentally this happens by creating EnumType for KeySets and adressing keys through their enum ID. Sets of keys
// are organized in tuples. Custom key sets can be created by selecting specific keys of different key sets.
//
// The descriptions are created through the type `KeyDefinition` as constexpr and a
// more lightweight type `ScopedKey` which can have modifications on string representation..
// The whole key set for an enum type is described through template specialization of a struct `KeySetDescription`.
// Here the step of keys is described and wrapped into a tuple type, also a default scope name is given (e.g. "mars")
//
// A special type `KeySet` or `CustomKeySet` wraps a tuple of keys and provides scoping mechanims.
//
// The type `KeyValue` allows making a real value from a `KeyDefinition`.
// The type `KeyValueSet` combines a tuple of reified values and a key set - making it a real object with values.
//
// A `KeyValue` can contain a `MissingValue`, the specific `ValueType` or a reference to the value type.
// The creating of a `KeyValue` from a `KeyDefinition` happens throuh a function called `reify` - which defaults
// initiaties all values to missing.
//
// To interact with containers (like Metadata) a `KeyValueReader` and `KeyValueWriter` can be specialized.
// The readers are then accessed through the `read` call. The readers can check if a key is required and alread throw
// exceptions. Otherwise after the read `alterAndValidate` is called which first tryes to apply defaults to specific
// keys, then calls a user provided `alter` function that may do more complex operations, and eventually validates that
// all optional or defaulted keys are given.
//
// By default values are referenced and not copied. To create values, either directly use `readValue` or call `acquire`
// on a `KeyValueSet`.
//
// TBD:
//  * Clean up some functions; reorganize reify, read, write function....


namespace multio::datamod {


//-----------------------------------------------------------------------------
// Helpers to perform access on tuple through enum
// Requires contained types to have a `::id` defined
//-----------------------------------------------------------------------------

namespace keyUtils {

template <auto val, typename Tup, std::size_t I1, std::size_t... I, std::enable_if_t<(sizeof...(I) == 0), bool> = true>
constexpr std::size_t getKeyIndexById(std::index_sequence<I1, I...>) {
    using T1 = std::tuple_element_t<I1, Tup>;
    static_assert(std::is_same_v<std::decay_t<decltype(T1::id)>, decltype(val)>);
    static_assert(T1::id == val, "Non of the types match the key");
    return 0;
}
template <auto val, typename Tup, std::size_t I1, std::size_t... I, std::enable_if_t<(sizeof...(I) > 0), bool> = true>
constexpr std::size_t getKeyIndexById(std::index_sequence<I1, I...>) {
    using T1 = std::tuple_element_t<I1, Tup>;
    if constexpr (std::is_same_v<std::decay_t<decltype(T1::id)>, decltype(val)>) {
        if constexpr (T1::id == val) {
            return 0;
        }
        else {
            return 1 + getKeyIndexById<val, Tup>(std::index_sequence<I...>{});
        }
    }
    else {
        return 1 + getKeyIndexById<val, Tup>(std::index_sequence<I...>{});
    }
    return 0;  // Unreachable - avoid compiler warning
}


template <auto keyId, typename Tup, std::enable_if_t<util::IsTuple_v<std::decay_t<Tup>>, bool> = true>
decltype(auto) getById(Tup&& tup) {
    return std::get<getKeyIndexById<keyId, std::decay_t<Tup>>(
        std::make_index_sequence<std::tuple_size_v<std::decay_t<Tup>>>{})>(std::forward<Tup>(tup));
}


}  // namespace keyUtils


//-----------------------------------------------------------------------------
// Definitions to describe key-value pairs
//-----------------------------------------------------------------------------


// Forward declaration
enum class KVTag : std::uint64_t
{
    Required,   // Strictly required and can not be defaulted or conditionally depending on other keys
    Defaulted,  // Can be missing after reading from container but then may be defaulted through a custom alter function
    Optional,   // Can be missing after validation
};

template <typename KVTag_, std::enable_if_t<std::is_same_v<KVTag_, KVTag>, bool> = true>
std::string toString(KVTag_ t) {
    switch (t) {
        case KVTag::Required:
            return "required";
        case KVTag::Defaulted:
            return "defaulted";
        default:
            return "optional";
    }
}


// Dummy type as default
struct NoDefaultFunctor {};

template <auto id_, typename ValueType_, typename Mapper_ = DefaultMapper, KVTag tag_ = KVTag::Required,
          typename DefaultValueFunctor = NoDefaultFunctor>
struct KeyDef {
    using ValueType = ValueType_;
    using Mapper = Mapper_;
    using This = KeyDef<id_, ValueType_, Mapper_, tag_, DefaultValueFunctor>;

    static const auto id = id_;
    static constexpr KVTag tag = tag_;

    template <typename V>
    inline static constexpr bool CanCreateFromValue_v = HasRead_v<Reader<ValueType, Mapper>, V>;

    static constexpr bool hasDefaultValueFunctor = !std::is_same_v<DefaultValueFunctor, NoDefaultFunctor>;

    //---------------------------------
    // Accessors
    //---------------------------------

    // To be removed in the future, used as for string replacement
    const std::string_view& key() const noexcept { return key_; }
    ValueType defaultValue() const noexcept {
        static_assert(hasDefaultValueFunctor, "No default functor given");
        return defaultFunctor_();
    }
    const std::optional<std::string_view>& description() const noexcept { return description_; }

    // std::string describe() const {
    //     return std::string(key()) + std::string(" (") + util::typeToString<ValueType>() + std::string(", ")
    //          + toString(tag) + std::string{")"};
    // }

    //---------------------------------
    // Mutation
    //---------------------------------

    // Make the key-value pair optional - meaning it can be missing after alter & validation
    constexpr auto tagOptional() const {
        static_assert(tag_ != KVTag::Defaulted, "Description is already defaulted and can not be made optional");
        return KeyDef<id_, ValueType_, Mapper_, KVTag::Optional, DefaultValueFunctor>{key_, description_,
                                                                                      defaultFunctor_};
    }

    // Make the key-value pair defaulted - meaning it will be set through default functor or alter function and is
    // guaranteed to contain a value after validation
    constexpr auto tagDefaulted() const {
        static_assert(tag_ != KVTag::Defaulted, "Description is already defaulted");
        return KeyDef<id_, ValueType_, Mapper_, KVTag::Optional, DefaultValueFunctor>{key_, description_,
                                                                                      defaultFunctor_};
    }

    // Make the key-value pair defaulted and set a functon that generates a default value
    template <typename NewDefValFtor,
              std::enable_if_t<!std::is_convertible_v<NewDefValFtor, ValueType>
                                   && std::is_convertible_v<std::invoke_result_t<NewDefValFtor>, ValueType>,
                               bool>
              = true>
    constexpr auto withDefault(NewDefValFtor&& ftor) const {
        return KeyDef<id_, ValueType_, Mapper_, KVTag::Defaulted, std::decay_t<NewDefValFtor>>{
            key_, description_, std::forward<NewDefValFtor>(ftor)};
    }

    // Make the key-value pair defaulted and set a default value (need to be constexpr literal type, or wrap generation
    // of ValueType in a lambda otherwise)
    template <typename Val_, std::enable_if_t<std::is_convertible_v<Val_, ValueType>, bool> = true>
    constexpr auto withDefault(Val_ v) const {
        return withDefault([v = std::move(v)]() { return v; });
    }
    // Sets the description of the value
    constexpr auto withDescription(std::string_view descr) const {
        return This{key_, description_, std::move(defaultFunctor_)};
    }

    //---------------------------------
    // Static methods
    //---------------------------------

    template <typename Val, std::enable_if_t<CanCreateFromValue_v<Val>, bool> = true>
    static decltype(auto) read(Val&& val) {
        return Reader<ValueType, Mapper>::read(std::forward<Val>(val));
    }

    template <typename Container>
    static decltype(auto) write(const ValueType& val) {
        return Writer<ValueType, Container, Mapper>::write(val);
    }

    //---------------------------------


    // Members - all "simple" to be constexpr constructable. Would be more relaxed with C++20, but it's all we need
    std::string_view key_;
    std::optional<std::string_view> description_{};
    DefaultValueFunctor defaultFunctor_{};
};


//-----------------------------------------------------------------------------

template <typename T>
struct IsKeyDefinition {
    static constexpr bool value = false;
};
template <auto id, typename ValueType, typename Mapper, KVTag tag, typename DefFunctor>
struct IsKeyDefinition<KeyDef<id, ValueType, Mapper, tag, DefFunctor>> {
    static constexpr bool value = true;
};

template <typename T>
inline constexpr bool IsKeyDefinition_v = IsKeyDefinition<T>::value;


//-----------------------------------------------------------------------------
// Definitions to handle key sets
//-----------------------------------------------------------------------------

// To be specialized to retrieve a keyset with all keys for a Enum and further information
template <typename EnumType>
struct KeySetDescription;


template <typename T>
struct IsKeySetDescription {
    static constexpr bool value = false;
};
template <typename EnumType>
struct IsKeySetDescription<KeySetDescription<EnumType>> {
    static constexpr bool value = true;
};

template <typename T>
inline constexpr bool IsKeySetDescription_v = IsKeySetDescription<T>::value;


// Get name of a keyset
template <typename EnumType>
inline constexpr std::string_view KeySetDescriptionName_v = KeySetDescription<EnumType>::name;


// TODO Remove this macro
#define MULTIO_KEY_SET_DESCRIPTION(EnumName, keySetName, ...)         \
    template <>                                                       \
    struct KeySetDescription<EnumName> {                              \
        static constexpr std::string_view name = keySetName;          \
                                                                      \
        static constexpr auto keyDefs = std::make_tuple(__VA_ARGS__); \
    };


// To be specialized by Enums to provide custom alter function with KeyValueSet<KeySet<EnumType>>
template <typename KeySet_>
struct KeySetAlter;


template <typename KeySet_, typename KeyValueSet_, class = void>
struct HasAlterKeySetFunction : std::false_type {};

template <typename KeySet_, typename KeyValueSet_>
struct HasAlterKeySetFunction<KeySet_, KeyValueSet_,
                              std::void_t<decltype(KeySetAlter<KeySet_>::alter(std::declval<KeyValueSet_&>()))>>
    : std::true_type {};


template <typename KeySet_, typename KeyValueSet_>
inline constexpr bool HasAlterKeySetFunction_v = HasAlterKeySetFunction<KeySet_, KeyValueSet_>::value;


template <typename KeySet_>
struct AlterKeySetFunctor {
    template <typename KVS_, std::enable_if_t<(HasAlterKeySetFunction_v<KeySet_, KVS_>), bool> = true>
    void operator()(KVS_& kvs) const {
        KeySetAlter<KeySet_>::alter(kvs);
    }

    template <typename KVS_, std::enable_if_t<(!HasAlterKeySetFunction_v<KeySet_, KVS_>), bool> = true>
    void operator()(KVS_&) const {}
};


//-----------------------------------------------------------------------------
// Direct accessors to key definitions - expected to be used internally only
//-----------------------------------------------------------------------------

template <auto keyId, typename Tup, std::enable_if_t<util::IsTuple_v<std::decay_t<Tup>>, bool> = true>
decltype(auto) keyDef(Tup&& keySet) {
    return keyUtils::getById<keyId>(std::forward<Tup>(keySet));
}

template <auto keyId>
decltype(auto) keyDef() {
    return keyDef<keyId>(KeySetDescription<decltype(keyId)>::keyDefs);
}


//-----------------------------------------------------------------------------

template <auto id_>
struct ScopedKey {
    using KeyType = util::PrehashedKey<std::string>;

    using Definition = std::decay_t<decltype(keyDef<id_>())>;
    using ValueType = typename Definition::ValueType;
    using Mapper = typename Definition::Mapper;
    using This = ScopedKey<id_>;

    static const auto id = id_;
    static constexpr KVTag tag = Definition::tag;

    static constexpr bool hasDefaultValueFunctor = Definition::hasDefaultValueFunctor;


    template <typename V>
    inline static constexpr bool CanCreateFromValue_v = Definition::template CanCreateFromValue_v<V>;

    // To be removed in future when glossary is refactored
    operator const KeyType&() const { return key_; }
    operator const std::string&() const { return key_; }

    const KeyType& key() const { return key_; }

    ValueType defaultValue() const noexcept { return keyDef<id_>().defaultValue(); }
    const std::optional<std::string_view>& description() const noexcept { return keyDef<id_>().description(); }


    std::string describe() const {
        return std::string(key()) + std::string(" (") + util::typeToString<ValueType>() + std::string(", ")
             + toString(tag) + std::string{")"};
    }

    // Static methods
    template <typename Val, std::enable_if_t<CanCreateFromValue_v<Val>, bool> = true>
    static decltype(auto) read(Val&& val) {
        return Definition::read(std::forward<Val>(val));
    }
    template <typename Container>
    static decltype(auto) write(const ValueType& val) {
        return Definition::template write<Container>(val);
    }

    // Members
    KeyType key_;
};


template <typename T>
struct IsScopedKey {
    static constexpr bool value = false;
};

template <auto id>
struct IsScopedKey<ScopedKey<id>> {
    static constexpr bool value = true;
};

template <typename T>
inline constexpr bool IsScopedKey_v = IsScopedKey<T>::value;


template <auto id>
struct IsKeyDefinition<ScopedKey<id>> {
    static constexpr bool value = true;
};


template <typename KDEF,
          std::enable_if_t<IsKeyDefinition_v<std::decay_t<KDEF>> && !IsScopedKey_v<std::decay_t<KDEF>>, bool> = true>
auto toScopedKey(const KDEF& kdef) {
    return ScopedKey<std::decay_t<KDEF>::id>{kdef.key()};
}

template <typename KDEF,
          std::enable_if_t<IsKeyDefinition_v<std::decay_t<KDEF>> && !IsScopedKey_v<std::decay_t<KDEF>>, bool> = true>
auto toScopedKey(const KDEF& kdef, std::string_view scope) {
    return ScopedKey<std::decay_t<KDEF>::id>{std::string(scope) + std::string("-") + std::string(kdef.key())};
}

template <typename KDEF,
          std::enable_if_t<IsKeyDefinition_v<std::decay_t<KDEF>> && IsScopedKey_v<std::decay_t<KDEF>>, bool> = true>
auto toScopedKey(const KDEF& kdef, std::string_view scope) {
    constexpr auto id = std::decay_t<KDEF>::id;
    return ScopedKey<id>{std::string(scope) + std::string("-") + std::string(keyDef<id>().key())};
}


//-----------------------------------------------------------------------------


// Static accessor to key sets with/without scope and prehashed (may be removed in favor of constexpr in C++20)
template <typename EnumType>
struct StaticKeySetStore {
    static const auto& keys() {
        static const auto keys
            = util::map([&](const auto& kdef) { return toScopedKey(kdef); }, KeySetDescription<EnumType>::keyDefs);
        return keys;
    }

    static const auto& scopedKeys() {
        static const auto keys
            = util::map([&](const auto& kdef) { return toScopedKey(kdef, KeySetDescriptionName_v<EnumType>); },
                        KeySetDescription<EnumType>::keyDefs);
        return keys;
    }
};


template <auto keyId, typename Tup, std::enable_if_t<util::IsTuple_v<std::decay_t<Tup>>, bool> = true>
decltype(auto) key(Tup&& keySet) {
    return keyUtils::getById<keyId>(std::forward<Tup>(keySet));
}

template <auto keyId>
decltype(auto) key() {
    return key<keyId>(StaticKeySetStore<decltype(keyId)>::keys());
}


//-----------------------------------------------------------------------------
// KeySet implementation
//-----------------------------------------------------------------------------

enum class KeySetScope : std::uint64_t
{
    None,
    Default,
    Custom,
};


template <typename Derived, typename GetKeyPolicy>
class BaseKeySet {
public:
    using This = Derived;
    using TupleType = std::decay_t<decltype(GetKeyPolicy::getKeys())>;

    This& unscoped() {
        scope_ = KeySetScope::None;
        return static_cast<Derived&>(*this);
    };

    // Set scope in place
    This& scoped(std::optional<std::string> customScope = {}) {
        if (customScope) {
            scope_ = KeySetScope::Custom;
            customScopedKeys_
                = util::map([&](const auto& kdef) { return toScopedKey(kdef, *customScope); }, GetKeyPolicy::getKeys());
        }
        else {
            scope_ = KeySetScope::Default;
        }
        return static_cast<Derived&>(*this);
    };


    // Create a new KeySet with different scope
    This makeUnscoped() const {
        This ret{static_cast<const Derived&>(*this)};
        ret.unscoped();
        return ret;
    };

    // Set scope in place
    This makeScoped(std::optional<std::string> customScope = {}) const {
        This ret{static_cast<const Derived&>(*this)};
        ret.scoped(std::move(customScope));
        return ret;
    };


    const TupleType& keys() const {
        switch (scope_) {
            case KeySetScope::Default:
                return GetKeyPolicy::getScopedKeys();
            case KeySetScope::Custom:
                return customScopedKeys_.value();
            default:
                return GetKeyPolicy::getKeys();
        }
    }


private:
    KeySetScope scope_{KeySetScope::None};
    std::optional<TupleType> customScopedKeys_{};
};


template <typename EnumType>
struct KeySetGetKeysPolicy {
    static_assert(util::IsTuple_v<std::decay_t<decltype(StaticKeySetStore<EnumType>::keys())>>, "Expected a tuple");

    static const auto& getKeys() { return StaticKeySetStore<EnumType>::keys(); }
    static const auto& getScopedKeys() { return StaticKeySetStore<EnumType>::scopedKeys(); }
};


template <typename EnumType>
class KeySet : public BaseKeySet<KeySet<EnumType>, KeySetGetKeysPolicy<EnumType>> {};


template <auto... Ids>
struct CustomKeySetGetKeysPolicy {
    static const auto& getKeys() {
        static const auto keys = std::make_tuple(key<Ids>()...);
        return keys;
    }

    static const auto& getScopedKeys() {
        static const auto scopedKeys
            = std::make_tuple(toScopedKey(keyDef<Ids>(), KeySetDescriptionName_v<decltype(Ids)>)...);
        return scopedKeys;
    }
};


template <auto... Ids>
class CustomKeySet : public BaseKeySet<CustomKeySet<Ids...>, CustomKeySetGetKeysPolicy<Ids...>> {};


template <typename T>
struct IsKeySet {
    static constexpr bool value = false;
};
template <typename EnumType>
struct IsKeySet<KeySet<EnumType>> {
    static constexpr bool value = true;
};
template <auto... Ids>
struct IsKeySet<CustomKeySet<Ids...>> {
    static constexpr bool value = true;
};

template <typename T>
inline constexpr bool IsKeySet_v = IsKeySet<T>::value;


//-----------------------------------------------------------------------------


template <typename EnumType>
constexpr auto keySet() {
    return KeySet<EnumType>{};
}

template <auto... Ids>
constexpr auto keySet() {
    return CustomKeySet<Ids...>{};
}

// This is the accossor that will be used to access individual keys
template <auto keyId, typename KS, std::enable_if_t<IsKeySet_v<std::decay_t<KS>>, bool> = true>
decltype(auto) key(KS&& keySet) {
    return keyUtils::getById<keyId>(std::forward<KS>(keySet).keys());
}


//-----------------------------------------------------------------------------
// Value containers
//-----------------------------------------------------------------------------

struct MissingValue {};

// Operators for missing value ... use SFINAE to let this code remain header only
// Also implement operator for other types to simplify comparison implementation for KeyValue
template <typename MV1, typename MV2,
          std::enable_if_t<(std::is_same_v<MV1, MissingValue> || std::is_same_v<MV2, MissingValue>), bool> = true>
bool operator==(const MV1& lhs, const MV2& rhs) noexcept {
    return std::is_same_v<MV1, MissingValue> && std::is_same_v<MV2, MissingValue>;
}

template <typename MV1, typename MV2,
          std::enable_if_t<(std::is_same_v<MV1, MissingValue> || std::is_same_v<MV2, MissingValue>), bool> = true>
bool operator!=(const MV1& lhs, const MV2& rhs) noexcept {
    return !(lhs == rhs);
}


template <auto id_>
struct KeyValue {
    using Description = std::decay_t<decltype(key<id_>())>;
    using ValueType = typename Description::ValueType;
    using Mapper = typename Description::Mapper;
    using This = KeyValue<id_>;

    static const auto id = id_;

    using RefType = std::reference_wrapper<const ValueType>;
    using Container = std::variant<MissingValue, ValueType, RefType>;

    // Actual value
    Container value;

    bool isMissing() const { return std::holds_alternative<MissingValue>(value); }
    bool has() const { return !std::holds_alternative<MissingValue>(value); }
    bool holdsReference() const { return std::holds_alternative<const ValueType>(value); }

    // Function to get the contained value if it's not missing - due to the possibility of containing a reference,
    // only const& versions can get Optimized rvalue handling is achieved through visit
    const ValueType& get() const {
        return std::visit(eckit::Overloaded{
                              [&](const ValueType& val) -> const ValueType& { return val; },
                              [&](const RefType& val) -> const ValueType& { return val.get(); },
                              [&](const MissingValue&) -> const ValueType& {
                                  throw DataModellingException(
                                      std::string("Value is missing for key ") + key<id>().describe(), Here());
                              },
                          },
                          value);
    }
    operator const ValueType&() const { return get(); }

    void setMissing() noexcept { value = MissingValue{}; }

    template <typename V,
              std::enable_if_t<
                  (!std::is_same_v<std::decay_t<V>, MissingValue> && !std::is_same_v<std::decay_t<V>, RefType>), bool>
              = true>
    void set(V&& v) noexcept {
        value = Description::read(std::forward<V>(v));
    }
    template <typename V,
              std::enable_if_t<
                  (std::is_same_v<std::decay_t<V>, MissingValue> || std::is_same_v<std::decay_t<V>, RefType>), bool>
              = true>
    void set(V&& v) noexcept {
        value = std::forward<V>(v);
    }

    // Visit function that handles RValues and references properly
    template <typename Func, typename Val>
    static decltype(auto) visitHelper(Func&& func, Val&& val) {
        return std::visit(eckit::Overloaded{
                              [&](RefType ref) { return std::forward<Func>(func)(ref.get()); },
                              [&](auto&& v) { return std::forward<Func>(func)(std::forward<decltype(v)>(v)); },
                          },
                          std::forward<Val>(val));
    }

    template <typename Func>
    decltype(auto) visit(Func&& func) & {
        return visitHelper(std::forward<Func>(func), value);
    }
    template <typename Func>
    decltype(auto) visit(Func&& func) const& {
        return visitHelper(std::forward<Func>(func), value);
    }
    template <typename Func>
    decltype(auto) visit(Func&& func) && {
        return visitHelper(std::forward<Func>(func), std::move(value));
    }


    // Make sure no reference is hold and value is owned
    void acquire() {
        std::visit(eckit::Overloaded{
                       [&](const RefType& val) { this->value = val.get(); },
                       [&](auto) {},
                   },
                   value);
    }

    template <typename Func>
    This& withDefault(Func&& func) {
        if (isMissing()) {
            this->set(std::forward<Func>(func)());
        }
        return *this;
    }
};


template <auto Id>
bool operator==(const KeyValue<Id>& lhs, const KeyValue<Id>& rhs) noexcept {
    return lhs.visit(
        [&](const auto& lhsVal) { return rhs.visit([&](const auto& rhsVal) { return lhsVal == rhsVal; }); });
}
template <auto Id>
bool operator!=(const KeyValue<Id>& lhs, const KeyValue<Id>& rhs) noexcept {
    return lhs.visit(
        [&](const auto& lhsVal) { return rhs.visit([&](const auto& rhsVal) { return lhsVal != rhsVal; }); });
}


//-----------------------------------------------------------------------------


template <typename T>
struct IsKeyValue {
    static constexpr bool value = false;
};
template <auto id>
struct IsKeyValue<KeyValue<id>> {
    static constexpr bool value = true;
};

template <typename T>
inline constexpr bool IsKeyValue_v = IsKeyValue<T>::value;


template <auto id>
decltype(auto) toMissingValue() {
    return KeyValue<id>{MissingValue{}};
}

template <typename KVD, std::enable_if_t<IsKeyDefinition_v<KVD>, bool> = true>
decltype(auto) toMissingValue(const KVD&) {
    return toMissingValue<KVD::id>();
}


template <typename KVD, std::enable_if_t<IsKeyDefinition_v<KVD>, bool> = true>
decltype(auto) toMissingOrDefaultValue(const KVD& kvd) {
    if constexpr (KVD::hasDefaultValueFunctor) {
        return KeyValue<KVD::id>{kvd.defaultValue()};
    }
    return KeyValue<KVD::id>{MissingValue{}};
}

template <auto id>
decltype(auto) toMissingOrDefaultValue() {
    return toMissingOrDefaultValue(key<id>());
}


// Creates a KeyValue and uses a reference_wrapper if possible
template <auto id, typename V, std::enable_if_t<!IsKeyValue_v<std::decay_t<V>>, bool> = true>
decltype(auto) toKeyValueRef(V&& v) {
    using KV = KeyValue<id>;
    if constexpr (util::IsOptional_v<std::decay_t<V>>) {
        if (v) {
            return toKeyValueRef<id>(std::forward<V>(v).value());
        }
        else {
            return KV{};
        }
    }
    else {
        if constexpr (std::is_same_v<std::decay_t<V>, MissingValue>) {
            return KV{};
        }
        else if constexpr (std::is_same_v<std::decay_t<V>, typename KV::ValueType>) {
            if constexpr (!std::is_lvalue_reference_v<V>) {
                return KV{std::move(v)};
            }
            else {
                return KV{typename KV::RefType(v)};
            }
        }
        else {
            if constexpr (!std::is_lvalue_reference_v<V>) {
                return KV{KV::Description::read(std::move(v))};
            }
            else {
                return KV{KV::Description::read(v)};
            }
        }
    }
    return KV{};  // unreachable - prevent compiler warning
}
template <typename KVD, typename V, std::enable_if_t<IsKeyDefinition_v<KVD>, bool> = true>
decltype(auto) toKeyValueRef(const KVD&, V&& v) {
    return toKeyValueRef<KVD::id>(std::forward<V>(v));
}

// Creates a KeyValue and always copies values
template <auto id, typename V, std::enable_if_t<!IsKeyValue_v<std::decay_t<V>>, bool> = true>
decltype(auto) toKeyValue(V&& v) {
    auto res = toKeyValueRef<id>(std::forward<V>(v));
    res.acquire();
    return res;
}

template <typename KVD, typename V, std::enable_if_t<IsKeyDefinition_v<KVD>, bool> = true>
decltype(auto) toKeyValue(const KVD&, V&& v) {
    return toKeyValue<KVD::id>(std::forward<V>(v));
}


//-----------------------------------------------------------------------------


// Takes a tuple of KeyDefinition and creates an instance of the keyset with all fields set to missing
template <typename DescTup, std::enable_if_t<(util::IsTuple_v<std::decay_t<DescTup>>
                                              && IsKeyDefinition_v<std::tuple_element_t<0, std::decay_t<DescTup>>>),
                                             bool>
                            = true>
decltype(auto) reify(DescTup&& tup) {
    return util::map([&](const auto& kvd) { return toMissingValue(kvd); }, std::forward<DescTup>(tup));
}


//-----------------------------------------------------------------------------


template <typename KeySet_>
struct KeyValueSet {
    using This = KeyValueSet<KeySet_>;
    using KeySetType = KeySet_;
    using TupleType = decltype(reify(std::declval<KeySetType>().keys()));

    KeySetType keySet;
    TupleType values;

    This& unscoped() {
        keySet.unscoped();
        return *this;
    };

    // Set scope in place
    This& scoped(std::optional<std::string> customScope = {}) {
        keySet.scoped(std::move(customScope));
        return *this;
    };


    // Create a new KeySet with different scope
    This makeUnscoped() const {
        This ret{*this};
        ret.unscoped();
        return ret;
    };

    // Set scope in place
    This makeScoped(std::optional<std::string> customScope = {}) const {
        This ret{*this};
        ret.scoped(customScope);
        return ret;
    };

    static void alter(This& v) { AlterKeySetFunctor<KeySet_>{}(v); }
};

template <typename KeySet_>
bool operator==(const KeyValueSet<KeySet_>& lhs, const KeyValueSet<KeySet_>& rhs) noexcept {
    return lhs.values == rhs.values;
}
template <typename KeySet_>
bool operator!=(const KeyValueSet<KeySet_>& lhs, const KeyValueSet<KeySet_>& rhs) noexcept {
    return lhs.values != rhs.values;
}


template <typename T>
struct IsKeyValueSet {
    static constexpr bool value = false;
};
template <typename KeySet_>
struct IsKeyValueSet<KeyValueSet<KeySet_>> {
    static constexpr bool value = true;
};

template <typename T>
inline constexpr bool IsKeyValueSet_v = IsKeyValueSet<T>::value;


//-----------------------------------------------------------------------------


template <auto keyId, typename KVS, std::enable_if_t<IsKeyValueSet_v<std::decay_t<KVS>>, bool> = true>
decltype(auto) key(KVS&& keyValueSet) {
    return keyUtils::getById<keyId>(std::forward<KVS>(keyValueSet).values);
}


//-----------------------------------------------------------------------------


// Takes a tuple of KeyValues and converts all references to value by copying
template <typename Tup,
          std::enable_if_t<(util::IsTuple_v<std::decay_t<Tup>> && IsKeyValue_v<std::tuple_element_t<0, Tup>>), bool>
          = true>
Tup& acquire(Tup& tup) {
    util::forEach([](auto& kv) { kv.acquire(); }, tup);
    return tup;
}

// Takes a tuple of KeyValues and converts all references to value by copying
template <typename KVS, std::enable_if_t<(IsKeyValueSet_v<KVS>), bool> = true>
KVS& acquire(KVS& kvs) {
    acquire(kvs.values);
    return kvs;
}
}  // namespace multio::datamod

//-----------------------------------------------------------------------------

namespace multio::util {

//-----------------------------------------------------------------------------
// Extend tuple utilities (forEach, map) to KeyValueSets
//-----------------------------------------------------------------------------

// General forEach function for KeySet
template <typename Func, typename KS, std::enable_if_t<(multio::datamod::IsKeySet_v<std::decay_t<KS>>), bool> = true>
void forEach(Func&& func, KS&& ks) {
    util::forEach(std::forward<Func>(func), ks.keys());
}

// General map function for KeySet
template <typename Func, typename KS, std::enable_if_t<(multio::datamod::IsKeySet_v<std::decay_t<KS>>), bool> = true>
decltype(auto) map(Func&& func, KS&& ks) {
    return util::map(std::forward<Func>(func), ks.keys());
}

// General forEach function to iterate on KeyValueSets
template <typename Func, typename KVS,
          std::enable_if_t<(multio::datamod::IsKeyValueSet_v<std::decay_t<KVS>>), bool> = true>
void forEach(Func&& func, KVS&& kvs) {
    const auto& keys = kvs.keySet.keys();
    util::forEach(
        [&](auto&& kv) {
            func(multio::datamod::key<std::decay_t<decltype(kv)>::id>(keys), std::forward<decltype(kv)>(kv));
        },
        std::forward<KVS>(kvs).values);
}

// General map function to iterate on KeyValueSets
template <typename Func, typename KVS,
          std::enable_if_t<(multio::datamod::IsKeyValueSet_v<std::decay_t<KVS>>), bool> = true>
decltype(auto) map(Func&& func, KVS&& kvs) {
    const auto& keys = kvs.keySet.keys();
    return util::map(
        [&](auto&& kv) {
            return func(multio::datamod::key<std::decay_t<decltype(kv)>::id>(keys), std::forward<decltype(kv)>(kv));
        },
        std::forward<KVS>(kvs).values);
}

}  // namespace multio::util

namespace multio::datamod {

//-----------------------------------------------------------------------------
// Alter (apply defaults) and validation
//-----------------------------------------------------------------------------


// Takes a tuple of KeyValue and verifies that all required keys are set
template <typename KVD, typename KV,
          std::enable_if_t<(IsKeyDefinition_v<std::decay_t<KVD>> && IsKeyValue_v<std::decay_t<KV>>), bool> = true>
void validate(const KVD&, const KV& kv) {
    // Only optional tagged keys can be missing
    if constexpr (KVD::tag != KVTag::Optional) {
        if (kv.isMissing()) {
            throw DataModellingException(std::string("Missing required key: ") + key<KV::id>().describe(), Here());
        }
    }
}

// Takes a tuple of KeyValue and verifies that all required keys are set
template <typename KV, std::enable_if_t<(IsKeyValue_v<std::decay_t<KV>>), bool> = true>
void validate(const KV& kv) {
    validate(key<KV::id>(), kv);
}


// Takes a tuple of KeyValue and verifies that all required keys are set
template <typename KVD, typename KV,
          std::enable_if_t<(IsKeyDefinition_v<std::decay_t<KVD>> && IsKeyValue_v<std::decay_t<KV>>), bool> = true>
KV& alter(const KVD& kvd, KV& kv) {
    // Only optional tagged keys can be missing
    if constexpr (KVD::hasDefaultValueFunctor) {
        if (kv.isMissing()) {
            kv.set(kvd.defaultValue());
        }
    }
    return kv;
}

// Takes a tuple of KeyValue and verifies that all required keys are set
template <typename KV, std::enable_if_t<(IsKeyValue_v<std::decay_t<KV>>), bool> = true>
KV& alter(KV& kv) {
    return alter(key<KV::id>(), kv);
}


// Takes a tuple of KeyValue and verifies that all required keys are set
template <typename KVS, std::enable_if_t<(IsKeyValueSet_v<std::decay_t<KVS>>), bool> = true>
void validate(const KVS& kvs) {
    const auto& keys = kvs.keySet.keys();
    util::forEach([&](const auto& kv) { validate(key<std::decay_t<decltype(kv)>::id>(keys), kv); }, kvs.values);
}

// Takes a tuple of KeyValue and verifies that all required keys are set
template <
    typename ValTup,
    std::enable_if_t<(util::IsTuple_v<std::decay_t<ValTup>> && IsKeyValue_v<std::tuple_element_t<0, ValTup>>), bool>
    = true>
void validate(const ValTup& tup) {
    util::forEach([&](const auto& kv) { validate(kv); }, tup);
}


// Takes a tuple of KeyValue and verifies that all required keys are set
template <typename KVS, std::enable_if_t<(IsKeyValueSet_v<std::decay_t<KVS>>), bool> = true>
KVS& alter(KVS& kvs) {
    const auto& keys = kvs.keySet.keys();
    // Alter single entries first (make sure they have a default applied if given)
    util::forEach([&](auto& kv) { alter(key<std::decay_t<decltype(kv)>::id>(keys), kv); }, kvs.values);

    // Now call the Keyset specific alter function if given
    std::decay_t<KVS>::alter(kvs);
    return kvs;
}

// Takes a tuple of KeyValue and verifies that all required keys are set
template <
    typename ValTup,
    std::enable_if_t<(util::IsTuple_v<std::decay_t<ValTup>> && IsKeyValue_v<std::tuple_element_t<0, ValTup>>), bool>
    = true>
ValTup& alter(ValTup& tup) {
    util::forEach([&](const auto& kv) { alter(kv); }, tup);
    return tup;
}


// Takes a tuple of KeyValue and verifies that all required keys are set
template <typename KVS, std::enable_if_t<(IsKeyValueSet_v<std::decay_t<KVS>>), bool> = true>
KVS& alterAndValidate(KVS& kvs) {
    alter(kvs);
    validate(kvs);
    return kvs;
}

// Takes a tuple of KeyValue and verifies that all required keys are set
template <
    typename ValTup,
    std::enable_if_t<(util::IsTuple_v<std::decay_t<ValTup>> && IsKeyValue_v<std::tuple_element_t<0, ValTup>>), bool>
    = true>
ValTup& alterAndValidate(ValTup& tup) {
    alter(tup);
    validate(tup);
    return tup;
}


//-----------------------------------------------------------------------------
// Reading from other containers
//-----------------------------------------------------------------------------

// Methods to read/write tuples of KeyValues to specific types (i.e. from Metadata)
// getByValue(description, container)
// getByRef(description, container)
template <typename Container>
struct KeyValueReader;

template <typename Container>
struct BaseKeyValueReader {
    template <typename KVD, typename Cont_,
              std::enable_if_t<(IsKeyDefinition_v<KVD> && std::is_base_of_v<Cont_, std::decay_t<Container>>), bool>
              = true>
    static decltype(auto) getByValue(const KVD& kvd, Cont_&& c) {
        auto ret = KeyValueReader<Container>::getByRef(kvd, std::forward<Cont_>(c));
        acquire(ret);
        return ret;
    }
};


//-----------------------------------------------------------------------------
// Reading from keysets
//-----------------------------------------------------------------------------

template <auto id, typename... KVS>
struct KeyValueReader<std::tuple<KeyValue<id>, KVS...>> : BaseKeyValueReader<std::tuple<KeyValue<id>, KVS...>> {
    using Base = BaseKeyValueReader<std::tuple<KeyValue<id>, KVS...>>;
    using Base::getByValue;

    template <typename KVD, typename KVTup,
              std::enable_if_t<(IsKeyDefinition_v<std::decay_t<KVD>> && util::IsTuple_v<std::decay_t<KVTup>>
                                && IsKeyValue_v<std::tuple_element_t<0, std::decay_t<KVTup>>>),
                               bool>
              = true>
    static decltype(auto) getByRef(const KVD& kvd, KVTup&& kv) {
        return key<KVD::id>(std::forward<KVTup>(kv));
    }
};

template <typename KeySet_>
struct KeyValueReader<KeyValueSet<KeySet_>> : BaseKeyValueReader<KeyValueSet<KeySet_>> {
    using Base = BaseKeyValueReader<KeyValueSet<KeySet_>>;
    using BaseTup = KeyValueReader<typename KeyValueSet<KeySet_>::TupleType>;
    using Base::getByValue;

    template <typename KVD, typename KVS,
              std::enable_if_t<(IsKeyDefinition_v<std::decay_t<KVD>> && IsKeyValueSet_v<std::decay_t<KVS>>), bool>
              = true>
    static decltype(auto) getByRef(const KVD& kvd, KVS&& kv) {
        return BaseTup::getByRef(kvd, std::forward<KVS>(kv).values);
    }
};


//-----------------------------------------------------------------------------
// Writing to other containers
//-----------------------------------------------------------------------------


// Methods to read/write tuples of KeyValues to specific types (i.e. from Metadata)
// set(KeyDefinition, KeyValue, Container)
template <typename Container>
struct KeyValueWriter;


//-----------------------------------------------------------------------------
// Writing from keysets
//-----------------------------------------------------------------------------

template <auto id, typename... KVS>
struct KeyValueWriter<std::tuple<KeyValue<id>, KVS...>> {
    template <typename KVD, typename KV, typename KVTup,
              std::enable_if_t<(IsKeyDefinition_v<std::decay_t<KVD>> && IsKeyValue_v<std::decay_t<KV>>
                                && util::IsTuple_v<std::decay_t<KVTup>>
                                && IsKeyValue_v<std::tuple_element_t<0, std::decay_t<KVTup>>>),
                               bool>
              = true>
    static decltype(auto) set(const KVD& kvd, KV&& kv, KVTup& kvTup) {
        return key<KVD::id>(kvTup).set(std::forward<KV>(kv));
    }
};

template <typename KeySet_>
struct KeyValueWriter<KeyValueSet<KeySet_>> {
    using BaseTup = KeyValueWriter<typename KeyValueSet<KeySet_>::TupleType>;

    template <typename KVD, typename KV, typename KVS,
              std::enable_if_t<(IsKeyDefinition_v<std::decay_t<KVD>> && IsKeyValue_v<std::decay_t<KV>>
                                && IsKeyValueSet_v<std::decay_t<KVS>>),
                               bool>
              = true>
    static decltype(auto) set(const KVD& kvd, KV&& kv, KVS& kvs) {
        return BaseTup::set(kvd, std::forward<KV>(kv), kvs.values);
    }
};


template <typename KS, std::enable_if_t<(IsKeySet_v<std::decay_t<KS>>), bool> = true>
decltype(auto) reify(KS&& ks) {
    auto values = reify(ks.keys());
    return KeyValueSet<std::decay_t<KS>>{std::forward<KS>(ks), std::move(values)};
}


namespace details {

// Takes a tuple of KeyDefinition and a container from which to read/create an object from
template <typename DescTup, typename Container,
          std::enable_if_t<(util::IsTuple_v<std::decay_t<DescTup>>
                            && IsKeyDefinition_v<std::tuple_element_t<0, std::decay_t<DescTup>>>),
                           bool>
          = true>
decltype(auto) read(DescTup&& tup, Container&& c) {
    return util::map(
        [&](const auto& kvd) {
            return KeyValueReader<std::decay_t<Container>>::getByRef(kvd, std::forward<Container>(c));
        },
        std::forward<DescTup>(tup));
}
template <typename DescTup, typename Container,
          std::enable_if_t<(util::IsTuple_v<std::decay_t<DescTup>>
                            && IsKeyDefinition_v<std::tuple_element_t<0, std::decay_t<DescTup>>>),
                           bool>
          = true>
decltype(auto) readValue(DescTup&& tup, Container&& c) {
    return util::map(
        [&](const auto& kvd) {
            return KeyValueReader<std::decay_t<Container>>::getByValue(kvd, std::forward<Container>(c));
        },
        std::forward<DescTup>(tup));
}

}  // namespace details


template <typename KS, typename Container, std::enable_if_t<(IsKeySet_v<std::decay_t<KS>>), bool> = true>
decltype(auto) read(KS&& ks, Container&& c) {
    auto values = details::read(ks.keys(), std::forward<Container>(c));
    KeyValueSet<std::decay_t<KS>> ret{std::forward<KS>(ks), std::move(values)};
    alterAndValidate(ret);
    return ret;
}

template <typename KS, typename Container, std::enable_if_t<(IsKeySet_v<std::decay_t<KS>>), bool> = true>
decltype(auto) readValue(KS&& ks, Container&& c) {
    auto values = details::readValue(ks.keys(), std::forward<Container>(c));
    KeyValueSet<std::decay_t<KS>> ret{std::forward<KS>(ks), std::move(values)};
    alterAndValidate(ret);
    return ret;
}


//-----------------------------------------------------------------------------


// Takes a tuple of KeyDefinition and a container from which to read/create an object from
template <
    typename KVTup, typename Container,
    std::enable_if_t<
        (util::IsTuple_v<std::decay_t<KVTup>> && IsKeyValue_v<std::tuple_element_t<0, std::decay_t<KVTup>>>), bool>
    = true>
void write(KVTup&& tup, Container& c) {
    util::forEach(
        [&](auto&& kv) {
            return KeyValueWriter<std::decay_t<Container>>::set(key<std::decay_t<decltype(kv)>::id>(),
                                                                std::forward<decltype(kv)>(kv), c);
        },
        std::forward<KVTup>(tup));
}

template <
    typename Container, typename KVTup,
    std::enable_if_t<
        (util::IsTuple_v<std::decay_t<KVTup>> && IsKeyValue_v<std::tuple_element_t<0, std::decay_t<KVTup>>>), bool>
    = true>
Container write(KVTup&& tup) {
    Container c{};
    write(std::forward<KVTup>(tup), c);
    return c;
}


template <typename KVS, typename Container, std::enable_if_t<(IsKeyValueSet_v<std::decay_t<KVS>>), bool> = true>
void write(KVS&& kvs, Container& c) {
    util::forEach(
        [&](const auto& kvd, auto&& kv) {
            return KeyValueWriter<std::decay_t<Container>>::set(kvd, std::forward<decltype(kv)>(kv), c);
        },
        std::forward<KVS>(kvs));
}

template <typename Container, typename KVS, std::enable_if_t<(IsKeyValueSet_v<std::decay_t<KVS>>), bool> = true>
Container write(KVS&& kvs) {
    Container c{};
    write(std::forward<KVS>(kvs), c);
    return c;
}


//-----------------------------------------------------------------------------

// Printing something readable to ostream
template <typename KeySet_>
std::ostream& operator<<(std::ostream& os, const multio::datamod::KeyValueSet<KeySet_>& kvs) {
    os << "{";
    bool first = true;
    util::forEach(
        [&](const auto& key, const auto& value) {
            if (first) {
                first = false;
            }
            else {
                os << ", ";
            }

            os << key.key() << "=";
            if (value.isMissing()) {
                os << "<MISSING>";
            }
            else {
                os << value.get();
            }
        },
        kvs);
    os << "}";
    return os;
}


//-----------------------------------------------------------------------------

}  // namespace multio::datamod


//-----------------------------------------------------------------------------
// Hashing of KeyValueSets
//-----------------------------------------------------------------------------

template <>
struct std::hash<multio::datamod::MissingValue> {
    std::size_t operator()(const multio::datamod::MissingValue&) const noexcept { return 0; }
};

template <auto id>
struct std::hash<multio::datamod::KeyValue<id>> {
    std::size_t operator()(const multio::datamod::KeyValue<id>& kv) const
        noexcept(noexcept(multio::util::hash(std::declval<typename multio::datamod::KeyValue<id>::ValueType>()))) {
        return kv.visit([&](const auto& v) -> std::size_t { return multio::util::hash(v); });
    }
};

template <auto id, typename... KVS>
struct std::hash<std::tuple<multio::datamod::KeyValue<id>, KVS...>> {
    std::size_t operator()(const std::tuple<multio::datamod::KeyValue<id>, KVS...>& t) const
        noexcept(noexcept(multio::util::hashCombine(std::declval<multio::datamod::KeyValue<id>>(),
                                                    std::declval<KVS>()...))) {
        return std::apply([](const auto&... args) { return multio::util::hashCombine(args...); }, t);
    }
};

template <typename KeySet>
struct std::hash<multio::datamod::KeyValueSet<KeySet>> {
    std::size_t operator()(const multio::datamod::KeyValueSet<KeySet>& kvs) const
        noexcept(noexcept(multio::util::hash(std::declval<multio::datamod::KeyValueSet<KeySet>>().values))) {
        return multio::util::hash(kvs.values);
    }
};
