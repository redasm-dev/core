#pragma once

#include <redasm/io/reader.h>

typedef struct RDParseRequest {
    RDReader* input;
    const char* filepath;
    const char* name;
    const char* ext;
} RDParseRequest;
