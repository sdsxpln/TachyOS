/*
 * tch_segment.h
 *
 *  Created on: 2015. 7. 5.
 *      Author: innocentevil
 */

#ifndef TCH_SEGMENT_H_

#define TCH_SEGMENT_H_

#include "tch_mm.h"
#include "owtree.h"
#include "wtree.h"



#define tch_getRegionBase(regionp)			(void*) ((size_t) ((struct mem_region*)regionp)->poff * PAGE_SIZE)
#define tch_getRegionSize(regionp)			(size_t) (((struct mem_region*)regionp)->psz * PAGE_SIZE)



struct mem_segment {
	rbtreeRoot_t            reg_root;		///< red-black tree root to track region allocation
	rbtreeNode_t            addr_rbn;		///< red-black tree node for segment lookup by address
	rbtreeNode_t            id_rbn;			///< red-black tree node for segment lookup by id
	wtreeRoot_t             alloc_root;
	uint32_t                flags;			///< flags
	uint32_t                poff;			///< page offset of segment
	uint32_t                psize;			///< total segment size in page
	dlistEntry_t            pfree_list;		///< free page list
	uint32_t                pfree_cnt;		///< the total number of free pages in this segment
};

/**
 * represent allocated memory chunk from mem_node
 */
struct mem_region {
	rbtreeNode_t			rbn;			///< rb node for association to its parent
	rbtreeNode_t			mm_rbn;			///< rb node for association to mapping tree
	struct tch_mm*			owner;
	uint32_t				flags;
	struct mem_segment*		segp;
	uint32_t				poff;
	uint32_t				psz;
};



extern void tch_initSegment(struct section_descriptor* init_section);
extern void tch_initRegion(struct mem_region* regp,struct mem_segment* parent,uint32_t poff,uint32_t psz,uint32_t perm);

/**
 *  \brief register memory section into segment
 *  \param[in] pointer to input section desciptor
 *  \param[in] pointer to segment segment control block
 */
extern int tch_segmentRegister(struct section_descriptor* section);
extern struct mem_segment* tch_segmentLookup(int seg_id);
extern void tch_segmentUnregister(int seg_id);
extern size_t tch_segmentGetSize(int seg_id);
extern size_t tch_segmentGetFreeSize(int seg_id);
extern void tch_mapSegment(struct tch_mm* mm,int seg_id);
extern void tch_unmapSegment(struct tch_mm* mm,int seg_id);


extern uint32_t tch_segmentAllocRegion(int seg_id,struct mem_region* mreg,size_t sz,uint32_t permission);
/*!
 * \brief free allocated region
 * \param[in] mreg pointer to region structure which is to be freed
 * \param[in] discard discard option indicate whether the memory region is actually discarded so it can be new allocation request.
 *  if true, it'll be available to allocation request, otherwise, the memory region is kept and the region node is removed from
 *  segment allocation table. this option can be useful when a region is merged into another externally.
 */
extern void tch_segmentFreeRegion(const struct mem_region* mreg, BOOL discard);
extern struct mem_region* tch_segmentGetRegionFromPtr(void* ptr);
extern void tch_mapRegion(struct tch_mm* mm,struct mem_region* mreg);
extern void tch_unmapRegion(struct tch_mm* mm,struct mem_region* mreg);
extern int tch_regionGetSize(struct mem_region* mreg);




#endif /* TCH_SEGMENT_H_ */
