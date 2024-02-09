//===----------- ReduceStruct.cpp - Specialized Delta Pass ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Try to reduce structs.
//
//===----------------------------------------------------------------------===//

#include "ReduceStruct.h"
#include "Delta.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/Casting.h"
#include "llvm/ADT/SmallSet.h"
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "llvm-reduce"

static void reduceStruct(Oracle &O, ReducerWorkItem &WorkItem) {

    // Track set of struct type which has atleast one non const index to their value.
    SmallSet<StructType *, 2> StructDontOpt;
    // Track set of constant index access for given struct type.
    // Invariant: Type present in StructDontOpt can't be present in StructValueOffset.
    DenseMap<StructType *, std::vector<uint64_t>> StructValueOffset; 
    // first pass where we collect information
  for (Function &F : WorkItem.getModule()) {
    for (Instruction &I : instructions(F)) {
        if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
            auto source_type = GEP->getSourceElementType();
            if (source_type->isStructTy() && GEP->getNumIndices() >= 2) {
                auto source_type = dyn_cast<StructType>(GEP->getSourceElementType());
                auto iter = StructValueOffset.find(source_type);
                // second operand must be index into struct value
                if (auto *CI = dyn_cast<ConstantInt>(GEP->getOperand(2))) {
                    LLVM_DEBUG(dbgs() << *GEP << " opd 2 "<< CI->getValue());
                    if (StructDontOpt.find(source_type) == StructDontOpt.end()) {
                        if (iter != StructValueOffset.end()) {
                        iter->second.push_back(CI->getZExtValue());
                        } else {
                            std::vector<uint64_t> vec;
                            vec.push_back(CI->getZExtValue());
                            StructValueOffset.try_emplace(source_type, vec);
                        }
                    }
                } else {
                    // reduction can not be applied to given struct type
                    // as at least one access is non const
                    StructDontOpt.insert(source_type);
                    StructValueOffset.erase(source_type);
                }
            }
        }
    }
  }

  // Maps old struct type to new struct type.
  DenseMap<StructType *, StructType *> MappedStructTypes;
  // Maps index in old struct type to index in new struct type.
  DenseMap<StructType *, DenseMap<uint64_t, uint64_t>> MappedStructIndex;

  for (auto &Pair: StructValueOffset) {
      auto structType = Pair.first;
      // check for Invariant.
      assert(StructDontOpt.find(Pair.first) == StructDontOpt.end());
      // sort and dedup indices
      llvm::sort(Pair.second);
      Pair.second.erase(std::unique(Pair.second.begin(), Pair.second.end()), Pair.second.end());

      std::vector<Type*> usedTypes;
      auto newIndex = 0;
      DenseMap<uint64_t, uint64_t> oldToNewIndexMap;
      for(auto index: Pair.second) {
        usedTypes.push_back(structType->getTypeAtIndex(index));
        oldToNewIndexMap.try_emplace(index, newIndex);
        newIndex++;
      }
      auto newStructType = StructType::create(usedTypes, structType->getName());
      MappedStructTypes.try_emplace(structType, newStructType);
      MappedStructIndex.try_emplace(newStructType, oldToNewIndexMap);
  }

  // Transformation happens here
  for (Function &F : WorkItem.getModule()) {
    for (Instruction &I : instructions(F)) {
        if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
            auto source_type = GEP->getSourceElementType();
            if (source_type->isStructTy()) {
                auto structType = cast<StructType>(source_type);
                auto newStructType = MappedStructTypes.find(structType);
                if (newStructType != MappedStructTypes.end()) {
                    auto oldToNewIndexMap = MappedStructIndex.find(newStructType->second);
                    if (oldToNewIndexMap != MappedStructIndex.end()) {

                if (auto *CI = dyn_cast<ConstantInt>(GEP->getOperand(2))) {
                    auto index = CI->getZExtValue();
                    auto newIndex = oldToNewIndexMap->second.find(index);
                    if (newIndex != oldToNewIndexMap->second.end()) {
                        // replace newIndex value
                        Value *newIndexValue = ConstantInt::get(CI->getType(), newIndex->second);
                        GEP->setOperand(2, newIndexValue);
                    // also change source type
                    GEP->setSourceElementType(newStructType->second);
                    }
                }
                    }
                }
            }
        }
    }
  }
}

void llvm::reduceStructDeltaPass(TestRunner &Test) {
  runDeltaPass(Test, reduceStruct, "Reducing Struct");
}
