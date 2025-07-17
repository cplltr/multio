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

// template<typename KeySet_>
// declytpe(auto) pyInitForKeySet(const KeySet_& ks) {
//     return std::apply([](const auto& kd){
//         return py::init<>()
//     }, ks.keys())
// }

template <auto id_>
using RetVar = std::variant<py::none, multio::datamod::KeyDefValueType_t<id_>>;

template <auto id_, typename KS_ = md::KeySet<decltype(id_)>>
struct GetFtor {
    using ReadWrite = typename md::KeyValue<id_>::ReadWrite;
    
    RetVar<id_> operator()(const md::KeyValueSet<KS_>& kvs) const {
        if (kvs.template key<id_>().isMissing()) {
            return py::none{};
        }
        // ReadWrite::write(kvs.template key<id_>());
        return kvs.template key<id_>().get();
    }
};

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

template <typename MOD, typename Enum>
decltype(auto) makeEnum(MOD&& m, const std::string& enumName, const std::vector<Enum>& values) {
    auto ret = py::enum_<Enum>(m, enumName.c_str());
    for (auto v : values) {
        ret.value(md::Writer<Enum>::write(v).c_str(), v);
    }
    return ret.export_values();
};

PYBIND11_MODULE(pyencode_mtg2, m) {
    // py::class_<md::MissingValue>(m, "MissingValue")
    //     .def(py::init<>());
    makeEnum(m, "LevType", md::allLevTypes());

    py::class_<md::MarsKeyValueSet>(m, "Mars")
        .def(py::init<>())
        .def_property("param", GetFtor<md::MarsKeys::PARAM>{}, SetFtor<md::MarsKeys::PARAM>{})
        .def_property(
            "levtype", GetFtor<md::MarsKeys::LEVTYPE>{},
            SetFtor<md::MarsKeys::LEVTYPE>{})
        .def("alterAndValidate", [](md::MarsKeyValueSet& mars) { md::alterAndValidate(mars); });
}
