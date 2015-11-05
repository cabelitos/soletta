/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "sol-http.h"
#include "sol-log.h"
#include "sol-util.h"
#include "sol-vector.h"

SOL_API bool
sol_http_param_add(struct sol_http_param *params,
    struct sol_http_param_value value)
{
    struct sol_http_param_value *ptr;

    SOL_NULL_CHECK(params, -EINVAL);

    if (params->api_version != SOL_HTTP_PARAM_API_VERSION) {
        SOL_ERR("API version mistmatch; expected %u, got %u",
            SOL_HTTP_PARAM_API_VERSION, params->api_version);
        return false;
    }

    ptr = sol_vector_append(&params->params);
    if (!ptr) {
        SOL_WRN("Could not append option to parameter vector");
        return false;
    }

    memcpy(ptr, &value, sizeof(value));
    return true;
}

SOL_API bool
sol_http_param_add_copy(struct sol_http_param *params,
    struct sol_http_param_value value)
{
    struct sol_http_param_value *ptr;
    int r;

    SOL_NULL_CHECK(params, -EINVAL);

    if (params->api_version != SOL_HTTP_PARAM_API_VERSION) {
        SOL_ERR("API version mistmatch; expected %u, got %u",
            SOL_HTTP_PARAM_API_VERSION, params->api_version);
        return false;
    }

    if (!params->arena) {
        params->arena = sol_arena_new();
        SOL_NULL_CHECK(params->arena, false);
    }

    if (value.type == SOL_HTTP_PARAM_QUERY_PARAM ||
        value.type == SOL_HTTP_PARAM_COOKIE ||
        value.type == SOL_HTTP_PARAM_POST_FIELD ||
        value.type == SOL_HTTP_PARAM_HEADER) {
        r = sol_arena_slice_dup(params->arena, &value.value.key_value.key,
            value.value.key_value.key);
        SOL_INT_CHECK(r, < 0, false);
        if (value.value.key_value.value.len) {
            r = sol_arena_slice_dup(params->arena, &value.value.key_value.value,
                value.value.key_value.value);
            SOL_INT_CHECK(r, < 0, false);
        }
    } else if (value.type == SOL_HTTP_PARAM_POST_DATA) {
        r = sol_arena_slice_dup(params->arena,
            (struct sol_str_slice *)&value.value.data.value,
            value.value.data.value);
        SOL_INT_CHECK(r, < 0, false);
    } else if (value.type == SOL_HTTP_PARAM_AUTH_BASIC) {
        r = sol_arena_slice_dup(params->arena, &value.value.auth.user,
            value.value.auth.user);
        SOL_INT_CHECK(r, < 0, false);
        r = sol_arena_slice_dup(params->arena, &value.value.auth.password,
            value.value.auth.password);
        SOL_INT_CHECK(r, < 0, false);
    }

    ptr = sol_vector_append(&params->params);
    if (!ptr) {
        SOL_WRN("Could not append option to parameter vector");
        return false;
    }

    memcpy(ptr, &value, sizeof(value));
    return true;
}

SOL_API void
sol_http_param_free(struct sol_http_param *params)
{
    SOL_NULL_CHECK(params);

    if (params->api_version != SOL_HTTP_PARAM_API_VERSION) {
        SOL_ERR("API version mistmatch; expected %u, got %u",
            SOL_HTTP_PARAM_API_VERSION, params->api_version);
        return;
    }
    sol_vector_clear(&params->params);
    if (params->arena) {
        sol_arena_del(params->arena);
        params->arena = NULL;
    }
}

SOL_API int
sol_http_escape_string(char **escaped, const char *value)
{
    int r;
    size_t value_len;
    struct sol_buffer buf = SOL_BUFFER_INIT_EMPTY;

    SOL_NULL_CHECK(value, -EINVAL);
    SOL_NULL_CHECK(escaped, -EINVAL);

    value_len = strlen(value);

    while (value_len--) {
        unsigned char c = *value;
        switch (c) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case '_': case '~': case '.': case '-':
            r = sol_buffer_append_char(&buf, c);
            SOL_INT_CHECK_GOTO(r, < 0, end);
            break;
        default:
            r = sol_buffer_append_printf(&buf, "%%%02X", c);
            SOL_INT_CHECK_GOTO(r, < 0, end);
        }
        value++;
    }

    r = sol_buffer_ensure_nul_byte(&buf);
    SOL_INT_CHECK_GOTO(r, < 0, end);

    *escaped = sol_buffer_steal(&buf, NULL);

end:
    sol_buffer_fini(&buf);
    return r;
}
