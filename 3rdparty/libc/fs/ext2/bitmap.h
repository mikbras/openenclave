/*
**==============================================================================
**
** LSVMTools
**
** MIT License
**
** Copyright (c) Microsoft Corporation. All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE
**
**==============================================================================
*/

#ifndef _BITMAP_H
#define _BITMAP_H

#include "config.h"

bool bitmap_zero_filled(const uint8_t* data, uint32_t size);

uint32_t bitmap_count_bits(uint8_t byte);

uint32_t bitmap_count_bits_n(const uint8_t* data, uint32_t size);

EXT2_INLINE bool bitmap_test(const uint8_t* data, uint32_t size, uint32_t i)
{
    uint32_t byte = i / 8;
    uint32_t bit = i % 8;

    if (byte >= size)
        return 0;

    return ((uint32_t)(data[byte]) & (1 << bit)) ? 1 : 0;
}

EXT2_INLINE void bitmap_set(uint8_t* data, uint32_t size, uint32_t i)
{
    uint32_t byte = i / 8;
    uint32_t bit = i % 8;

    if (byte >= size)
        return;

    data[byte] |= (1 << bit);
}

EXT2_INLINE void bitmap_clear(uint8_t* data, uint32_t size, uint32_t i)
{
    uint32_t byte = i / 8;
    uint32_t bit = i % 8;

    if (byte >= size)
        return;

    data[byte] &= ~(1 << bit);
}

void bitmap_dump(const uint8_t* data, uint32_t size);

#endif /* _BITMAP_H */
