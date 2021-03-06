/*
Copyright 2010-2011, D. E. Shaw Research.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions, and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of D. E. Shaw Research nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _r123array_dot_h__
#define _r123array_dot_h__
#include "features/compilerfeatures.h"
#include "features/sse.h"

/* _r123array_tpl expands to a declaration of struct r123arrayNxW.  

   It's nothing more than a struct containing an array of N
   objects of type T.
*/

#define _r123array_tpl(_N, W, T)    \
struct r123array##_N##x##W{         \
    T v[_N];                        \
};

_r123array_tpl(1, 32, uint32_t)  /* r123array1x32 */
_r123array_tpl(2, 32, uint32_t)  /* r123array2x32 */
_r123array_tpl(4, 32, uint32_t)  /* r123array4x32 */
_r123array_tpl(8, 32, uint32_t)  /* r123array8x32 */

_r123array_tpl(1, 64, uint64_t)  /* r123array1x64 */
_r123array_tpl(2, 64, uint64_t)  /* r123array2x64 */
_r123array_tpl(4, 64, uint64_t)  /* r123array4x64 */

_r123array_tpl(16, 8, uint8_t)  /* r123array16x8 for ARSsw, AESsw */

#if R123_USE_SSE
_r123array_tpl(1, m128i, r123m128i) /* r123array1x128i for ARSni, AESni */
#endif

/* In C++, it's natural to use sizeof(a::value_type), but in C it's
   pretty convoluted to figure out the width of the value_type of an
   r123arrayNxW: */
#define R123_W(a)   (8*sizeof(((a *)0)->v[0]))

#endif

