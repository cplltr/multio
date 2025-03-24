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

#include "eckit/config/LocalConfiguration.h"
#include "multio/datamod/DataModelling.h"
#include "multio/datamod/DataModellingException.h"
#include "multio/message/Metadata.h"
#include "multio/util/TypeTraits.h"


namespace multio::datamod {

//-----------------------------------------------------------------------------
// Reading from metadata
//-----------------------------------------------------------------------------

template <>
struct KeyValueReader<message::BaseMetadata> : BaseKeyValueReader<message::BaseMetadata> {
    using Base = BaseKeyValueReader<message::BaseMetadata>;
    using Base::getByValue;

    template <typename KVD, typename MD,
              std::enable_if_t<
                  (IsKeyValueDescription_v<KVD> && std::is_base_of_v<message::BaseMetadata, std::decay_t<MD>>), bool>
              = true>
    static decltype(auto) getByRef(const KVD& kvd, MD&& md) {
        if constexpr (KVD::hasMapper) {
            if (auto search = std::forward<MD>(md).find(kvd.key); search != md.end()) {
                return toKeyValueRef(
                    kvd, search->second.visit([&](auto&& v) { return kvd.mapper.read(std::forward<decltype(v)>(v)); }));
            }
            if constexpr (KVD::tag == KVTag::Required) {
                std::ostringstream oss;
                oss << "Missing required key " << kvd.describe() << " in metadata " << md << std::endl;
                throw DataModellingException(oss.str(), Here());
            }
            return toMissingOrDefaultValue(kvd);
        }
        else {
            if (auto search = std::forward<MD>(md).find(kvd.key); search != md.end()) {
                // If container is not an lvalue we can move from
                if constexpr (!std::is_lvalue_reference_v<MD>) {
                    return toKeyValueRef(kvd, std::move(search->second.template get<typename KVD::ValueType>()));
                }
                else {
                    return toKeyValueRef(kvd, search->second.template get<typename KVD::ValueType>());
                }
            }
            if constexpr (KVD::tag == KVTag::Required) {
                std::ostringstream oss;
                oss << "Missing required key " << kvd.describe() << " in metadata " << md << std::endl;
                throw DataModellingException(oss.str(), Here());
            }
            return toMissingOrDefaultValue(kvd);
        }
        return toMissingValue(kvd);  // unreachable - prevent compiler warning
    }
};

template <>
struct KeyValueReader<message::Metadata> : KeyValueReader<message::BaseMetadata> {
    using Base = KeyValueReader<message::BaseMetadata>;
    using Base::getByRef;
    using Base::getByValue;
};


//-----------------------------------------------------------------------------
// Writing to metadata
//-----------------------------------------------------------------------------

template <>
struct KeyValueWriter<message::BaseMetadata> {
    template <typename KVD, typename KV_, typename MD,
              std::enable_if_t<(IsKeyValueDescription_v<std::decay_t<KVD>> && IsKeyValue_v<std::decay_t<KV_>>
                                && std::is_base_of_v<message::BaseMetadata, std::decay_t<MD>>),
                               bool>
              = true>
    static void set(const KVD& kvd, KV_&& kv, MD& md) {
        // TODO think about handling missing value by setting Null ?
        if constexpr (KVD::hasMapper) {
            std::forward<KV_>(kv).visit(
                eckit::Overloaded{[&](MissingValue v) {},
                                  [&](auto&& v) { md.set(kvd.key, kvd.mapper.write(std::forward<decltype(v)>(v))); }});
        }
        else {
            std::forward<KV_>(kv).visit(eckit::Overloaded{
                [&](MissingValue v) {}, [&](auto&& v) { md.set(kvd.key, std::forward<decltype(v)>(v)); }});
        }
    }
};

template <>
struct KeyValueWriter<message::Metadata> : KeyValueWriter<message::BaseMetadata> {
    using Base = KeyValueWriter<message::BaseMetadata>;
    using Base::set;
};


//-----------------------------------------------------------------------------
// Reading from LocalConfiguration
//-----------------------------------------------------------------------------

template <>
struct KeyValueReader<eckit::Configuration> : BaseKeyValueReader<eckit::Configuration> {
    using Base = BaseKeyValueReader<eckit::Configuration>;
    using Base::getByValue;


    template <typename T, typename Conf, typename Key>
    static T getValueByType(const Conf& c, const Key& key) {
        T val;
        c.get(key, val);
        return val;
    }


    template <typename Key, typename Conf, typename Func>
    static decltype(auto) visitNonNullValue(const Key& key, const Conf& c, Func&& func) {
        // Ridiculous chain of "reflective" calls
        if (c.isBoolean(key)) {
            return std::forward<Func>(func)(util::TypeTag<bool>{});
        }
        if (c.isBooleanList(key)) {
            return std::forward<Func>(func)(util::TypeTag<std::vector<std::int64_t>>{});
        }
        if (c.isFloatingPoint(key)) {
            return std::forward<Func>(func)(util::TypeTag<double>{});
        }
        if (c.isFloatingPointList(key)) {
            return std::forward<Func>(func)(util::TypeTag<std::vector<double>>{});
        }
        if (c.isIntegral(key)) {
            return std::forward<Func>(func)(util::TypeTag<std::int64_t>{});
        }
        if (c.isIntegralList(key)) {
            return std::forward<Func>(func)(util::TypeTag<std::vector<double>>{});
        }
        // if (c.isList(key)) {
        //     // Not supported
        // }
        if (c.isString(key)) {
            return std::forward<Func>(func)(util::TypeTag<std::string>{});
        }
        if (c.isStringList(key)) {
            return std::forward<Func>(func)(util::TypeTag<std::vector<std::string>>{});
        }
        // if (c.isSubConfiguration(key)) {
        //     // Not supported
        // }

        return std::forward<Func>(func)();
    }


    template <typename KVD, typename Conf,
              std::enable_if_t<
                  (IsKeyValueDescription_v<KVD> && std::is_base_of_v<eckit::Configuration, std::decay_t<Conf>>), bool>
              = true>
    static decltype(auto) getByRef(const KVD& kvd, Conf&& conf) {
        if (!conf.has(kvd.key)) {
            if constexpr (KVD::tag == KVTag::Required) {
                std::ostringstream oss;
                oss << "Configuration has no key " << kvd.describe() << ": " << conf << std::endl;
                throw DataModellingException(oss.str(), Here());
            }
            return toMissingOrDefaultValue(kvd);
        }

        if (conf.isNull(kvd.key)) {
            if constexpr (KVD::tag == KVTag::Required) {
                std::ostringstream oss;
                oss << "Key \"" << kvd.key << "\" in configuration should have a non-null value: " << conf << std::endl;
                throw DataModellingException(oss.str(), Here());
            }
            return toMissingOrDefaultValue(kvd);
        }


        if constexpr (KVD::hasMapper) {
            return visitNonNullValue(
                kvd.key, conf,
                eckit::Overloaded{[&]() {
                                      std::ostringstream oss;
                                      oss << "Unsupported value for Key " << kvd.describe()
                                          << " in configuration: " << conf << std::endl;
                                      throw DataModellingException(oss.str(), Here());

                                      return toMissingValue(kvd);  // unreachable
                                  },
                                  [&](auto tt) {
                                      using Type = typename std::decay_t<decltype(tt)>::type;
                                      return toKeyValue(kvd, kvd.mapper.read(getValueByType<Type>(conf, kvd.key)));
                                  }});
        }
        else {
            return visitNonNullValue(
                kvd.key, conf,
                eckit::Overloaded{[&]() {
                                      std::ostringstream oss;
                                      oss << "Unsupported value for Key " << kvd.describe()
                                          << " in configuration: " << conf << std::endl;
                                      throw DataModellingException(oss.str(), Here());

                                      return toMissingValue(kvd);  // unreachable
                                  },
                                  [&](auto tt) {
                                      using Type = typename std::decay_t<decltype(tt)>::type;
                                      if constexpr (std::is_same_v<Type, typename KVD::ValueType>) {
                                          return toKeyValue(kvd, getValueByType<Type>(conf, kvd.key));
                                      }
                                      else {
                                          std::ostringstream oss;
                                          oss << "Unsupported type " << util::typeToString<Type>() << " for Key "
                                              << kvd.describe() << " in configuration: " << conf << std::endl;
                                          throw DataModellingException(oss.str(), Here());
                                      }

                                      return toMissingValue(kvd);  // unreachable
                                  }});
        }


        return toMissingValue(kvd);  // unreachable - prevent compiler warning
    }
};


template <>
struct KeyValueReader<eckit::LocalConfiguration> : KeyValueReader<eckit::Configuration> {
    using Base = KeyValueReader<eckit::Configuration>;
    using Base::getByRef;
    using Base::getByValue;
};


//-----------------------------------------------------------------------------
// Writing to LocalConfiguration
//-----------------------------------------------------------------------------

template <>
struct KeyValueWriter<eckit::LocalConfiguration> {
    template <typename KVD, typename KV_, typename LConf,
              std::enable_if_t<(IsKeyValueDescription_v<std::decay_t<KVD>> && IsKeyValue_v<std::decay_t<KV_>>
                                && std::is_base_of_v<eckit::LocalConfiguration, std::decay_t<LConf>>),
                               bool>
              = true>
    static void set(const KVD& kvd, KV_&& kv, LConf& md) {
        using KV = std::decay_t<KV_>;
        // TODO think about handling missing value by setting Null ?
        if constexpr (KVD::hasMapper) {
            std::forward<KV_>(kv).visit(
                eckit::Overloaded{[&](MissingValue v) {},
                                  [&](auto&& v) { md.set(kvd.key, kvd.mapper.write(std::forward<decltype(v)>(v))); }});
        }
        else {
            std::forward<KV_>(kv).visit(eckit::Overloaded{
                [&](MissingValue v) {}, [&](auto&& v) { md.set(kvd.key, std::forward<decltype(v)>(v)); }});
        }
    }
};


//-----------------------------------------------------------------------------

}  // namespace multio::datamod
