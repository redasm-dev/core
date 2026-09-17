#include "types.h"
#include "core/mapping.h"
#include "support/containers.h"

int _rd_i_db_segment_cmp_pred(const void* a, const void* b) {
    const RDSegment* sa = *(const RDSegment**)a;
    const RDSegment* sb = *(const RDSegment**)b;
    if(sa->rel_start < sb->rel_start) return -1;
    if(sa->rel_start > sb->rel_start) return 1;
    return 0;
}

int _rd_i_db_segment_find_pred(const void* key, const void* item) {
    RDRelAddress rel_addr = *(const RDRelAddress*)key;
    const RDSegment* seg = *(const RDSegment**)item;
    if(rel_addr < seg->rel_start) return -1;
    if(rel_addr >= seg->rel_end) return 1;
    return 0;
}

int _rd_i_db_mapping_cmp_pred(const void* a, const void* b) {
    const RDInputMapping* sa = *(const RDInputMapping**)a;
    const RDInputMapping* sb = *(const RDInputMapping**)b;
    if(sa->rel_start < sb->rel_start) return -1;
    if(sa->rel_start > sb->rel_start) return 1;
    return 0;
}

int _rd_i_db_mapping_find_pred(const void* key, const void* item) {
    RDRelAddress rel_addr = *(const RDRelAddress*)key;
    const RDInputMapping* m = *(const RDInputMapping**)item;
    if(rel_addr < m->rel_start) return -1;
    if(rel_addr >= m->rel_end) return 1;
    return 0;
}

int _rd_i_db_segmentreg_cmp(const void* a, const void* b) {
    const RDSegmentReg* ea = (const RDSegmentReg*)a;
    const RDSegmentReg* eb = (const RDSegmentReg*)b;
    if(ea->address < eb->address) return -1;
    if(ea->address > eb->address) return 1;
    return 0;
}

RDSegmentRegVect* _rd_i_db_segmentregs_find_vect(RDSegmentRegsVect* self,
                                                 const char* reg) {
    RDSegmentRegVect* rv;

    vect_each(rv, self) {
        if(rv->name == reg) return rv;
    }

    return NULL;
}

RDSegmentRegVect* _rd_i_db_segmentregs_get_vect(RDSegmentRegsVect* self,
                                                RDSegmentRegNameVect* names,
                                                const char* reg) {
    RDSegmentRegVect* rv = _rd_i_db_segmentregs_find_vect(self, reg);
    if(rv) return rv;

    // new register, add to both vects
    vect_push(self, (RDSegmentRegVect){.name = reg});
    vect_push(names, reg);
    return vect_last(self);
}
