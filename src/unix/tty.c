/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>


int uv_tty_init(uv_loop_t* loop, uv_tty_t* tty, int fd) {
  uv__nonblock(fd, 1);
  uv__stream_init(loop, (uv_stream_t*)tty, UV_TTY);
  uv__stream_open((uv_stream_t*)tty, fd, UV_READABLE | UV_WRITABLE);
  loop->counters.tty_init++;
  tty->mode = 0;
  return 0;
}


int uv_tty_set_mode(uv_tty_t* tty, int mode) {
  int fd = tty->fd;
  struct termios raw;

  if (mode && tty->mode == 0) {
    /* on */

    if (tcgetattr(fd, &tty->orig_termios)) {
      goto fatal;
    }

    raw = tty->orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag |= (ONLCR);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    /* Put terminal in raw mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH, &raw)) {
      goto fatal;
    }

    tty->mode = 1;
    return 0;
  } else if (mode == 0 && tty->mode) {
    /* off */

    /* Put terminal in original mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH, &tty->orig_termios)) {
      goto fatal;
    }

    tty->mode = 0;
    return 0;
  }

fatal:
  uv__set_sys_error(tty->loop, errno);
  return -1;
}


int uv_tty_get_winsize(uv_tty_t* tty, int* width, int* height) {
  struct winsize ws;

  if (ioctl(tty->fd, TIOCGWINSZ, &ws) < 0) {
    uv__set_sys_error(tty->loop, errno);
    return -1;
  }

  *width = ws.ws_col;
  *height = ws.ws_row;

  return 0;
}


uv_handle_type uv_guess_handle(uv_file file) {
  struct stat s;

  if (file < 0) {
    uv__set_sys_error(NULL, EINVAL); /* XXX Need loop? */
    return -1;
  }

  if (isatty(file)) {
    return UV_TTY;
  }

  if (fstat(file, &s)) {
    uv__set_sys_error(NULL, errno); /* XXX Need loop? */
    return -1;
  }

  if (!S_ISSOCK(s.st_mode) && !S_ISFIFO(s.st_mode)) {
    return UV_FILE;
  }

  return UV_NAMED_PIPE;
}
