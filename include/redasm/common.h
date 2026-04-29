#pragma once

typedef struct RDContext RDContext;

#define RD_UNUSED(x) (void)(x)
#define rd_count_of(x) (sizeof(x) / sizeof(*(x)))

#define rd_slice_at(slice, n)                                                  \
    (assert((usize)(n) < (slice).length &&                                     \
            "rd_slice_at: index out of bounds"),                               \
     (slice).data[(n)])

#define rd_slice_length(slice) ((slice).length)
#define rd_slice_is_empty(slice) (rd_slice_length(slice) == 0)

#define rd_slice_each(it, self)                                                \
    for((it) = (self).data;                                                    \
        (self).data && ((it) < (self).data + (self).length); (it)++)
