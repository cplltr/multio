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

#include <multio/datamod/MarsMiscGeo.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "eccodes.h"

#include "multio/action/encode/GribEncoder.h"
#include "multio/datamod/AtlasGeo.h"
#include "multio/datamod/core/DataModellingException.h"

#include "multio/mars2mars/Mars2MarsException.h"
#include "multio/mars2grib/Mars2GribException.h"
#include "multio/mars2grib/api/StableAPI.h"
#include "multio/util/Print.h"
#include "multio/util/TypeTraits.h"

#include "multio/datamod/core/EntryDef.h"
#include "multio/datamod/core/Record.h"

#include "multio/mars2grib/api/RawAPI.h"


namespace py = pybind11;
namespace mio = multio;
namespace dm = multio::datamod;
namespace mu = multio::util;
namespace m2g = multio::mars2grib;
namespace m2m = multio::mars2mars;


namespace multio::util {
template <>
struct TypeToString<py::none> {
    std::string operator()() const { return "None"; };
};
}  // namespace multio::util

// Fake container that would allow specialization of `WriteSpec<>` in case some type needs customized conversion for
// python output
struct PyFakeContainer {};

template <typename EntryDef_>
const char* getValidPyKey(const EntryDef_& ed) {
    // Map some reserved keys
    if (ed.key().value() == "class") {
        return "klass";
    }

    // Each key is templated - so we can use a static for each key to properly return a modified const char *
    static std::optional<std::string> mappedKey;
    if (!mappedKey) {
        mappedKey = ed.key().value();
        std::replace(mappedKey->begin(), mappedKey->end(), '-', '_');
    }

    return mappedKey->c_str();
}

template <typename Rec>
bool containsKey(const std::string& k) {
    return std::apply([&](const auto&... entryDef) { return ((entryDef.key().value() == k) || ... || false); },
                      dm::recordEntries<Rec>());
};


template <typename EntryDef, typename Record>
struct SetFtor {
    using KeyParsableTypes = typename EntryDef::ParserDumper::ParsableTypes;

    using ParsableTypes = mu::MergeTypeList_t<
        std::conditional_t<EntryDef::hasDefaultValueFunctor, mu::TypeList<>, mu::TypeList<py::none>>, KeyParsableTypes>;

    const EntryDef& entryDef;

    template <typename Rec, typename T, std::enable_if_t<std::is_same_v<std::decay_t<T>, py::none>, bool> = true>
    void setHelper(Rec& rec, T&&) const {
        entryDef.get(rec).unset();
    }

    template <typename T, std::enable_if_t<(!std::is_same_v<std::decay_t<T>, py::none>
                                            && mu::TypeListContains_v<std::decay_t<T>, ParsableTypes>),
                                           bool>
                          = true>
    void setHelper(Record& rec, T&& val) const {
        entryDef.get(rec).set(std::forward<T>(val));
    }

    template <typename T, std::enable_if_t<(!std::is_same_v<std::decay_t<T>, py::none>
                                            && !mu::TypeListContains_v<std::decay_t<T>, ParsableTypes>),
                                           bool>
                          = true>
    void setHelper(Record& rec, T&& val) const {
        std::ostringstream oss;
        oss << "Key " << entryDef.keyInfo() << " can not be constructed from " << mu::typeToString<std::decay_t<T>>();
        oss << ". Accepted types: " << mu::typeToString<ParsableTypes>();
        throw dm::DataModellingException(oss.str(), Here());
    }

    // Set for any variant -- to be specialized at different places
    template <typename... T>
    void set(Record& rec, std::variant<T...> val) const {
        std::visit([&](auto&& vi) { this->setHelper(rec, std::forward<decltype(vi)>(vi)); }, std::move(val));
    }

    // Explicit operator with definit type signature
    using ValType = mu::ApplyTypeList_t<std::variant, ParsableTypes>;

    void operator()(Record& rec, ValType val) const { set(rec, std::move(val)); }
};

template <typename Record, typename... EntryDef>
struct AllSetVar {
    using type = mu::ApplyTypeList_t<
        std::variant, mu::UniqueTypeList_t<mu::MergeTypeList_t<mu::TypeList<py::none>,
                                                               typename SetFtor<EntryDef, Record>::ParsableTypes...>>>;
};

template <typename Record, typename... EntryDef>
struct AllSetVar<Record, std::tuple<EntryDef...>> {
    using type = typename AllSetVar<Record, EntryDef...>::type;
};


template <typename Rec>
struct AllSetVarForRecord {
    using type = typename AllSetVar<Rec, std::decay_t<decltype(dm::recordEntries<Rec>())>>::type;
};

template <typename EntryDef, typename Record>
struct GetFtor {
    using ParserDumper = typename EntryDef::ParserDumper;
    using ParsableTypes = typename SetFtor<EntryDef, Record>::ParsableTypes;

    // Some types are not exported but rather converted back to int or string via wrie
    static constexpr bool doWrite = !mu::TypeListContains_v<dm::EntryValueType_t<EntryDef>, ParsableTypes>;
    using RetValue = std::conditional_t<doWrite,
                                        std::decay_t<decltype(ParserDumper::template dumpTo<PyFakeContainer>(
                                            std::declval<dm::EntryValueType_t<EntryDef>>()))>,
                                        dm::EntryValueType_t<EntryDef>>;

    using RetVar = std::variant<py::none, RetValue>;

    const EntryDef& entryDef;

    template <typename CustomRet>
    CustomRet get(const Record& rec) const {
        if (!entryDef.get(rec).isSet()) {
            return {};
        }
        if constexpr (doWrite) {
            return ParserDumper::template write<PyFakeContainer>(entryDef.get(rec));
        }
        else {
            return entryDef.get(rec).get();
        }
        return {};  // Unreachable avoid compiler warning on clang
    }

    RetVar operator()(const Record& rec) const { return get<RetVar>(rec); }
};

// Helper to get a variant containing all possibile types for any of the entries in a record
template <typename Record, typename... EntryDef>
struct AllGetVar {
    using type = mu::ApplyTypeList_t<
        std::variant, mu::UniqueTypeList_t<mu::TypeList<py::none, typename GetFtor<EntryDef, Record>::RetValue...>>>;
};


template <typename MOD, typename Enum>
decltype(auto) makeEnum(MOD&& m, const std::string& enumName, const std::vector<Enum>& values) {
    auto ret = py::enum_<Enum>(m, enumName.c_str());
    for (auto v : values) {
        ret.value(dm::TypeDumper<Enum>::dump(v).c_str(), v);
    }
    return ret.export_values();
};

template <typename Record, typename EntryDef>
decltype(auto) makeArg(const EntryDef& entryDef) {
    using RetVar = typename GetFtor<EntryDef, Record>::RetVar;

    if constexpr (EntryDef::hasDefaultValueFunctor) {
        // todo use py:::arg_v to pass string repres
        return py::arg(getValidPyKey(entryDef)) = RetVar{entryDef.defaultValue()};
    }
    return py::arg(getValidPyKey(entryDef)) = RetVar{py::none{}};
}


template <typename EntryDef>
const char* makeKeyInfo(const EntryDef& entryDef) {
    static std::optional<std::string> info;

    if (!info) {
        info = entryDef.keyInfo()
             + (entryDef.description() ? (std::string(": ") + std::string(entryDef.description().value_or("")))
                                       : std::string(""));
    }
    return info->c_str();
}

template <typename Record>
const std::vector<std::string>& recordKeys() {
    static std::vector<std::string> keys
        = std::apply([](const auto&... entryDef) { return std::vector<std::string>{{entryDef.key()...}}; },
                     dm::recordEntries<Record>());

    return keys;
}


template <typename Record, typename MOD>
decltype(auto) addRecord(MOD&& m, const std::string& className) {
    auto ret = py::class_<Record>(m, className.c_str());

    // Build init function
    std::apply(
        [&](const auto&... entryDef) {
            // Build a lambda that takes all keys with the python represented values (defined through SetFtor)
            // and then use the SetFtor to set them on a default initiated KeyValueSet
            // Effectively given default arguments are applied from python here
            ret.def(py::init([&](typename SetFtor<std::decay_t<decltype(entryDef)>, Record>::ValType... args) {
                        Record rec;
                        (SetFtor<std::decay_t<decltype(entryDef)>, Record>{entryDef}(rec, std::move(args)), ...);
                        return rec;
                    }),
                    makeArg<Record>(entryDef)...);
        },
        dm::recordEntries<Record>());

    ret.def("applyDefaults", [](Record& rec) { dm::applyRecordDefaults(rec); });
    ret.def("validate", [](const Record& rec) { dm::validateRecord(rec); });
    ret.def("__repr__", [](const Record& rec) {
        std::ostringstream oss;
        mu::PrintStream ps(oss);
        ps << rec;
        return oss.str();
    });

    ret.def(
        "keys",
        [](const Record&) { return py::make_iterator(recordKeys<Record>().begin(), recordKeys<Record>().end()); },
        py::keep_alive<0, 1>());
    ret.def(
        "__iter__",
        [](const Record&) { return py::make_iterator(recordKeys<Record>().begin(), recordKeys<Record>().end()); },
        py::keep_alive<0, 1>());

    // Complex function just to allow getting indexes with []
    // Convenient to access by string or key infor from iterator
    ret.def("__getitem__", [](const Record& rec, const std::string& k) {
        if (!containsKey<Record>(k)) {
            throw py::key_error(k);
        }
        return std::apply(
            [&](const auto&... entryDef) {
                // Build a big variant with all possible return types
                using RetVar = typename AllGetVar<Record, std::decay_t<decltype(entryDef)>...>::type;
                RetVar ret;
                (((entryDef.key().value() == k)
                      ? (ret = GetFtor<std::decay_t<decltype(entryDef)>, Record>{entryDef}.template get<RetVar>(rec))
                      : ret),
                 ...);
                return ret;
            },
            dm::recordEntries<Record>());
    });

    // Complex function just to allow getting indexes with []
    // Convenient to access by string or key infor from iterator
    ret.def("__setitem__", [](Record& rec, const std::string& k, typename AllSetVarForRecord<Record>::type val) {
        if (!containsKey<Record>(k)) {
            throw py::key_error(k);
        }
        mu::forEach(
            [&](const auto& entryDef) {
                if (entryDef.key().value() == k) {
                    SetFtor<std::decay_t<decltype(entryDef)>, Record>{entryDef}.set(rec, std::move(val));
                }
            },
            dm::recordEntries<Record>());
    });

    ret.def("__contains__", [](Record& rec, const std::string& k) { return containsKey<Record>(k); });

    // Most useful and important part:
    // Defining the properties
    mu::forEach(
        [&](const auto& entryDef) {
            ret.def_property(getValidPyKey(entryDef), GetFtor<std::decay_t<decltype(entryDef)>, Record>{entryDef},
                             SetFtor<std::decay_t<decltype(entryDef)>, Record>{entryDef}, makeKeyInfo(entryDef));
        },
        dm::recordEntries<Record>());

    return ret;
};

std::uintptr_t toPyCodesHandle(std::unique_ptr<multio::action::encode::MioGribHandle> handle) {
    codes_handle* h = codes_handle_clone(handle.get()->raw());
    // Very unsafe - but eccodes gribapi does these things
    return reinterpret_cast<std::uintptr_t>(h);
}
std::uintptr_t toPyCodesHandle(std::unique_ptr<codes_handle> handle) {
    codes_handle* h = codes_handle_clone(handle.get());
    // Very unsafe - but eccodes gribapi does these things
    return reinterpret_cast<std::uintptr_t>(h);
}


PYBIND11_MODULE(pyencode_mtg2, m) {
    py::register_exception<dm::DataModellingException>(m, "DataModellingException");
    py::register_exception<m2m::Mars2MarsException>(m, "Mars2MarsException");
    py::register_exception<m2g::Mars2GribException>(m, "Mars2GribException");


    //-------------------------------------------------------------------------
    // Raw API
    //-------------------------------------------------------------------------
    makeEnum(m, "LevType", dm::allLevTypes());

    addRecord<dm::MiscRecord>(m, "MiscRecord").doc()
        = "Set of additional keys that are required to encode a grib file with the details it should have.";

    addRecord<dm::GeoGGRecord>(m, "GeoGGRecord").doc() = "Set of additional keys to describe gaussian grids";
    addRecord<dm::GeoLLRecord>(m, "GeoLLRecord").doc() = "Set of additional keys to describe lat/lons";
    addRecord<dm::GeoSHRecord>(m, "GeoSHRecord").doc() = "Set of additional keys to describe spherical harmonics";
    addRecord<dm::GeoHEALPixRecord>(m, "GeoHEALPixRecord").doc() = "Set of additional keys to describe HEALPix grids";

    auto mars = addRecord<dm::FullMarsRecord>(m, "MarsRecord");
    mars.doc() = "Set of descriptive MARS keys that are used to properly produce grib2";
    mars.def("makeGeometry", [](const dm::FullMarsRecord& mars) { return dm::makeUnscopedGeometry(mars); });

    auto mars2gribRaw
        = py::class_<m2g::Mars2GribRaw>(m, "Mars2GribRaw")  //
              .def(py::init([](bool cached) { return m2g::Mars2GribRaw{{cached = true}}; }), py::arg("cached") = true)
              .def("getHandle",
                   [](m2g::Mars2GribRaw& enc, const dm::FullMarsRecord& mars, const dm::MiscRecord& misc,
                      const dm::Geometry& geo) { return toPyCodesHandle(enc.getHandle(mars, misc, geo)); });

    mars2gribRaw.doc()
        = "Mars2Grib RawAPI interface: object that performs mars to grib mapping by preparing a grib2 handle with "
          "metadata being preset.";


    //-------------------------------------------------------------------------
    // Stable API
    //-------------------------------------------------------------------------
    auto geometryType = py::enum_<m2g::GeometryType>(m, "GeometryType");
    // TODO (pgeier) don't put the raw strings here
    geometryType("gg", m2g::GeometryType::GG);
    geometryType("ll", m2g::GeometryType::LL);
    geometryType("sh", m2g::GeometryType::SH);
    geometryType("HEALPix", m2g::GeometryType::HEALPix);

    auto marsValues = py::class_<m2g::MarsValues>(m, "MarsValues")
                          .def(py::init())
                          .def("set", [](m2g::MarsValues& marsValues, const std::string& key,
                                         std::int64_t value) { marsValues.set(key, value); })
                          .def("set", [](m2g::MarsValues& marsValues, const std::string& key,
                                         const std::string& value) { marsValues.set(key, value); });

    marsValues.doc()
        = "Mars2Grib MarsValues: stable interfacing object for setting relevant MARS keys. The detailed keys are "
          "listed up in the RawAPI.";


    auto geometryValues = py::class_<m2g::GeometryValues>(m, "GeometryValues")
                              .def(py::init())
                              .def("setType", [](m2g::GeometryValues& geometryValues,
                                                 const std::string& type) { geometryValues.setGeometryType(type); })
                              .def("setType", [](m2g::GeometryValues& geometryValues,
                                                 m2g::GeometryType type) { geometryValues.setGeometryType(type); })
                              .def("set", [](m2g::GeometryValues& geometryValues, const std::string& key,
                                             std::int64_t value) { geometryValues.set(key, value); })
                              .def("set", [](m2g::GeometryValues& geometryValues, const std::string& key,
                                             const std::string& value) { geometryValues.set(key, value); })
                              .def("set", [](m2g::GeometryValues& geometryValues, const std::string& key,
                                             std::reference_wrapper<const std::vector<double>> value) {
                                  geometryValues.set(key, value);
                              });

    geometryValues.doc()
        = "Mars2Grib GeometryValues: stable interfacing object for setting geometry information. First the "
          "`GeometryType` must be specified: gg, ll, sh, HEALPix";

    auto additionalValues
        = py::class_<m2g::AdditionalValues>(m, "AdditionalValues")
              .def(py::init())
              .def("setTablesVersion",
                   [](m2g::AdditionalValues& values, std::int64_t value) { values.setTablesVersion(value); })
              .def("setGeneratingProcessIdentifier",
                   [](m2g::AdditionalValues& values, std::int64_t value) {
                       values.setGeneratingProcessIdentifier(value);
                   })
              .def("setGeneratingProcessIdentifier",
                   [](m2g::AdditionalValues& values, std::int64_t value) {
                       values.setGeneratingProcessIdentifier(value);
                   })
              .def("setTypeOfProcessedData",
                   [](m2g::AdditionalValues& values, std::int64_t value) { values.setTypeOfProcessedData(value); })
              .def("setInitialStep",
                   [](m2g::AdditionalValues& values, std::int64_t value) { values.setInitialStep(value); })
              .def("setTimeIncrementInSeconds",
                   [](m2g::AdditionalValues& values, std::int64_t value) { values.setTimeIncrementInSeconds(value); })
              .def("setLengthOfTimeWindowInSeconds",
                   [](m2g::AdditionalValues& values, std::int64_t value) {
                       values.setLengthOfTimeWindowInSeconds(value);
                   })
              .def("setBitmapPresent",
                   [](m2g::AdditionalValues& values, bool value) { values.setBitmapPresent(value); })
              .def("setMissingValue",
                   [](m2g::AdditionalValues& values, double value) { values.setMissingValue(value); })
              .def("setTypeOfEnsembleForecast",
                   [](m2g::AdditionalValues& values, std::int64_t value) { values.setTypeOfEnsembleForecast(value); })
              .def("setNumberOfForecastsInEnsemble",
                   [](m2g::AdditionalValues& values, std::int64_t value) {
                       values.setNumberOfForecastsInEnsemble(value);
                   })
              .def("setSatelliteSeries",
                   [](m2g::AdditionalValues& values, std::int64_t value) { values.setSatelliteSeries(value); })
              .def("setScaleFactorOfCentralWaveNumber",
                   [](m2g::AdditionalValues& values, std::int64_t value) {
                       values.setScaleFactorOfCentralWaveNumber(value);
                   })
              .def("setScaledValueOfCentralWaveNumber",
                   [](m2g::AdditionalValues& values, std::int64_t value) {
                       values.setScaledValueOfCentralWaveNumber(value);
                   })
              .def("setPV", [](m2g::AdditionalValues& values,
                               std::reference_wrapper<const std::vector<double>> value) { values.setPV(value); })
              .def("setPV",
                   [](m2g::AdditionalValues& values, std::int64_t numberOfLevels) { values.setPV(numberOfLevels); })
              .def("setWaveDirections",
                   [](m2g::AdditionalValues& values, std::reference_wrapper<const std::vector<double>> value) {
                       values.setWaveDirections(value);
                   })
              .def("setWaveFrequencies",
                   [](m2g::AdditionalValues& values, std::reference_wrapper<const std::vector<double>> value) {
                       values.setWaveFrequencies(value);
                   })
              .def("setBitsPerValue",
                   [](m2g::AdditionalValues& values, std::int64_t value) { values.setBitsPerValue(value); });

    additionalValues.doc()
        = "Mars2Grib AdditionalValues: stable interfacing object for setting additional encoding details. In some "
          "cases they are inferred, for some they need to be passed. Unfortunately this is hard to track - ideal (and "
          "intented) behaviour is that all values will be set to reasonable defaults used across the center.";


    auto mars2grib
        = py::class_<m2g::Mars2Grib>(m, "Mars2Grib")  //
              .def(py::init([](bool cached) { return m2g::Mars2Grib{{cached = true}}; }), py::arg("cached") = true)
              .def("encode",
                   [](m2g::Mars2Grib& enc, const m2g::MarsValues& mars, const m2g::AdditionalValues& misc,
                      const m2g::GeometryValues& geo) { return toPyCodesHandle(enc.encode(mars, misc, geo)); })
              .def("encode", [](m2g::Mars2Grib& enc, const m2g::MarsValues& mars,
                                const m2g::AdditionalValues& misc) { return toPyCodesHandle(enc.encode(mars, misc)); })
              .def("encode",
                   [](m2g::Mars2Grib& enc, const m2g::MarsValues& mars, const m2g::AdditionalValues& misc,
                      const m2g::GeometryValues& geo, const std::vector<double>& values) {
                       return toPyCodesHandle(enc.encode(mars, misc, geo, values));
                   })
              .def("encode",
                   [](m2g::Mars2Grib& enc, const m2g::MarsValues& mars, const m2g::AdditionalValues& misc,
                      const std::vector<double>& values) { return toPyCodesHandle(enc.encode(mars, misc, values)); })
              .def("encode",
                   [](m2g::Mars2Grib& enc, const m2g::MarsValues& mars, const m2g::AdditionalValues& misc,
                      const m2g::GeometryValues& geo, const std::vector<float>& values) {
                       return toPyCodesHandle(enc.encode(mars, misc, geo, values));
                   })
              .def("encode",
                   [](m2g::Mars2Grib& enc, const m2g::MarsValues& mars, const m2g::AdditionalValues& misc,
                      const std::vector<float>& values) { return toPyCodesHandle(enc.encode(mars, misc, values)); });

    mars2grib.doc()
        = "Mars2Grib API interface: object that performs mars to grib mapping by preparing a grib2 handle with "
          "metadata being preset.";
}
