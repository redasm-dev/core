#include <redasm/version.h>
#include <stdio.h>

RDVersion rd_version(void) {
    RDVersion v = {0};
    bool ok = rd_version_parse(RD_VERSION, &v);
    assert(ok && "invalid version string");
    return v;
}

const char* rd_version_string(void) { return RD_VERSION; }
const char* rd_version_build(void) { return RD_BUILD_VERSION; }

bool rd_version_parse(const char* s, RDVersion* v) {
    if(!s || !v) return false;

    int major, minor, patch;
    int consumed = 0;

    if(sscanf(s, "%d.%d.%d%n", &major, &minor, &patch, &consumed) != 3) {
        return -1;
    }

    if(s[consumed] == '-') // point to char after '-'
        v->suffix = s + consumed + 1;
    else if(s[consumed] != '\0') // not end-of-string (eg. "4.0.0x")
        return -1;
    else // no suffix
        v->suffix = NULL;

    v->major = major;
    v->minor = minor;
    v->patch = patch;
    return true;
}
