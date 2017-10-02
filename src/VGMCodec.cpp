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

#include <kodi/addon-instance/AudioDecoder.h>
#include <kodi/Filesystem.h>

extern "C" {
#include "src/vgmstream.h"

struct VGMContext
{
  STREAMFILE sf;
  kodi::vfs::CFile* file = nullptr;
  char name[260];
  VGMSTREAM* stream = nullptr;
  size_t pos;
};


static size_t read_VFS(struct _STREAMFILE* streamfile, uint8_t* dest, off_t offset, size_t length)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx && ctx->file)
  {
    ctx->file->Seek(offset, SEEK_SET);
    return ctx->file->Read(dest, length);
  }
  return 0;
}

static void close_VFS(struct _STREAMFILE* streamfile)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx && ctx->file)
    delete ctx->file;
  ctx->file = nullptr;
}

static size_t get_size_VFS(struct _STREAMFILE* streamfile)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx && ctx->file)
    return ctx->file->GetLength();

  return 0;
}

static off_t get_offset_VFS(struct _STREAMFILE* streamfile)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx && ctx->file)
    return ctx->file->GetPosition();

  return 0;
}

static void get_name_VFS(struct _STREAMFILE* streamfile, char* buffer, size_t length)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  if (ctx)
    strcpy(buffer, ctx->name);
}

static struct _STREAMFILE* open_VFS(struct _STREAMFILE* streamfile, const char* const filename, size_t buffersize)
{
  VGMContext *ctx = (VGMContext*) streamfile;
  ctx->pos = 0;
  ctx->file = new kodi::vfs::CFile;
  ctx->file->OpenFile(filename, READ_CACHED);
  ctx->sf.read = read_VFS;
  ctx->sf.get_size = get_size_VFS;
  ctx->sf.get_offset = get_offset_VFS;
  ctx->sf.get_name = get_name_VFS;
  ctx->sf.get_realname = get_name_VFS;
  ctx->sf.open = open_VFS;
  ctx->sf.close = close_VFS;
  strcpy(ctx->name, filename);

  return streamfile;
}

}

class CVGMCodec : public kodi::addon::CInstanceAudioDecoder,
                  public kodi::addon::CAddonBase
{
public:
  CVGMCodec(KODI_HANDLE instance) :
    CInstanceAudioDecoder(instance) {}

  virtual ~CVGMCodec()
  {
    if (ctx.stream)
      close_vgmstream(ctx.stream);

    delete ctx.file;
  }

  virtual bool Init(const std::string& filename, unsigned int filecache,
                    int& channels, int& samplerate,
                    int& bitspersample, int64_t& totaltime,
                    int& bitrate, AEDataFormat& format,
                    std::vector<AEChannel>& channellist) override
  {
    open_VFS((struct _STREAMFILE*)&ctx, filename.c_str(), 0);

    ctx.stream = init_vgmstream_from_STREAMFILE((struct _STREAMFILE*)&ctx);
    if (!ctx.stream)
    {
      close_VFS((struct _STREAMFILE*)&ctx);
      return false;
    }

    channels = ctx.stream->channels;
    samplerate = ctx.stream->sample_rate;
    bitspersample =  16;
    totaltime =  ctx.stream->num_samples/ctx.stream->sample_rate*1000;
    format = AE_FMT_S16NE;

    static std::vector<std::vector<enum AEChannel>> map = {
      {AE_CH_FC},
      {AE_CH_FL, AE_CH_FR},
      {AE_CH_FL, AE_CH_FC, AE_CH_FR},
      {AE_CH_FL, AE_CH_FR, AE_CH_BL, AE_CH_BR},
      {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_BL, AE_CH_BR},
      {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_BL, AE_CH_BR, AE_CH_LFE},
      {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_SL, AE_CH_SR, AE_CH_BL, AE_CH_BR},
      {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_SL, AE_CH_SR, AE_CH_BL, AE_CH_BR, AE_CH_LFE}
      };

    if (ctx.stream->channels <= 8)
      channellist = map[ctx.stream->channels-1];

    bitrate = 0;

    return true;
  }

  virtual int ReadPCM(uint8_t* buffer, int size, int& actualsize) override
  {
    render_vgmstream((sample*)buffer, size/(2*ctx.stream->channels), ctx.stream);
    actualsize = size;

    ctx.pos += size;
    return 0;
  }

  virtual int64_t Seek(int64_t time) override
  {
    int16_t* buffer = new int16_t[576*ctx.stream->channels];
    if (!buffer)
      return 0;

    long samples_to_do = (long)time * ctx.stream->sample_rate / 1000L;
    if (samples_to_do < ctx.stream->current_sample )
      reset_vgmstream(ctx.stream);
    else
      samples_to_do -= ctx.stream->current_sample;

    while (samples_to_do > 0)
    {
      long l = samples_to_do>576?576:samples_to_do;
      render_vgmstream(buffer, l, ctx.stream);
      samples_to_do -= l;
    }
    delete[] buffer;

    return time;
  }

private:
  VGMContext ctx;
};


class ATTRIBUTE_HIDDEN CMyAddon : public kodi::addon::CAddonBase
{
public:
  CMyAddon() { }
  virtual ADDON_STATUS CreateInstance(int instanceType, std::string instanceID, KODI_HANDLE instance, KODI_HANDLE& addonInstance) override
  {
    addonInstance = new CVGMCodec(instance);
    return ADDON_STATUS_OK;
  }
  virtual ~CMyAddon()
  {
  }
};


ADDONCREATOR(CMyAddon);
