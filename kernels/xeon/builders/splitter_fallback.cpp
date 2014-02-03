// ======================================================================== //
// Copyright 2009-2013 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "splitter_fallback.h"
#include "heuristics.h"

namespace embree
{
  template<typename Heuristic>
  void FallBackSplitter<Heuristic>::split(size_t threadIndex, PrimRefAlloc* alloc, 
                                          const BuildSource* geom,
                                          atomic_set<PrimRefBlock>& prims, const PrimInfo& pinfo,
                                          atomic_set<PrimRefBlock>& lprims, PrimInfo& linfo, Split& lsplit,
                                          atomic_set<PrimRefBlock>& rprims, PrimInfo& rinfo, Split& rsplit)
  {
    /* enforce split */
    size_t lnum = 0; BBox3fa lgeomBounds = empty; BBox3fa lcentBounds = empty;
    size_t rnum = 0; BBox3fa rgeomBounds = empty; BBox3fa rcentBounds = empty;
    atomic_set<PrimRefBlock>::item* lblock = lprims.insert(alloc->malloc(threadIndex));
    atomic_set<PrimRefBlock>::item* rblock = rprims.insert(alloc->malloc(threadIndex));
    
    while (atomic_set<PrimRefBlock>::item* block = prims.take()) 
    {
      for (size_t i=0; i<block->size(); i++) 
      {
        const PrimRef& prim = block->at(i); 
        const BBox3fa bounds = prim.bounds();
        
        if ((lnum+rnum)&1) 
        {
          lnum++;
          lgeomBounds.extend(bounds);
          lcentBounds.extend(center2(bounds));
          if (likely(lblock->insert(prim))) continue; 
          lblock = lprims.insert(alloc->malloc(threadIndex));
          lblock->insert(prim);
        } else {
          rnum++;
          rgeomBounds.extend(bounds);
          rcentBounds.extend(center2(bounds));
          if (likely(rblock->insert(prim))) continue;
          rblock = rprims.insert(alloc->malloc(threadIndex));
          rblock->insert(prim);
        }
      }
    }
    new (&linfo) PrimInfo(lnum,lgeomBounds,lcentBounds,pinfo);
    new (&rinfo) PrimInfo(rnum,rgeomBounds,rcentBounds,pinfo);

    /* perform binning of left side */
    Heuristic lheuristic(linfo,geom);
    typename atomic_set<PrimRefBlock>::iterator liter(lprims);
    while (typename atomic_set<PrimRefBlock>::item* block = liter.next()) {
      lheuristic.bin(block->base(),block->size());
    }
    lheuristic.best(lsplit);

    /* perform binning of right side */
    Heuristic rheuristic(rinfo,geom);
    typename atomic_set<PrimRefBlock>::iterator riter(rprims);
    while (typename atomic_set<PrimRefBlock>::item* block = riter.next()) {
      rheuristic.bin(block->base(),block->size());
    }
    rheuristic.best(rsplit);
  }

  template<typename Heuristic>
  void FallBackSplitter<Heuristic>::split(size_t threadIndex, PrimRefAlloc* alloc, 
                                          const BuildSource* geom,
                                          atomic_set<PrimRefBlock>& prims, const PrimInfo& pinfo,
                                          atomic_set<PrimRefBlock>& lprims, PrimInfo& linfo,
                                          atomic_set<PrimRefBlock>& rprims, PrimInfo& rinfo)
  {
    /* enforce split */
    size_t lnum = 0; BBox3fa lgeomBounds = empty; BBox3fa lcentBounds = empty;
    size_t rnum = 0; BBox3fa rgeomBounds = empty; BBox3fa rcentBounds = empty;
    atomic_set<PrimRefBlock>::item* lblock = lprims.insert(alloc->malloc(threadIndex));
    atomic_set<PrimRefBlock>::item* rblock = rprims.insert(alloc->malloc(threadIndex));
    
    while (atomic_set<PrimRefBlock>::item* block = prims.take()) 
    {
      for (size_t i=0; i<block->size(); i++) 
      {
        const PrimRef& prim = block->at(i); 
        const BBox3fa bounds = prim.bounds();
        
        if ((lnum+rnum)&1) 
        {
          lnum++;
          lgeomBounds.extend(bounds);
          lcentBounds.extend(center2(bounds));
          if (likely(lblock->insert(prim))) continue; 
          lblock = lprims.insert(alloc->malloc(threadIndex));
          lblock->insert(prim);
        } else {
          rnum++;
          rgeomBounds.extend(bounds);
          rcentBounds.extend(center2(bounds));
          if (likely(rblock->insert(prim))) continue;
          rblock = rprims.insert(alloc->malloc(threadIndex));
          rblock->insert(prim);
        }
      }
    }
    new (&linfo) PrimInfo(lnum,lgeomBounds,lcentBounds,pinfo);
    new (&rinfo) PrimInfo(rnum,rgeomBounds,rcentBounds,pinfo);
  }

  /*! explicit template instantiations */
  INSTANTIATE_TEMPLATE_BY_HEURISTIC(FallBackSplitter);
}
