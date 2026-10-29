// picture.c
// Reads a png/jpg/bmp/tga file, scales it into the 512x256 canvas the game's flag picture uses and encodes it as flipped BC2 pixels.
// Copyright (c) 2026 TpRedNinja (also known as TpRedninja, TopRedNinja, NotTpRedNinja and NotTopRedNinja). MIT licence, see LICENSE.
#include "ac1ach.h"
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include "stb_image.h"

static uint16_t to565(int r, int g, int b) { return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }
static void from565(uint16_t c, int* r, int* g, int* b) {
  *r = ((c >> 11) & 31) * 255 / 31; *g = ((c >> 5) & 63) * 255 / 63; *b = (c & 31) * 255 / 31;
}
static uint8_t* encode_bc2(const uint8_t* rgba /*512*256*4, already flipped*/) {
  uint8_t* out = (uint8_t*)malloc(0x20000); uint8_t* o = out;
  for (int by = 0; by < 64; by++) for (int bx = 0; bx < 128; bx++) {
    const uint8_t* px[16]; for (int i = 0; i < 16; i++) px[i] = rgba + ((by * 4 + i / 4) * 512 + bx * 4 + i % 4) * 4;
    for (int r = 0; r < 4; r++) {
      uint16_t w = 0; for (int x = 0; x < 4; x++) w |= (uint16_t)((px[r * 4 + x][3] >> 4) << (4 * x));
      *o++ = w & 255; *o++ = w >> 8;
    }
    int lmax = -1, lmin = 1 << 30, imax = 0, imin = 0, any = 0;
    for (int i = 0; i < 16; i++) { if (px[i][3] == 0) continue; any = 1;
      int l = px[i][0] * 299 + px[i][1] * 587 + px[i][2] * 114;
      if (l > lmax) { lmax = l; imax = i; } if (l < lmin) { lmin = l; imin = i; } }
    uint16_t c0 = any ? to565(px[imax][0], px[imax][1], px[imax][2]) : 0;
    uint16_t c1 = any ? to565(px[imin][0], px[imin][1], px[imin][2]) : 0;
    int pr[4], pg[4], pb[4]; from565(c0, &pr[0], &pg[0], &pb[0]); from565(c1, &pr[1], &pg[1], &pb[1]);
    for (int k = 0; k < 3; k++) { /*placeholder*/ }
    pr[2] = (2 * pr[0] + pr[1]) / 3; pg[2] = (2 * pg[0] + pg[1]) / 3; pb[2] = (2 * pb[0] + pb[1]) / 3;
    pr[3] = (pr[0] + 2 * pr[1]) / 3; pg[3] = (pg[0] + 2 * pg[1]) / 3; pb[3] = (pb[0] + 2 * pb[1]) / 3;
    *o++ = c0 & 255; *o++ = c0 >> 8; *o++ = c1 & 255; *o++ = c1 >> 8;
    for (int r = 0; r < 4; r++) { uint8_t bits = 0;
      for (int x = 0; x < 4; x++) { const uint8_t* p = px[r * 4 + x]; int best = 0, bd = 1 << 30;
        for (int k = 0; k < 4; k++) { int dr = p[0] - pr[k], dg = p[1] - pg[k], db = p[2] - pb[k]; int d = dr * dr + dg * dg + db * db; if (d < bd) { bd = d; best = k; } }
        bits |= (uint8_t)(best << (2 * x)); }
      *o++ = bits; }
  }
  return out;
}
static double cubic_w(double t) { t = t < 0 ? -t : t; return t < 1 ? 1.5 * t * t * t - 2.5 * t * t + 1 : t < 2 ? -0.5 * t * t * t + 2.5 * t * t - 4 * t + 2 : 0; }
static uint8_t* build_picture(const char* path) {
  int w, h, n; uint8_t* img = stbi_load(path, &w, &h, &n, 4); if (!img) return NULL;
  uint8_t* canvas = (uint8_t*)calloc(512 * 256, 4);
  double s = 256.0 / (w > h ? w : h); int dw = (int)(w * s + 0.5), dh = (int)(h * s + 0.5); if (dw > 512) dw = 512; if (dh > 256) dh = 256;
  int ox = (512 - dw) / 2, oy = (256 - dh) / 2;
  uint8_t* flipped = (uint8_t*)calloc(512 * 256, 4);
  for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
    double fx = (x + 0.5) / s - 0.5, fy = (y + 0.5) / s - 0.5;      // source position of this output pixel
    double acc[4] = { 0, 0, 0, 0 };
    if (s >= 1.0) {                                                  // enlarging: bicubic (Catmull-Rom) instead of blocky pixels
      int ix = (int)floor(fx), iy = (int)floor(fy); double tx = fx - ix, ty = fy - iy;
      double wx[4], wy[4];
      for (int k = 0; k < 4; k++) { wx[k] = cubic_w(k - 1 - tx); wy[k] = cubic_w(k - 1 - ty); }
      for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
        int px = ix - 1 + i, py = iy - 1 + j; if (px < 0) px = 0; if (px >= w) px = w - 1; if (py < 0) py = 0; if (py >= h) py = h - 1;
        const uint8_t* p = img + (py * w + px) * 4; double wgt = wx[i] * wy[j], a = p[3] / 255.0;
        acc[0] += wgt * p[0] * a; acc[1] += wgt * p[1] * a; acc[2] += wgt * p[2] * a; acc[3] += wgt * a;   // premultiplied so transparent edges do not bleed
      }
    } else {                                                         // shrinking: average the covered source pixels
      int x0 = (int)floor(x / s), x1 = (int)ceil((x + 1) / s), y0 = (int)floor(y / s), y1 = (int)ceil((y + 1) / s);
      if (x1 > w) x1 = w; if (y1 > h) y1 = h; if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
      double cnt = 0;
      for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        const uint8_t* p = img + (py * w + px) * 4; double a = p[3] / 255.0;
        acc[0] += p[0] * a; acc[1] += p[1] * a; acc[2] += p[2] * a; acc[3] += a; cnt++;
      }
      for (int c = 0; c < 4; c++) acc[c] /= cnt;
    }
    uint8_t* o = canvas + ((oy + y) * 512 + ox + x) * 4;
    double a = acc[3]; if (a < 0) a = 0; if (a > 1) a = 1;
    for (int c = 0; c < 3; c++) { double v = a > 0.0001 ? acc[c] / acc[3] : 0; o[c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5); }
    o[3] = (uint8_t)(a * 255 + 0.5);
  }
  for (int y = 0; y < 256; y++) memcpy(flipped + (255 - y) * 512 * 4, canvas + y * 512 * 4, 512 * 4);
  uint8_t* bc2 = encode_bc2(flipped);
  free(canvas); free(flipped); stbi_image_free(img);
  return bc2;
}
// Finds the picture for an achievement (the ini name, else Pictures\NN.png) and returns it ready for the game, or NULL.
uint8_t* load_slot_picture(int slot) {
  char sec[8], name[MAX_PATH], path[MAX_PATH]; snprintf(sec, sizeof sec, "%02d", slot);
  ini_get(sec, "Picture", "", name, sizeof name);
  static const char* ext[] = { "png", "jpg", "jpeg", "bmp", "tga" };
  if (name[0]) { snprintf(path, sizeof path, "%sPictures\\%s", g_mod, name); uint8_t* r = build_picture(path); if (r) return r; }
  for (int i = 0; i < 5; i++) { snprintf(path, sizeof path, "%sPictures\\%02d.%s", g_mod, slot, ext[i]); uint8_t* r = build_picture(path); if (r) return r; }
  return NULL;
}
