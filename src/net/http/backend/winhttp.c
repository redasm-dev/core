#include "core/state.h"
#include "net/http/http.h"
// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <wchar.h>
// clang-format on

typedef struct RDNetHttp {
    HINTERNET session;
} RDNetHttp;

static wchar_t* _rd_net_http_utf8_to_wide(const char* s) {
    if(!s) return NULL;

    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if(n <= 0) return NULL;

    wchar_t* w = rd_alloc(sizeof(wchar_t) * (usize)n);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static wchar_t* _rd_net_http_build_headers(const RDNetHeader* headers) {
    if(!headers || !headers->key) return NULL;

    usize total = 0;
    for(const RDNetHeader* h = headers; h->key; h++)
        total += strlen(h->key) + 2 + strlen(h->value) + 2; // ": " + "\r\n"

    wchar_t* buf = rd_alloc0(sizeof(wchar_t) * (total + 1), 1);
    wchar_t* cursor = buf;

    for(const RDNetHeader* h = headers; h->key; h++) {
        wchar_t* wk = _rd_net_http_utf8_to_wide(h->key);
        wchar_t* wv = _rd_net_http_utf8_to_wide(h->value);
        cursor += swprintf(cursor, total + 1 - (usize)(cursor - buf),
                           L"%ls: %ls\r\n", wk, wv);
        rd_free(wk);
        rd_free(wv);
    }

    return buf;
}

static bool _rd_net_http_split_url(const char* url, wchar_t** out_host,
                                   wchar_t** out_path, INTERNET_PORT* out_port,
                                   bool* out_https) {
    wchar_t* wurl = _rd_net_http_utf8_to_wide(url);
    if(!wurl) return false;

    URL_COMPONENTS uc = {0};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;

    WinHttpCrackUrl(wurl, 0, 0, &uc);

    if(uc.dwHostNameLength == 0) {
        rd_free(wurl);
        return false; // not a URL WinHTTP could parse at all
    }

    wchar_t* hostbuf = rd_alloc(sizeof(wchar_t) * (uc.dwHostNameLength + 1));
    wchar_t* pathbuf = rd_alloc(sizeof(wchar_t) * (uc.dwUrlPathLength + 1));
    wchar_t* querybuf =
        uc.dwExtraInfoLength
            ? rd_alloc(sizeof(wchar_t) * (uc.dwExtraInfoLength + 1))
            : NULL;

    uc.lpszHostName = hostbuf;
    uc.dwHostNameLength += 1;
    uc.lpszUrlPath = pathbuf;
    uc.dwUrlPathLength += 1;
    uc.lpszExtraInfo = querybuf;
    uc.dwExtraInfoLength = querybuf ? uc.dwExtraInfoLength + 1 : 0;

    bool ok = WinHttpCrackUrl(wurl, 0, 0, &uc);
    rd_free(wurl);

    if(!ok) {
        rd_free(hostbuf);
        rd_free(pathbuf);
        rd_free(querybuf);
        return false;
    }

    if(querybuf) {
        usize pathlen = wcslen(pathbuf);
        usize querylen = wcslen(querybuf);
        wchar_t* combined =
            rd_alloc(sizeof(wchar_t) * (pathlen + querylen + 1));
        wcscpy(combined, pathbuf);
        wcscat(combined, querybuf);
        rd_free(pathbuf);
        rd_free(querybuf);
        pathbuf = combined;
    }

    *out_host = hostbuf;
    *out_path = pathbuf;
    *out_port = uc.nPort;
    *out_https = uc.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

void _rd_net_http_init(void) {
    if(rd_i_state.net_http) return;

    HINTERNET session =
        WinHttpOpen(L"" RD_NET_USERAGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if(!session) {
        RD_LOG_FAIL("WinHttpOpen failed (%lu)", GetLastError());
        return;
    }

    rd_i_state.net_http = rd_alloc0(1, sizeof(RDNetHttp));
    rd_i_state.net_http->session = session;
}

void _rd_net_http_deinit(void) {
    if(!rd_i_state.net_http) return;

    WinHttpCloseHandle(rd_i_state.net_http->session);
    rd_free(rd_i_state.net_http);
    rd_i_state.net_http = NULL;
}

RDNetStatus _rd_net_http_request(const RDNetRequest* req,
                                 RDScratchBuffer* reply, u32 timeout_ms) {
    RDNetStatus st = {.ok = false, .code = 0};
    if(!rd_i_state.net_http) return st; // not initialized

    wchar_t* host = NULL;
    wchar_t* path = NULL;
    INTERNET_PORT port;
    bool https;

    if(!_rd_net_http_split_url(req->url, &host, &path, &port, &https)) {
        RD_LOG_FAIL("net: malformed URL '%s'", req->url);
        return st;
    }

    HINTERNET connect =
        WinHttpConnect(rd_i_state.net_http->session, host, port, 0);
    rd_free(host);

    if(!connect) {
        rd_free(path);
        return st;
    }

    wchar_t* wmethod = _rd_net_http_utf8_to_wide(req->method);

    HINTERNET request = WinHttpOpenRequest(
        connect, wmethod, path, NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);

    rd_free(wmethod);
    rd_free(path);

    if(!request) {
        WinHttpCloseHandle(connect);
        return st;
    }

    WinHttpSetTimeouts(request, (int)timeout_ms, (int)timeout_ms, 0, 0);

    wchar_t* headers = _rd_net_http_build_headers(req->headers);

    bool sent = WinHttpSendRequest(
        request, headers ? headers : WINHTTP_NO_ADDITIONAL_HEADERS,
        headers ? (DWORD)-1L : 0, (LPVOID)req->body, (DWORD)req->body_size,
        (DWORD)req->body_size, 0);

    rd_free(headers);

    if(sent && WinHttpReceiveResponse(request, NULL)) {
        DWORD code = 0;
        DWORD codesize = sizeof(code);
        WinHttpQueryHeaders(
            request, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
            WINHTTP_HEADER_NAME_BY_INDEX, &code, &codesize,
            WINHTTP_NO_HEADER_INDEX);

        u8 chunk[8192];
        DWORD avail = 0;

        for(;;) {
            if(!WinHttpQueryDataAvailable(request, &avail)) break;
            if(!avail) break;

            DWORD toread = avail < sizeof(chunk) ? avail : sizeof(chunk);
            DWORD got = 0;

            if(!WinHttpReadData(request, chunk, toread, &got) || !got) break;
            rd_scratch_append(reply, chunk, got);
        }

        st.ok = true;
        st.code = (int)code;
    }
    else
        RD_LOG_FAIL("net: request failed (%lu)", GetLastError());

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    return st;
}
