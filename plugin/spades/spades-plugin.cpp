#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "dmtcp.h"
#include "config.h"
#include "jassert.h"
#include "../src/constants.h"

using namespace dmtcp;

dmtcp::string ckptDir;
dmtcp::string outputDirName;
dmtcp::string outputDirAbsPath;
pid_t leader_pid;

// Every process would try to get the lock
void pre_ckpt()
{
  /*
    QUE: should we use random wait to give preference to one process?
    struct timespec tim;
    tim.tv_sec = 1;
    tim.tv_nsec = 500000L * rand() / RAND_MAX;
    nanosleep(&tim ,(struct timespec *)NULL);
  */
  // get the output directory of spades
  ckptDir = getenv(ENV_VAR_CHECKPOINT_DIR);
  JASSERT (ckptDir != "") ("checkpoint dir is not set!");
  // open the file in the output directory
  dmtcp::string flock_file  = ckptDir + "/precious_file";
  int flag = O_RDONLY;
  if( access(flock_file.c_str(), F_OK ) == -1 ) {
    flag = O_RDWR |  O_CREAT;
  }
  int fd = open(flock_file.c_str(), flag);
  JASSERT ((fd != -1) || (errno == EACCES)) (JASSERT_ERRNO) (fd);
  int lock = LOCK_EX;
  if (fd != -1) {
    // get the lock
    int ret = flock(fd, lock);
    if (ret != -1) {
      JTRACE("leader is elected") (getpid());
      leader_pid = getpid();
    }
    close(fd);
    unlink(flock_file.c_str());
  }
}

int save_output_dir()
{
  outputDirAbsPath = getenv(ENV_VAR_OUTDIR);
  JASSERT (outputDirAbsPath != "") ("spades output dir name is not found");

  // directory name
  size_t index = outputDirAbsPath.find_last_of("/");
  outputDirName = outputDirAbsPath.substr(index+1);

  // make the directory if not exist
  ostringstream mkdirCmd;
  mkdirCmd << "mkdir -p "<< ckptDir << "/" << outputDirName << "_bck";
  system(mkdirCmd.str().c_str());
  JTRACE("directory make command")(mkdirCmd.str());
  // copy the output directory
  ostringstream cmd;
  cmd << "cp -rf " << outputDirAbsPath << "/. " << ckptDir
    << "/" << outputDirName << "_bck/";
  system(cmd.str().c_str());
  JTRACE("directory copied command ")(cmd.str());
  return 1;
}

int is_leader()
{
  return (getpid() == leader_pid);
}

/* 
 * Every process 
 * select the leader to save the precious files
 */
void drain_precious_files()
{
  // save the output directory of the spades if you are the leader
  if (is_leader())
  {
    save_output_dir();
  }
}

/*
  restore the output dir
*/
void restart()
{
  if (is_leader()) {
    JTRACE("Output dir is restore back to the original path");
    ostringstream mkdirCmd;
    // make the directory if doesn't exist
    mkdirCmd << "mkdir -p "<< outputDirAbsPath;
    system(mkdirCmd.str().c_str());
    JTRACE("directory restore command")(mkdirCmd.str());

    // copy the output directory
    ostringstream cmd;
    cmd << "cp -rf " << ckptDir << "/" << outputDirName << "_bck/. "
      << outputDirAbsPath << "/";
    system(cmd.str().c_str());
    JTRACE("directory copied command ")(cmd.str());
  }
}

  static void
cuda_plugin_event_hook(DmtcpEvent_t event, DmtcpEventData_t *data)
{
  switch (event) {
    case DMTCP_EVENT_INIT:
      {
        JTRACE("*** DMTCP_EVENT_INIT");
        JTRACE("Plugin intialized");
        break;
      }
    case DMTCP_EVENT_EXIT:
      JTRACE("*** DMTCP_EVENT_EXIT");
      break;
    default:
      break;
  }
}

static DmtcpBarrier spadesPluginBarriers[] = {
  {DMTCP_GLOBAL_BARRIER_PRE_CKPT, pre_ckpt, "checkpoint"},
  {DMTCP_GLOBAL_BARRIER_PRE_CKPT, drain_precious_files, "save-precious-files"},
  {DMTCP_GLOBAL_BARRIER_RESTART, restart, "restart"}
};

DmtcpPluginDescriptor_t spades_plugin = {
  DMTCP_PLUGIN_API_VERSION,
  PACKAGE_VERSION,
  "spades_plugin",
  "DMTCP",
  "dmtcp@ccs.neu.edu",
  "Cuda Split Plugin",
  DMTCP_DECL_BARRIERS(spadesPluginBarriers),
  cuda_plugin_event_hook
};

DMTCP_DECL_PLUGIN(spades_plugin);

