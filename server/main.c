/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "render_context.h"
#include "render_server.h"
#include "render_worker.h"

#include <string.h>

/* The main process is the server process.  It enters render_server_main and
 * never returns except on fatal errors.
 *
 * The server process supports only one connection currently.  It creates a
 * render_client to manage the connection.  There is a client process at the
 * other end of the connection.  When the client process requests a new
 * context to be created, the server process creates a worker.  It also sets
 * up a socket pair, with one end owned by the worker and the other end sent
 * to and owned by the client process.
 *
 * A worker can be a subprocess forked from the server process, or a thread
 * created by the server process.  When a worker is a subprocess, the
 * subprocess returns from render_server_main and enters render_context_main.
 *
 * When a worker is a thread, the thread enters render_context_main directly
 * from its start function.  In this case, render_context_main must be
 * thread-safe.
 *
 * On macOS, subprocess workers use posix_spawn() instead of pure fork() to
 * ensure XPC connections (used by Metal) remain valid.  The spawned process
 * starts here with --worker-mode and enters render_context_main directly.
 */

#ifdef __APPLE__
static bool
is_worker_mode(int argc, char **argv)
{
   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--worker-mode") == 0)
         return true;
   }
   return false;
}
#endif

int
main(int argc, char **argv)
{
   render_log_init();

#ifdef __APPLE__
   if (is_worker_mode(argc, argv)) {
      struct render_context_args ctx_args;
      if (!render_worker_parse_args(argc, argv, &ctx_args))
         return -1;
      return render_context_main(&ctx_args) ? 0 : -1;
   }
#endif

   struct render_context_args ctx_args;
   bool ok = render_server_main(argc, argv, &ctx_args);

   /* this is a subprocess (Linux pure-fork path) */
   if (ok && ctx_args.valid)
      ok = render_context_main(&ctx_args);

   return ok ? 0 : -1;
}
