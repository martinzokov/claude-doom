// doomgeneric's platform layer for the Claude Code doom mod.
//
// Frames: every ~66 ms the screen is scaled down to DOOM_COLS x DOOM_ROWS
// terminal cells, two pixels per cell (an upper half block, the top pixel as
// its foreground and the bottom one as its background), and written to stdout
// as one line, "F <cols>x<rows> <base64>\n", already in the shape of a Raster's `cells`.
//
// Keys: the mod rewrites DOOM_KEYS with the latest events, one "<seq> <name>"
// per line; we act on the sequence numbers we have not seen. A terminal sends
// no key releases, so each key is released by itself once its repeats stop.
// A line "<seq> QUIT" ends the game.

#include "doomgeneric.h"
#include "doomkeys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int cols = 80, rows = 25;
static const char *keys_path;
static uint32_t last_frame, last_poll;
static long last_seq;
static unsigned char *cells;
static char *b64;

// Held keys and when each lets go.
static uint32_t release_at[256];
#define QUEUE 64
static unsigned short queue[QUEUE];
static int q_head, q_tail;

static uint32_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void push(int pressed, unsigned char key) {
  int next = (q_tail + 1) % QUEUE;
  if (next == q_head) return;
  queue[q_tail] = (unsigned short)((pressed << 8) | key);
  q_tail = next;
}

static void resize(int c, int r) {
  cols = c;
  rows = r;
  free(cells);
  free(b64);
  cells = malloc((size_t)cols * rows * 12);
  b64 = malloc((size_t)cols * rows * 16 + 8);
  last_frame = 0;
}

static int to_doom(const char *name) {
  if (!strcmp(name, "up") || !strcmp(name, "w")) return KEY_UPARROW;
  if (!strcmp(name, "down") || !strcmp(name, "s")) return KEY_DOWNARROW;
  if (!strcmp(name, "left") || !strcmp(name, "a")) return KEY_LEFTARROW;
  if (!strcmp(name, "right") || !strcmp(name, "d")) return KEY_RIGHTARROW;
  if (!strcmp(name, "q")) return KEY_STRAFE_L;
  if (!strcmp(name, "e")) return KEY_STRAFE_R;
  if (!strcmp(name, "space")) return KEY_FIRE;
  if (!strcmp(name, "f")) return KEY_USE;
  if (!strcmp(name, "return") || !strcmp(name, "enter")) return KEY_ENTER;
  if (!strcmp(name, "m")) return KEY_ESCAPE;
  if (!strcmp(name, "tab")) return KEY_TAB;
  if (!strcmp(name, "backspace")) return KEY_BACKSPACE;
  if (strlen(name) == 1 && name[0] > ' ' && name[0] < 127) return (unsigned char)name[0];
  return -1;
}

static void poll_keys(void) {
  uint32_t now = now_ms();
  if (now - last_poll >= 10) {
    last_poll = now;
    if (getppid() == 1) exit(0); // Claude Code is gone
    FILE *f = keys_path ? fopen(keys_path, "r") : NULL;
    if (f) {
      char text[4096];
      size_t n = fread(text, 1, sizeof text - 1, f);
      fclose(f);
      text[n] = 0;
      // A line still being written has no newline yet: leave it for next time.
      char *end = strrchr(text, '\n');
      if (end) end[1] = 0; else text[0] = 0;
      long seq;
      char name[32];
      int used;
      for (char *p = text; sscanf(p, "%ld %31s%n", &seq, name, &used) == 2; p += used) {
        if (seq <= last_seq) continue;
        last_seq = seq;
        if (!strcmp(name, "QUIT")) exit(0);
        int c, r;
        if (sscanf(name, "SIZE:%dx%d", &c, &r) == 2 && c > 0 && r > 0 && c <= 512 && r <= 256) {
          resize(c, r);
          continue;
        }
        int key = to_doom(name);
        if (key < 0) continue;
        // The first press holds past the terminal's repeat delay; a repeat
        // only needs to last until the next one.
        if (!release_at[key]) {
          push(1, (unsigned char)key);
          release_at[key] = now + 450;
        } else {
          release_at[key] = now + 130;
        }
      }
    }
  }
  for (int k = 0; k < 256; k++) {
    if (release_at[k] && (int32_t)(now - release_at[k]) >= 0) {
      release_at[k] = 0;
      push(0, (unsigned char)k);
    }
  }
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t base64(const unsigned char *in, size_t n, char *out) {
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = in[i] << 16;
    if (i + 1 < n) v |= in[i + 1] << 8;
    if (i + 2 < n) v |= in[i + 2];
    out[o++] = B64[(v >> 18) & 63];
    out[o++] = B64[(v >> 12) & 63];
    out[o++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < n ? B64[v & 63] : '=';
  }
  return o;
}

// The average colour of one output pixel's box of screen pixels.
static uint32_t sample(int px, int py, int pw, int ph) {
  int x0 = px * DOOMGENERIC_RESX / pw, x1 = (px + 1) * DOOMGENERIC_RESX / pw;
  int y0 = py * DOOMGENERIC_RESY / ph, y1 = (py + 1) * DOOMGENERIC_RESY / ph;
  uint32_t r = 0, g = 0, b = 0, n = 0;
  for (int y = y0; y < y1; y += 2) {
    for (int x = x0; x < x1; x += 2) {
      uint32_t c = DG_ScreenBuffer[y * DOOMGENERIC_RESX + x];
      r += (c >> 16) & 255;
      g += (c >> 8) & 255;
      b += c & 255;
      n++;
    }
  }
  if (!n) return 0;
  return ((r / n) << 16) | ((g / n) << 8) | (b / n);
}

static void put32(unsigned char *p, uint32_t v) {
  p[0] = v & 255;
  p[1] = (v >> 8) & 255;
  p[2] = (v >> 16) & 255;
  p[3] = (v >> 24) & 255;
}

void DG_Init(void) {
  const char *c = getenv("DOOM_COLS"), *r = getenv("DOOM_ROWS");
  if (c && atoi(c) > 0) cols = atoi(c);
  if (r && atoi(r) > 0) rows = atoi(r);
  keys_path = getenv("DOOM_KEYS");
  cells = malloc((size_t)cols * rows * 12);
  b64 = malloc((size_t)cols * rows * 16 + 8);
}

void DG_DrawFrame(void) {
  uint32_t now = now_ms();
  if (now - last_frame < 66) return;
  last_frame = now;
  int ph = rows * 2;
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      unsigned char *p = cells + ((size_t)y * cols + x) * 12;
      put32(p, 0x2580);
      put32(p + 4, sample(x, y * 2, cols, ph));
      put32(p + 8, sample(x, y * 2 + 1, cols, ph));
    }
  }
  size_t n = base64(cells, (size_t)cols * rows * 12, b64);
  printf("F %dx%d ", cols, rows);
  fwrite(b64, 1, n, stdout);
  fputc('\n', stdout);
  fflush(stdout);
}

void DG_SleepMs(uint32_t ms) { usleep(ms * 1000); }

uint32_t DG_GetTicksMs(void) { return now_ms(); }

int DG_GetKey(int *pressed, unsigned char *key) {
  poll_keys();
  if (q_head == q_tail) return 0;
  unsigned short e = queue[q_head];
  q_head = (q_head + 1) % QUEUE;
  *pressed = e >> 8;
  *key = e & 255;
  return 1;
}

void DG_SetWindowTitle(const char *title) { (void)title; }

int main(int argc, char **argv) {
  doomgeneric_Create(argc, argv);
  for (;;) doomgeneric_Tick();
  return 0;
}
