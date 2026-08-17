#pragma once

#include "io/buffer.h"
#include <redasm/redasm.h>

typedef struct RDParseResult {
    RDLoader* loader;
    const RDProcessorPlugin* processorplugin;
} RDParseResult;

typedef struct RDTestResult {
    char* filepath;
    RDByteBuffer* input_buffer;
    const RDLoaderPlugin* loaderplugin;
    const RDProcessorPlugin* processorplugin;
    char* loader_name;
    RDLoader* loader;
} RDTestResult;

RDLoader* rd_i_loader_create(const RDLoaderPlugin* plugin);
void rd_i_loader_destroy(const RDLoaderPlugin* plugin, RDLoader* ldr);

RDParseResult rd_i_parse(const RDLoaderPlugin* plugin, RDByteBuffer* inputbuf,
                         const char* filepath);

RDTestResultSlice rd_i_test(RDByteBuffer* inputbuf, const char* filepath);
