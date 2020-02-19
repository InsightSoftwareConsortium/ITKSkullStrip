/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkMaskImageFilter.h"
#include "itkStripTsImageFilter.h"
#include "itkTestingMacros.h"

#include <ctime>


int
itkStripTsImageFilterTest(int argc, char * argv[])
{
  if (argc < 6)
  {
    std::cerr << "Missing parameters." << std::endl;
    std::cerr << "Usage: " << argv[0] << " patientImageFile atlasImageFile atlasMaskFile"
              << " outputMask outputPatientMask" << std::endl;
    return EXIT_FAILURE;
  }

  double startTime = time(0);


  std::string patientImageFilename = argv[1];
  std::string atlasImageFilename = argv[2];
  std::string atlasMaskFilename = argv[3];

  using ImageType = itk::Image<int, 3>;
  using AtlasImageType = itk::Image<short, 3>;
  using AtlasLabelType = itk::Image<unsigned char, 3>;


  // Read input images
  using ReaderType = itk::ImageFileReader<ImageType>;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(patientImageFilename);

  TRY_EXPECT_NO_EXCEPTION(reader->Update());


  using AtlasReaderType = itk::ImageFileReader<AtlasImageType>;
  AtlasReaderType::Pointer atlasReader = AtlasReaderType::New();
  atlasReader->SetFileName(atlasImageFilename);

  TRY_EXPECT_NO_EXCEPTION(atlasReader->Update());


  using LabelReaderType = itk::ImageFileReader<AtlasLabelType>;
  LabelReaderType::Pointer labelReader = LabelReaderType::New();
  labelReader->SetFileName(atlasMaskFilename);

  TRY_EXPECT_NO_EXCEPTION(labelReader->Update());


  // Perform skull-stripping using stripTsImageFilter
  std::cout << std::endl << "Performing skull-stripping" << std::endl;

  // Set up skull-stripping filter
  using StripTsFilterType = itk::StripTsImageFilter<ImageType, AtlasImageType, AtlasLabelType>;
  StripTsFilterType::Pointer stripTsFilter = StripTsFilterType::New();

  EXERCISE_BASIC_OBJECT_METHODS(stripTsFilter, StripTsImageFilter, ImageToImageFilter);

  // Set the required inputs for the stripTsImageFilter
  stripTsFilter->SetInput(reader->GetOutput());
  stripTsFilter->SetAtlasImage(atlasReader->GetOutput());
  stripTsFilter->SetAtlasBrainMask(labelReader->GetOutput());

  TRY_EXPECT_NO_EXCEPTION(stripTsFilter->Update());


  // Mask the patient image using the output generated from the stripTsImageFilter as mask
  using MaskFilterType = itk::MaskImageFilter<ImageType, AtlasLabelType, ImageType>;
  MaskFilterType::Pointer maskFilter = MaskFilterType::New();

  maskFilter->SetInput1(reader->GetOutput());
  maskFilter->SetInput2(stripTsFilter->GetOutput());

  TRY_EXPECT_NO_EXCEPTION(maskFilter->Update());


  // Write mask and masked patient image
  using MaskWriterType = itk::ImageFileWriter<AtlasLabelType>;
  MaskWriterType::Pointer maskWriter = MaskWriterType::New();
  maskWriter->SetInput(stripTsFilter->GetOutput());
  maskWriter->SetFileName(argv[4]);

  TRY_EXPECT_NO_EXCEPTION(maskWriter->Update());


  using ImageWriterType = itk::ImageFileWriter<ImageType>;
  ImageWriterType::Pointer imageWriter = ImageWriterType::New();
  imageWriter->SetInput(maskFilter->GetOutput());
  imageWriter->SetFileName(argv[5]);

  TRY_EXPECT_NO_EXCEPTION(imageWriter->Update());


  double endTime = time(0);
  std::cout << "Total computation time: " << endTime - startTime << " seconds" << std::endl;
  std::cout << stripTsFilter->GetTimerReport();


  std::cout << "Test finished." << std::endl;
  return EXIT_SUCCESS;
}
