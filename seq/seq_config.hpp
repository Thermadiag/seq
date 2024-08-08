/**
 * MIT License
 *
 * Copyright (c) 2024 Victor Moncada <vtr.moncada@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SEQ_CONFIG_HPP
#define SEQ_CONFIG_HPP

/** @file */

/// This is the default config file, just in case people will use the seq folder directly without installing the library

#define SEQ_VERSION_MAJOR 0
#define SEQ_VERSION_MINOR 0
#define SEQ_VERSION "0.0"

#if !defined(SEQ_BUILD_SHARED_LIBRARY) && !defined(SEQ_BUILD_STATIC_LIBRARY)
#define SEQ_DETECT_IS_HEADER_ONLY 1
#else
#define SEQ_DETECT_IS_HEADER_ONLY 0
#endif

#if SEQ_DETECT_IS_HEADER_ONLY == 1
#if !defined(SEQ_HEADER_ONLY) && !defined(SEQ_NO_HEADER_ONLY)
#define SEQ_HEADER_ONLY
#elif defined(SEQ_NO_HEADER_ONLY) && defined(SEQ_HEADER_ONLY)
#undef SEQ_HEADER_ONLY
#endif
#endif

#endif
