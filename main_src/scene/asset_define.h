#ifndef ASSET_DEFINE_H
#define ASSET_DEFINE_H

#include <string>

// PROSOPHOR_SOURCE_DIR comes from CMake compile definition (-DPROSOPHOR_SOURCE_DIR=...)
// Convert to string constant for runtime use
#ifndef PROSOPHOR_SOURCE_DIR
#define PROSOPHOR_SOURCE_DIR "."
#endif

inline std::string AssetBase() { return std::string(PROSOPHOR_SOURCE_DIR) + "/assets/"; }

inline std::string PortraitDir() { return AssetBase() + "characters/portraits/"; }
inline std::string BackwallDir() { return AssetBase() + "backwalls/"; }
inline std::string ImageDir()   { return AssetBase() + "image/"; }
inline std::string SoundDir()   { return AssetBase() + "sound/"; }
inline std::string MusicDir()   { return AssetBase() + "music/"; }
inline std::string FontDir()    { return AssetBase() + "font/"; }
inline std::string EffectDir()  { return AssetBase() + "effect/"; }

#endif
