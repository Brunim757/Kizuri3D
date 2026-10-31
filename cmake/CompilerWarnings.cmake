function(caligo_configure_target target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4
      /WX
      /permissive-
      /Zc:__cplusplus
      /Zc:preprocessor
      /utf-8
      /EHsc
      /MP
    )
  endif()

  target_compile_definitions(${target} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    UNICODE
    _UNICODE
  )

  target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:CALIGO_DEBUG>)
endfunction()
