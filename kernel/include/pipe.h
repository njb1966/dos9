#pragma once

/* Create an anonymous pipe.  Fills fds[0]=read end, fds[1]=write end.
   Returns 0 on success, -1 on failure. */
int pipe_create(int fds[2]);
