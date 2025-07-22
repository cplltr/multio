/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */
// #include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
// #include <pybind11/stl/filesystem.h>

#include <multio/datamod/MarsMiscGeo.h>

#include <algorithm>
#include <string>
#include <variant>
#include <vector>
#include <cstdint>

#include "eccodes.h"

#include "eckit/utils/Overloaded.h"
#include "multio/datamod/DataModelling.h"
#include "multio/datamod/DataModellingException.h"
#include "multio/datamod/MarsTypes.h"
#include "multio/datamod/ReaderWriter.h"
#include "multio/util/TypeTraits.h"

#include "multio/action/encode-mtg2/AtlasGeoSetter.h"
#include "multio/action/encode-mtg2/EncoderCache.h"
#include "multio/action/encode-mtg2/EncoderConf.h"
#include "multio/action/encode-mtg2/Options.h"
#include "multio/action/encode-mtg2/Rules.h"


namespace py = pybind11;
namespace mio = multio;
namespace md = multio::datamod;
namespace ma = multio::action;
namespace mar = multio::action::rules;
namespace mu = multio::util;


namespace multio::datamod {
template <>
struct WriteSpec<eckit::PathName> {
    static std::string write(const eckit::PathName& n) { return std::string(n); }
};
template <>
struct ReadableTypes<eckit::PathName> {
    using type = util::TypeList<std::string>;
};
template <>
struct ReadSpec<eckit::PathName> {
    static eckit::PathName read(const std::string& s) { return eckit::PathName{s}; };
};
};  // namespace multio::datamod


namespace multio::util {
template <>
struct TypeToString<py::none> {
    std::string operator()() const { return "None"; };
};
}  // namespace multio::util

// Fake container that would allow specialization of `WriteSpec<>` in case some type needs customized conversion for
// python output
struct PyFakeContainer {};

// const char* mapReservedName(const std::string& v) {
//     if (v == "class") {
//         return "klass";
//     }
//     return v.c_str();
// }
template <auto id_>
const char* getValidPyKey(const md::ScopedKey<id_>& kd) {
    // Map some reserved keys
    if (kd.key().value() == "class") {
        return "klass";
    }

    // Each key is templated - so we can use a static for each key to properly return a modified const char *
    static std::optional<std::string> mappedKey;
    if (!mappedKey) {
        mappedKey = kd.key().value();
        std::replace(mappedKey->begin(), mappedKey->end(), '-', '_');
    }

    return mappedKey->c_str();

    // return kd.key().value().c_str();
}

template <typename KS_>
const auto& keyInfoVect() {
    static const auto ret{([]() {
        std::vector<std::reference_wrapper<const md::DynKeyInfo>> vec;
        mu::forEach([&](const auto& kd) { vec.push_back(std::cref(static_cast<const md::DynKeyInfo&>(kd))); },
                    KS_{}.keys());
        return vec;
    })()};
    return ret;
}

template <typename KS_>
bool containsKey(const std::string& k) {
    return (std::find_if(keyInfoVect<KS_>().begin(), keyInfoVect<KS_>().end(),
                         [&](auto ref) { return ref.get().key().value() == k; })
            != keyInfoVect<KS_>().end());
};


template <auto id_, typename KS_ = md::KeySet<decltype(id_)>>
struct SetFtor {
    using KeyReadableTypes = typename md::KeyValue<id_>::ReadWrite::ReadableTypes;
    using KeyDefinition = typename md::KeyValue<id_>::Definition;

    using ReadableTypes = mu::MergeTypeList_t<
        std::conditional_t<KeyDefinition::hasDefaultValueFunctor, mu::TypeList<>, mu::TypeList<py::none>>,
        KeyReadableTypes>;

    template <typename T, std::enable_if_t<std::is_same_v<std::decay_t<T>, py::none>, bool> = true>
    void setHelper(md::KeyValueSet<KS_>& kvs, T&&) const {
        kvs.template set<id_>(md::MissingValue{});
    }
    template <typename T, std::enable_if_t<(!std::is_same_v<std::decay_t<T>, py::none>
                                            && mu::TypeListContains_v<std::decay_t<T>, ReadableTypes>),
                                           bool>
                          = true>
    void setHelper(md::KeyValueSet<KS_>& kvs, T&& val) const {
        kvs.template set<id_>(std::forward<T>(val));
    }
    template <typename T, std::enable_if_t<(!std::is_same_v<std::decay_t<T>, py::none>
                                            && !mu::TypeListContains_v<std::decay_t<T>, ReadableTypes>),
                                           bool>
                          = true>
    void setHelper(md::KeyValueSet<KS_>& kvs, T&& val) const {
        std::ostringstream oss;
        oss << "Key " << md::key<id_>().keyInfo() << " can not be constructed from "
            << mu::typeToString<std::decay_t<T>>();
        oss << ". Accepted types: " << mu::typeToString<ReadableTypes>();
        throw md::DataModellingException(oss.str(), Here());
    }

    // Set for any variant -- to be specialized at different places
    template <typename... T>
    void set(md::KeyValueSet<KS_>& kvs, std::variant<T...> val) const {
        std::visit([&](auto&& vi) { this->setHelper(kvs, std::forward<decltype(vi)>(vi)); }, std::move(val));
    }

    // Explicit operator with definit type signature
    using ValType = mu::ApplyTypeList_t<std::variant, ReadableTypes>;
    void operator()(md::KeyValueSet<KS_>& kvs, ValType val) const { set(kvs, std::move(val)); }
};

template <typename... KD>
struct AllSetVar {
    using type = mu::ApplyTypeList_t<
        std::variant,
        mu::UniqueTypeList_t<mu::MergeTypeList_t<mu::TypeList<py::none>, typename SetFtor<KD::id>::ReadableTypes...>>>;
};

template <typename... KD>
struct AllSetVar<std::tuple<KD...>> {
    using type = typename AllSetVar<KD...>::type;
};

template <typename KS>
struct AllSetVarForKeySet {
    using type = typename AllSetVar<typename KS::TupleType>::type;
};

template <auto id_, typename KS_ = md::KeySet<decltype(id_)>>
struct GetFtor {
    using ReadWrite = typename md::KeyValue<id_>::ReadWrite;
    using ReadableTypes = typename SetFtor<id_, KS_>::ReadableTypes;

    // Some types are not exported but rather converted back to int or string via wrie
    static constexpr bool doWrite = !mu::TypeListContains_v<md::KeyDefValueType_t<id_>, ReadableTypes>;
    using RetValue = std::conditional_t<
        doWrite,
        std::decay_t<decltype(ReadWrite::template write<PyFakeContainer>(std::declval<md::KeyDefValueType_t<id_>>()))>,
        md::KeyDefValueType_t<id_>>;

    using RetVar = std::variant<py::none, RetValue>;

    template <typename CustomRet>
    CustomRet get(const md::KeyValueSet<KS_>& kvs) const {
        if (kvs.template key<id_>().isMissing()) {
            return {};
        }
        if constexpr (doWrite) {
            return ReadWrite::template write<PyFakeContainer>(kvs.template get<id_>());
        }
        else {
            return kvs.template key<id_>().get();
        }
        return {};  // Unreachable avoid compiler warning on clang
    }

    RetVar operator()(const md::KeyValueSet<KS_>& kvs) const { return get<RetVar>(kvs); }
};

template <typename... KD>
struct AllGetVar {
    using type
        = mu::ApplyTypeList_t<std::variant,
                              mu::UniqueTypeList_t<mu::TypeList<py::none, typename GetFtor<KD::id>::RetValue...>>>;
};

template <typename MOD, typename Enum>
decltype(auto) makeEnum(MOD&& m, const std::string& enumName, const std::vector<Enum>& values) {
    auto ret = py::enum_<Enum>(m, enumName.c_str());
    for (auto v : values) {
        ret.value(md::Writer<Enum>::write(v).c_str(), v);
    }
    return ret.export_values();
};

template <typename KD>
decltype(auto) makeArg(const KD& kd) {
    using KeyDefinition = typename md::KeyValue<KD::id>::Definition;
    using RetVar = typename GetFtor<KD::id>::RetVar;
    if constexpr (KeyDefinition::hasDefaultValueFunctor) {
        // todo use py:::arg_v to pass string repres
        return py::arg(getValidPyKey(kd)) = RetVar{kd.defaultValue()};
    }
    return py::arg(getValidPyKey(kd)) = RetVar{py::none{}};
}

std::string makeKeyInfo(const md::DynKeyInfo& ki) {
    return ki.keyInfo()
         + (ki.description() ? (std::string(": ") + std::string(ki.description().value_or(""))) : std::string(""));
}
template <auto id_>
const char* makeKeyInfo(const md::ScopedKey<id_>& ki) {
    static std::optional<std::string> info;

    if (!info) {
        info = makeKeyInfo(static_cast<const md::DynKeyInfo&>(ki));
    }
    return info->c_str();
}


template <typename KS, typename MOD>
decltype(auto) addKeyValueSet(MOD&& m, const std::string& className) {
    using KVS = md::KeyValueSet<KS>;
    auto ret = py::class_<KVS>(m, className.c_str());

    // Build init function
    std::apply(
        [&](const auto&... kd) {
            // Build a lambda that takes all keys with the python represented values (defined through SetFtor)
            // and then use the SetFtor to set them on a default initiated KeyValueSet
            // Effectively given default arguments are applied from python here
            ret.def(py::init([](typename SetFtor<std::decay_t<decltype(kd)>::id, KS>::ValType... args) {
                        KVS kvs;
                        (SetFtor<std::decay_t<decltype(kd)>::id, KS>{}(kvs, std::move(args)), ...);
                        return kvs;
                    }),
                    makeArg(kd)...);
        },
        KS{}.keys());

    ret.def("alterAndValidate", [](KVS& kvs) { md::alterAndValidate(kvs); });
    ret.def("__repr__", [](const KVS& kvs) {
        std::ostringstream oss;
        // customPrintKeyValueSet(kvs);
        oss << kvs;
        return oss.str();
    });

    ret.def(
        "keys", [](const KVS&) { return py::make_iterator(keyInfoVect<KS>().begin(), keyInfoVect<KS>().end()); },
        py::keep_alive<0, 1>());
    ret.def(
        "__iter__", [](const KVS&) { return py::make_iterator(keyInfoVect<KS>().begin(), keyInfoVect<KS>().end()); },
        py::keep_alive<0, 1>());

    // Complex function just to allow getting indexes with []
    // Convenient to access by string or key infor from iterator
    ret.def("__getitem__", [](const KVS& kvs,
                              std::variant<std::string, std::reference_wrapper<const md::DynKeyInfo>> k) {
        std::string kStr = std::holds_alternative<std::string>(k) ? std::get<0>(k) : std::get<1>(k).get().key().value();
        if (!containsKey<KS>(kStr)) {
            throw py::key_error(kStr);
        }
        return std::apply(
            [&](const auto&... kd) {
                // Build a big variant with all possible return types
                using RetVar = typename AllGetVar<std::decay_t<decltype(kd)>...>::type;
                RetVar ret;
                (((kd.key().value() == kStr)
                      ? (ret = GetFtor<std::decay_t<decltype(kd)>::id, KS>{}.template get<RetVar>(kvs))
                      : ret),
                 ...);
                return ret;
            },
            KS{}.keys());
    });

    // Complex function just to allow getting indexes with []
    // Convenient to access by string or key infor from iterator
    ret.def("__setitem__", [](KVS& kvs, std::variant<std::string, std::reference_wrapper<const md::DynKeyInfo>> k,
                              typename AllSetVarForKeySet<KS>::type val) {
        std::string kStr = std::holds_alternative<std::string>(k) ? std::get<0>(k) : std::get<1>(k).get().key().value();
        if (!containsKey<KS>(kStr)) {
            throw py::key_error(kStr);
        }
        return std::apply(
            [&](const auto&... kd) {
                (((kd.key().value() == kStr)
                      ? ((SetFtor<std::decay_t<decltype(kd)>::id, KS>{}.set(kvs, std::move(val))), 0)
                      : 0),
                 ...);
            },
            KS{}.keys());
    });

    ret.def("__contains__", [](KVS& kvs, std::variant<std::string, std::reference_wrapper<const md::DynKeyInfo>> k) {
        std::string kStr = std::holds_alternative<std::string>(k) ? std::get<0>(k) : std::get<1>(k).get().key().value();
        return containsKey<KS>(kStr);
    });

    // Most useful and important part:
    // Defining the properties
    mu::forEach(
        [&](const auto& kd) {
            using KD = std::decay_t<decltype(kd)>;
            ret.def_property(getValidPyKey(kd), GetFtor<KD::id, KS>{}, SetFtor<KD::id, KS>{}, makeKeyInfo(kd));
        },
        KS{});

    return ret;
};

// Build geometry from mars and possibly infer values
md::Geometry makeGeometry(const md::MarsKeyValueSet& mars, bool inferGeo = true) {
    return std::visit(
        [&](const auto& geoKS) -> md::Geometry {
            // Create it unscoped...
            md::KeyValueSet<std::decay_t<decltype(geoKS)>> ret{};
            const auto& grid = md::key<md::MarsKeys::GRID>(mars);
            if (inferGeo && grid.has()) {
                ma::extract::setKeysFromAtlas(ret, grid.get());
            }
            return ret;
        },
        md::getGeometryKeySet(mars));
}


PYBIND11_MODULE(pyencode_mtg2, m) {
    makeEnum(m, "LevType", md::allLevTypes());

    auto keyInfo
        = py::class_<md::DynKeyInfo>(m, "KeyInfo")
              .def_property_readonly(
                  "key", [](const md::DynKeyInfo& ki) { return ki.key().value(); }, "Key name")
              .def_property_readonly(
                  "info", [](const md::DynKeyInfo& ki) { return ki.keyInfo(); },
                  "Short information - key name, type, optional")
              .def_property_readonly(
                  "scope", [](const md::DynKeyInfo& ki) { return ki.initScope(); },
                  "Scope/keyset in which the key is defined/used")
              .def_property_readonly(
                  "description", [](const md::DynKeyInfo& ki) { return std::string(ki.description().value_or("")); },
                  "Longer description of the key")
              .def("__repr__", [](const md::DynKeyInfo& ki) { return makeKeyInfo(ki); });
    keyInfo.doc() = "Abstract object to retrieve information about key, type, scope and a description";


    addKeyValueSet<md::MiscKeySet>(m, "Misc").doc()
        = "Set of additional keys that are required to encode a grib file with the details it should have.";

    addKeyValueSet<md::KeySet<md::GeoGG>>(m, "GeoGG").doc() = "Set of additional keys to describe gaussian grids";
    addKeyValueSet<md::KeySet<md::GeoSH>>(m, "GeoSH").doc() = "Set of additional keys to describe spherical harmonics";
    addKeyValueSet<md::KeySet<md::GeoHEALPix>>(m, "GeoHEALPix").doc()
        = "Set of additional keys to describe HEALPix grids";

    auto mars = addKeyValueSet<md::MarsKeySet>(m, "Mars");
    mars.doc() = "Set of descriptive MARS keys that are used to properly produce grib2";
    mars.def(
        "makeGeometry",
        [](const md::MarsKeyValueSet& mars, bool inferGeo = true) { return makeGeometry(mars, inferGeo); },
        py::arg("infer_geo") = true);

    addKeyValueSet<ma::EncodeMtg2KeySet>(m, "EncoderConf").doc()
        = "Configuration for the encoder. Most of the arguments are passed through to MultIOM.";

    auto encoder = py::class_<ma::EncoderCache>(m, "Encoder")  //
                       .def(py::init<const ma::EncodeMtg2Conf&>(), py::arg("conf") = ([]() {
                                                                       ma::EncodeMtg2Conf conf{};
                                                                       alterAndValidate(conf);
                                                                       return conf;
                                                                   }()))
                       .def("getSample", [](ma::EncoderCache& enc, const md::MarsKeyValueSet& mars,
                                            const md::MiscKeyValueSet& misc, const md::Geometry& geo) {
                           // auto gribapi = py::module::import("gribapi");
                           auto sample = enc.getSample(mars, misc, geo);
                           codes_handle* h = codes_handle_clone(sample.get()->raw());
                           // Very unsafe - but eccodes gribapi does these things
                           return reinterpret_cast<std::uintptr_t>(h);
                       });
    encoder.doc()
        = "Encoder object that performs mars to grib mapping by preparing a grib2 handle with metadata being preset.";

    // addKeyValueSet<ma::EncoderSectionsKeySet>(m, "EncoderSections").doc()
    //     = "Intermediate configuration for a specific mars keyset";
    // mars.def("buildEncoderConf", [](const md::MarsKeyValueSet& mars) { mar::buildEncoderConf(mars); });
}


