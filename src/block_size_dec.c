/*Daala video codec
Copyright (c) 2013 Daala project contributors.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "block_size.h"
#include "block_size_dec.h"

void od_block_size_decode(od_ec_dec *dec, od_adapt_ctx *adapt,
 unsigned char *bsize, int stride) {
  int i, j;
  int split16;
  int bsize_range_id;
  int min_size;
  int max_size;
  int min_ctx_size;
  min_ctx_size = bsize[-1 - stride];
  for (i = 0; i < 4; i++) {
    min_ctx_size = OD_MINI(min_ctx_size, bsize[i*stride - 1]);
    min_ctx_size = OD_MINI(min_ctx_size, bsize[-stride + i]);
  }
  bsize_range_id = od_decode_cdf_adapt(dec,
   adapt->bsize_range_cdf[min_ctx_size], 7, adapt->bsize_range_increment);
  min_size = od_bsize_range[bsize_range_id][0];
  max_size = od_bsize_range[bsize_range_id][1];
  if (min_size == OD_BLOCK_32X32) {
    for (i = 0; i < 4; i++) {
      for (j = 0; j < 4; j++) {
        bsize[i*stride + j] = OD_BLOCK_32X32;
      }
    }
  }
  else {
    if (max_size >= OD_BLOCK_16X16 && min_size < OD_BLOCK_16X16) {
      split16 = od_decode_cdf_adapt(dec, adapt->bsize16_cdf[min_size], 16,
         adapt->bsize16_increment);
    }
    else if (min_size == OD_BLOCK_16X16) split16 = 0xf;
    else split16 = 0;
    bsize[0] = (split16 & 8) ? OD_BLOCK_16X16 : OD_BLOCK_8X8;
    bsize[2] = (split16 & 4) ? OD_BLOCK_16X16 : OD_BLOCK_8X8;
    bsize[2*stride] = (split16 & 2) ? OD_BLOCK_16X16 : OD_BLOCK_8X8;
    bsize[2*stride + 2] = (split16 & 1) ? OD_BLOCK_16X16 : OD_BLOCK_8X8;
    for (i = 0; i < 2; i++) {
      for (j = 0; j < 2; j++) {
        if (min_size == OD_BLOCK_4X4 && max_size > OD_BLOCK_4X4
         && bsize[2*i*stride + 2*j] != OD_BLOCK_16X16) {
          int split8;
          split8 = od_decode_cdf_adapt(dec, adapt->bsize8_cdf, 16,
           adapt->bsize8_increment);
          bsize[2*i*stride + 2*j] = split8 & 1;
          bsize[2*i*stride + 2*j + 1] = (split8 >> 1) & 1;
          bsize[(2*i + 1)*stride + 2*j] = (split8 >> 2) & 1;
          bsize[(2*i + 1)*stride + 2*j + 1] = (split8 >> 3) & 1;
        }
        else {
          if (max_size == OD_BLOCK_4X4) bsize[2*i*stride + 2*j] = OD_BLOCK_4X4;
          /* Copy the same value to all four 8x8 blocks in the 16x16. */
          bsize[(2*i + 1)*stride + 2*j] = bsize[2*i*stride + 2*j + 1] =
           bsize[(2*i + 1)*stride + 2*j + 1] = bsize[2*i*stride + 2*j];
        }
      }
    }
  }
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      bsize[i*stride + j] = OD_MINI(OD_NBSIZES-1, OD_MAXI(0,
       bsize[i*stride + j]));
    }
  }
}
