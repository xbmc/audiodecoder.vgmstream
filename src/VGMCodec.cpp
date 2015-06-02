/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "libXBMC_addon.h"

ADDON::CHelper_libXBMC_addon *XBMC           = NULL;

extern "C" {

#include "src/vgmstream.h"
#include "kodi_audiodec_dll.h"
#include "AEChannelData.h"

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!XBMC)
    XBMC = new ADDON::CHelper_libXBMC_addon;

  if (!XBMC->RegisterMe(hdl))
  {
    delete XBMC, XBMC=NULL;
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  return ADDON_STATUS_OK;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Stop()
{
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Destroy()
{
  XBMC=NULL;
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
bool ADDON_HasSettings()
{
  return false;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_SetSetting(const char *strSetting, const void* value)
{
  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

struct VGMContext
{
  STREAMFILE sf;
  void* file;
  char name[260];
  VGMSTREAM* stream;
  size_t pos;
};

static size_t read_XBMC(struct _STREAMFILE* streamfile, uint8_t* dest, off_t offset, size_t length)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx && ctx->file && XBMC)
  {
    XBMC->SeekFile(ctx->file, offset, SEEK_SET);
    return XBMC->ReadFile(ctx->file, dest, length);
  }
  return 0;
}

static void close_XBMC(struct _STREAMFILE* streamfile)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx && ctx->file && XBMC)
    XBMC->CloseFile(ctx->file);
  delete ctx;
}

static size_t get_size_XBMC(struct _STREAMFILE* streamfile)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx && ctx->file && XBMC)
    return XBMC->GetFilePosition(ctx->file);

  return 0;
}

static off_t get_offset_XBMC(struct _STREAMFILE* streamfile)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx && ctx->file && XBMC)
    return XBMC->GetFilePosition(ctx->file);

  return 0;
}

static void get_name_XBMC(struct _STREAMFILE* streamfile, char* buffer, size_t length)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx)
    strcpy(buffer, ctx->name);
}

static struct _STREAMFILE* open_XBMC(struct _STREAMFILE* streamfile, const char* const filename, size_t buffersize)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  ctx->pos = 0;
  ctx->file = XBMC->OpenFile(filename, 0);
  ctx->sf.read = read_XBMC;
  ctx->sf.get_size = get_size_XBMC;
  ctx->sf.get_offset = get_offset_XBMC;
  ctx->sf.get_name = get_name_XBMC;
  ctx->sf.get_realname = get_name_XBMC;
  ctx->sf.open = open_XBMC;
  ctx->sf.close = close_XBMC;
  strcpy(ctx->name, filename);

  return streamfile;
}

#define SET_IF(ptr, value) \
{ \
  if ((ptr)) \
   *(ptr) = (value); \
}

void* Init(const char* strFile, unsigned int filecache, int* channels,
           int* samplerate, int* bitspersample, int64_t* totaltime,
           int* bitrate, AEDataFormat* format, const AEChannel** channelinfo)
{
  if (!strFile)
    return NULL;

  VGMContext* result = new VGMContext;
  if (!result)
    return NULL;

  open_XBMC((struct _STREAMFILE*)result, strFile, 0);

  result->stream = init_vgmstream_from_STREAMFILE((struct _STREAMFILE*)result);
  if (!result->stream)
  {
    delete result;
    return NULL;
  }

  SET_IF(channels, result->stream->channels)
  SET_IF(samplerate, result->stream->sample_rate)
  SET_IF(bitspersample, 16)
  SET_IF(totaltime, result->stream->num_samples/result->stream->sample_rate*1000)
  SET_IF(format, AE_FMT_S16NE)
  static enum AEChannel map[8][9] = {
    {AE_CH_FC, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FR, AE_CH_BL, AE_CH_BR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_BL, AE_CH_BR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_BL, AE_CH_BR, AE_CH_LFE, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_SL, AE_CH_SR, AE_CH_BL, AE_CH_BR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_SL, AE_CH_SR, AE_CH_BL, AE_CH_BR, AE_CH_LFE, AE_CH_NULL}
  };

  SET_IF(channelinfo, NULL)
  if (result->stream->channels <= 8)
    SET_IF(channelinfo, map[result->stream->channels-1])

  SET_IF(bitrate, 0)

  return result;
}

int ReadPCM(void* context, uint8_t* pBuffer, int size, int *actualsize)
{
  if (!context || !actualsize)
    return 1;

  VGMContext* ctx = (VGMContext*)context;

  render_vgmstream((sample*)pBuffer, size/(2*ctx->stream->channels), ctx->stream);
  *actualsize = size;

  ctx->pos += size;
  return 0;
}

int64_t Seek(void* context, int64_t time)
{
  if (!context)
    return 0;

  VGMContext* ctx = (VGMContext*)context;
  int16_t* buffer = new int16_t[576*ctx->stream->channels];
  if (!buffer)
    return 0;

  long samples_to_do = (long)time * ctx->stream->sample_rate / 1000L;
  if (samples_to_do < ctx->stream->current_sample )
     reset_vgmstream(ctx->stream);
  else
    samples_to_do -= ctx->stream->current_sample;
      
  while (samples_to_do > 0)
  {
    long l = samples_to_do>576?576:samples_to_do;
    render_vgmstream(buffer, l, ctx->stream);
    samples_to_do -= l;
  }
  delete[] buffer;

  return time;
}

bool DeInit(void* context)
{
  if (!context)
    return true;

  VGMContext* ctx = (VGMContext*)context;

  close_vgmstream(ctx->stream);
  delete ctx;

  return true;
}

bool ReadTag(const char* strFile, char* title, char* artist,
             int* length)
{
  return true;
}

int TrackCount(const char* strFile)
{
  return 1;
}
}
