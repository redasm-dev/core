#include "loader.h"
#include "core/state.h"
#include "io/reader.h"
#include "plugins/builtin/builtin.h"
#include "plugins/common.h"
#include "support/containers.h"
#include "support/logging.h"

static bool _rd_part_loaders(const RDTestResult** tr) {
    return !((*tr)->loaderplugin->flags & RD_PF_LAST);
}

RDTestResultSlice rd_i_test(RDByteBuffer* inputbuf, const char* filepath) {
    if(!inputbuf) return (RDTestResultSlice){0};

    assert(filepath);
    vect_destroy(&rd_i_state.tests);

    if(rd_i_buffer_is_empty((RDBuffer*)inputbuf)) return (RDTestResultSlice){0};

    RDPlugin** it;
    vect_each(it, &rd_i_state.loaders) {
        const RDLoaderPlugin* loaderplugin = (*it)->loader;
        LOG_DEBUG("testing '%s' ...", loaderplugin->id);

        RDParseResult pr = rd_i_parse(loaderplugin, inputbuf, filepath);
        if(!pr.processorplugin) continue;

        assert(loaderplugin->get_name);

        RDTestResult* tr = rd_i_testresult_create(
            loaderplugin, pr.processorplugin, inputbuf, filepath);
        tr->loader = pr.loader;

        tr->loader_name = rd_strdup(loaderplugin->get_name(pr.loader));
        if(!tr->loader_name) tr->loader_name = rd_strdup(loaderplugin->id);
        assert(tr->loader_name);

        vect_push(&rd_i_state.tests, tr);
    }

    // Sort results by flags
    vect_stable_part(&rd_i_state.tests, _rd_part_loaders);

    return (RDTestResultSlice){
        .data = (const RDTestResult**)rd_i_state.tests.data,
        .length = rd_i_state.tests.length,
    };
}

RDParseResult rd_i_parse(const RDLoaderPlugin* plugin, RDByteBuffer* inputbuf,
                         const char* filepath) {
    RDParseResult pr = {
        .loader = plugin->create ? plugin->create(plugin) : NULL,
    };

    RDLoaderRequest req = {
        .filepath = filepath,
        .input = rd_i_reader_create((RDBuffer*)inputbuf),
        .name = rd_i_get_file_name(filepath),
        .ext = rd_i_get_file_ext(filepath),
    };

    if(plugin->parse(pr.loader, &req)) {
        if(plugin->get_processor) {
            const char* procid = plugin->get_processor(pr.loader);
            if(procid) pr.processorplugin = rd_processor_find(procid);
        }

        if(!pr.processorplugin)
            pr.processorplugin = rd_processor_find(RD_NULL_PROCESSOR_ID);

        assert(pr.processorplugin);
    }
    else if(plugin->destroy)
        plugin->destroy(pr.loader);

    rd_i_reader_destroy(req.input);
    return pr;
}

RDTestResultSlice rd_test_data(const char* data, usize n) {
    if(!data || !n) return (RDTestResultSlice){0};

    RDByteBuffer* input = rd_i_fromdata(data, n);
    return rd_i_test(input, ":memory:");
}

RDTestResultSlice rd_test(const char* filepath) {
    if(!filepath) return (RDTestResultSlice){0};

    RDByteBuffer* input = rd_i_readfile(filepath);
    return rd_i_test(input, filepath);
}

bool rd_register_loader(const RDLoaderPlugin* l) {
    if(!rd_i_validate_plugin(l->level, l->id, "loader")) return false;

    if(!l->get_name) {
        LOG_FAIL("loader '%s' requires a name", l->id);
        return false;
    }

    if(!l->parse) {
        LOG_FAIL("loader '%s' requires a parse function", l->id);
        return false;
    }

    if(!l->load) {
        LOG_FAIL("loader '%s' requires a load function", l->id);
        return false;
    }

    if(rd_loader_find(l->id)) {
        LOG_WARN("loader '%s' already registered", l->id);
        return false;
    }

    LOG_DEBUG("registering loader '%s'", l->id);
    RDPlugin* plugin = rd_alloc(sizeof(*plugin));
    plugin->loader = l;
    vect_push(&rd_i_state.loaders, plugin);
    return true;
}
