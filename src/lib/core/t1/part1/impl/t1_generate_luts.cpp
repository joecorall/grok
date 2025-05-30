/*
 *    Copyright (C) 2016-2025 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *    This source code incorporates work covered by the BSD 2-clause license.
 *    Please see the LICENSE file in the root directory for details.
 *
 */

#include <algorithm>
using namespace std;

#include "t1_common.h"

static int t1_init_ctxno_zc(uint32_t f, uint32_t orientation)
{
  int h, v, d, n, t, hv;
  n = 0;
  h = ((f & T1_SIGMA_3) != 0) + ((f & T1_SIGMA_5) != 0);
  v = ((f & T1_SIGMA_1) != 0) + ((f & T1_SIGMA_7) != 0);
  d = ((f & T1_SIGMA_0) != 0) + ((f & T1_SIGMA_2) != 0) + ((f & T1_SIGMA_8) != 0) +
      ((f & T1_SIGMA_6) != 0);

  switch(orientation)
  {
    case 2:
      t = h;
      h = v;
      v = t;
    case 0:
    case 1:
      if(!h)
      {
        if(!v)
        {
          if(!d)
          {
            n = 0;
          }
          else if(d == 1)
          {
            n = 1;
          }
          else
          {
            n = 2;
          }
        }
        else if(v == 1)
        {
          n = 3;
        }
        else
        {
          n = 4;
        }
      }
      else if(h == 1)
      {
        if(!v)
        {
          if(!d)
          {
            n = 5;
          }
          else
          {
            n = 6;
          }
        }
        else
        {
          n = 7;
        }
      }
      else
      {
        n = 8;
      }
      break;
    case 3:
      hv = h + v;
      if(!d)
      {
        if(!hv)
        {
          n = 0;
        }
        else if(hv == 1)
        {
          n = 1;
        }
        else
        {
          n = 2;
        }
      }
      else if(d == 1)
      {
        if(!hv)
        {
          n = 3;
        }
        else if(hv == 1)
        {
          n = 4;
        }
        else
        {
          n = 5;
        }
      }
      else if(d == 2)
      {
        if(!hv)
        {
          n = 6;
        }
        else
        {
          n = 7;
        }
      }
      else
      {
        n = 8;
      }
      break;
  }

  return (T1_CTXNO_ZC + n);
}

static int t1_init_ctxno_sc(uint32_t f)
{
  int hc, vc, n;
  n = 0;

  hc = min(((f & (T1_LUT_SIG_E | T1_LUT_SGN_E)) == T1_LUT_SIG_E) +
               ((f & (T1_LUT_SIG_W | T1_LUT_SGN_W)) == T1_LUT_SIG_W),
           1) -
       min(((f & (T1_LUT_SIG_E | T1_LUT_SGN_E)) == (T1_LUT_SIG_E | T1_LUT_SGN_E)) +
               ((f & (T1_LUT_SIG_W | T1_LUT_SGN_W)) == (T1_LUT_SIG_W | T1_LUT_SGN_W)),
           1);

  vc = min(((f & (T1_LUT_SIG_N | T1_LUT_SGN_N)) == T1_LUT_SIG_N) +
               ((f & (T1_LUT_SIG_S | T1_LUT_SGN_S)) == T1_LUT_SIG_S),
           1) -
       min(((f & (T1_LUT_SIG_N | T1_LUT_SGN_N)) == (T1_LUT_SIG_N | T1_LUT_SGN_N)) +
               ((f & (T1_LUT_SIG_S | T1_LUT_SGN_S)) == (T1_LUT_SIG_S | T1_LUT_SGN_S)),
           1);

  if(hc < 0)
  {
    hc = -hc;
    vc = -vc;
  }
  if(!hc)
  {
    if(vc == -1)
    {
      n = 1;
    }
    else if(!vc)
    {
      n = 0;
    }
    else
    {
      n = 1;
    }
  }
  else if(hc == 1)
  {
    if(vc == -1)
    {
      n = 2;
    }
    else if(!vc)
    {
      n = 3;
    }
    else
    {
      n = 4;
    }
  }

  return (T1_CTXNO_SC + n);
}

static int t1_init_spb(uint32_t f)
{
  int hc, vc, n;

  hc = min(((f & (T1_LUT_SIG_E | T1_LUT_SGN_E)) == T1_LUT_SIG_E) +
               ((f & (T1_LUT_SIG_W | T1_LUT_SGN_W)) == T1_LUT_SIG_W),
           1) -
       min(((f & (T1_LUT_SIG_E | T1_LUT_SGN_E)) == (T1_LUT_SIG_E | T1_LUT_SGN_E)) +
               ((f & (T1_LUT_SIG_W | T1_LUT_SGN_W)) == (T1_LUT_SIG_W | T1_LUT_SGN_W)),
           1);

  vc = min(((f & (T1_LUT_SIG_N | T1_LUT_SGN_N)) == T1_LUT_SIG_N) +
               ((f & (T1_LUT_SIG_S | T1_LUT_SGN_S)) == T1_LUT_SIG_S),
           1) -
       min(((f & (T1_LUT_SIG_N | T1_LUT_SGN_N)) == (T1_LUT_SIG_N | T1_LUT_SGN_N)) +
               ((f & (T1_LUT_SIG_S | T1_LUT_SGN_S)) == (T1_LUT_SIG_S | T1_LUT_SGN_S)),
           1);

  if(!hc && !vc)
  {
    n = 0;
  }
  else
  {
    n = (!(hc > 0 || (!hc && vc > 0)));
  }

  return n;
}

static void dump_array16(int array[], int size)
{
  int i;
  --size;
  for(i = 0; i < size; ++i)
  {
    printf("0x%04x,", array[i]);
    if(!((i + 1) & 0x7))
    {
      printf("\n    ");
    }
    else
    {
      printf(" ");
    }
  }
  printf("0x%04x\n};\n\n", array[size]);
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
  unsigned int i, j;
  double u, v, t;

  int lut_ctxno_zc[2048];
  int lut_nmsedec_sig[1 << T1_NMSEDEC_BITS];
  int lut_nmsedec_sig0[1 << T1_NMSEDEC_BITS];
  int lut_nmsedec_ref[1 << T1_NMSEDEC_BITS];
  int lut_nmsedec_ref0[1 << T1_NMSEDEC_BITS];

  printf("/* This file was automatically generated by t1_generate_luts.c */\n\n");

  /* lut_ctxno_zc */
  for(j = 0; j < 4; ++j)
  {
    for(i = 0; i < 512; ++i)
    {
      uint32_t orientation = j;
      if(orientation == 2)
      {
        orientation = 1;
      }
      else if(orientation == 1)
      {
        orientation = 2;
      }
      lut_ctxno_zc[(orientation << 9) | i] = t1_init_ctxno_zc(i, j);
    }
  }

  printf("static const uint8_t lut_ctxno_zc[2048] = {\n    ");
  for(i = 0; i < 2047; ++i)
  {
    printf("%i,", lut_ctxno_zc[i]);
    if(!((i + 1) & 0x1f))
    {
      printf("\n    ");
    }
    else
    {
      printf(" ");
    }
  }
  printf("%i\n};\n\n", lut_ctxno_zc[2047]);

  /* lut_ctxno_sc */
  printf("static const uint8_t lut_ctxno_sc[256] = {\n    ");
  for(i = 0; i < 255; ++i)
  {
    printf("0x%x,", t1_init_ctxno_sc(i));
    if(!((i + 1) & 0xf))
    {
      printf("\n    ");
    }
    else
    {
      printf(" ");
    }
  }
  printf("0x%x\n};\n\n", t1_init_ctxno_sc(255));

  /* lut_spb */
  printf("static const uint8_t lut_spb[256] = {\n    ");
  for(i = 0; i < 255; ++i)
  {
    printf("%i,", t1_init_spb(i));
    if(!((i + 1) & 0x1f))
    {
      printf("\n    ");
    }
    else
    {
      printf(" ");
    }
  }
  printf("%i\n};\n\n", t1_init_spb(255));

  /* FIXME FIXME FIXME */
  /* fprintf(stdout,"nmsedec luts:\n"); */
  for(i = 0U; i < (1U << T1_NMSEDEC_BITS); ++i)
  {
    t = i / pow(2, T1_NMSEDEC_FRACBITS);
    u = t;
    v = t - 1.5;
    lut_nmsedec_sig[i] = max(0, (int)(floor((u * u - v * v) * pow(2, T1_NMSEDEC_FRACBITS) + 0.5) /
                                      pow(2, T1_NMSEDEC_FRACBITS) * 8192.0));
    lut_nmsedec_sig0[i] = max(0, (int)(floor((u * u) * pow(2, T1_NMSEDEC_FRACBITS) + 0.5) /
                                       pow(2, T1_NMSEDEC_FRACBITS) * 8192.0));
    u = t - 1.0;
    if(i & (1 << (T1_NMSEDEC_BITS - 1)))
    {
      v = t - 1.5;
    }
    else
    {
      v = t - 0.5;
    }
    lut_nmsedec_ref[i] = max(0, (int)(floor((u * u - v * v) * pow(2, T1_NMSEDEC_FRACBITS) + 0.5) /
                                      pow(2, T1_NMSEDEC_FRACBITS) * 8192.0));
    lut_nmsedec_ref0[i] = max(0, (int)(floor((u * u) * pow(2, T1_NMSEDEC_FRACBITS) + 0.5) /
                                       pow(2, T1_NMSEDEC_FRACBITS) * 8192.0));
  }

  printf("static const int16_t lut_nmsedec_sig[1U << T1_NMSEDEC_BITS] = {\n    ");
  dump_array16(lut_nmsedec_sig, 1U << T1_NMSEDEC_BITS);

  printf("static const int16_t lut_nmsedec_sig0[1U << T1_NMSEDEC_BITS] = {\n    ");
  dump_array16(lut_nmsedec_sig0, 1U << T1_NMSEDEC_BITS);

  printf("static const int16_t lut_nmsedec_ref[1U << T1_NMSEDEC_BITS] = {\n    ");
  dump_array16(lut_nmsedec_ref, 1U << T1_NMSEDEC_BITS);

  printf("static const int16_t lut_nmsedec_ref0[1U << T1_NMSEDEC_BITS] = {\n    ");
  dump_array16(lut_nmsedec_ref0, 1U << T1_NMSEDEC_BITS);

  return 0;
}
