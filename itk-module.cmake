set(DOCUMENTATION "This module contains a class to perform automatic skull-stripping for neuroimage analysis.
It is based on the ITK level-set and registration frameworks." )

itk_module( SkullStrip
  DEPENDS
    ITKLevelSets
    ITKRegistrationCommon
    ITKIOImageBase
    ITKImageIntensity
    ITKIOMeta
# Image IO Modules
  ITKIOJPEG
  ITKIOGDCM
  ITKIOBMP
  ITKIOLSM
  ITKIOPNG
  ITKIOTIFF
  ITKIOVTK
  ITKIOStimulate
  ITKIOBioRad
  ITKIOMeta
  ITKIONIFTI
  ITKIONRRD
  ITKIOGIPL
# Transform IO Modules
  ITKIOTransformMatlab
  ITKIOTransformHDF5
  ITKIOTransformInsightLegacy
  TEST_DEPENDS
    ITKTestKernel
  EXCLUDE_FROM_ALL
  DESCRIPTION
    "${DOCUMENTATION}"
)
