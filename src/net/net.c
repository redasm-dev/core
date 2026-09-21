#if defined(RD_HAS_NETWORK)
#include "core/state.h"
#endif

#include <redasm/common.h>
#include <redasm/net/net.h>
#include <redasm/support/logging.h>

bool rd_net_is_enabled(void) {
#if defined(RD_HAS_NETWORK)
    return rd_i_state.is_network_enabled;
#else
    return false;
#endif
}
