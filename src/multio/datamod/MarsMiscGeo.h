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

/// @date Sep 2023

#pragma once

#include "multio/datamod/DataModelling.h"
#include "multio/datamod/DataModellingException.h"
#include "multio/datamod/ReaderWriter.h"
#include "multio/util/TypeTraits.h"
#include "multio/util/VariantHelpers.h"

#include <chrono>
#include <sstream>
#include <string>


namespace multio::datamod {

//-----------------------------------------------------------------------------


using TimeDuration = std::variant<std::chrono::hours, std::chrono::seconds>;


enum class Repres : std::size_t
{
    GG,
    LL,
    SH,
    HEALPix          // We added it here because we use repres as an intermediate type. Officially healpix is not mapped to any of the others...
};

std::ostream& operator<<(std::ostream&, const TimeDuration&);
std::ostream& operator<<(std::ostream&, const Repres&);
}  // namespace multio::datamod


namespace multio::util {
template <>
struct TypeToString<datamod::Repres> {
    std::string operator()() const { return "datamod::Repres"; };
};
}  // namespace multio::util

namespace multio::datamod {

template <>
struct WriteSpec<TimeDuration> {
    static std::string write(const TimeDuration&) noexcept;
};

template <>
struct ReadSpec<TimeDuration> {
    static TimeDuration read(std::int64_t hours) noexcept;
    static TimeDuration read(const std::string& s);
};


template <>
struct WriteSpec<Repres> {
    static std::string write(Repres) noexcept;
};

template <>
struct ReadSpec<Repres> {
    static inline Repres read(Repres v) noexcept { return v; };
    static Repres read(const std::string& s);
};


Repres represFromGrid(const std::string& grid);


namespace mapper {

// TODO Discuss ith Param should get it's own type ParamId (wrapping an int..)
// Currently `metkit::Param` is used to create a paramId from string
// There is also the existing type `metkit::ParamID` which (unfortunately) can not be constructed from an eisting int.
struct ParamMapper {
    static std::int64_t write(std::int64_t) noexcept;
    static std::int64_t read(std::int64_t) noexcept;
    static std::int64_t read(const std::string&);
};

struct IntToBoolMapper {
    static inline bool write(bool v) noexcept { return v; };
    static inline bool read(bool v) noexcept { return v; };
    static inline bool read(std::int64_t v) { return v > 0; };
};

}  // namespace mapper


//-----------------------------------------------------------------------------
// Mars Keys
//-----------------------------------------------------------------------------

enum class MarsKeys : std::uint64_t
{
    EXPVER,
    STREAM,
    TYPE,
    CLASS,
    PARAM,
    ORIGIN,
    ANOFFSET,
    PACKING,
    NUMBER,
    IDENT,
    INSTRUMENT,
    CHANNEL,
    CHEM,
    MODEL,
    LEVTYPE,
    LEVELIST,
    DIRECTION,
    FREQUENCY,
    DATE,
    TIME,
    STEP,
    TIMEPROC,
    HDATE,
    GRID,
    TRUNCATION,
    REPRES
};


MULTIO_KEY_SET_DESCRIPTION(
    MarsKeys,  //
    "mars",    //
               //
    describeKeyValue<MarsKeys::EXPVER, std::string, KVTag::Required>("expver"),
    describeKeyValue<MarsKeys::STREAM, std::string, KVTag::Required>("stream"),
    describeKeyValue<MarsKeys::TYPE, std::string, KVTag::Required>("type"),
    describeKeyValue<MarsKeys::CLASS, std::string, KVTag::Required>("class"),
    describeKeyValue<MarsKeys::PARAM, std::int64_t, KVTag::Required, mapper::ParamMapper>("param"),
    describeKeyValue<MarsKeys::ORIGIN, std::string, KVTag::Defaulted>("origin").withDefault("ecmf"),
    describeKeyValue<MarsKeys::ANOFFSET, std::int64_t, KVTag::Optional>("anoffset"),
    describeKeyValue<MarsKeys::PACKING, std::string, KVTag::Optional>("packing"),
    describeKeyValue<MarsKeys::NUMBER, std::int64_t, KVTag::Optional>("number"),
    describeKeyValue<MarsKeys::IDENT, std::int64_t, KVTag::Optional>("ident"),
    describeKeyValue<MarsKeys::INSTRUMENT, std::int64_t, KVTag::Optional>("instrument"),
    describeKeyValue<MarsKeys::CHANNEL, std::int64_t, KVTag::Optional>("channel"),
    describeKeyValue<MarsKeys::CHEM, std::int64_t, KVTag::Optional>("chem"),
    describeKeyValue<MarsKeys::MODEL, std::string, KVTag::Optional>("model"),
    describeKeyValue<MarsKeys::LEVTYPE, std::string, KVTag::Optional>("levtype"),
    describeKeyValue<MarsKeys::LEVELIST, std::int64_t, KVTag::Optional>("levelist"),
    describeKeyValue<MarsKeys::DIRECTION, std::int64_t, KVTag::Optional>("direction"),
    describeKeyValue<MarsKeys::FREQUENCY, std::int64_t, KVTag::Optional>("frequency"),
    describeKeyValue<MarsKeys::DATE, std::int64_t, KVTag::Required>("date"),
    describeKeyValue<MarsKeys::TIME, std::int64_t, KVTag::Required>("time"),
    describeKeyValue<MarsKeys::STEP, TimeDuration, KVTag::Required>("step"),
    describeKeyValue<MarsKeys::TIMEPROC, TimeDuration, KVTag::Optional>("timeproc"),
    describeKeyValue<MarsKeys::HDATE, std::int64_t, KVTag::Optional>("hdate"),
    describeKeyValue<MarsKeys::GRID, std::string, KVTag::Optional>("grid"),
    describeKeyValue<MarsKeys::TRUNCATION, std::int64_t, KVTag::Optional>("truncation"),
    describeKeyValue<MarsKeys::REPRES, Repres, KVTag::Defaulted>("repres"));

using MarsKeySet = KeySet<MarsKeys>;
using MarsKeyValueSet = KeyValueSet<MarsKeySet>;


template <>
struct KeySetAlter<MarsKeySet> {
    static void alter(MarsKeyValueSet& mars) {
        // TODO setting conditional defaults and perform validation
        const auto& grid = key<MarsKeys::GRID>(mars);
        const auto& trunc = key<MarsKeys::TRUNCATION>(mars);
        auto& repres = key<MarsKeys::REPRES>(mars);

        if (grid.isMissing() && trunc.isMissing()) {
            std::ostringstream oss;
            oss << "Either mars key 'grid' (x)or 'truncation' must to be given to describe geometry - both are "
                   "missing: "
                << mars;
            throw DataModellingException(oss.str(), Here());
        }
        if (!grid.isMissing() && !trunc.isMissing()) {
            std::ostringstream oss;
            oss << "Either mars key 'grid' or 'truncation' needs to be given to describe geometry - both ore given: "
                << mars;
            throw DataModellingException(oss.str(), Here());
        }

        if (!grid.isMissing()) {
            auto detRepres = represFromGrid(grid.get());

            if (!repres.isMissing() && (detRepres != repres.get())) {
                std::ostringstream oss;
                oss << "Passed value for repres is " << repres.get() << " but derived value  " << detRepres
                    << " from grid " << grid.get();
                throw DataModellingException(oss.str(), Here());
            }
            repres.set(detRepres);
        }
        else if (!trunc.isMissing()) {
            auto detRepres = Repres::SH;
            if (!repres.isMissing() && (detRepres != repres.get())) {
                std::ostringstream oss;
                oss << "Passed value for repres is " << repres.get() << " but derived value  " << detRepres
                    << " from truncation " << std::to_string(trunc.get());
                throw DataModellingException(oss.str(), Here());
            }
            repres.set(detRepres);
        }
    }
};


//-----------------------------------------------------------------------------
// MARS encoder hash keys
//-----------------------------------------------------------------------------

// TODO implement some utilites to exclude types from a list
using EncoderCacheMarsKeySet = CustomKeySet<MarsKeys::EXPVER, MarsKeys::STREAM, MarsKeys::TYPE, MarsKeys::CLASS,
                                            MarsKeys::PARAM, MarsKeys::ORIGIN, MarsKeys::ANOFFSET, MarsKeys::PACKING,
                                            MarsKeys::NUMBER, MarsKeys::IDENT, MarsKeys::INSTRUMENT, MarsKeys::CHANNEL,
                                            MarsKeys::CHEM, MarsKeys::MODEL, MarsKeys::LEVTYPE, MarsKeys::LEVELIST,
                                            // MarsKeys::DIRECTION,
                                            // MarsKeys::FREQUENCY,
                                            MarsKeys::DATE, MarsKeys::TIME, MarsKeys::STEP, MarsKeys::TIMEPROC,
                                            MarsKeys::HDATE, MarsKeys::GRID, MarsKeys::TRUNCATION>;

using EncoderCacheMarsKeyValueSet = KeyValueSet<EncoderCacheMarsKeySet>;

template <>
struct KeySetAlter<EncoderCacheMarsKeySet> {
    static void alter(EncoderCacheMarsKeyValueSet& cacheKeys) {

        const auto& levtype = key<MarsKeys::LEVTYPE>(cacheKeys);

        if (!levtype.isMissing() && levtype.get() == "ml") {
            key<MarsKeys::LEVELIST>(cacheKeys).setMissing();
        }

        acquire(cacheKeys);
    }
};


//-----------------------------------------------------------------------------
// Parametrization keys
//-----------------------------------------------------------------------------


enum class MiscKeys : std::uint64_t
{
    TablesVersion,
    GeneratingProcessIdentifier,
    Typeofprocesseddata,
    EncodeStepZero,
    InitialStep,
    LengthOfTimeRange,
    LengthOfTimeStep,
    LengthOfTimeRangeInSeconds,
    LengthOfTimeStepInSeconds,
    ValuesScaleFactor,
    Pv,
    NumberOfMissingValues,
    ValueOfMissingValues,
    TypeOfEnsembleForecast,
    NumberOfForecastsInEnsemble,
    LengthOfTimeWindow,
    LengthOfTimeWindowInSeconds,
    BitsPerValue,
    PeriodMin,
    PeriodMax,
    WaveDirections,
    WaveFrequencies,
    SatelliteSeries,
    ScaleFactorOfCentralWavenumber,
    ScaledValueOfCentralWavenumber,

    // TBD - move to mars
    MethodNumber,
    SystemNumber
};


MULTIO_KEY_SET_DESCRIPTION(
    MiscKeys,  //
    "misc",    //
               //
    describeKeyValue<MiscKeys::TablesVersion, std::int64_t, KVTag::Optional>("tablesVersion"),
    describeKeyValue<MiscKeys::GeneratingProcessIdentifier, std::int64_t, KVTag::Optional>(
        "generatingProcessIdentifier"),
    describeKeyValue<MiscKeys::Typeofprocesseddata, std::int64_t, KVTag::Optional>("typeofprocesseddata"),
    describeKeyValue<MiscKeys::EncodeStepZero, bool, KVTag::Optional, mapper::IntToBoolMapper>("encodeStepZero"),
    describeKeyValue<MiscKeys::InitialStep, std::int64_t, KVTag::Defaulted>("initialStep").withDefault(0),
    describeKeyValue<MiscKeys::LengthOfTimeRange, std::int64_t, KVTag::Optional>("lengthOfTimeRange"),
    describeKeyValue<MiscKeys::LengthOfTimeStep, std::int64_t, KVTag::Optional>("lengthOfTimeStep"),
    describeKeyValue<MiscKeys::LengthOfTimeRangeInSeconds, std::int64_t, KVTag::Optional>("lengthOfTimeRangeInSeconds"),
    describeKeyValue<MiscKeys::LengthOfTimeStepInSeconds, std::int64_t, KVTag::Defaulted>("lengthOfTimeStepInSeconds")
        .withDefault(3600),
    describeKeyValue<MiscKeys::ValuesScaleFactor, double, KVTag::Optional>("valuesScaleFactor"),
    describeKeyValue<MiscKeys::Pv, std::vector<double>, KVTag::Optional>("pv"),
    describeKeyValue<MiscKeys::NumberOfMissingValues, std::int64_t, KVTag::Optional>("numberOfMissingValues"),
    describeKeyValue<MiscKeys::ValueOfMissingValues, double, KVTag::Optional>("valueOfMissingValues"),
    describeKeyValue<MiscKeys::TypeOfEnsembleForecast, std::int64_t, KVTag::Optional>("typeOfEnsembleForecast"),
    describeKeyValue<MiscKeys::NumberOfForecastsInEnsemble, std::int64_t, KVTag::Optional>(
        "numberOfForecastsInEnsemble"),
    describeKeyValue<MiscKeys::LengthOfTimeWindow, std::int64_t, KVTag::Optional>("lengthOfTimeWindow"),
    describeKeyValue<MiscKeys::LengthOfTimeWindowInSeconds, std::int64_t, KVTag::Optional>(
        "lengthOfTimeWindowInSeconds"),
    describeKeyValue<MiscKeys::BitsPerValue, std::int64_t, KVTag::Optional>("bitsPerValue"),
    describeKeyValue<MiscKeys::PeriodMin, std::int64_t, KVTag::Optional>("periodMin"),
    describeKeyValue<MiscKeys::PeriodMax, std::int64_t, KVTag::Optional>("periodMax"),
    describeKeyValue<MiscKeys::WaveDirections, std::vector<double>, KVTag::Optional>("waveDirections"),
    describeKeyValue<MiscKeys::WaveFrequencies, std::vector<double>, KVTag::Optional>("waveFrequencies"),
    describeKeyValue<MiscKeys::SatelliteSeries, std::int64_t, KVTag::Optional>("satelliteSeries"),
    describeKeyValue<MiscKeys::ScaleFactorOfCentralWavenumber, std::int64_t, KVTag::Optional>(
        "scaleFactorOfCentralWavenumber"),
    describeKeyValue<MiscKeys::ScaledValueOfCentralWavenumber, std::int64_t, KVTag::Optional>(
        "scaledValueOfCentralWavenumber"),

    // TBD - move to marse
    describeKeyValue<MiscKeys::MethodNumber, std::int64_t, KVTag::Optional>("methodNumber"),
    describeKeyValue<MiscKeys::SystemNumber, std::int64_t, KVTag::Optional>("systemNumber"));


//-----------------------------------------------------------------------------
// Geometry keys - gg
//-----------------------------------------------------------------------------

enum class GeoGG : std::uint64_t
{
    TruncateDegrees,
    NumberOfPointsAlongAMeridian,
    NumberOfParallelsBetweenAPoleAndTheEquator,
    LatitudeOfFirstGridPointInDegrees,
    LongitudeOfFirstGridPointInDegrees,
    LatitudeOfLastGridPointInDegrees,
    LongitudeOfLastGridPointInDegrees,
    Pl
};

MULTIO_KEY_SET_DESCRIPTION(GeoGG,     //
                           "geo-gg",  //
                                      //
                           describeKeyValue<GeoGG::TruncateDegrees, std::int64_t, KVTag::Optional>("truncateDegrees"),
                           describeKeyValue<GeoGG::NumberOfPointsAlongAMeridian, std::int64_t, KVTag::Optional>(
                               "numberOfPointsAlongAMeridian"),
                           describeKeyValue<GeoGG::NumberOfParallelsBetweenAPoleAndTheEquator, std::int64_t,
                                            KVTag::Required>("numberOfParallelsBetweenAPoleAndTheEquator"),
                           describeKeyValue<GeoGG::LatitudeOfFirstGridPointInDegrees, double, KVTag::Required>(
                               "latitudeOfFirstGridPointInDegrees"),
                           describeKeyValue<GeoGG::LongitudeOfFirstGridPointInDegrees, double, KVTag::Required>(
                               "longitudeOfFirstGridPointInDegrees"),
                           describeKeyValue<GeoGG::LatitudeOfLastGridPointInDegrees, double, KVTag::Required>(
                               "latitudeOfLastGridPointInDegrees"),
                           describeKeyValue<GeoGG::LongitudeOfLastGridPointInDegrees, double, KVTag::Required>(
                               "longitudeOfLastGridPointInDegrees"),
                           describeKeyValue<GeoGG::Pl, std::vector<std::int64_t>, KVTag::Optional>("pl"));

//-----------------------------------------------------------------------------
// Geometry keys - sh
//-----------------------------------------------------------------------------

enum class GeoSH : std::uint64_t
{
    PentagonalResolutionParameterJ,
    PentagonalResolutionParameterK,
    PentagonalResolutionParameterM
};

MULTIO_KEY_SET_DESCRIPTION(GeoSH,     //
                           "geo-sh",  //
                                      //
                           describeKeyValue<GeoSH::PentagonalResolutionParameterJ, std::int64_t, KVTag::Required>(
                               "pentagonalResolutionParameterJ"),
                           describeKeyValue<GeoSH::PentagonalResolutionParameterK, std::int64_t, KVTag::Required>(
                               "pentagonalResolutionParameterK"),
                           describeKeyValue<GeoSH::PentagonalResolutionParameterM, std::int64_t, KVTag::Required>(
                               "pentagonalResolutionParameterM"));

//-----------------------------------------------------------------------------
// Geometry keys - ll
//-----------------------------------------------------------------------------

// enum class GeoLL : std::uint64_t
// {
// };

// MULTIO_KEY_SET_DESCRIPTION(GeoLL,     //
//                            "geo-ll",  //
//                                       //

//-----------------------------------------------------------------------------
// Geometry keys - HEALPix
//-----------------------------------------------------------------------------

enum class GeoHEALPix : std::uint64_t
{
    NSide,
    OrderingConvention,
    LongitudeOfFirstGridPointInDegrees,
};

MULTIO_KEY_SET_DESCRIPTION(GeoHEALPix,     //
                           "geo-sh",  //
                                      //
                           describeKeyValue<GeoHEALPix::NSide, std::int64_t, KVTag::Required>(
                               "nside"),
                           describeKeyValue<GeoHEALPix::OrderingConvention, std::string, KVTag::Optional>(
                               "orderingConvention"),
                           describeKeyValue<GeoHEALPix::LongitudeOfFirstGridPointInDegrees, double, KVTag::Optional>(
                               "longitudeOfFirstGridPointInDegrees"));


//-----------------------------------------------------------------------------
// Evaluate geometry from mars
//-----------------------------------------------------------------------------

template <typename KVS, typename Func>
decltype(auto) withScopedGeometryKeySet(const KVS& kvs, Func&& func) {
    const auto& grid = key<MarsKeys::GRID>(kvs);
    const auto& trunc = key<MarsKeys::TRUNCATION>(kvs);
    const auto& repres = key<MarsKeys::REPRES>(kvs);

    switch (repres.get()) {
        case Repres::GG: {
            std::string scope = std::string("geo-") + grid.get();
            std::forward<Func>(func)(Repres::GG, scope, keySet<GeoGG>().scoped(scope));
            return;
        case Repres::HEALPix: {
            std::string scope = std::string("geo-") + grid.get();
            std::forward<Func>(func)(Repres::HEALPix, scope, keySet<GeoHEALPix>().scoped(scope));
            return;
        }
        }
        case Repres::SH: {
            std::string scope = std::string("geo-TCO") + std::to_string(trunc.get());
            std::forward<Func>(func)(Repres::SH, scope, keySet<GeoSH>().scoped(scope));
            return;
        }
        // TODO uncomment once there are keys specified...
        // case Repres::GG: {
        //     std::string scope = std::string("geo-") + grid.get();
        //     std::forward<Func>(func)(Repres::GG, scope, keySet<GeoGG>().scoped(scope));
        //     return;
        // }
        default:
            throw DataModellingException(
                std::string("withScopedGeometryKeySet: Unhandled repres ") + Writer<Repres>::write(repres.get()),
                Here());
    }
}


//-----------------------------------------------------------------------------

}  // namespace multio::datamod


template <>
struct std::hash<multio::datamod::TimeDuration> {
    std::size_t operator()(const multio::datamod::TimeDuration& td) const noexcept {
        return std::visit(
            eckit::Overloaded{[&](const std::chrono::hours& h) { return multio::util::hashCombine(h.count(), 'h'); },
                              [&](const std::chrono::seconds& s) { return multio::util::hashCombine(s.count(), 's'); }},
            td);
    }
};
