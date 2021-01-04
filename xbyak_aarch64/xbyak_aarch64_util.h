#pragma once
/*******************************************************************************
 * Copyright 2020 FUJITSU LIMITED
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/
#ifndef XBYAK_AARCH64_UTIL_H_
#define XBYAK_AARCH64_UTIL_H_

#include <stdint.h>
#include <sys/prctl.h>
#include <xbyak_aarch64/low_func.h>

namespace Xbyak_aarch64 {
namespace util {

enum sveLen_t {
  SVE_NONE = 0,
  SVE_128 = 16 * 1,
  SVE_256 = 16 * 2,
  SVE_384 = 16 * 3,
  SVE_512 = 16 * 4,
  SVE_640 = 16 * 5,
  SVE_768 = 16 * 6,
  SVE_896 = 16 * 7,
  SVE_1024 = 16 * 8,
  SVE_1152 = 16 * 9,
  SVE_1280 = 16 * 10,
  SVE_1408 = 16 * 11,
  SVE_1536 = 16 * 12,
  SVE_1664 = 16 * 13,
  SVE_1792 = 16 * 14,
  SVE_1920 = 16 * 15,
  SVE_2048 = 16 * 16,
};

/**
   CPU detection class
*/
class Cpu {
  uint64_t type_;
  sveLen_t sveLen_;

public:
  typedef uint64_t Type;

  static const Type tNONE = 0;
  static const Type tADVSIMD = 1 << 1;
  static const Type tFP = 1 << 2;
  static const Type tSVE = 1 << 3;
  static const Type tATOMIC = 1 << 4;

  static const uint64_t ZCR_EL1_LEN_SHIFT = 0;
  static const uint64_t ZCR_EL1_LEN_MASK = 0xf;

  Cpu() : type_(tNONE), sveLen_(SVE_NONE) {

    Type_id_aa64isar0_el1 isar0 = xbyak_aarch64_get_id_aa64isar0_el1();
    if (isar0.atomic == 2) {
      type_ |= tATOMIC;
    }

    Type_id_aa64pfr0_el1 pfr0 = xbyak_aarch64_get_id_aa64pfr0_el1();
    if (pfr0.fp == 1) {
      type_ |= tFP;
    }
    if (pfr0.advsimd == 1) {
      type_ |= tADVSIMD;
    }
    if (pfr0.sve == 1) {
      type_ |= tSVE;
      /* Can not read ZCR_EL1 system register from application level.*/
#ifdef PR_SVE_GET_VL
      sveLen_ = static_cast<sveLen_t>(prctl(PR_SVE_GET_VL));
#else
      sveLen_ = static_cast<sveLen_t>(prctl(51));
#endif
    }
  }

  Type getType() const { return type_; }
  bool has(Type type) const { return (type & type_) != 0; }
  uint64_t getSveLen() const { return sveLen_; }
  bool isAtomicSupported() const { return type_ & tATOMIC; }
};
} // namespace util
} // namespace Xbyak_aarch64
#endif
