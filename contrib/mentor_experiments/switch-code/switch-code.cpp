#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "jassert.h"
#include "config.h"
#include "dmtcp.h"
#include "trampolines.h"

#ifdef SWITCH_CODE_PLUGIN_DEBUG
# undef JTRACE
# define JTRACE JNOTE
#endif // ifdef SWITCH_CODE_PLUGIN_DEBUG

using namespace std;
static trampoline_info_t main_trampoline_info;
char * DEBUG_LIB = getenv("DEBUG_LIB");
char * BASE_EXE = getenv("BASE_EXE");
char * SYMBOL_LIST = getenv("SYMBOL_LIST");

static void
replace_symbol(void * handle, void * debugHandle, char * symbol)
{
  void *addr = dlsym(handle, symbol);
  if (addr != NULL) {
    JNOTE("Address found")(handle);
    void *debugWrapper = dlsym(debugHandle, symbol);
    if (debugWrapper != NULL) {
      JNOTE("Old Address")(addr);
      JNOTE("New Address")(debugWrapper);
      dmtcp_setup_trampoline_by_addr(addr, debugWrapper, &main_trampoline_info);
    } else {
      JNOTE("Address not present in the debug library") (symbol);
    }
  } else {
    JNOTE("Address not found");
  }
}
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
  // get the handle for both optimized executable and unoptimzed library
  // by opening with dlopen
  void *handle = dlopen(NULL, RTLD_NOW);
  void *debugHandle = dlopen(DEBUG_LIB, RTLD_NOW);
  JASSERT(debugHandle != NULL)(dlerror());
  // TODO(Twinkle) : migrate work from python to c/c++
  ostringstream o;
  o << "python get_symbol_list.py " << BASE_EXE << " " << DEBUG_LIB;
  string cmd = o.str();
  system(cmd.c_str());
  cout << DEBUG_LIB << "\n" << BASE_EXE << "\n" << SYMBOL_LIST << "\n";
  // get all the symbols to be replaced from the file created by the python
  // script.
  // TODO(Twinkle) :use read system call
  FILE * fp;
  char symbol[200];
  fp = fopen(SYMBOL_LIST, "r+");
  JASSERT(fp != NULL) (JASSERT_ERRNO);
  while (fscanf(fp, "%s", symbol) == 1)
  {
    printf("symbol = %s\n", symbol);
    replace_symbol(handle, debugHandle, symbol);
  }
  int dummy = 0;
  while(!dummy);
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
