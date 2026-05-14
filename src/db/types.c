#include "types.h"

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
