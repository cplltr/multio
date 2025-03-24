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

#include <chrono>
#include <string>


namespace multio::datamod {

//-----------------------------------------------------------------------------


using TimeDuration = std::variant<std::chrono::hours, std::chrono::seconds>;


namespace mapper {
struct TimeDurationMapper {
    std::string write(const TimeDuration&) const noexcept;


    template <typename T, std::enable_if_t<!std::is_same_v<std::decay_t<T>, std::string>, bool> = true>
    TimeDuration read(T&& t) const {
        throw DataModellingException("TimeDuration must be an int or string, not " + util::typeToString<T>(), Here());
    }

    TimeDuration read(std::int64_t hours) const noexcept;
    TimeDuration read(const std::string& s) const;
};
struct ParamMapper {
    std::int64_t write(std::int64_t) const noexcept;
    std::int64_t read(std::int64_t) const noexcept;
    std::int64_t read(const std::string&) const;

    template <typename T, std::enable_if_t<!std::is_same_v<std::decay_t<T>, std::string>, bool> = true>
    std::int64_t read(T&& t) const {
        throw DataModellingException("Param must be an int or string, not " + util::typeToString<T>(), Here());
    }
};
struct IntToBoolMapper {
    inline bool write(bool v) const noexcept { return v; };
    inline bool read(bool v) const noexcept { return v; };
    inline bool read(std::int64_t v) const { return v > 0; };
    template <typename T>
    bool read(T&& t) const {
        throw DataModellingException("Value must be an int or bool, not " + util::typeToString<T>(), Here());
    }
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
};


MULTIO_KEY_SET_DESCRIPTION(
    MarsKeys,  //
    "mars",    //
               //
    describeKeyValue<MarsKeys::EXPVER, std::string, KVTag::Required>("expver"),
    describeKeyValue<MarsKeys::STREAM, std::string, KVTag::Required>("stream"),
    describeKeyValue<MarsKeys::TYPE, std::string, KVTag::Required>("type"),
    describeKeyValue<MarsKeys::CLASS, std::string, KVTag::Required>("class"),
    describeKeyValue<MarsKeys::PARAM, std::int64_t, KVTag::Required>("param", mapper::ParamMapper{}),
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
    describeKeyValue<MarsKeys::STEP, TimeDuration, KVTag::Required>("step", mapper::TimeDurationMapper{}),
    describeKeyValue<MarsKeys::TIMEPROC, TimeDuration, KVTag::Optional>("timeproc", mapper::TimeDurationMapper{}),
    describeKeyValue<MarsKeys::HDATE, std::int64_t, KVTag::Optional>("hdate"),
    describeKeyValue<MarsKeys::GRID, std::string, KVTag::Optional>("grid"),
    describeKeyValue<MarsKeys::TRUNCATION, std::int64_t, KVTag::Optional>("truncation"));

using MarsKeySet = KeySet<MarsKeys>;
using MarsKeyValueSet = KeyValueSet<MarsKeySet>;


template <>
struct KeySetAlter<MarsKeySet> {
    static void alter(MarsKeyValueSet& mars) {
        // TODO setting conditional defaults and perform validation
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
    describeKeyValue<MiscKeys::EncodeStepZero, bool, KVTag::Optional>("encodeStepZero", mapper::IntToBoolMapper{}),
    describeKeyValue<MiscKeys::InitialStep, std::int64_t, KVTag::Defaulted>("initialStep").withDefault(0),
    describeKeyValue<MiscKeys::LengthOfTimeRange, std::int64_t, KVTag::Optional>("lengthOfTimeRange"),
    describeKeyValue<MiscKeys::LengthOfTimeStep, std::int64_t, KVTag::Optional>("lengthOfTimeStep"),
    describeKeyValue<MiscKeys::LengthOfTimeRangeInSeconds, std::int64_t, KVTag::Optional>("lengthOfTimeRangeInSeconds"),
    describeKeyValue<MiscKeys::LengthOfTimeStepInSeconds, std::int64_t, KVTag::Defaulted>("lengthOfTimeStepInSeconds").withDefault(3600),
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
                           describeKeyValue<GeoGG::NumberOfPointsAlongAMeridian, std::int64_t, KVTag::Required>(
                               "numberOfPointsAlongAMeridian"),
                           describeKeyValue<GeoGG::NumberOfParallelsBetweenAPoleAndTheEquator, std::int64_t,
                                            KVTag::Optional>("numberOfParallelsBetweenAPoleAndTheEquator"),
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

enum class GridType : std::size_t
{
    GG,
    LL,
    SH,
    HEALPix
};

std::tuple<GridType, std::string> gridTypeAndScopeFromGrid(const std::string& grid);

template <typename KVS, typename Func>
decltype(auto) withScopedGeometryKeySet(const KVS& kvs, Func&& func) {
    const auto& grid = key<MarsKeys::GRID>(kvs);
    const auto& trunc = key<MarsKeys::TRUNCATION>(kvs);

    if (grid.isMissing() && trunc.isMissing()) {
        throw DataModellingException(
            "Either mars key 'grid' (x)or 'truncation' must to be given to describe geometry - both are missing",
            Here());
    }
    if (!grid.isMissing() && !trunc.isMissing()) {
        throw DataModellingException(
            "Either mars key 'grid' or 'truncation' needs to be given to describe geometry - both ore given", Here());
    }

    if (!grid.isMissing()) {
        auto [gridType, scope] = gridTypeAndScopeFromGrid(grid.get());

        switch (gridType) {
            case GridType::GG: {
                std::forward<Func>(func)(GridType::GG, scope, keySet<GeoGG>().scoped(scope));
                return;
            }
            case GridType::HEALPix: {
                std::forward<Func>(func)(GridType::HEALPix, scope, keySet<GeoHEALPix>().scoped(scope));
                return;
            }
            // TODO uncomment once there are keys specified...
            // case GridType::LL: {
            //     std::forward<Func>(func)(GridType::LL, keySet<GeoLL>().scoped(std::move(scope)));
            //     return;
            // }
            default:
                throw DataModellingException("Unhandled gridType", Here());
        }
    }
    else if (!trunc.isMissing()) {
        std::string scope = std::string("geo-TCO") + std::to_string(trunc.get());
        std::forward<Func>(func)(GridType::SH, scope, keySet<GeoSH>().scoped(scope));
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
