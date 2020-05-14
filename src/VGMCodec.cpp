/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <kodi/addon-instance/AudioDecoder.h>
#include <kodi/Filesystem.h>
#include <kodi/General.h>

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
  ctx->sf.open = open_VFS;
  ctx->sf.close = close_VFS;
  strcpy(ctx->name, filename);

  return streamfile;
}

}

class ATTRIBUTE_HIDDEN CVGMCodec : public kodi::addon::CInstanceAudioDecoder
{
public:
  CVGMCodec(KODI_HANDLE instance) :
    CInstanceAudioDecoder(instance) {}

  ~CVGMCodec() override
  {
    if (ctx.stream)
      close_vgmstream(ctx.stream);

    delete ctx.file;

    // Set the static to false only from one where has set it before
    if (m_loopForEverInUse)
      m_loopForEverActive = false;
  }

  bool Init(const std::string& filename, unsigned int filecache,
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
    m_loopForEver = kodi::GetSettingBoolean("loopforever");
    if (!m_loopForEverActive && m_loopForEver && ctx.stream->loop_flag)
    {
      m_loopForEverActive = true; // Set static to know on others that becomes active
      m_loopForEverInUse = true; // Set to class it's own, to know on desctruction that created from here
      kodi::QueueNotification(QUEUE_INFO, "", kodi::GetLocalizedString(30002));
    }

    m_endReached = false;

    return true;
  }

  int ReadPCM(uint8_t* buffer, int size, int& actualsize) override
  {
    if (m_endReached)
      return -1;

    bool loopForever = m_loopForEver && ctx.stream->loop_flag;
    if (!loopForever)
    {
      int decodePosSamples = size / (2 * ctx.stream->channels);
      if (decodePosSamples + ctx.stream->current_sample > ctx.stream->num_samples)
      {
        size = (decodePosSamples - ctx.stream->num_samples) * (ctx.stream->channels / 2);
        m_endReached = true;
      }
    }

    render_vgmstream((sample*)buffer, size/(2*ctx.stream->channels), ctx.stream);
    actualsize = size;

    ctx.pos += size;
    return 0;
  }

  int64_t Seek(int64_t time) override
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

  bool ReadTag(const std::string& file, std::string& title, std::string& artist, int& length) override
  {
    open_VFS((struct _STREAMFILE*)&ctx, file.c_str(), 0);

    ctx.stream = init_vgmstream_from_STREAMFILE((struct _STREAMFILE*)&ctx);
    if (!ctx.stream)
    {
      close_VFS((struct _STREAMFILE*)&ctx);
      return false;
    }

    length = ctx.stream->num_samples/ctx.stream->sample_rate;
    return true;
  }

private:
  VGMContext ctx;
  bool m_loopForEver = false;
  bool m_endReached = false;
  bool m_loopForEverInUse = false;

  // Static because Kodi opens the next file before the end of this and
  // otherwise notification comes twice at the same playback.
  static bool m_loopForEverActive;
};

bool CVGMCodec::m_loopForEverActive = false;


class ATTRIBUTE_HIDDEN CMyAddon : public kodi::addon::CAddonBase
{
public:
  CMyAddon() { }
  ADDON_STATUS CreateInstance(int instanceType, std::string instanceID, KODI_HANDLE instance, KODI_HANDLE& addonInstance) override
  {
    addonInstance = new CVGMCodec(instance);
    return ADDON_STATUS_OK;
  }
  ~CMyAddon() override = default;
};


ADDONCREATOR(CMyAddon);
