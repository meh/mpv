/*
 * This file is part of mpv.
 * Copyright (c) 2015 meh <meh@schizofreni.co>
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "omx.h"
#include <stdbool.h>

// we need to do reference counting for init and uninit because it has to be called
// just once to initialize and deinitialize the OMX framework.
//
// NOTE: no need for atomics since ad, vd and vo init/uninit are single-threaded.
static int _references = 0;

OMX_ERRORTYPE mp_omx_init(void)
{
    static OMX_ERRORTYPE result = OMX_ErrorMax;

    if (result != OMX_ErrorNone) {
        result = OMX_Init();
    }

    if (result == OMX_ErrorNone) {
        _references++;
    }

    return result;
}

OMX_ERRORTYPE mp_omx_uninit(void)
{
    static OMX_ERRORTYPE result = OMX_ErrorNone;

    if (_references == 1) {
        result = OMX_Deinit();

        if (result == OMX_ErrorNone) {
            _references--;
        }
    }
    else {
        _references--;
    }

    return result;
}
