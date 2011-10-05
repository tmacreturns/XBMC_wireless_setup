/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "infotagmusic.h"
#include "pyutil.h"

#ifndef __GNUC__
#pragma code_seg("PY_TEXT")
#pragma data_seg("PY_DATA")
#pragma bss_seg("PY_BSS")
#pragma const_seg("PY_RDATA")
#endif

#ifdef __cplusplus
extern "C" {
#endif

using namespace MUSIC_INFO;

namespace PYXBMC
{
  /*
   * allocate a new InfoTagMusic. Used for c++ and not the python user
   * returns a new reference
   */
  InfoTagMusic* InfoTagMusic_FromCMusicInfoTag(const MUSIC_INFO::CMusicInfoTag& infoTag)
  {
    InfoTagMusic* self = (InfoTagMusic*)InfoTagMusic_Type.tp_alloc(&InfoTagMusic_Type, 0);
    if (!self) return NULL;
    new(&self->infoTag) MUSIC_INFO::CMusicInfoTag();
    self->infoTag = infoTag;

    return self;
  }

  void InfoTagMusic_Dealloc(InfoTagMusic* self)
  {
    self->infoTag.~CMusicInfoTag();
    self->ob_type->tp_free((PyObject*)self);
  }

  // InfoTagMusic_GetURL
  PyDoc_STRVAR(getURL__doc__,
    "getURL() -- returns a string.\n");

  PyObject* InfoTagMusic_GetURL(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"s", self->infoTag.GetURL().c_str());
  }

  // InfoTagMusic_GetTitle
  PyDoc_STRVAR(getTitle__doc__,
    "getTitle() -- returns a string.\n");

  PyObject* InfoTagMusic_GetTitle(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"s", self->infoTag.GetTitle().c_str());
  }

  // InfoTagMusic_GetArtist
  PyDoc_STRVAR(getArtist__doc__,
    "getArtist() -- returns a string.\n");

  PyObject* InfoTagMusic_GetArtist(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"s", self->infoTag.GetArtist().c_str());
  }

  // InfoTagMusic_GetAlbum
  PyDoc_STRVAR(getAlbum__doc__,
    "getAlbum() -- returns a string.\n");

  PyObject* InfoTagMusic_GetAlbum(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"s", self->infoTag.GetAlbum().c_str());
  }

  // InfoTagMusic_GetGenre
  PyDoc_STRVAR(getGenre__doc__,
    "getAlbum() -- returns a string.\n");

  PyObject* InfoTagMusic_GetGenre(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"s", self->infoTag.GetGenre().c_str());
  }

  // InfoTagMusic_GetDuration
  PyDoc_STRVAR(getDuration__doc__,
    "getDuration() -- returns an integer.\n");

  PyObject* InfoTagMusic_GetDuration(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"i", self->infoTag.GetDuration());
  }

  // InfoTagMusic_GetTrack
  PyDoc_STRVAR(getTrack__doc__,
    "getTrack() -- returns an integer.\n");

  PyObject* InfoTagMusic_GetTrack(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"s", self->infoTag.GetTrackNumber());
  }

  // InfoTagMusic_GetDisc
  PyDoc_STRVAR(getDisc__doc__,
    "getDisc() -- returns an integer.\n");

  PyObject* InfoTagMusic_GetDisc(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"s", self->infoTag.GetDiscNumber());
  }

  // InfoTagMusic_ReleaseDate
  PyDoc_STRVAR(getReleaseDate__doc__,
    "getReleaseDate() -- returns a string.\n");

  PyObject* InfoTagMusic_GetReleaseDate(InfoTagMusic *self, PyObject *args)
  {
    return Py_BuildValue((char*)"s", self->infoTag.GetYearString().c_str());
  }

  PyMethodDef InfoTagMusic_methods[] = {
    {(char*)"getURL", (PyCFunction)InfoTagMusic_GetURL, METH_VARARGS, getURL__doc__},
    {(char*)"getTitle", (PyCFunction)InfoTagMusic_GetTitle, METH_VARARGS, getTitle__doc__},
    {(char*)"getAlbum", (PyCFunction)InfoTagMusic_GetAlbum, METH_VARARGS, getAlbum__doc__},
    {(char*)"getArtist", (PyCFunction)InfoTagMusic_GetArtist, METH_VARARGS, getArtist__doc__},
    {(char*)"getGenre", (PyCFunction)InfoTagMusic_GetGenre, METH_VARARGS, getGenre__doc__},
    {(char*)"getDuration", (PyCFunction)InfoTagMusic_GetDuration, METH_VARARGS, getDuration__doc__},
    {(char*)"getTrack", (PyCFunction)InfoTagMusic_GetTrack, METH_VARARGS, getTrack__doc__},
    {(char*)"getDisc", (PyCFunction)InfoTagMusic_GetDisc, METH_VARARGS, getDisc__doc__},
    {(char*)"getReleaseDate", (PyCFunction)InfoTagMusic_GetReleaseDate, METH_VARARGS, getReleaseDate__doc__},
    {NULL, NULL, 0, NULL}
  };

  PyDoc_STRVAR(musicInfoTag__doc__,
    "InfoTagMusic class.\n"
    "\n"
    "");

// Restore code and data sections to normal.
#ifndef __GNUC__
#pragma code_seg()
#pragma data_seg()
#pragma bss_seg()
#pragma const_seg()
#endif

  PyTypeObject InfoTagMusic_Type;

  void initInfoTagMusic_Type()
  {
    PyXBMCInitializeTypeObject(&InfoTagMusic_Type);

    InfoTagMusic_Type.tp_name = (char*)"xbmc.InfoTagMusic";
    InfoTagMusic_Type.tp_basicsize = sizeof(InfoTagMusic);
    InfoTagMusic_Type.tp_dealloc = (destructor)InfoTagMusic_Dealloc;
    InfoTagMusic_Type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    InfoTagMusic_Type.tp_doc = musicInfoTag__doc__;
    InfoTagMusic_Type.tp_methods = InfoTagMusic_methods;
    InfoTagMusic_Type.tp_base = 0;
    InfoTagMusic_Type.tp_new = 0;
  }
}

#ifdef __cplusplus
}
#endif
