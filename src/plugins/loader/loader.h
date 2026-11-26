#pragma once

#include "io/buffer.h"
#include "plugins/loader/option.h"
#include <redasm/redasm.h>

typedef struct RDLoaderParseResult {
    RDLoader* loader;
    const RDProcessorPlugin* processorplugin;
} RDLoaderParseResult;

typedef struct RDTestResult {
    char* filepath;
    RDByteBuffer* input_buffer;
    const RDLoaderPlugin* loaderplugin;
    const RDProcessorPlugin* processorplugin;
    RDLoaderOptionBuilder loader_options;
    char* loader_name;
    RDLoader* loader;
} RDTestResult;

RDLoader* rd_i_loader_create(const RDLoaderPlugin* plugin);
void rd_i_loader_destroy(const RDLoaderPlugin* plugin, RDLoader* ldr);
const char* rd_i_loader_get_platform(const RDLoaderPlugin* plugin,
                                     const RDLoader* ldr);

RDLoaderParseResult rd_i_parse(const RDLoaderPlugin* plugin,
                               RDByteBuffer* inputbuf, const char* filepath);

RDTestResultSlice rd_i_test(RDByteBuffer* inputbuf, const char* filepath);
