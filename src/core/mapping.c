#include "mapping.h"

int rd_i_mapping_cmp_pred(const void* a, const void* b) {
    const RDInputMapping* sa = (const RDInputMapping*)a;
    const RDInputMapping* sb = (const RDInputMapping*)b;
    if(sa->start_address < sb->start_address) return -1;
    if(sa->start_address > sb->start_address) return 1;
    return 0;
}

int rd_i_mapping_find_pred(const void* key, const void* item) {
    RDAddress addr = *(const RDAddress*)key;
    const RDInputMapping* m = (const RDInputMapping*)item;
    if(addr < m->start_address) return -1;
    if(addr >= m->end_address) return 1;
    return 0;
}
