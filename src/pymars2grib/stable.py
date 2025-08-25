# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# All ependencies have to be loaded prior to importing
# pymars2grib_bindings
import findlibs

findlibs.load("multio")
findlibs.load("multiom")
findlibs.load("multiom-encoders")
findlibs.load("multio-mars2grib")

from pymars2grib_bindings import (
    Mars2Grib,
    GeometryType,
    MarsValues,
    AdditionalValues,
    GeometryValues
)

__all__ = [
    "Mars2Grib",
    "GeometryType",
    "MarsValues",
    "AdditionalValues",
    "GeometryValues"
]
