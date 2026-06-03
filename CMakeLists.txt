target_include_directories(app PRIVATE include)

if(CONFIG_SHIELD_PKKBLAB_LEFT)
  target_sources(app PRIVATE src/behaviors/behavior_cheonjiin.c)
endif()
