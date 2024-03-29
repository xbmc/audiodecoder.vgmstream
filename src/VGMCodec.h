/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/Filesystem.h>
#include <kodi/addon-instance/AudioDecoder.h>

extern "C"
{
#include "src/vgmstream.h"

  struct ATTR_DLL_LOCAL VGMContext
  {
    STREAMFILE sf;
    kodi::vfs::CFile* file = nullptr;
    char name[260];
    VGMSTREAM* stream = nullptr;
    size_t pos;
  };

} /* extern "C" */

class ATTR_DLL_LOCAL CVGMCodec : public kodi::addon::CInstanceAudioDecoder
{
public:
  CVGMCodec(const kodi::addon::IInstanceInfo& instance);
  ~CVGMCodec() override;

  bool Init(const std::string& filename,
            unsigned int filecache,
            int& channels,
            int& samplerate,
            int& bitspersample,
            int64_t& totaltime,
            int& bitrate,
            AudioEngineDataFormat& format,
            std::vector<AudioEngineChannel>& channellist) override;

  int ReadPCM(uint8_t* buffer, size_t size, size_t& actualsize) override;
  int64_t Seek(int64_t time) override;
  bool ReadTag(const std::string& filename, kodi::addon::AudioDecoderInfoTag& tag) override;

private:
  VGMContext ctx;
  bool m_loopForEver = false;
  bool m_endReached = false;
  bool m_loopForEverInUse = false;

  // Static because Kodi opens the next file before the end of this and
  // otherwise notification comes twice at the same playback.
  static bool m_loopForEverActive;
};
