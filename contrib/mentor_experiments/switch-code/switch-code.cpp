#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "jassert.h"
#include "config.h"
#include "dmtcp.h"
#include "trampolines.h"

#ifdef SWITCH_CODE_PLUGIN_DEBUG
# undef JTRACE
# define JTRACE JNOTE
#endif // ifdef SWITCH_CODE_PLUGIN_DEBUG

static trampoline_info_t main_trampoline_info;

// Use REPLACE_SYMBOL_WITH_DEBUG to specify the name of the symbol to replace
// on restart. The production version of the symbol is replaced with its debug
// version on restart.
// Use DEBUG_LIB to specify the path to the library with debug symbol
//
// Example usage:
//    DEBUG_LIB=./libdmtcp1.so REPLACE_SYMBOL_WITH_DEBUG=time_loop \
//       ../../bin/dmtcp_launch --with-plugin ./dmtcp_switch-code.so ./dmtcp1
//
// In this example, the function "time_loop" defined in dmtcp1 will be replaced
// with its debug version (defined in libdmtcp1.so) on restart.
static void
restart()
{
  void *handle = dlopen(NULL, RTLD_NOW);
  void *addr = dlsym(handle, getenv("REPLACE_SYMBOL_WITH_DEBUG"));
  if (addr != NULL) {
    void *debugHandle = dlopen(getenv("DEBUG_LIB"), RTLD_NOW);
    void *debugWrapper = dlsym(debugHandle,
                               getenv("REPLACE_SYMBOL_WITH_DEBUG"));
    JASSERT(debugWrapper != NULL);
    dmtcp_setup_trampoline_by_addr(addr, debugWrapper, &main_trampoline_info);
  }
}

static void
switch_code_event_hook(DmtcpEvent_t event, DmtcpEventData_t *data)
{
  switch (event) {
  case DMTCP_EVENT_INIT:
  {
    JTRACE("The plugin has been initialized.");
    break;
  }
  default:
    break;
  }
}

static DmtcpBarrier switchcodeBarriers[] = {
  { DMTCP_GLOBAL_BARRIER_RESTART, restart, "restart" }
};

DmtcpPluginDescriptor_t switch_code_plugin = {
  DMTCP_PLUGIN_API_VERSION,
  PACKAGE_VERSION,
  "switch-code",
  "DMTCP",
  "dmtcp@ccs.neu.edu",
  "Switch-code plugin",
  DMTCP_DECL_BARRIERS(switchcodeBarriers),
  switch_code_event_hook
};

DMTCP_DECL_PLUGIN(switch_code_plugin);
