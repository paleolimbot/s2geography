// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \file geography_glue.h
///
/// This file exposes C functions and/or data structures used to call
/// s2geography from C or languages that provide C FFI infrastructure
/// such as Rust or Julia.

typedef int S2GeogErrorCode;

struct S2GeogError;

S2GeogErrorCode S2GeogErrorCreate(struct S2GeogError** err);

const char* S2GeogErrorGetMessage(struct S2GeogError* err);

void S2GeogErrorDestroy(struct S2GeogError* err);

const char* S2GeogNanoarrowVersion(void);

const char* S2GeogGeoArrowVersion(void);

const char* S2GeogOpenSSLVersion(void);

const char* S2GeogS2GeometryVersion(void);

const char* S2GeogAbseilVersion(void);

uint64_t S2GeogLngLatToCellId(double lng, double lat);

#define S2GEOGRAPHY_KERNEL_FORMAT_SEDONA_UDF 1

size_t S2GeogNumKernels(void);

int S2GeogInitKernels(void* kernels_array, size_t kernels_array_size_bytes,
                      int format);

struct S2Geog;

S2GeogErrorCode S2GeogCreate(struct S2Geog** geog);

void S2GeogDestroy(struct S2Geog* geog);

struct S2GeogFactory;

S2GeogErrorCode S2GeogFactoryCreate(struct S2GeogFactory** geog_factory);

S2GeogErrorCode S2GeogFactoryInitFromWkbNonOwning(
    struct S2GeogFactory* geog_factory, const uint8_t* buf, size_t buf_size,
    struct S2Geog** out, struct S2GeogError* err);

void S2GeogFactoryDestroy(struct S2GeogFactory* geog_factory);

#ifdef __cplusplus
}
#endif
