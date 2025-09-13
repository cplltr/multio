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
    GeometryType,
    MarsIdentifiers,
    AdditionalValues,
    GeometryValues
)

from pymars2grib_bindings import Mars2Grib as _Mars2Grib

import numpy as np
from typing import Any, Optional, Tuple
import eccodes

def wrap_message(method):
    """Decorator to wrap raw codes_handle* into an eccodes.Message."""
    def wrapper(self, *args, **kwargs):
        handle = method(self, *args, **kwargs)
        return self._wrap(handle)
    return wrapper


class Mars2Grib:
    """
    # Prototype... needs iteration and testing
    
    Unified Python wrapper for the C++ Mars2Grib class.

    Provides a single `encode()` method with optional geometry & values,
    automatically wrapping the result in an `eccodes.Message`.
    """

    def __init__(self, *args, **kwargs):
        self._impl = _Mars2Grib(*args, **kwargs)

    def _wrap(self, handle):
        """Convert raw codes_handle* → eccodes.Message."""
        return eccodes.Message(handle) if handle else None

    def _prepare_values(self, values: Any) -> Tuple[Optional[Any], Optional[int], Optional[str]]:
        """
        Prepare values for C++: ensure contiguous buffer, detect type, get pointer + size.
        Returns (buffer, size, type) or (None, None, None) if values is None.
        """
        if values is None:
            return None, None, None

        # Convert lists or other objects to numpy array if needed
        if not isinstance(values, np.ndarray):
            values = np.array(values)

        # Ensure contiguous memory
        values = np.ascontiguousarray(values)
        return values

    @wrap_message
    def encode(
        self,
        mars: MarsIdentifier,
        misc: AdditionalValues,
        geometry: Optional[GeometryValues] = None,
        values: Optional[Any] = None,
    ) -> "eccodes.Message":
        """
        Encode data with MARS Identifiers as metadata description into a GRIB message.

        Args:
            mars: MarsIdentifier object with main metadata.
            misc: AdditionalValues object with extra parameters.
            geometry: Optional GeometryValues describing the grid geometry.
            values: Optional array-like with data values (list, numpy array, memoryview, etc.).
                Supports float32 or float64. Passed to C++ via buffer pointer.

        Returns:
            eccodes.Message: Encoded GRIB message ready for writing/inspection.
        """

        buf, size, dtype = self._prepare_values(values)

        # Dispatch based on args + type
        if geometry is not None and buf is not None:
                return self._impl.encode(mars, misc, geometry, buf, size)
        elif geometry is None and buf is not None:
                return self._impl.encode(mars, misc, buf, size)
        elif geometry is not None and buf is None:
            return self._impl.encode(mars, misc, geometry)
        else:
            return self._impl.encode(mars, misc)


__all__ = [
    "Mars2Grib",
    "GeometryType",
    "MarsIdentifiers",
    "AdditionalValues",
    "GeometryValues"
]
