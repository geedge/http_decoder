/*
**********************************************************************************************
*	File: http_decoder_utils.c
*   Description: 
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#include <string.h>

#include "stellar/utils.h"

char *safe_dup(const char *str, size_t len)
{
    if (str == NULL || len == 0) {
        return NULL;
    }

    char *dup = CALLOC(char, len + 1);
    memcpy(dup, str, len);

    return dup;
}