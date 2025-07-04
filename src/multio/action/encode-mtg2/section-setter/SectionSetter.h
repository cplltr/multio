/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

#pragma once

#include "multio/action/encode-mtg2/EncodeMtg2Exception.h"
#include "multio/action/encode-mtg2/EncoderConf.h"
#include "multio/datamod/DataModelling.h"
#include "multio/datamod/MarsMiscGeo.h"
#include "multio/util/MioGribHandle.h"

#include <memory>
#include <sstream>
#include <vector>


// DRAFT - to be discussed if we really need that many setters that an organization like this one is necessary
// The encoder configuration actually already maps to different setters and their configuration.
// If they are described properly through enums, it may be easier to keep them all static
// and do an explicit table lookup via a switch case on enum


namespace multio::action::sections {

struct DynSectionSetter {
    struct Config {
        bool registerPrepare;
        bool registerAllocate;
        bool registerPreset;
        bool registerRuntime;
    };

    // Returns information about which methods are implemented and need to be called
    virtual Config sectionInfo() const = 0;


    // Default implementation is to do nothing
    virtual void prepare(util::MioGribHandle&, const datamod::MarsKeyValueSet&, const datamod::MiscKeyValueSet&,
                         const datamod::Geometry&) const;
    // Default implementation is to do nothing
    virtual void allocate(util::MioGribHandle&, const datamod::MarsKeyValueSet&, const datamod::MiscKeyValueSet&,
                          const datamod::Geometry&) const;
    // Default implementation is to do nothing
    virtual void preset(util::MioGribHandle&, const datamod::MarsKeyValueSet&, const datamod::MiscKeyValueSet&,
                        const datamod::Geometry&) const;
    // Default implementation is to do nothing
    virtual void runtime(util::MioGribHandle&, const datamod::MarsKeyValueSet&, const datamod::MiscKeyValueSet&,
                         const datamod::Geometry&) const;
};


class SectionCollector {
public:
    // Registers a new section
    void add(std::unique_ptr<DynSectionSetter>);

    void prepare(util::MioGribHandle&, const datamod::MarsKeyValueSet&, const datamod::MiscKeyValueSet&,
                 const datamod::Geometry&) const;
    void allocate(util::MioGribHandle&, const datamod::MarsKeyValueSet&, const datamod::MiscKeyValueSet&,
                  const datamod::Geometry&) const;
    void preset(util::MioGribHandle&, const datamod::MarsKeyValueSet&, const datamod::MiscKeyValueSet&,
                const datamod::Geometry&) const;
    void runtime(util::MioGribHandle&, const datamod::MarsKeyValueSet&, const datamod::MiscKeyValueSet&,
                 const datamod::Geometry&) const;

private:
    // Storage of all sections
    std::vector<std::unique_ptr<DynSectionSetter>> sections_;

    // References to specific setters that perform operation in orderd
    std::vector<std::reference_wrapper<const DynSectionSetter>> prepare_;
    std::vector<std::reference_wrapper<const DynSectionSetter>> allocate_;
    std::vector<std::reference_wrapper<const DynSectionSetter>> preset_;
    std::vector<std::reference_wrapper<const DynSectionSetter>> runtime_;
};


}  // namespace multio::action::sections

