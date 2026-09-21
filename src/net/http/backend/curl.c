#include "core/state.h"
#include "net/http/http.h"
#include <curl/curl.h>

typedef struct RDNetHttp {
    RDCharVect hdr_line_buf;
    CURL* curl;
} RDNetHttp;

static usize _curl_write_cb(void* data, usize size, usize nmemb, void* ud) {
    RDScratchBuffer* buf = (RDScratchBuffer*)ud;
    usize n = size * nmemb;
    rd_scratch_append(buf, data, n);
    return n;
}

static struct curl_slist* _curl_build_headers(const RDNetHeader* headers) {
    struct curl_slist* list = NULL;
    if(!headers) return NULL;

    for(const RDNetHeader* h = headers; h->key; h++) {
        // format is: "Key: Value"
        // curl_slist_append copies the string internally,
        // the local buffer doesn't need to outlive this call.
        const char* line = rd_i_format(&rd_i_state.net_http->hdr_line_buf,
                                       "%s: %s", h->key, h->value);
        list = curl_slist_append(list, line);
    }

    return list;
}

static void _curl_apply_fixed_options(CURL* curl) { // NOLINT
    // clang-format off
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // required off the main thread
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _curl_write_cb);
    // clang-format on
}

static void _curl_apply_request(CURL* curl, const RDNetRequest* req, // NOLINT
                                struct curl_slist* headers,
                                RDScratchBuffer* reply, u32 timeout_ms) {
    // clang-format off
    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req->method);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, reply);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, req->user_agent ? req->user_agent : RD_NET_USERAGENT);
    // clang-format on

    if(headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if(req->body && req->body_size) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req->body_size);
    }
}

void _rd_net_http_init(void) {
    if(rd_i_state.net_http) return;
    if(curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return;

    rd_i_state.net_http = rd_alloc0(1, sizeof(RDNetHttp));
    rd_i_state.net_http->curl = curl_easy_init();

    if(!rd_i_state.net_http->curl) {
        RD_LOG_FAIL("CURL initialization failed");
        _rd_net_http_deinit();
    }
}

void _rd_net_http_deinit(void) {
    if(!rd_i_state.net_http) return;

    curl_easy_cleanup(rd_i_state.net_http->curl);
    vect_destroy(&rd_i_state.net_http->hdr_line_buf);
    curl_global_cleanup();
    rd_free(rd_i_state.net_http);
    rd_i_state.net_http = NULL;
}

RDNetStatus _rd_net_http_request(const RDNetRequest* req,
                                 RDScratchBuffer* reply, u32 timeout_ms) {
    RDNetStatus st = {.ok = false, .code = 0};
    if(!rd_i_state.net_http || !req || !reply) return st;

    CURL* curl = rd_i_state.net_http->curl;
    curl_easy_reset(curl); // clears all prior setopt state
    _curl_apply_fixed_options(curl);

    struct curl_slist* headers = _curl_build_headers(req->headers);
    _curl_apply_request(curl, req, headers, reply, timeout_ms);

    CURLcode res = curl_easy_perform(curl);

    if(res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        st.ok = true;
        st.code = (int)code;
    }
    else {
        RD_LOG_FAIL("%s (%s %s)", curl_easy_strerror(res), req->method,
                    req->url);
    }

    if(headers) curl_slist_free_all(headers);
    return st;
}
