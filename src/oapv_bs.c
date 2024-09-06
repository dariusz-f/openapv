/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the copyright owner, nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "oapv_def.h"

///////////////////////////////////////////////////////////////////////////////
// start of encoder code
#if ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////
/* number of bytes to be sunk */
#define BSW_GET_SINK_BYTE(bs)     ((32 - (bs)->leftbits + 7) >> 3)

static int oapv_bsw_flush(oapv_bs_t * bs, int bytes)
{
    if (bytes == 0) bytes = BSW_GET_SINK_BYTE(bs);

    while(bytes--)
    {
        *bs->cur++ = (bs->code >> 24) & 0xFF;
        bs->code <<= 8;
    }

    bs->leftbits = 32;

    return 0;
}

void oapv_bsw_init(oapv_bs_t * bs, u8 * buf, int size, oapv_bs_fn_flush_t fn_flush)
{
    bs->size = size;
    bs->beg = buf;
    bs->cur = buf;
    bs->end = buf + size - 1;
    bs->code = 0;
    bs->leftbits = 32;
    bs->fn_flush = (fn_flush == NULL ? oapv_bsw_flush : fn_flush);
}

void oapv_bsw_deinit(oapv_bs_t * bs)
{
    bs->fn_flush(bs, 0);
}

void * oapv_bsw_sink(oapv_bs_t* bs)
{
    oapv_assert_rv(bs->cur + BSW_GET_SINK_BYTE(bs) <= bs->end, NULL);
    bs->fn_flush(bs, 0);
    bs->code = 0;
    bs->leftbits = 32;
    return (void*)bs->cur;
}

int oapv_bsw_write_direct(void * bits, u32 val, int len)
{
    int i;
    unsigned char * p = (unsigned char *)bits;

    oapv_assert_rv((len & 0x7) == 0, -1); // len should be byte-aligned

    val <<= (32 - len);
    for (i = 0; i < (len >> 3); i++) {
        p[i] = (val >> 24) & 0xFF;
        val <<= 8;
    }
    return 0;
}

#if TRACE_HLS
int oapv_bsw_write_trace(oapv_bs_t * bs, u32 val, char * name, int len) /* len(1 ~ 32) */
{
    int leftbits;

    oapv_assert(bs);

    if (bs->is_bin_count)
    {
      bs->bin_count+= len;
      return 0;
    }

    if (name != NULL && name[0] != 0)
    {
        OAPV_TRACE_STR(name);
        OAPV_TRACE_STR(" ");
        OAPV_TRACE_INT(val);
        OAPV_TRACE_STR("\n");
    }

    leftbits = bs->leftbits;
    val <<= (32 - len);
    bs->code |= (val >> (32 - leftbits));

    if (len < leftbits)
    {
        bs->leftbits -= len;
    }
    else
    {
        oapv_assert_rv(bs->cur + 4 <= bs->end, -1);

        bs->leftbits = 0;
        bs->fn_flush(bs, 0);
        bs->code = (leftbits < 32 ? val << leftbits : 0);
        bs->leftbits = 32 - (len - leftbits);
    }

    return 0;
}

int oapv_bsw_write1_trace(oapv_bs_t * bs, int val, char * name)
{
    oapv_assert(bs);

    if (bs->is_bin_count)
    {
      bs->bin_count++;
      return 0;
    }

    if (name)
    {
        OAPV_TRACE_STR(name);
        OAPV_TRACE_STR(" ");
        OAPV_TRACE_INT(val);
        OAPV_TRACE_STR("\n");
    }
    else
    {
        OAPV_TRACE_STR("0");
        OAPV_TRACE_STR(" ");
        OAPV_TRACE_INT(val);
        OAPV_TRACE_STR("\n");
    }

    bs->leftbits--;
    bs->code |= ((val & 0x1) << bs->leftbits);

    if (bs->leftbits == 0)
    {
        oapv_assert_rv(bs->cur <= bs->end, -1);
        bs->fn_flush(bs, 0);

        bs->code = 0;
        bs->leftbits = 32;
    }

    return 0;
}
#else
int oapv_bsw_write1(oapv_bs_t * bs, int val)
{
    oapv_assert(bs);

    if (bs->is_bin_count)
    {
        bs->bin_count++;
        return 0;
    }

    bs->leftbits--;
    bs->code |= ((val & 0x1) << bs->leftbits);

    if (bs->leftbits == 0)
    {
        oapv_assert_rv(bs->cur <= bs->end, -1);
        bs->fn_flush(bs, 0);

        bs->code = 0;
        bs->leftbits = 32;
    }

    return 0;
}

int oapv_bsw_write(oapv_bs_t * bs, u32 val, int len) /* len(1 ~ 32) */
{
    int leftbits;

    oapv_assert(bs);

    if (bs->is_bin_count)
    {
      bs->bin_count += len;
      return 0;
    }

    leftbits = bs->leftbits;
    val <<= (32 - len);
    bs->code |= (val >> (32 - leftbits));

    if(len < leftbits)
    {
        bs->leftbits -= len;
    }
    else
    {
        oapv_assert_rv(bs->cur + 4 <= bs->end, -1);

        bs->leftbits = 0;
        bs->fn_flush(bs, 0);
        bs->code = (leftbits < 32 ? val << leftbits : 0);
        bs->leftbits = 32 - (len - leftbits);
    }

    return 0;
}
#endif



///////////////////////////////////////////////////////////////////////////////
// end of encoder code
#endif // ENABLE_ENCODER
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// start of decoder code
#if ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////

/* Table of count of leading zero for 4 bit value */
static const u8 tbl_zero_count4[16] =
{
    4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0
};

// skip code if lefbits are larger than skip bit count;
static void inline bsr_skip_code(oapv_bs_t* bs, int size)
{
    oapv_assert(size <= 32);
    oapv_assert(bs->leftbits >= size);
    if(size == 32) {
        bs->code = 0;
        bs->leftbits = 0;
    }
    else {
        bs->code <<= size;
        bs->leftbits -= size;
    }
}

void oapv_bsr_init(oapv_bs_t * bs, u8 * buf, int size, oapv_bs_fn_flush_t fn_flush)
{
    bs->size     = size;
    bs->cur      = buf;
    bs->beg      = buf;
    bs->end      = buf + size - 1;
    bs->code     = 0;
    bs->leftbits = 0;
    bs->fn_flush = (fn_flush == NULL)? oapv_bsr_flush : fn_flush;
}

int oapv_bsr_flush(oapv_bs_t * bs, int byte)
{
    int    shift = 24, remained;
    u32 code = 0;

    oapv_assert(byte);

    remained = (int)(bs->end - bs->cur) + 1;
    if(byte > remained) byte = remained;

    if(byte <= 0)
    {
        bs->code = 0;
        bs->leftbits = 0;
        return -1;
    }

    bs->leftbits = byte << 3;

    bs->cur += byte;
    while(byte)
    {
        code |= *(bs->cur - byte) << shift;
        byte--;
        shift -= 8;
    }
    bs->code = code;
    return 0;
}

int oapv_bsr_clz_in_code(u32 code)
{
    int clz, bits4, shift;

    if(code == 0) return 32; /* to protect infinite loop */

    bits4 = 0;
    clz = 0;
    shift = 28;

    while(bits4 == 0 && shift >= 0)
    {
        bits4 = (code >> shift) & 0xf;
        clz += tbl_zero_count4[bits4];
        shift -= 4;
    }
    return clz;
}

void oapv_bsr_align8(oapv_bs_t* bs)
{
    u32 t0;
    while (!oapv_bsr_is_align8(bs)) {
        oapv_bsr_read1(bs, &t0);
    }
}

void oapv_bsr_skip(oapv_bs_t* bs, int size)
{
    oapv_assert(size > 0 && size <= 32);

    if (bs->leftbits < size) {
        size -= bs->leftbits;
        if (bs->fn_flush(bs, 4)) {
            //oapv_trace("already reached the end of bitstream\n");  /* should be updated */
            return;
        }
    }
    bsr_skip_code(bs, size);
}

void oapv_bsr_peek(oapv_bs_t* bs, u32 * val, int size)
{
    int byte, leftbits;
    u32 code = 0;

    if(bs->leftbits < size)
    {
        byte = (32 - bs->leftbits) >> 3;

        /* We should not check the return value
        because this function could be failed at the EOB. */
        if(byte)
        {
            code = bs->code;
            leftbits = bs->leftbits;

            bs->fn_flush(bs, byte);

            bs->code >>= leftbits;
            bs->code |= code;
            bs->leftbits += leftbits;
        }
    }

    oapv_assert(bs->leftbits <= 32);

    code  = bs->code >> (32 - size);
    size -= bs->leftbits;

    if(size > 0)
    {
        /* even though we update several bytes, the requested size would be
        larger than current bs->leftbits.
        In this case, we should read one more byte, but we could not store
        the read byte. */
        if(bs->cur <= bs->end)
        {
            code |= *(bs->cur) >> (8 - size);
        }
    }
    *val = code;
}



#if TRACE_HLS
void oapv_bsr_read_trace(oapv_bs_t * bs, u32 * val, char * name, int size)
{
    u32 code = 0;

    oapv_assert(size > 0);

    if (bs->leftbits < size)
    {
        code = bs->code >> (32 - size);
        size -= bs->leftbits;

        if (bs->fn_flush(bs, 4))
        {
            //oapv_trace("already reached the end of bitstream\n"); /* should be updated */
            *val = (u32)-1;
            return;
        }
    }
    code |= bs->code >> (32 - size);

    bsr_skip_code(bs, size);

    *val = code;

    if (name)
    {
        OAPV_TRACE_STR(name + 1);
        OAPV_TRACE_STR(" ");
        OAPV_TRACE_INT(*val);
        OAPV_TRACE_STR("\n");
        OAPV_TRACE_FLUSH;
    }
}

void oapv_bsr_read1_trace(oapv_bs_t * bs, u32 * val, char * name)
{
    int code;
    if (bs->leftbits == 0)
    {
        if (bs->fn_flush(bs, 4))
        {
            //oapv_trace("already reached the end of bitstream\n");  /* should be updated */
            return;
        }
    }
    code = (int)(bs->code >> 31);

    bs->code <<= 1;
    bs->leftbits -= 1;

    *val = code;
    if (name)
    {
        OAPV_TRACE_STR(name + 1);
        OAPV_TRACE_STR(" ");
        OAPV_TRACE_INT(*val);
        OAPV_TRACE_STR("\n");
    }
    else
    {
        OAPV_TRACE_STR("0");
        OAPV_TRACE_STR(" ");
        OAPV_TRACE_INT(*val);
        OAPV_TRACE_STR("\n");
    }
}
#else
void oapv_bsr_read(oapv_bs_t * bs, u32 * val, int size)
{
    u32 code = 0;

    oapv_assert(size > 0);

    if (bs->leftbits < size)
    {
        code = bs->code >> (32 - size);
        size -= bs->leftbits;
        if (bs->fn_flush(bs, 4))
        {
            //oapv_trace("already reached the end of bitstream\n");  /* should be updated */
            *val = (u32)-1;
            return;
        }
    }
    code |= bs->code >> (32 - size);

    bsr_skip_code(bs, size);

    *val = code;
}

void oapv_bsr_read1(oapv_bs_t * bs, u32 * val)
{
    int code;
    if (bs->leftbits == 0)
    {
        if (bs->fn_flush(bs, 4))
        {
            //oapv_trace("already reached the end of bitstream\n");  /* should be updated */
            return;
        }
    }
    code = (int)(bs->code >> 31);

    bs->code <<= 1;
    bs->leftbits -= 1;

    *val = code;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// end of decoder code
#endif // ENABLE_DECODER
///////////////////////////////////////////////////////////////////////////////

