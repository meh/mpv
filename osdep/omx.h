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

#ifndef MP_OMX_H
#define MP_OMX_H

#include <IL/OMX_Core.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Audio.h>
#include <IL/OMX_Video.h>

OMX_ERRORTYPE mp_omx_init(void);
OMX_ERRORTYPE mp_omx_uninit(void);

#define OMX_INIT_STRUCTURE(a) \
  memset((a), 0, sizeof(*a)); \
  (a)->nSize = sizeof(*a); \
  (a)->nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
  (a)->nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
  (a)->nVersion.s.nRevision = OMX_VERSION_REVISION; \
  (a)->nVersion.s.nStep = OMX_VERSION_STEP

#endif
