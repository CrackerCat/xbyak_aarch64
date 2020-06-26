/*******************************************************************************
 * Copyright 2019 FUJITSU LIMITED
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
#pragma once

#ifndef _XBYAK_AARCH64_LABEL_
#define _XBYAK_AARCH64_LABEL_

#include "xbyak_aarch64_code_array.h"
#include "xbyak_aarch64_err.h"
#include "xbyak_aarch64_inner.h"

struct JmpLabelAArch64 {
  // type of partially applied function for encoding
  typedef std::function<uint32_t(int64_t)> EncFunc;
  size_t endOfJmp; /* offset from top to the end address of jmp */
  EncFunc encFunc;
  explicit JmpLabelAArch64(const EncFunc& encFunc, size_t endOfJmp = 0)
      : endOfJmp(endOfJmp), encFunc(encFunc) {}
};

class LabelManagerAArch64;

class LabelAArch64 {
  mutable LabelManagerAArch64* mgr;
  mutable int id;
  friend class LabelManagerAArch64;

 public:
     LabelAArch64() : mgr(nullptr), id(0) {}
     LabelAArch64(const LabelAArch64& rhs);
     LabelAArch64& operator=(const LabelAArch64& rhs);
  ~LabelAArch64();
  void clear() {
    mgr = nullptr;
    id = 0;
  }
  int getId() const { return id; }
  const uint32_t* getAddress() const;
};

#ifdef XBYAK_TRANSLATE_AARCH64
  using namespace Xbyak::Xbyak_aarch64;
#endif

class LabelManagerAArch64 {

  // for Label class
  struct ClabelValAArch64 {
    ClabelValAArch64(size_t offset = 0) : offset(offset), refCount(1) {}
    size_t offset;
    int refCount;
  };
  typedef std::unordered_map<int, ClabelValAArch64> ClabelDefListAArch64;
  typedef std::unordered_multimap<int, const JmpLabelAArch64> ClabelUndefListAArch64;
  typedef std::unordered_set<LabelAArch64*> LabelPtrListAArch64;

  CodeArrayAArch64* base_;
  // global : stateList_.front(), local : stateList_.back()
  mutable int labelId_;
  ClabelDefListAArch64 clabelDefListAArch64_;
  ClabelUndefListAArch64 clabelUndefListAArch64_;
  LabelPtrListAArch64 labelPtrListAArch64_;

  int getId(const LabelAArch64& label) const {
    if (label.id == 0) label.id = labelId_++;
    return label.id;
  }
  template <class DefList, class UndefList, class T>
  void define_inner(DefList& defList, UndefList& undefList, const T& labelId,
                    size_t addrOffset) {
    // add label
    typename DefList::value_type item(labelId, addrOffset);
    std::pair<typename DefList::iterator, bool> ret = defList.insert(item);
    if (!ret.second) throw Error(ERR_LABEL_IS_REDEFINED);
    // search undefined label
    for (;;) {
      typename UndefList::iterator itr = undefList.find(labelId);
      if (itr == undefList.end()) break;
      const JmpLabelAArch64* jmp = &itr->second;
      const size_t offset = jmp->endOfJmp;
      int64_t labelOffset = (addrOffset - offset) * CSIZE;
      uint32_t disp = jmp->encFunc(labelOffset);
      if (base_->isAutoGrow()) {
        base_->save(offset, disp, jmp->encFunc);
      } else {
        base_->rewrite(offset, disp);
      }
      undefList.erase(itr);
    }
  }
  template <class DefList, class T>
  bool getOffset_inner(const DefList& defList, size_t* offset,
                       const T& label) const {
    typename DefList::const_iterator i = defList.find(label);
    if (i == defList.end()) return false;
    *offset = i->second.offset;
    return true;
  }
  friend class LabelAArch64;
  void incRefCount(int id, LabelAArch64* label) {
    clabelDefListAArch64_[id].refCount++;
    labelPtrListAArch64_.insert(label);
  }
  void decRefCount(int id, LabelAArch64* label) {
    labelPtrListAArch64_.erase(label);
    ClabelDefListAArch64::iterator i = clabelDefListAArch64_.find(id);
    if (i == clabelDefListAArch64_.end()) return;
    if (i->second.refCount == 1) {
      clabelDefListAArch64_.erase(id);
    } else {
      --i->second.refCount;
    }
  }
  template <class T>
  bool hasUndefinedLabel_inner(const T& list) const {
#ifndef NDEBUG
    for (typename T::const_iterator i = list.begin(); i != list.end(); ++i) {
      std::cerr << "undefined label:" << i->first << std::endl;
    }
#endif
    return !list.empty();
  }
  // detach all labels linked to LabelManager
  void resetLabelPtrList() {
    for (LabelPtrListAArch64::iterator i = labelPtrListAArch64_.begin(),
                                ie = labelPtrListAArch64_.end();
         i != ie; ++i) {
      (*i)->clear();
    }
    labelPtrListAArch64_.clear();
  }

 public:
  LabelManagerAArch64() { reset(); }
  ~LabelManagerAArch64() { resetLabelPtrList(); }
  void reset() {
    base_ = 0;
    labelId_ = 1;
    clabelDefListAArch64_.clear();
    clabelUndefListAArch64_.clear();
    resetLabelPtrList();
  }

  void set(CodeArrayAArch64* base) { base_ = base; }

  void defineClabel(LabelAArch64& label) {
    define_inner(clabelDefListAArch64_, clabelUndefListAArch64_, getId(label),
                 base_->getSize());
    label.mgr = this;
    labelPtrListAArch64_.insert(&label);
  }
  void assign(LabelAArch64& dst, const LabelAArch64& src) {
    ClabelDefListAArch64::const_iterator i = clabelDefListAArch64_.find(src.id);
    if (i == clabelDefListAArch64_.end()) throw Error(ERR_LABEL_ISNOT_SET_BY_L);
    define_inner(clabelDefListAArch64_, clabelUndefListAArch64_, dst.id, i->second.offset);
    dst.mgr = this;
    labelPtrListAArch64_.insert(&dst);
  }
  bool getOffset(size_t* offset, const LabelAArch64& label) const {
    return getOffset_inner(clabelDefListAArch64_, offset, getId(label));
  }
  void addUndefinedLabel(const LabelAArch64& label, const JmpLabelAArch64& jmp) {
    clabelUndefListAArch64_.insert(ClabelUndefListAArch64::value_type(label.id, jmp));
  }
  bool hasUndefClabel() const {
    return hasUndefinedLabel_inner(clabelUndefListAArch64_);
  }
#ifdef XBYAK_TRANSLATE_AARCH64
  const uint8_t* getCode() const { return base_->getCode(); }
  const uint32_t* getCode32() const { return base_->getCode32(); }
#else
  const uint32_t* getCode() const { return base_->getCode(); }
#endif
  bool isReady() const {
    return !base_->isAutoGrow() || base_->isCalledCalcJmpAddress();
  }
};

inline LabelAArch64::LabelAArch64(const LabelAArch64& rhs) {
  id = rhs.id;
  mgr = rhs.mgr;
  if (mgr) mgr->incRefCount(id, this);
}
inline LabelAArch64& LabelAArch64::operator=(const LabelAArch64& rhs) {
  if (id) throw Error(ERR_LABEL_IS_ALREADY_SET_BY_L);
  id = rhs.id;
  mgr = rhs.mgr;
  if (mgr) mgr->incRefCount(id, this);
  return *this;
}
inline LabelAArch64::~LabelAArch64() {
  if (id && mgr) mgr->decRefCount(id, this);
}
inline const uint32_t* LabelAArch64::getAddress() const {
  if (mgr == 0 || !mgr->isReady()) return 0;
  size_t offset;
  if (!mgr->getOffset(&offset, *this)) return 0;
#ifdef XBYAK_TRANSLATE_AARCH64
  return mgr->getCode32() + offset * CSIZE;
#else
  return mgr->getCode() + offset * CSIZE;
#endif
}

#endif
