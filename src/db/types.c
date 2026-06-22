#include "types.h"
#include "support/containers.h"

int _rd_i_db_segment_cmp_pred(const void* a, const void* b) {
    const RDSegmentFull* sa = *(const RDSegmentFull**)a;
    const RDSegmentFull* sb = *(const RDSegmentFull**)b;
    if(sa->base.start_address < sb->base.start_address) return -1;
    if(sa->base.start_address > sb->base.start_address) return 1;
    return 0;
}

int _rd_i_db_segment_find_pred(const void* key, const void* item) {
    RDAddress addr = *(const RDAddress*)key;
    const RDSegmentFull* seg = *(const RDSegmentFull**)item;
    if(addr < seg->base.start_address) return -1;
    if(addr >= seg->base.end_address) return 1;
    return 0;
}

int _rd_i_db_mapping_cmp_pred(const void* a, const void* b) {
    const RDInputMapping* sa = (const RDInputMapping*)a;
    const RDInputMapping* sb = (const RDInputMapping*)b;
    if(sa->start_address < sb->start_address) return -1;
    if(sa->start_address > sb->start_address) return 1;
    return 0;
}

int _rd_i_db_mapping_find_pred(const void* key, const void* item) {
    RDAddress addr = *(const RDAddress*)key;
    const RDInputMapping* m = (const RDInputMapping*)item;
    if(addr < m->start_address) return -1;
    if(addr >= m->end_address) return 1;
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
