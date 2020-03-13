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

#ifndef _BUF_H
#define _BUF_H

#include "config.h"
#include <stddef.h>

/*
**==============================================================================
**
** buf_t
**
**==============================================================================
*/

#define BUF_INITIALIZER { NULL, 0, 0 }

typedef struct _buf
{
    void* data;
    size_t size;
    size_t cap;
}
buf_t;

void buf_init(buf_t* buf);

void buf_release(buf_t* buf);

int buf_clear(buf_t* buf);

int buf_reserve(buf_t* buf, size_t cap);

int buf_append(buf_t* buf, const void* data, size_t size);

/*
**==============================================================================
**
** buf_u32_t
**
**==============================================================================
*/

#define BUF_U32_INITIALIZER { NULL, 0, 0 }

typedef struct _buf_u32
{
    uint32_t* data;
    size_t size;
    size_t cap;
}
buf_u32_t;

void buf_u32_release(buf_u32_t* buf);

int buf_u32_append(buf_u32_t* buf, const uint32_t* data, size_t size);

#endif /* _BUF_H */
