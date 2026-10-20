add_library(KizuriAudio STATIC
  src/Audio.cpp
  src/AudioMixer.cpp
)
target_include_directories(KizuriAudio PUBLIC include)
target_link_libraries(KizuriAudio PUBLIC kizuri_miniaudio)
