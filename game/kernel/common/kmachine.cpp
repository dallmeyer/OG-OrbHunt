#include "kmachine.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <random>
#include <thread>
#include <list>

#define MINIAUDIO_IMPLEMENTATION
// NOTE - this is needed, because on macOS, there is a file called `MacTypes.h`
// inside it, it defines something named `Ptr`
// Our `Ptr` is not namespaced, so there is ambiguity.
//
// Second fix is because miniaudio redefines functions in the stdlib based on bad pre-processor
// assumptions AppleClang apparently does not define POSIX macros, leading to future ambiguity
namespace MiniAudioLib {
#if defined(__APPLE__)
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#include "third-party/miniaudio.h"
#undef _POSIX_C_SOURCE
#else
// It should work if it's defined, but for some reason it didn't this is the unlikely branch
// but lets maintain the original value
#define NOT_REAL_OLD_POSIX_C_SOURCE _POSIX_C_SOURCE
#include "third-party/miniaudio.h"
#define _POSIX_C_SOURCE NOT_REAL_OLD_POSIX_C_SOURCE
#undef NOT_REAL_OLD_POSIX_C_SOURCE
#endif
#else
#include "third-party/miniaudio.h"
#endif
}  // namespace MiniAudioLib

#include "common/global_profiler/GlobalProfiler.h"
#include "common/log/log.h"
#include "common/symbols.h"
#include "common/util/FileUtil.h"
#include "common/util/FontUtils.h"
#include "common/util/Timer.h"
#include "common/util/string_util.h"

#include "game/external/discord.h"
#include "game/graphics/display.h"
#include "game/graphics/gfx.h"
#include "game/graphics/screenshot.h"
#include "game/kernel/common/Ptr.h"
#include "game/kernel/common/kernel_types.h"
#include "game/kernel/common/kprint.h"
#include "game/kernel/common/kscheme.h"
#include "game/kernel/jak1/kscheme.h"
#include "game/mips2c/mips2c_table.h"
#include "game/sce/libcdvd_ee.h"
#include "game/sce/libpad.h"
#include "game/sce/libscf.h"
#include "game/sce/sif_ee.h"

/*!
 * Where does OVERLORD load its data from?
 */
OverlordDataSource isodrv;

// Get IOP modules from DVD or from dsefilesv
u32 modsrc;

// Reboot IOP with IOP kernel from DVD/CD on boot
u32 reboot_iop;

const char* init_types[] = {"fakeiso", "deviso", "iso_cd"};
u8 pad_dma_buf[2 * SCE_PAD_DMA_BUFFER_SIZE];

// added
u32 vif1_interrupt_handler = 0;
u32 vblank_interrupt_handler = 0;

Timer ee_clock_timer;

MiniAudioLib::ma_engine maEngine;
std::map<std::string, std::list<MiniAudioLib::ma_sound>> maSoundMap;
MiniAudioLib::ma_sound* mainMusicSound;

// TFL Note: Added
MiniAudioLib::ma_engine g_ma_engine_tfl;

void kmachine_init_globals_common() {
  memset(pad_dma_buf, 0, sizeof(pad_dma_buf));
  isodrv = fakeiso;  // changed. fakeiso is the only one that works in opengoal.
  modsrc = 1;
  reboot_iop = 1;
  vif1_interrupt_handler = 0;
  vblank_interrupt_handler = 0;
  ee_clock_timer = Timer();
#ifdef _WIN32  // only do this on windows, because it only works on windows?
  MiniAudioLib::ma_engine_uninit(&maEngine);
  MiniAudioLib::ma_engine_uninit(&g_ma_engine_tfl);
#endif
  MiniAudioLib::ma_engine_init(NULL, &maEngine);

  MiniAudioLib::ma_engine_init(nullptr, &g_ma_engine_tfl);
}

/*!
 * Initialize the CD Drive
 * DONE, EXACT
 */
void InitCD() {
  lg::info("Initializing CD drive. This may take a while...");
  ee::sceCdInit(SCECdINIT);
  ee::sceCdMmode(SCECdDVD);
  while (ee::sceCdDiskReady(0) == SCECdNotReady) {
    lg::debug("Drive not ready... insert a disk!");
  }
  lg::debug("Disk type {}\n", ee::sceCdGetDiskType());
}

/*!
 * Initialize the GS and display the splash screen.
 * Not yet implemented. TODO
 */
void InitVideo() {}

/*!
 * Flush caches.  Does all the memory, regardless of what you specify
 */
void CacheFlush(void* mem, int size) {
  (void)mem;
  (void)size;
  // FlushCache(0);
  // FlushCache(2);
}

/*!
 * Open a new controller pad.
 * Set the new_pad flag to 1 and state to 0.
 * Prints an error if it fails to open.
 */
u64 CPadOpen(u64 cpad_info, s32 pad_number) {
  auto cpad = Ptr<CPadInfo>(cpad_info).c();
  if (cpad->cpad_file == 0) {
    // not open, so we will open it
    cpad->cpad_file =
        ee::scePadPortOpen(pad_number, 0, pad_dma_buf + pad_number * SCE_PAD_DMA_BUFFER_SIZE);
    if (cpad->cpad_file < 1) {
      MsgErr("dkernel: !open cpad #%d (%d)\n", pad_number, cpad->cpad_file);
    }
    cpad->new_pad = 1;
    cpad->state = 0;
  }
  return cpad_info;
}

// Mutex to synchronize access to activeMusics
std::mutex activeMusicsMutex;

// Declare a mutex for synchronizing access to mainMusicInstance
std::mutex mainMusicMutex;

// Function to stop all instances of specific sound by filepath
void stopMP3(u32 filePathu32) {
  std::string filePath = Ptr<String>(filePathu32).c()->data();
  std::cout << "Trying to stop file: " << filePath << std::endl;

  std::lock_guard<std::mutex> lock(activeMusicsMutex);
  auto it = maSoundMap.find(filePath);
  if (it == maSoundMap.end()) {
    std::cerr << "Couldn't find sound to stop: " << filePath << std::endl;
  } else {
    // stop all instances of this sound
    for (auto sound : it->second) {
      if (MiniAudioLib::ma_sound_stop(&sound) != MiniAudioLib::MA_SUCCESS) {
        std::cerr << "Failed to stop sound: " << filePath << std::endl;
      }
      // let the thread finish and handle ma_sound_uninit
    }
    // clear list of sounds for this filepath
    it->second.clear();
  }
}

// Function to stop all currently playing sounds.
void stopAllSounds() {
  for (auto& pair : maSoundMap) {
    // stop all instances of this sound
    for (auto sound : pair.second) {
      MiniAudioLib::ma_sound_stop(&sound);
    }
    pair.second.clear();
  }
  maSoundMap.clear();
}

// Function to get the names of currently playing files.
std::vector<std::string> getPlayingFileNames() {
  std::vector<std::string> playingFileNames;
  for (const auto& pair : maSoundMap) {
    playingFileNames.push_back(pair.first);
  }
  return playingFileNames;
}

u64 playMP3_internal(u32 filePathu32, u32 volume, bool isMainMusic) {
  std::string filePath = Ptr<String>(filePathu32).c()->data();
  std::string fullFilePath = fs::path(file_util::get_jak_project_dir() / "custom_assets" /
                                  game_version_names[g_game_version] / "audio" / filePath).string();
  
  if (!file_util::file_exists(fullFilePath)) {
    // file doesn't exist, let GOAL side know we didn't find it
    return bool_to_symbol(false);
  }

  std::thread thread([=]() {

    std::cout << "Playing file: " << filePath << std::endl;

    MiniAudioLib::ma_result result;
    MiniAudioLib::ma_sound sound;

    result = MiniAudioLib::ma_sound_init_from_file(&maEngine, fullFilePath.c_str(), 0, NULL, NULL,
                                                    &sound);
    if (result != MiniAudioLib::MA_SUCCESS) {
      std::cout << "Failed to load: " << filePath << std::endl;
      return;
    }

    MiniAudioLib::ma_sound_set_volume(&sound, ((float)volume) / 100.0);

    if (isMainMusic) {
      MiniAudioLib::ma_sound_set_looping(&sound, MA_TRUE);
      mainMusicMutex.lock();
      mainMusicSound = &sound;
      mainMusicMutex.unlock();
    }

    MiniAudioLib::ma_sound_start(&sound);

    if (!isMainMusic) {
      std::lock_guard<std::mutex> lock(activeMusicsMutex);
      if (maSoundMap.find(filePath) == maSoundMap.end()) {
        maSoundMap.insert(std::make_pair(filePath, std::list<MiniAudioLib::ma_sound>()));
      }
      maSoundMap[filePath].push_back(sound);
    }

    // sleep/loop until we're no longer main music, or non-looping sound is stopped/ends
    while (mainMusicSound == &sound || MiniAudioLib::ma_sound_is_playing(&sound)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    MiniAudioLib::ma_sound_stop(&sound);
    MiniAudioLib::ma_sound_uninit(&sound);
    std::cout << "Finished playing file: " << filePath << std::endl;

    if (!isMainMusic) {
      std::lock_guard<std::mutex> lock(activeMusicsMutex);
      if (maSoundMap.find(filePath) != maSoundMap.end()) {
        maSoundMap[filePath].remove_if(
            [&](MiniAudioLib::ma_sound l_sound) { return &sound == &l_sound; });
      }
    }
  });

  thread.detach();
  return bool_to_symbol(true);
}

u64 playMP3(u32 filePathu32, u32 volume) {
  return playMP3_internal(filePathu32, volume, false);
}

// Function to stop the Main Music.
void stopMainMusic() {
  mainMusicMutex.lock();
  if (mainMusicSound && MiniAudioLib::ma_sound_is_playing(mainMusicSound)) {
    std::cout << "Stopping Main Music..." << std::endl;
    MiniAudioLib::ma_sound_stop(mainMusicSound);
    mainMusicSound = NULL;
    std::cout << "Stopped Main Music " << std::endl;
  }
  mainMusicMutex.unlock();
}

// Function to play the Main Music.
void playMainMusic(u32 filePathu32, u32 volume) {
  stopMainMusic();

  std::cout << "Playing Main Music" << std::endl;

  playMP3_internal(filePathu32, volume, true);
}

void pauseMainMusic() {
  mainMusicMutex.lock();
  if (mainMusicSound && MiniAudioLib::ma_sound_is_playing(mainMusicSound)) {
    MiniAudioLib::ma_sound_stop(mainMusicSound);
  }
  mainMusicMutex.unlock();
}

void resumeMainMusic() {
  mainMusicMutex.lock();
  if (mainMusicSound && !MiniAudioLib::ma_sound_is_playing(mainMusicSound)) {
    MiniAudioLib::ma_sound_start(mainMusicSound);
  }
  mainMusicMutex.unlock();
}

// Function to change the volume of the Main Music.
void changeMainMusicVolume(u32 volume) {
  mainMusicMutex.lock();
  if (mainMusicSound) {
    MiniAudioLib::ma_sound_set_volume(mainMusicSound, ((float)volume) / 100.0);
  }
  mainMusicMutex.unlock();
}

// TFL note: added
std::vector<std::pair<MiniAudioLib::ma_sound*, std::string>> g_tfl_hints;
std::mutex g_tfl_hints_mtx;
bool g_interrupt_hint = false;

void stop_tfl_hint() {
  for (auto& pair : g_tfl_hints) {
    MiniAudioLib::ma_sound_stop(pair.first);
    // ma_sound_uninit(pair.first);
    // delete pair.first;
    // pair.first = nullptr;
  }
  // jak1::intern_from_c("*tfl-hint-playing?*")->value = offset_of_s7();
  g_tfl_hints.clear();
}

std::vector<std::string> tfl_get_current_hint() {
  std::vector<std::string> playing;
  for (const auto& pair : g_tfl_hints) {
    playing.push_back(pair.second);
  }
  return playing;
}

u32 play_tfl_hint(u32 file_name, u32 volume, u32 interrupt) {
  auto hint_is_playing = jak1::intern_from_c("*tfl-hint-playing?*")->value ==
                         offset_of_s7() + jak1_symbols::FIX_SYM_TRUE;
  if (hint_is_playing && interrupt == offset_of_s7()) {
    printf("TFL hint is already playing!\n");
    return offset_of_s7();
  }

  if (hint_is_playing && interrupt == offset_of_s7() + jak1_symbols::FIX_SYM_TRUE) {
    // auto current_hint = tfl_get_current_hint();
    printf("Interrupting TFL hint\n");
    stop_tfl_hint();
  }

  std::thread hint_thread([=]() {
    auto name_str = std::string(Ptr<String>(file_name)->data());
    auto path = (file_util::get_jak_project_dir() / "custom_assets" / "jak1" / "audio" / "tfl" /
                 "hints" / name_str)
                    .string();
    printf("Playing hint: %s\n", name_str.c_str());

    auto* hint = new MiniAudioLib::ma_sound;
    auto hint_result = MiniAudioLib::ma_sound_init_from_file(&g_ma_engine_tfl, path.c_str(), 0,
                                                             nullptr, nullptr, hint);
    if (hint_result != MiniAudioLib::MA_SUCCESS) {
      printf("Failed to load: %s\n", path.c_str());
      jak1::intern_from_c("*tfl-hint-playing?*")->value = offset_of_s7();
      delete hint;
      return;
    }
    float vol;
    memcpy(&vol, &volume, 4);
    MiniAudioLib::ma_sound_set_volume(hint, vol);
    MiniAudioLib::ma_sound_set_looping(hint, MA_FALSE);
    MiniAudioLib::ma_sound_start(hint);
    jak1::intern_from_c("*tfl-hint-playing?*")->value = offset_of_s7() + jak1_symbols::FIX_SYM_TRUE;

    {
      std::lock_guard lock(g_tfl_hints_mtx);
      g_tfl_hints.emplace_back(hint, path.c_str());
    }

    auto paused_func = [](MiniAudioLib::ma_sound* hint) {
      while (!MiniAudioLib::ma_sound_is_playing(hint)) {
        if (MasterExit != RuntimeExitStatus::RUNNING) {
          stop_tfl_hint();
          return;
        }
        auto pause = jak1::call_goal_function_by_name("paused?");
        if (pause == offset_of_s7()) {
          MiniAudioLib::ma_sound_start(hint);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    };

    auto play_func = [&hint, &paused_func]() {
      while (MiniAudioLib::ma_sound_is_playing(hint) && !MiniAudioLib::ma_sound_at_end(hint)) {
        auto paused = jak1::call_goal_function_by_name("paused?");
        if (paused == offset_of_s7() + jak1_symbols::FIX_SYM_TRUE) {
          // hint->pause();
          MiniAudioLib::ma_sound_stop(hint);
          paused_func(hint);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    };

    play_func();

    {
      std::lock_guard lock(g_tfl_hints_mtx);
      g_tfl_hints.erase(std::remove_if(g_tfl_hints.begin(), g_tfl_hints.end(),
                                       [hint](const auto& pair) { return pair.first == hint; }),
                        g_tfl_hints.end());
    }

    MiniAudioLib::ma_sound_stop(hint);
    MiniAudioLib::ma_sound_uninit(hint);
    delete hint;
    hint = nullptr;

    jak1::intern_from_c("*tfl-hint-playing?*")->value = offset_of_s7();
  });

  hint_thread.detach();
  return offset_of_s7() + jak1_symbols::FIX_SYM_TRUE;
}

MiniAudioLib::ma_sound* g_tfl_music;

void stop_tfl_music(bool force) {
  if (g_tfl_music) {
    if (force) {
      MiniAudioLib::ma_sound_stop(g_tfl_music);
      MiniAudioLib::ma_sound_uninit(g_tfl_music);
#ifdef MA_UNIX
      MiniAudioLib::ma_engine_stop(&g_ma_engine_tfl);
      MiniAudioLib::ma_engine_uninit(&g_ma_engine_tfl);
#endif
      jak1::intern_from_c("*tfl-music-playing?*")->value = offset_of_s7();
      return;
    }
    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
    float time_elapsed = 1.f;
    float start = MiniAudioLib::ma_sound_get_volume(g_tfl_music);
    float end = 0.f;
    float val = start;
    while (val >= 0.01f) {
      val = lerp(start, end, 1.0f - time_elapsed);
      MiniAudioLib::ma_sound_set_volume(g_tfl_music, val);
      printf("Fading out music volume: %f\n", val);
      time_elapsed -= 0.01f;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    MiniAudioLib::ma_sound_stop(g_tfl_music);
    jak1::intern_from_c("*tfl-music-playing?*")->value = offset_of_s7();
    // delete g_tfl_music;
  }
}

u32 play_tfl_music(u32 file_name, u32 volume) {
  auto music_playing = jak1::intern_from_c("*tfl-music-playing?*")->value ==
                          offset_of_s7() + jak1_symbols::FIX_SYM_TRUE;
  auto boss = jak1::intern_from_c("*tfl-boss-music*")->value;
  auto music_is_playing = music_playing && boss == offset_of_s7();
  if (music_is_playing) {
    printf("TFL music is already playing!\n");
    return offset_of_s7();
  }

  std::thread music_thread([=]() {
    auto name_str = std::string(Ptr<String>(file_name)->data());
    std::string file;
    auto music_dir =
        file_util::get_jak_project_dir() / "custom_assets" / "jak1" / "audio" / "tfl" / "music";
    for (const auto& entry : fs::directory_iterator(music_dir)) {
      // printf("Checking file %s\n",entry.path().string().c_str());
      if (entry.is_regular_file() && entry.path().stem().string() == name_str) {
        file = entry.path().string();
        break;
      }
    }

    auto* music = new MiniAudioLib::ma_sound;
    auto music_result = MiniAudioLib::ma_sound_init_from_file(&g_ma_engine_tfl, file.c_str(), 0,
                                                              nullptr, nullptr, music);
    if (music_result != MiniAudioLib::MA_SUCCESS) {
      printf("Failed to load music: %s\n", file.c_str());
      delete music;
      return;
    }
    float vol;
    memcpy(&vol, &volume, 4);
    printf("Playing music: %s (volume %f)\n", name_str.c_str(), vol);
    MiniAudioLib::ma_sound_set_volume(music, 0.f);
    MiniAudioLib::ma_sound_set_looping(music, MA_TRUE);
    MiniAudioLib::ma_sound_start(music);
    jak1::intern_from_c("*tfl-music-playing?*")->value =
        offset_of_s7() + jak1_symbols::FIX_SYM_TRUE;
    g_tfl_music = music;
    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
    float time_elapsed = 0.f;
    float start = 0.f;
    float end = vol;
    float val = start;
    while (val < vol) {
      val = lerp(start, end, time_elapsed);
      MiniAudioLib::ma_sound_set_volume(g_tfl_music, val);
      printf("Fading in music volume: %f (target volume: %f)\n", val, vol);
      time_elapsed += 0.01f;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto paused_func = [](MiniAudioLib::ma_sound* music) {
      while (!MiniAudioLib::ma_sound_is_playing(music)) {
        auto pause = jak1::call_goal_function_by_name("tfl-music-player-paused?");
        if (pause == offset_of_s7()) {
          MiniAudioLib::ma_sound_start(music);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    };

    auto play_func = [&music, &paused_func]() {
      while (MiniAudioLib::ma_sound_is_playing(music)) {
        if (MasterExit != RuntimeExitStatus::RUNNING) {
          stop_tfl_music(true);
          return;
        }
        auto stop = jak1::intern_from_c("*tfl-music-stop*")->value;
        if (stop == offset_of_s7() + jak1_symbols::FIX_SYM_TRUE) {
          jak1::intern_from_c("*tfl-music-stop*")->value = offset_of_s7();
          stop_tfl_music(false);
          // delete g_tfl_music;
          // std::terminate();
        }
        float vol;
        auto volume = jak1::call_goal_function_by_name("tfl-music-player-volume");
        memcpy(&vol, &volume, 4);
        MiniAudioLib::ma_sound_set_volume(music, vol);
        auto paused = jak1::call_goal_function_by_name("tfl-music-player-paused?");
        if (paused != offset_of_s7()) {
          MiniAudioLib::ma_sound_stop(music);
          paused_func(music);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    };

    play_func();
  });

  music_thread.detach();
  return offset_of_s7() + jak1_symbols::FIX_SYM_TRUE;
}

/*!
 * Not checked super carefully for jak 2, but looks the same
 */
u64 CPadGetData(u64 cpad_info) {
  using namespace ee;
  auto cpad = Ptr<CPadInfo>(cpad_info).c();
  auto pad_state = scePadGetState(cpad->number, 0);
  if (pad_state == scePadStateDiscon) {
    cpad->state = 0;
  }
  cpad->valid = pad_state | 0x80;
  switch (cpad->state) {
    // case 99: // functional
    default:  // controller is functioning as normal
      if (pad_state == scePadStateStable || pad_state == scePadStateFindCTP1) {
        scePadRead(cpad->number, 0, (u8*)cpad);
        // ps2 controllers would send an enabled bit if the button was NOT pressed, but we don't do
        // that here. removed code that flipped the bits.

        if (cpad->change_time) {
          scePadSetActDirect(cpad->number, 0, cpad->direct);
        }
        cpad->valid = pad_state;
      }
      break;
    case 0:  // unavailable
      if (pad_state == scePadStateStable || pad_state == scePadStateFindCTP1) {
        auto pad_mode = scePadInfoMode(cpad->number, 0, InfoModeCurID, 0);
        if (pad_mode != 0) {
          auto vibration_mode = scePadInfoMode(cpad->number, 0, InfoModeCurExID, 0);
          if (vibration_mode > 0) {
            // vibration supported
            pad_mode = vibration_mode;
          }
          if (pad_mode == 4) {
            // controller mode
            cpad->state = 40;
          } else if (pad_mode == 7) {
            // dualshock mode
            cpad->state = 70;
          } else {
            // who knows mode
            cpad->state = 90;
          }
        }
      }
      break;
    case 40:  // controller mode - check for extra modes
      // cpad->change_time = 0;
      cpad->change_time = 0;
      if (scePadInfoMode(cpad->number, 0, InfoModeIdTable, -1) == 0) {
        // no controller modes
        cpad->state = 90;
        return cpad_info;
      }
      cpad->state = 41;
    case 41:  // controller mode - change to dualshock mode!
      // try to enter the 2nd controller mode (dualshock for ds2's)
      if (scePadSetMainMode(cpad->number, 0, 1, 3) == 1) {
        cpad->state = 42;
      }
      break;
    case 42:  // controller mode change check
      if (scePadGetReqState(cpad->number, 0) == scePadReqStateFailed) {
        // failed to change to DS2
        cpad->state = 41;
      }
      if (scePadGetReqState(cpad->number, 0) == scePadReqStateComplete) {
        // change successful. go back to the beginning.
        cpad->state = 0;
      }
      break;
    case 70:  // dualshock mode - check vibration
      // get number of actuators (2 for DS2)
      if (scePadInfoAct(cpad->number, 0, -1, 0) < 1) {
        // no actuators means no vibration. skip to end!
        // cpad->change_time = 0;
        cpad->change_time = 0;
        cpad->state = 99;
      } else {
        // we have actuators to use.
        // cpad->change_time = 1;  // remember to update pad times.
        cpad->change_time = 1;
        cpad->state = 75;
      }
      break;
    case 75:  // set actuator vib param info
      if (scePadSetActAlign(cpad->number, 0, cpad->align) != 0) {
        if (scePadInfoPressMode(cpad->number, 0) == 1) {
          // pressure buttons supported
          cpad->state = 76;
        } else {
          // no pressure buttons, done with controller setup
          cpad->state = 99;
        }
      }
      break;
    case 76:  // enter pressure mode
      if (scePadEnterPressMode(cpad->number, 0) == 1) {
        cpad->state = 78;
      }
      break;
    case 78:  // pressure mode request check
      if (scePadGetReqState(cpad->number, 0) == scePadReqStateFailed) {
        cpad->state = 76;
      }
      if (scePadGetReqState(cpad->number, 0) == scePadReqStateComplete) {
        cpad->state = 99;
      }
      break;
    case 90:
      break;  // unsupported controller. too bad!
  }
  return cpad_info;
}

// should make sure this works the same way in jak 2
void InstallHandler(u32 handler_idx, u32 handler_func) {
  switch (handler_idx) {
    case 3:
      vblank_interrupt_handler = handler_func;
      break;
    case 5:
      vif1_interrupt_handler = handler_func;
      break;
    default:
      lg::error("unknown handler: {}\n", handler_idx);
      ASSERT(false);
  }
}

// nothing used this in jak1, hopefully same for 2
void InstallDebugHandler() {
  ASSERT(false);
}

/*!
 * Get length of a file.
 */
s32 klength(u64 fs) {
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    // first flag bit not set. This means no errors
    auto end_seek = ee::sceLseek(file_stream->file, 0, SCE_SEEK_END);
    auto reset_seek = ee::sceLseek(file_stream->file, 0, SCE_SEEK_SET);
    if (reset_seek < 0 || end_seek < 0) {
      // seeking failed, flag it
      file_stream->flags |= 1;
    }
    return end_seek;
  } else {
    return 0;
  }
}

/*!
 * Seek a file stream.
 */
s32 kseek(u64 fs, s32 offset, s32 where) {
  s32 result = -1;
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    result = ee::sceLseek(file_stream->file, offset, where);
    if (result < 0) {
      file_stream->flags |= 1;
    }
  }
  return result;
}

/*!
 * Read from a file stream.
 */
s32 kread(u64 fs, u64 buffer, s32 size) {
  s32 result = -1;
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    result = ee::sceRead(file_stream->file, Ptr<u8>(buffer).c(), size);
    if (result < 0) {
      file_stream->flags |= 1;
    }
  }
  return result;
}

/*!
 * Write to a file stream.
 */
s32 kwrite(u64 fs, u64 buffer, s32 size) {
  s32 result = -1;
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    result = ee::sceWrite(file_stream->file, Ptr<u8>(buffer).c(), size);
    if (result < 0) {
      file_stream->flags |= 1;
    }
  }
  return result;
}

/*!
 * Close a file stream.
 */
u64 kclose(u64 fs) {
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    ee::sceClose(file_stream->file);
    file_stream->file = -1;
  }
  file_stream->flags = 0;
  return fs;
}

// TODO dma_to_iop
void dma_to_iop() {
  ASSERT(false);
}

u64 DecodeLanguage() {
  return masterConfig.language;
}

u64 DecodeAspect() {
  return masterConfig.aspect;
}

u64 DecodeVolume() {
  return masterConfig.volume;
}

// NOTE: this is originally hardcoded, and returns different values depending on the disc region.
// it returns 0 for NTSC-U, 1 for PAL and 2 for NTSC-J
u64 DecodeTerritory() {
  return GAME_TERRITORY_SCEA;
}

u64 DecodeTimeout() {
  return masterConfig.timeout;
}

u64 DecodeInactiveTimeout() {
  return masterConfig.inactive_timeout;
}

void DecodeTime(u32 ptr) {
  Ptr<ee::sceCdCLOCK> clock(ptr);
  // in jak2, if this fails, they do a sceScfGetLocalTimefromRTC
  sceCdReadClock(clock.c());
}

void vif_interrupt_callback(int bucket_id) {
  // added for the PC port for faking VIF interrupts from the graphics system.
  if (vif1_interrupt_handler && MasterExit == RuntimeExitStatus::RUNNING) {
    call_goal(Ptr<Function>(vif1_interrupt_handler), bucket_id, 0, 0, s7.offset, g_ee_main_mem);
  }
}

/// PC PORT FUNCTIONS BEGIN

u32 offset_of_s7() {
  return s7.offset;
}

inline bool symbol_to_bool(const u32 symptr) {
  return symptr != s7.offset;
}

inline u64 bool_to_symbol(const bool val) {
  return val ? static_cast<u64>(s7.offset) + true_symbol_offset(g_game_version) : s7.offset;
}

u64 pc_filter_debug_string(u32 str_ptr, u32 dist_ptr) {
  auto str = std::string(Ptr<String>(str_ptr).c()->data());
  float dist;
  memcpy(&dist, &dist_ptr, 4);

  // Check distance first
  if (Gfx::g_debug_settings.text_check_range) {
    if (dist / 4096.F > Gfx::g_debug_settings.text_max_range) {
      return bool_to_symbol(true);
    }
  }

  // Get the current filters
  const auto& filters = Gfx::g_debug_settings.text_filters;
  if (filters.empty()) {
    // there are no filters, exit early
    return bool_to_symbol(false);
  }

  // Currently very dumb contains check
  for (const auto& filter : filters) {
    if (filter.type == DebugTextFilter::Type::CONTAINS) {
      if (!str.empty() && !filter.content.empty() && !str_util::contains(str, filter.content)) {
        return bool_to_symbol(true);
      }
    } else if (filter.type == DebugTextFilter::Type::NOT_CONTAINS) {
      if (!str.empty() && !filter.content.empty() && str_util::contains(str, filter.content)) {
        return bool_to_symbol(true);
      }
    } else if (filter.type == DebugTextFilter::Type::REGEX) {
      if (str_util::valid_regex(filter.content) &&
          std::regex_match(str, std::regex(filter.content))) {
        return bool_to_symbol(true);
      }
    }
  }
  return bool_to_symbol(false);
}

CommonPCPortFunctionWrappers g_pc_port_funcs;

u64 read_ee_timer() {
  u64 ns = ee_clock_timer.getNs();
  return (ns * 3) / 10;
}

void pc_memmove(u32 dst, u32 src, u32 size) {
  memmove(Ptr<u8>(dst).c(), Ptr<u8>(src).c(), size);
}

void send_gfx_dma_chain(u32 /*bank*/, u32 chain) {
  if (Gfx::GetCurrentRenderer()) {
    Gfx::GetCurrentRenderer()->send_chain(g_ee_main_mem, chain);
  }
}

void pc_texture_upload_now(u32 page, u32 mode) {
  if (Gfx::GetCurrentRenderer()) {
    Gfx::GetCurrentRenderer()->texture_upload_now(Ptr<u8>(page).c(), mode, s7.offset);
  }
}

void pc_texture_relocate(u32 dst, u32 src, u32 format) {
  if (Gfx::GetCurrentRenderer()) {
    Gfx::GetCurrentRenderer()->texture_relocate(dst, src, format);
  }
}

u64 pc_get_mips2c(u32 name) {
  const char* n = Ptr<String>(name).c()->data();
  return Mips2C::gLinkedFunctionTable.get(n);
}

u64 pc_get_display_id() {
  if (Display::GetMainDisplay()) {
    return Display::GetMainDisplay()->get_display_manager()->get_active_display_index();
  }
  return 0;
}

void pc_set_display_id(u64 display_id) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_display_manager()->enqueue_set_display_id(display_id);
  }
}

u64 pc_get_display_name(u32 id, u32 str_dest_ptr) {
  std::string name = "";
  if (Display::GetMainDisplay()) {
    name = Display::GetMainDisplay()->get_display_manager()->get_connected_display_name(id);
  }
  if (name.empty()) {
    return bool_to_symbol(false);
  }
  if (g_game_version == GameVersion::Jak1) {
    // The Jak 1 font has only caps
    name = str_util::to_upper(name).c_str();
  }
  // Encode the string to the game font
  const auto encoded_name = get_font_bank_from_game_version(g_game_version)
                                ->convert_utf8_to_game(str_util::titlize(name).c_str());
  strcpy(Ptr<String>(str_dest_ptr).c()->data(), encoded_name.c_str());
  strcpy(Ptr<String>(str_dest_ptr).c()->data(), str_util::titlize(encoded_name).c_str());
  return bool_to_symbol(true);
}

u32 pc_get_display_mode() {
  auto display_mode = game_settings::DisplaySettings::DisplayMode::Windowed;
  if (Display::GetMainDisplay()) {
    display_mode = Display::GetMainDisplay()->get_display_manager()->get_display_mode();
  }
  switch (display_mode) {
    case game_settings::DisplaySettings::DisplayMode::Borderless:
      return g_pc_port_funcs.intern_from_c("borderless").offset;
    case game_settings::DisplaySettings::DisplayMode::Fullscreen:
      return g_pc_port_funcs.intern_from_c("fullscreen").offset;
    default:
      return g_pc_port_funcs.intern_from_c("windowed").offset;
  }
}

void pc_set_display_mode(u32 symptr, u64 window_width, u64 window_height) {
  if (!Display::GetMainDisplay()) {
    return;
  }
  if (symptr == g_pc_port_funcs.intern_from_c("windowed").offset || symptr == s7.offset) {
    Display::GetMainDisplay()->get_display_manager()->enqueue_set_window_display_mode(
        game_settings::DisplaySettings::DisplayMode::Windowed, window_width, window_height);
  } else if (symptr == g_pc_port_funcs.intern_from_c("borderless").offset) {
    Display::GetMainDisplay()->get_display_manager()->enqueue_set_window_display_mode(
        game_settings::DisplaySettings::DisplayMode::Borderless, window_width, window_height);
  } else if (symptr == g_pc_port_funcs.intern_from_c("fullscreen").offset) {
    Display::GetMainDisplay()->get_display_manager()->enqueue_set_window_display_mode(
        game_settings::DisplaySettings::DisplayMode::Fullscreen, window_width, window_height);
  }
}

u64 pc_get_display_count() {
  if (Display::GetMainDisplay()) {
    return Display::GetMainDisplay()->get_display_manager()->num_connected_displays();
  }
  return 0;
}

void pc_get_active_display_size(u32 w_ptr, u32 h_ptr) {
  if (!Display::GetMainDisplay()) {
    return;
  }
  if (w_ptr) {
    auto w_out = Ptr<s64>(w_ptr).c();
    if (w_out) {
      *w_out = Display::GetMainDisplay()->get_display_manager()->get_screen_width();
    }
  }
  if (h_ptr) {
    auto h_out = Ptr<s64>(h_ptr).c();
    if (h_out) {
      *h_out = Display::GetMainDisplay()->get_display_manager()->get_screen_height();
    }
  }
}

s64 pc_get_active_display_refresh_rate() {
  if (Display::GetMainDisplay()) {
    return Display::GetMainDisplay()->get_display_manager()->get_active_display_refresh_rate();
  }
  return 0;
}

void pc_get_window_size(u32 w_ptr, u32 h_ptr) {
  if (!Display::GetMainDisplay()) {
    return;
  }
  if (w_ptr) {
    auto w = Ptr<s64>(w_ptr).c();
    if (w) {
      *w = Display::GetMainDisplay()->get_display_manager()->get_window_width();
    }
  }
  if (h_ptr) {
    auto h = Ptr<s64>(h_ptr).c();
    if (h) {
      *h = Display::GetMainDisplay()->get_display_manager()->get_window_height();
    }
  }
}

void pc_get_window_scale(u32 x_ptr, u32 y_ptr) {
  if (!Display::GetMainDisplay()) {
    return;
  }
  if (x_ptr) {
    auto x = Ptr<float>(x_ptr).c();
    if (x) {
      *x = Display::GetMainDisplay()->get_display_manager()->get_window_scale_x();
    }
  }
  if (y_ptr) {
    auto y = Ptr<float>(y_ptr).c();
    if (y) {
      *y = Display::GetMainDisplay()->get_display_manager()->get_window_scale_y();
    }
  }
}

void pc_set_window_size(u64 width, u64 height) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_display_manager()->enqueue_set_window_size(width, height);
  }
}

s64 pc_get_num_resolutions(u32 for_windowed) {
  if (Display::GetMainDisplay()) {
    return Display::GetMainDisplay()->get_display_manager()->get_num_resolutions(
        symbol_to_bool(for_windowed));
  }
  return 0;
}

void pc_get_resolution(u32 id, u32 for_windowed, u32 w_ptr, u32 h_ptr) {
  if (Display::GetMainDisplay()) {
    auto res = Display::GetMainDisplay()->get_display_manager()->get_resolution(
        id, symbol_to_bool(for_windowed));
    auto w = Ptr<s64>(w_ptr).c();
    if (w) {
      *w = res.width;
    }
    auto h = Ptr<s64>(h_ptr).c();
    if (h) {
      *h = res.height;
    }
  }
}

u64 pc_is_supported_resolution(u64 width, u64 height) {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(
        Display::GetMainDisplay()->get_display_manager()->is_supported_resolution(width, height));
  }
  return bool_to_symbol(false);
}

u64 pc_get_controller_name(u32 id, u32 str_dest_ptr) {
  std::string name = "";
  if (Display::GetMainDisplay()) {
    name = Display::GetMainDisplay()->get_input_manager()->get_controller_name(id);
  }
  if (name.empty()) {
    return bool_to_symbol(false);
  }

  if (g_game_version == GameVersion::Jak1) {
    // The Jak 1 font has only caps
    name = str_util::to_upper(name).c_str();
  }
  // Encode the string to the game font
  const auto encoded_name = get_font_bank_from_game_version(g_game_version)
                                ->convert_utf8_to_game(str_util::titlize(name).c_str());
  strcpy(Ptr<String>(str_dest_ptr).c()->data(), encoded_name.c_str());
  strcpy(Ptr<String>(str_dest_ptr).c()->data(), str_util::titlize(encoded_name).c_str());
  return bool_to_symbol(true);
}

u64 pc_get_current_bind(s32 bind_assignment_info, u32 str_dest_ptr) {
  if (!Display::GetMainDisplay()) {
    // TODO - return something that lets the runtime use a translatable string if unknown
    strcpy(Ptr<String>(str_dest_ptr).c()->data(), str_util::to_upper("unknown").c_str());
    return bool_to_symbol(true);
  }

  auto info = bind_assignment_info ? Ptr<BindAssignmentInfo>(bind_assignment_info).c() : NULL;
  auto port = (int)info->port;
  auto device_type = (int)info->device_type;
  auto for_button = info->buttons != s7.offset;
  auto input_idx = (int)info->input_idx;
  auto analog_min_range = info->analog_min_range != s7.offset;

  if (Display::GetMainDisplay()) {
    auto name = Display::GetMainDisplay()->get_input_manager()->get_current_bind(
        port, (InputDeviceType)device_type, for_button, input_idx, analog_min_range);
    if (name.empty()) {
      return bool_to_symbol(false);
    }
    if (g_game_version == GameVersion::Jak1) {
      // The Jak 1 font has only caps
      name = str_util::to_upper(name).c_str();
    }
    // Encode the string to the game font
    const auto encoded_name = get_font_bank_from_game_version(g_game_version)
                                  ->convert_utf8_to_game(str_util::titlize(name).c_str());
    strcpy(Ptr<String>(str_dest_ptr).c()->data(), encoded_name.c_str());
    return bool_to_symbol(true);
  }
  // TODO - return something that lets the runtime use a translatable string if unknown
  strcpy(Ptr<String>(str_dest_ptr).c()->data(), str_util::to_upper("unknown").c_str());
  return bool_to_symbol(true);
}

u64 pc_get_controller_count() {
  if (Display::GetMainDisplay()) {
    return Display::GetMainDisplay()->get_input_manager()->get_num_controllers();
  }
  return 0;
}

u64 pc_get_controller_index(u32 port) {
  if (Display::GetMainDisplay()) {
    return Display::GetMainDisplay()->get_input_manager()->get_controller_index(port);
  }
  return 0;
}

void pc_set_controller(u32 controller_id, u32 port) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->set_controller_for_port(controller_id, port);
  }
}

u32 pc_get_keyboard_enabled() {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(Display::GetMainDisplay()->get_input_manager()->is_keyboard_enabled());
  }
  return bool_to_symbol(false);
}

void pc_set_keyboard_enabled(u32 sym_val) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enable_keyboard(symbol_to_bool(sym_val));
  }
}

void pc_set_mouse_options(u32 enabled, u32 control_camera, u32 control_movement) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_update_mouse_options(
        symbol_to_bool(enabled), symbol_to_bool(control_camera), symbol_to_bool(control_movement));
  }
}

void pc_set_mouse_camera_sens(u32 xsens, u32 ysens) {
  float xsens_val;
  memcpy(&xsens_val, &xsens, 4);
  float ysens_val;
  memcpy(&ysens_val, &ysens, 4);
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->set_camera_sens(xsens_val, ysens_val);
  }
}

void pc_ignore_background_controller_events(u32 sym_val) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_ignore_background_controller_events(
        symbol_to_bool(sym_val));
  }
}

u64 pc_current_controller_has_led() {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(Display::GetMainDisplay()->get_input_manager()->controller_has_led(0));
  }
  return bool_to_symbol(false);
}

u64 pc_current_controller_has_rumble() {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(Display::GetMainDisplay()->get_input_manager()->controller_has_rumble(0));
  }
  return bool_to_symbol(false);
}

void pc_set_controller_led(const int port, const u8 red, const u8 green, const u8 blue) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_set_controller_led(port, red, green,
                                                                               blue);
  }
}

u64 pc_waiting_for_bind() {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(Display::GetMainDisplay()->get_input_manager()->get_waiting_for_bind());
  }
  return bool_to_symbol(false);
}

void pc_set_waiting_for_bind(InputDeviceType device_type,
                             u32 for_analog,
                             u32 for_minimum_analog,
                             s32 input_idx) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->set_wait_for_bind(
        device_type, symbol_to_bool(for_analog), symbol_to_bool(for_minimum_analog), input_idx);
  }
}

void pc_stop_waiting_for_bind() {
  if (Display::GetMainDisplay()) {
    return Display::GetMainDisplay()->get_input_manager()->stop_waiting_for_bind();
  }
}

void pc_reset_bindings_to_defaults(const int port, const InputDeviceType device_type) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->reset_input_bindings_to_defaults(port,
                                                                                     device_type);
  }
}

void pc_set_auto_hide_cursor(u32 val) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_set_auto_hide_mouse(
        symbol_to_bool(val));
  }
}

u64 pc_get_pressure_sensitivity_enabled() {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(
        Display::GetMainDisplay()->get_input_manager()->is_pressure_sensitivity_enabled());
  }
  return bool_to_symbol(false);
}

void pc_set_pressure_sensitivity_enabled(u32 val) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->set_pressure_sensitivity_enabled(
        symbol_to_bool(val));
  }
}

void pc_set_axis_scale(u32 val) {
  if (Display::GetMainDisplay()) {
    // wow dangerous i guess
    Display::GetMainDisplay()->get_input_manager()->set_axis_scale(*((float*)&val));
  }
}

u32 pc_get_axis_scale() {
  float out = 1.33f;
  if (Display::GetMainDisplay()) {
    out = Display::GetMainDisplay()->get_input_manager()->axis_scale();
  }
  // wow dangerous i guess
  return *((u32*)&out);
}

u64 pc_current_controller_has_pressure_sensitivity() {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(
        Display::GetMainDisplay()->get_input_manager()->controller_has_pressure_sensitivity_support(
            0));
  }
  return bool_to_symbol(false);
}

u64 pc_current_controller_has_trigger_effect_support() {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(
        Display::GetMainDisplay()->get_input_manager()->controller_has_trigger_effect_support(0));
  }
  return bool_to_symbol(false);
}

u64 pc_get_trigger_effects_enabled() {
  if (Display::GetMainDisplay()) {
    return bool_to_symbol(
        Display::GetMainDisplay()->get_input_manager()->are_trigger_effects_enabled());
  }
  return bool_to_symbol(false);
}

void pc_set_trigger_effects_enabled(u32 val) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_set_trigger_effects_enabled(
        symbol_to_bool(val));
  }
}

void pc_clear_trigger_effect(dualsense_effects::TriggerEffectOption option) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_controller_clear_trigger_effect(0,
                                                                                            option);
  }
}

void pc_send_trigger_effect_feedback(dualsense_effects::TriggerEffectOption option,
                                     u8 position,
                                     u8 strength) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_controller_send_trigger_effect_feedback(
        0, option, position, strength);
  }
}

void pc_send_trigger_effect_vibrate(dualsense_effects::TriggerEffectOption option,
                                    u8 position,
                                    u8 amplitude,
                                    u8 frequency) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_controller_send_trigger_effect_vibrate(
        0, option, position, amplitude, frequency);
  }
}

void pc_send_trigger_effect_weapon(dualsense_effects::TriggerEffectOption option,
                                   u8 start_position,
                                   u8 end_position,
                                   u8 strength) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_controller_send_trigger_effect_weapon(
        0, option, start_position, end_position, strength);
  }
}

void pc_send_trigger_rumble(u16 left_rumble, u16 right_rumble, u32 duration_ms) {
  if (Display::GetMainDisplay()) {
    Display::GetMainDisplay()->get_input_manager()->enqueue_controller_send_trigger_rumble(
        0, left_rumble, right_rumble, duration_ms);
  }
}

void pc_set_vsync(u32 sym_val) {
  Gfx::g_global_settings.vsync = symbol_to_bool(sym_val);
}

void pc_set_msaa(int samples) {
  Gfx::g_global_settings.msaa_samples = samples;
}

void pc_set_frame_rate(int rate) {
  Gfx::g_global_settings.target_fps = rate;
}

void pc_set_game_resolution(int w, int h) {
  Gfx::g_global_settings.game_res_w = w;
  Gfx::g_global_settings.game_res_h = h;
}

void pc_set_letterbox(int w, int h) {
  Gfx::g_global_settings.lbox_w = w;
  Gfx::g_global_settings.lbox_h = h;
}

void pc_renderer_tree_set_lod(Gfx::RendererTreeType tree, int lod) {
  switch (tree) {
    case Gfx::RendererTreeType::TFRAG3:
      Gfx::g_global_settings.lod_tfrag = lod;
      break;
    case Gfx::RendererTreeType::TIE3:
      Gfx::g_global_settings.lod_tie = lod;
      break;
    default:
      lg::error("Invalid tree {} specified for SetLod ({})", fmt::underlying(tree), lod);
      break;
  }
}

void pc_set_collision_mask(GfxGlobalSettings::CollisionRendererMode mode, s64 mask, u32 symptr) {
  if (symbol_to_bool(symptr)) {
    Gfx::CollisionRendererSetMask(mode, mask);
  } else {
    Gfx::CollisionRendererClearMask(mode, mask);
  }
}

u32 pc_get_collision_mask(GfxGlobalSettings::CollisionRendererMode mode, s64 mask) {
  return Gfx::CollisionRendererGetMask(mode, mask) ? s7.offset + true_symbol_offset(g_game_version)
                                                   : s7.offset;
}

void pc_set_collision_wireframe(u32 symptr) {
  Gfx::g_global_settings.collision_wireframe = symbol_to_bool(symptr);
}

void pc_set_collision(u32 symptr) {
  Gfx::g_global_settings.collision_enable = symbol_to_bool(symptr);
}

void pc_set_gfx_hack(u64 which, u32 symptr) {
  switch (which) {
    case 0:  // no tex
      Gfx::g_global_settings.hack_no_tex = symbol_to_bool(symptr);
      break;
  }
}

u32 pc_get_os() {
#ifdef _WIN32
  return g_pc_port_funcs.intern_from_c("windows").offset;
#elif __linux__
  return g_pc_port_funcs.intern_from_c("linux").offset;
#elif __APPLE__
  return g_pc_port_funcs.intern_from_c("darwin").offset;
#else
  return s7.offset;
#endif
}

time_t pc_get_unix_timestamp() {
  return std::time(nullptr);
}

u64 pc_filepath_exists(u32 filepath) {
  auto filepath_str = std::string(Ptr<String>(filepath).c()->data());
  return bool_to_symbol(fs::exists(filepath_str));
}

u64 pc_mkdir_filepath(u32 filepath) {
  auto filepath_str = std::string(Ptr<String>(filepath).c()->data());
  return bool_to_symbol(file_util::create_dir_if_needed_for_file(filepath_str));
}

void pc_prof(u32 name, ProfNode::Kind kind) {
  prof().event(Ptr<String>(name).c()->data(), kind);
}

std::mt19937 extra_random_generator;
u32 pc_rand() {
  return (u32)extra_random_generator();
}

void pc_treat_pad0_as_pad1(u32 symptr) {
  Gfx::g_debug_settings.treat_pad0_as_pad1 = symbol_to_bool(symptr);
}

u32 pc_is_imgui_visible() {
  return bool_to_symbol(Gfx::g_debug_settings.show_imgui);
}

void pc_screen_shot() {
  g_want_screenshot = true;
}

void pc_register_screen_shot_settings(u32 ptr) {
  register_screen_shot_settings(Ptr<ScreenShotSettings>(ptr).c());
}

void pc_encode_utf8_string(u32 src_str_ptr, u32 str_dest_ptr) {
  auto str = std::string(Ptr<String>(src_str_ptr).c()->data());
  std::string version = version_to_game_name(g_game_version);
  const std::string font_bank_name = version == "jak1" ? "jak1-v2" : version;
  std::string converted =
      get_font_bank(get_text_version_from_name(font_bank_name))->convert_utf8_to_game(str);
  strcpy(Ptr<String>(str_dest_ptr).c()->data(), converted.c_str());
}

/// Initializes all functions that are common across all game versions
/// These functions have the same implementation and do not use any game specific functions (other
/// than the one to create a function in the first place)
void init_common_pc_port_functions(
    std::function<Ptr<Function>(const char*, void*)> make_func_symbol_func,
    std::function<InternFromCInfo(const char*)> intern_from_c_func,
    std::function<u64(const char*)> make_string_from_c_func) {
  g_pc_port_funcs.intern_from_c = intern_from_c_func;
  g_pc_port_funcs.make_string_from_c = make_string_from_c_func;
  // Get a 300MHz timer value. Called from EE thread
  make_func_symbol_func("__read-ee-timer", (void*)read_ee_timer);
  // Do a fast memory copy.
  make_func_symbol_func("__mem-move", (void*)pc_memmove);
  // Called from game thread to submit rendering DMA chain.
  make_func_symbol_func("__send-gfx-dma-chain", (void*)send_gfx_dma_chain);
  // Called from game thread to upload a texture outside of the main DMA chain.
  make_func_symbol_func("__pc-texture-upload-now", (void*)pc_texture_upload_now);
  make_func_symbol_func("__pc-texture-relocate", (void*)pc_texture_relocate);
  // Called from the game thread at initialization. The game thread is the only one to touch the
  // mips2c function table (through the linker and ugh this function), so no locking is needed.
  make_func_symbol_func("__pc-get-mips2c", (void*)pc_get_mips2c);

  // -- DISPLAY RELATED --
  // Returns the name of the display with the given id or #f if not found / empty
  make_func_symbol_func("pc-get-display-id", (void*)pc_get_display_id);
  make_func_symbol_func("pc-set-display-id!", (void*)pc_set_display_id);
  make_func_symbol_func("pc-get-display-name", (void*)pc_get_display_name);
  make_func_symbol_func("pc-get-display-mode", (void*)pc_get_display_mode);
  make_func_symbol_func("pc-set-display-mode!", (void*)pc_set_display_mode);
  make_func_symbol_func("pc-get-display-count", (void*)pc_get_display_count);
  // Returns resolution of the monitor's current display mode
  make_func_symbol_func("pc-get-active-display-size", (void*)pc_get_active_display_size);
  // Returns the current refresh rate of the currently selected monitor's display mode.
  make_func_symbol_func("pc-get-active-display-refresh-rate",
                        (void*)pc_get_active_display_refresh_rate);
  // Returns size of window. Called from game thread
  make_func_symbol_func("pc-get-window-size", (void*)pc_get_window_size);
  // Returns scale of window. This is for DPI stuff.
  make_func_symbol_func("pc-get-window-scale", (void*)pc_get_window_scale);
  make_func_symbol_func("pc-set-window-size!", (void*)pc_set_window_size);
  make_func_symbol_func("pc-get-num-resolutions", (void*)pc_get_num_resolutions);
  make_func_symbol_func("pc-get-resolution", (void*)pc_get_resolution);
  make_func_symbol_func("pc-is-supported-resolution?", (void*)pc_is_supported_resolution);

  // -- INPUT RELATED --
  // Returns the name of the display with the given id or #f if not found / empty
  make_func_symbol_func("pc-get-controller-name", (void*)pc_get_controller_name);
  make_func_symbol_func("pc-get-current-bind", (void*)pc_get_current_bind);
  make_func_symbol_func("pc-get-controller-count", (void*)pc_get_controller_count);
  make_func_symbol_func("pc-get-controller-index", (void*)pc_get_controller_index);
  make_func_symbol_func("pc-set-controller!", (void*)pc_set_controller);
  make_func_symbol_func("pc-get-keyboard-enabled?", (void*)pc_get_keyboard_enabled);
  make_func_symbol_func("pc-set-keyboard-enabled!", (void*)pc_set_keyboard_enabled);
  make_func_symbol_func("pc-set-mouse-options!", (void*)pc_set_mouse_options);
  make_func_symbol_func("pc-set-mouse-camera-sens!", (void*)pc_set_mouse_camera_sens);
  make_func_symbol_func("pc-ignore-background-controller-events!",
                        (void*)pc_ignore_background_controller_events);
  make_func_symbol_func("pc-current-controller-has-led?", (void*)pc_current_controller_has_led);
  make_func_symbol_func("pc-current-controller-has-rumble?",
                        (void*)pc_current_controller_has_rumble);
  make_func_symbol_func("pc-set-controller-led!", (void*)pc_set_controller_led);
  make_func_symbol_func("pc-waiting-for-bind?", (void*)pc_waiting_for_bind);
  make_func_symbol_func("pc-set-waiting-for-bind!", (void*)pc_set_waiting_for_bind);
  make_func_symbol_func("pc-stop-waiting-for-bind!", (void*)pc_stop_waiting_for_bind);
  make_func_symbol_func("pc-reset-bindings-to-defaults!", (void*)pc_reset_bindings_to_defaults);
  make_func_symbol_func("pc-set-auto-hide-cursor!", (void*)pc_set_auto_hide_cursor);
  make_func_symbol_func("pc-get-pressure-sensitivity-enabled?",
                        (void*)pc_get_pressure_sensitivity_enabled);
  make_func_symbol_func("pc-set-pressure-sensitivity-enabled!",
                        (void*)pc_set_pressure_sensitivity_enabled);
  make_func_symbol_func("pc-set-axis-scale!", (void*)pc_set_axis_scale);
  make_func_symbol_func("pc-get-axis-scale", (void*)pc_get_axis_scale);
  make_func_symbol_func("pc-current-controller-has-pressure-sensitivity?",
                        (void*)pc_current_controller_has_pressure_sensitivity);
  make_func_symbol_func("pc-current-controller-has-trigger-effect-support?",
                        (void*)pc_current_controller_has_trigger_effect_support);
  make_func_symbol_func("pc-get-trigger-effects-enabled?", (void*)pc_get_trigger_effects_enabled);
  make_func_symbol_func("pc-set-trigger-effects-enabled!", (void*)pc_set_trigger_effects_enabled);
  make_func_symbol_func("pc-clear-trigger-effect!", (void*)pc_clear_trigger_effect);
  make_func_symbol_func("pc-send-trigger-effect-feedback!", (void*)pc_send_trigger_effect_feedback);
  make_func_symbol_func("pc-send-trigger-effect-vibrate!", (void*)pc_send_trigger_effect_vibrate);
  make_func_symbol_func("pc-send-trigger-effect-weapon!", (void*)pc_send_trigger_effect_weapon);
  make_func_symbol_func("pc-send-trigger-rumble!", (void*)pc_send_trigger_rumble);

  // graphics things
  make_func_symbol_func("pc-set-vsync", (void*)pc_set_vsync);
  make_func_symbol_func("pc-set-msaa", (void*)pc_set_msaa);
  make_func_symbol_func("pc-set-frame-rate", (void*)pc_set_frame_rate);
  make_func_symbol_func("pc-set-game-resolution", (void*)pc_set_game_resolution);
  make_func_symbol_func("pc-set-letterbox", (void*)pc_set_letterbox);
  make_func_symbol_func("pc-renderer-tree-set-lod", (void*)pc_renderer_tree_set_lod);
  make_func_symbol_func("pc-set-collision-mode", (void*)Gfx::CollisionRendererSetMode);
  make_func_symbol_func("pc-set-collision-mask", (void*)pc_set_collision_mask);
  make_func_symbol_func("pc-get-collision-mask", (void*)pc_get_collision_mask);
  make_func_symbol_func("pc-set-collision-wireframe", (void*)pc_set_collision_wireframe);
  make_func_symbol_func("pc-set-collision", (void*)pc_set_collision);
  make_func_symbol_func("pc-set-gfx-hack", (void*)pc_set_gfx_hack);

  // -- OTHER --
  // Return the current OS as a symbol. Actually returns what it was compiled for!
  make_func_symbol_func("pc-get-os", (void*)pc_get_os);
  make_func_symbol_func("pc-get-unix-timestamp", (void*)pc_get_unix_timestamp);
  make_func_symbol_func("pc-treat-pad0-as-pad1", (void*)pc_treat_pad0_as_pad1);
  make_func_symbol_func("pc-is-imgui-visible?", (void*)pc_is_imgui_visible);

  // file related functions
  make_func_symbol_func("pc-filepath-exists?", (void*)pc_filepath_exists);
  make_func_symbol_func("pc-mkdir-file-path", (void*)pc_mkdir_filepath);

  // Play sound file
  make_func_symbol_func("play-sound-file", (void*)playMP3);

  // Stop sound file (all instances)
  make_func_symbol_func("stop-sound-file", (void*)stopMP3);

  // Stop all sounds
  make_func_symbol_func("stop-all-sounds", (void*)stopAllSounds);

  // Main music stuff
  make_func_symbol_func("play-main-music", (void*)playMainMusic);
  make_func_symbol_func("pause-main-music", (void*)pauseMainMusic);
  make_func_symbol_func("stop-main-music", (void*)stopMainMusic);
  make_func_symbol_func("resume-main-music", (void*)resumeMainMusic);

  make_func_symbol_func("main-music-volume", (void*)changeMainMusicVolume);

  // TFL note: added
  make_func_symbol_func("play-tfl-hint", (void*)play_tfl_hint);
  make_func_symbol_func("play-tfl-music", (void*)play_tfl_music);

  // discord rich presence
  make_func_symbol_func("pc-discord-rpc-set", (void*)set_discord_rpc);

  // profiler
  make_func_symbol_func("pc-prof", (void*)pc_prof);

  // RNG
  make_func_symbol_func("pc-rand", (void*)pc_rand);

  // text
  make_func_symbol_func("pc-encode-utf8-string", (void*)pc_encode_utf8_string);

  // debugging tools
  make_func_symbol_func("pc-filter-debug-string?", (void*)pc_filter_debug_string);
  make_func_symbol_func("pc-screen-shot", (void*)pc_screen_shot);
  make_func_symbol_func("pc-register-screen-shot-settings",
                        (void*)pc_register_screen_shot_settings);
}
