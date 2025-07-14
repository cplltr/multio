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

#include <string>
#include <variant>
#include <vector>
#include "eckit/utils/Overloaded.h"
#include "multio/datamod/DataModelling.h"
#include "multio/datamod/MarsTypes.h"
#include "multio/datamod/ReaderWriter.h"
#include "multio/util/TypeTraits.h"

namespace py = pybind11;
namespace mio = multio;
namespace md = multio::datamod;
namespace mu = multio::util;

// Fake container that would allow specialization of `WriteSpec<>` in case some type needs customized conversion for
// python output
struct PyFakeContainer {};

const char* mapReservedName(const std::string& v) {
    if (v == "class") {
        return "klass";
    }
    return v.c_str();
}

// template <typename KS_>
// const std::vector<std::string> keyVect() {
//     static const std::vector<std::string> ret{([]() {
//         std::vector<std::string> vec;
//         mu::forEach([](const auto& kd) { vec.push_back(kd.key()); }, KS_{}.keys());
//         return vec;
//     })()};
//     return ret;
// }

template <auto id_, typename KS_ = md::KeySet<decltype(id_)>>
struct SetFtor {
    using KeyReadableTypes = typename md::KeyValue<id_>::ReadWrite::ReadableTypes;
    using KeyDefinition = typename md::KeyValue<id_>::Definition;

    using ReadableTypes = mu::MergeTypeList_t<
        std::conditional_t<KeyDefinition::hasDefaultValueFunctor, mu::TypeList<>, mu::TypeList<py::none>>,
        KeyReadableTypes>;

    using ValType = mu::ApplyTypeList_t<std::variant, ReadableTypes>;
    void operator()(md::KeyValueSet<KS_>& kvs, ValType val) const {
        std::visit(eckit::Overloaded{[&](py::none) { kvs.set<id_>(md::MissingValue{}); },
                                     [&](auto&& vi) { kvs.set<id_>(std::forward<decltype(vi)>(vi)); }},
                   std::move(val));
    }
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

    RetVar operator()(const md::KeyValueSet<KS_>& kvs) const {
        if (kvs.template key<id_>().isMissing()) {
            return py::none{};
        }
        if constexpr (doWrite) {
            return ReadWrite::template write<PyFakeContainer>(kvs.template get<id_>());
        }
        else {
            return kvs.template key<id_>().get();
        }
        return py::none{};  // Unreachable avoid compiler warning on clang
    }
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
        return py::arg(mapReservedName(kd.key())) = RetVar{kd.defaultValue()};
        // return py::arg(kd.key().value().c_str()) = RetVar{kd.defaultValue()};
    }
    return py::arg(mapReservedName(kd.key())) = RetVar{py::none{}};
    // return py::arg(kd.key().value().c_str()) = RetVar{py::none{}};
}

template <typename KS, typename MOD>
decltype(auto) addKeyValueSet(MOD&& m, const std::string& className) {
    using KVS = md::KeyValueSet<KS>;
    auto ret = py::class_<KVS>(m, className.c_str());

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
        oss << kvs;
        return oss.str();
    });

    mu::forEach(
        [&](const auto& kd) {
            using KD = std::decay_t<decltype(kd)>;
            ret.def_property(mapReservedName(kd.key().value()), GetFtor<KD::id>{}, SetFtor<KD::id>{},
                             std::string(md::keyDef<KD::id>().description().value_or("")).c_str());
        },
        KS{});

    return ret;
};

PYBIND11_MODULE(pyencode_mtg2, m) {
    makeEnum(m, "LevType", md::allLevTypes());

    addKeyValueSet<md::MarsKeySet>(m, "Mars");

    addKeyValueSet<md::MiscKeySet>(m, "Misc");
}


// TODO
// make stuff printable
// more docstrings?
// Export AlL some exceptions
//

