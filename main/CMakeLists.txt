set(3RDPARTY_DIR "${PROJECT_DIR}/3rdparty")

set(U8G2_DIR "${3RDPARTY_DIR}/u8g2")
set(U8G2_SRC
    "${U8G2_DIR}/csrc/u8x8_setup.c"
    "${U8G2_DIR}/csrc/u8x8_8x8.c"
    "${U8G2_DIR}/csrc/u8x8_display.c"
    "${U8G2_DIR}/csrc/u8x8_cad.c"
    "${U8G2_DIR}/csrc/u8x8_byte.c"
    "${U8G2_DIR}/csrc/u8x8_gpio.c"
    "${U8G2_DIR}/csrc/u8x8_d_ssd1306_128x64_noname.c"
    "${U8G2_DIR}/csrc/u8g2_setup.c"
    "${U8G2_DIR}/csrc/u8g2_cleardisplay.c"
    "${U8G2_DIR}/csrc/u8g2_buffer.c"
    "${U8G2_DIR}/csrc/u8g2_fonts.c"
    "${U8G2_DIR}/csrc/u8g2_font.c"
    "${U8G2_DIR}/csrc/u8g2_hvline.c"
    "${U8G2_DIR}/csrc/u8g2_ll_hvline.c"
    "${U8G2_DIR}/csrc/u8g2_intersection.c"
    "${U8G2_DIR}/csrc/u8g2_d_setup.c"
    "${U8G2_DIR}/csrc/u8g2_d_memory.c"
    "${U8G2_DIR}/csrc/u8g2_bitmap.c"
    "${U8G2_DIR}/cppsrc/U8x8lib.cpp")
set(U8G2_INC "${U8G2_DIR}/cppsrc" "${U8G2_DIR}/csrc")

set(KWS_DIR "${PROJECT_DIR}/main/kws")
set(KWS_SRC
    "${KWS_DIR}/kws_task.cpp"
    "${KWS_DIR}/vad_task.cpp"
    "${KWS_DIR}/kws_event_task.cpp"
    )
set(KWS_INC "${KWS_DIR}/")

set(WAV_PLAYER_DIR "${PROJECT_DIR}/main/VoiceMsgPlayer")
set(WAV_PLAYER_SRC
    "${WAV_PLAYER_DIR}/I2sTx.cpp"
    "${WAV_PLAYER_DIR}/VoiceMsgPlayer.cpp" "${WAV_PLAYER_DIR}/WavPlayer.cpp"
    )
set(WAV_PLAYER_INC "${WAV_PLAYER_DIR}/")

if(${CONFIG_APP_VOICE_RELAY})
  set(VOICE_RELAY_SRC "voice_relay/VoiceRelay.cpp" "voice_relay/model.cpp")
  set(VOICE_RELAY_INC "voice_relay")

  add_compile_definitions(VOICE_RELAY_INFERENCE_THRESHOLD=0.8)
  add_compile_definitions(CONFIG_MIC_GAIN=30)
  add_compile_definitions(SUSPEND_TIMEOUT_S=10)

  set(APP_SCENARIO_SRC
    ${KWS_SRC}
    ${VOICE_RELAY_SRC}
    )
  set(APP_SCENARIO_INC
    ${KWS_INC}
    ${VOICE_RELAY_INC}
    )
elseif(${CONFIG_APP_SOUND_EVENTS_DETECTION})
  set(SED_SRC
      "sed/SED.cpp" "sed/sed_task.cpp" "sed/baby_cry_model.cpp"
      "sed/glass_breaking_model.cpp" "sed/bark_model.cpp"
      "sed/coughing_model.cpp")
  set(SED_INC "sed")

  add_compile_definitions(SED_INFERENCE_THRESHOLD=0.9)

  set(APP_SCENARIO_SRC ${SED_SRC})
  set(APP_SCENARIO_INC ${SED_INC})
elseif(${CONFIG_APP_AI_TEACHER})
  set(AI_TEACHER_DIR "${PROJECT_DIR}/main/ai_teacher")
  set(ENG_DIR "${AI_TEACHER_DIR}/eng")

  set(LANG_MODEL_SRC
      "${ENG_DIR}/numbers_model.cpp"
      "${ENG_DIR}/objects_model.cpp"
      "${ENG_DIR}/objects_table.cpp")
  set(LANG_MODEL_INC "${ENG_DIR}/")

  set(LANG_REF_SAMPLES_SRC
      "${ENG_DIR}/ref_numbers_samples.cpp"
      "${ENG_DIR}/ref_objects_samples.cpp"
      "${ENG_DIR}/voice_msg_samples.cpp"
      )

  add_compile_definitions(NUMBERS_INFERENCE_THRESHOLD=0.7)
  add_compile_definitions(OBJECTS_INFERENCE_THRESHOLD=0.7)
  add_compile_definitions(CONFIG_MIC_GAIN=30)

  set(APP_SCENARIO_SRC
    ${KWS_SRC}
    ${WAV_PLAYER_SRC}
    ${LANG_MODEL_SRC}
    ${LANG_REF_SAMPLES_SRC}
    "${AI_TEACHER_DIR}/ObjectsRecognition.cpp"
    "${AI_TEACHER_DIR}/bitmaps.cpp"
    "${AI_TEACHER_DIR}/other_samples.cpp"
    "${AI_TEACHER_DIR}/sound_objects_samples.cpp"
    )
  set(APP_SCENARIO_INC
    ${KWS_INC}
    ${WAV_PLAYER_INC}
    ${ENG_DIR}
    ${AI_TEACHER_DIR}
    )
endif()

idf_component_register(
  SRCS
  "main.cpp"
  "./App/App.cpp"
  "./App/Event.cpp"
  "./App/Status.cpp"
  "./Hardware/Led.cpp"
  "./Hardware/Lcd.cpp"
  "./Hardware/I2C.cpp"
  "./Hardware/Button.cpp"
  ${U8G2_SRC}
  ${APP_SCENARIO_SRC}
  INCLUDE_DIRS
  "./"
  "./App/include"
  "./Hardware"
  ${U8G2_INC}
  ${APP_SCENARIO_INC}
  REQUIRES
  "esp-dsp"
  "esp-sr"
  "mic_reader"
  "nn_model"
  "led_strip"
  "bootloader_support"
  "esp_timer"
  "driver"
  )

target_compile_options(
  ${COMPONENT_LIB}
  PRIVATE -Wno-error=unused-const-variable -Wno-error=delete-non-virtual-dtor
          -Wno-error=implicit-function-declaration -fpermissive)
add_compile_definitions(U8X8_USE_PINS)

