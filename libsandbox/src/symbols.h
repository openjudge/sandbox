/*******************************************************************************
 * Copyright (C) 2004-2011 LIU Yu, pineapple.liu@gmail.com                     *
 * All rights reserved.                                                        *
 *                                                                             *
 * Redistribution and use in source and binary forms, with or without          *
 * modification, are permitted provided that the following conditions are met: *
 *                                                                             *
 * 1. Redistributions of source code must retain the above copyright notice,   *
 *    this list of conditions and the following disclaimer.                    *
 *                                                                             *
 * 2. Redistributions in binary form must reproduce the above copyright        *
 *    notice, this list of conditions and the following disclaimer in the      *
 *    documentation and/or other materials provided with the distribution.     *
 *                                                                             *
 * 3. Neither the name of the author(s) nor the names of its contributors may  *
 *    be used to endorse or promote products derived from this software        *
 *    without specific prior written permission.                               *
 *                                                                             *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE   *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  *
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE    *
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR         *
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF        *
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN     *
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)     *
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE  *
 * POSSIBILITY OF SUCH DAMAGE.                                                 *
 ******************************************************************************/
/** 
 * @file symbols.h 
 * @brief String names of sandbox constants.
 */
#ifndef __OJS_SYMBOLS_H__
#define __OJS_SYMBOLS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef NDEBUG

/**
 * @param[in] type any of the constant values defined in \c event_type_t
 * @return a statically allocated string name for the specified event type. 
 */
const char * s_event_type_name(int type);

/**
 * @param[in] type any of the constant values defined in \c action_type_t
 * @return a statically allocated string name for the specified action type. 
 */
const char * s_action_type_name(int type);

/**
 * @param[in] status any of the constant values defined in \c status_t
 * @return a statically allocated string name for the specified status. 
 */
const char * s_status_name(int status);

/**
 * @param[in] result any of the constant values defined in \c result_t
 * @return a statically allocated string name for the specified result. 
 */
const char * s_result_name(int result);

/**
 * @param[in] action any of the constant values defined in \c trace_action_t
 * @return a statically allocated string name for the specified action.
 */
const char * trace_act_name(int action);

#else

#define s_event_type_name(type) ((void)0)
#define s_action_type_name(type) ((void)0)
#define s_status_name(type) ((void)0)
#define s_result_name(type) ((void)0)

#endif /* NDEBUG */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OJS_SYMBOLS_H__ */
