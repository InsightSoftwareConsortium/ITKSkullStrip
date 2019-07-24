itk_module_test()

set(SkullStripTests
  itkStripTsImageFilterTest.cxx
  )

CreateTestDriver(SkullStrip "${SkullStrip-Test_LIBRARIES}" "${SkullStripTests}")

itk_add_test(NAME itkStripTsImageFilterTest
  COMMAND SkullStripTestDriver
  --compare DATA{Baseline/outputMaskBaseline.mha}
            ${ITK_TEST_OUTPUT_DIR}/outputMask.mha
  itkStripTsImageFilterTest
    DATA{Input/brainweb.mha}
    DATA{Input/atlasImage.mha}
    DATA{Input/atlasMask.mha}
    ${ITK_TEST_OUTPUT_DIR}/outputMask.mha
    ${ITK_TEST_OUTPUT_DIR}/outputMaskedImage.mha
  )
