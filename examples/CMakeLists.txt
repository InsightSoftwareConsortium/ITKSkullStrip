cmake_minimum_required(VERSION 3.9.5)
project(SkullStripExamples)

find_package(ITK REQUIRED
  COMPONENTS ITKImageIntensity ITKImageIO SkullStrip
  )
include(${ITK_USE_FILE})

add_executable(SkullStripping MACOSX_BUNDLE SkullStripping.cxx )
target_link_libraries(SkullStripping ${ITK_LIBRARIES})
