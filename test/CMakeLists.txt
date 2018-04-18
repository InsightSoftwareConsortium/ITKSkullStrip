itk_module_test()
set(SkullStripTests
SkullStrippingTest.cxx
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../examples)

CreateTestDriver(SkullStrip  "${SkullStrip-Test_LIBRARIES}" "${SkullStripTests}")

set(TEMP ${ITK_TEST_OUTPUT_DIR})
set(TEST_DATA_ROOT "${SkullStrip_SOURCE_DIR}/test")
itk_add_test( NAME SkullStrippingTest
  COMMAND SkullStripTestDriver
    --compare ${TEST_DATA_ROOT}/Baseline/outputMaskBaseline.mha
              ${TEMP}/outputMask.mha
    SkullStrippingTest
    ${TEST_DATA_ROOT}/Input/brainweb.mha
    ${TEST_DATA_ROOT}/Input/atlasImage.mha
    ${TEST_DATA_ROOT}/Input/atlasMask.mha
    ${TEMP}/outputMask.mha
    ${TEMP}/outputMaskedImage.mha
)
