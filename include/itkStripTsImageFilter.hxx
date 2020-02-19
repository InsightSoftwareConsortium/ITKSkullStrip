/*=========================================================================
 *
 *  Copyright NumFOCUS
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


#ifndef itkStripTsImageFilter_hxx
#define itkStripTsImageFilter_hxx

#include "itkStripTsImageFilter.h"
#include "itkLabelImageGaussianInterpolateImageFunction.h"

namespace itk
{

template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::StripTsImageFilter()
{
  // constructor
  m_PatientImage = ImageType::New();
  m_AtlasImage = AtlasImageType::New();
  m_AtlasLabels = AtlasLabelType::New();
  m_Progress = nullptr;
  m_TimerReport = "";
}

template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::~StripTsImageFilter() = default;


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::GenerateData()
{
  // do the processing

  m_Progress = ProgressAccumulator::New();
  m_Progress->SetMiniPipelineFilter(this);

  m_Timer.Start("1 DownsampleImage");
  this->DownsampleImage();
  m_Timer.Stop("1 DownsampleImage");

  m_Timer.Start("2 RescaleImages");
  this->RescaleImages();
  m_Timer.Stop("2 RescaleImages");

  m_Timer.Start("3 RigidRegistration");
  this->RigidRegistration();
  m_Timer.Stop("3 RigidRegistration");

  m_Timer.Start("4 AffineRegistration");
  this->AffineRegistration();
  m_Timer.Stop("4 AffineRegistration");

  m_Timer.Start("5 BinaryErosion");
  this->BinaryErosion();
  m_Timer.Stop("5 BinaryErosion");

  m_Timer.Start("6 LevelSet");
  this->MultiResLevelSet();
  m_Timer.Stop("6 LevelSet");

  m_Timer.Start("7 UpsampleLabels");
  this->UpsampleLabels();
  m_Timer.Stop("7 UpsampleLabels");

  std::ostringstream report;

  m_Timer.Report(report);
  m_TimerReport = report.str();

  this->GraftOutput(m_AtlasLabels);
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "end of PrintSelf." << std::endl;
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::SetAtlasImage(const TAtlasImageType * ptr)
{
  m_AtlasImage = const_cast<TAtlasImageType *>(ptr);
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::SetAtlasBrainMask(const TAtlasLabelType * ptr)
{
  m_AtlasLabels = const_cast<TAtlasLabelType *>(ptr);
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::DownsampleImage()
{
  // resample patient image to isotropic resolution

  // duplicate image
  using DuplicatorType = itk::ImageDuplicator<TImageType>;
  typename DuplicatorType::Pointer duplicator = DuplicatorType::New();
  duplicator->SetInputImage(this->GetInput());
  duplicator->Update();
  m_PatientImage = duplicator->GetOutput();
  m_PatientImage->DisconnectPipeline();

  // resample image
  using ResamplerType = itk::ResampleImageFilter<TImageType, TImageType>;
  typename ResamplerType::Pointer resampler = ResamplerType::New();

  using TransformType = itk::IdentityTransform<double, 3>;
  typename TransformType::Pointer transform = TransformType::New();

  using LinearInterpolatorType = itk::LinearInterpolateImageFunction<TImageType, double>;
  typename LinearInterpolatorType::Pointer lInterp = LinearInterpolatorType::New();

  transform->SetIdentity();

  resampler->SetTransform(transform);
  resampler->SetInput(m_PatientImage);

  typename TImageType::SpacingType spacing;
  typename TImageType::SizeType    size;
  spacing[0] = 1.0;
  spacing[1] = 1.0;
  spacing[2] = 1.0;
  size[0] = (m_PatientImage->GetLargestPossibleRegion().GetSize()[0]) * (m_PatientImage->GetSpacing()[0]) / spacing[0];
  size[1] = (m_PatientImage->GetLargestPossibleRegion().GetSize()[1]) * (m_PatientImage->GetSpacing()[1]) / spacing[1];
  size[2] = (m_PatientImage->GetLargestPossibleRegion().GetSize()[2]) * (m_PatientImage->GetSpacing()[2]) / spacing[2];

  resampler->SetInterpolator(lInterp);
  resampler->SetSize(size);
  resampler->SetOutputSpacing(spacing);
  resampler->SetOutputOrigin(m_PatientImage->GetOrigin());
  resampler->SetOutputDirection(m_PatientImage->GetDirection());
  resampler->SetDefaultPixelValue(0);

  try
  {
    m_Progress->RegisterInternalFilter(resampler, 0.004f);
    resampler->Update();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  m_PatientImage = resampler->GetOutput();
  m_PatientImage->DisconnectPipeline();
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::RescaleImages()
{
  // rescale patient image and atlas image intensities to 0-255

  using ImageRescalerType = itk::RescaleIntensityImageFilter<TImageType, TImageType>;
  typename ImageRescalerType::Pointer imageRescaler = ImageRescalerType::New();

  using AtlasRescalerType = itk::RescaleIntensityImageFilter<TAtlasImageType, TAtlasImageType>;
  typename AtlasRescalerType::Pointer atlasRescaler = AtlasRescalerType::New();

  imageRescaler->SetInput(m_PatientImage);
  imageRescaler->SetOutputMinimum(0);
  imageRescaler->SetOutputMaximum(255);

  atlasRescaler->SetInput(m_AtlasImage);
  atlasRescaler->SetOutputMinimum(0);
  atlasRescaler->SetOutputMaximum(255);

  try
  {
    m_Progress->RegisterInternalFilter(atlasRescaler, 0.001f);
    imageRescaler->Update();
    atlasRescaler->Update();
  }
  catch (itk::ExceptionObject & err)
  {
    std::cerr << "Exception caught" << std::endl;
    std::cerr << err << std::endl;
  }

  m_PatientImage = imageRescaler->GetOutput();
  m_PatientImage->DisconnectPipeline();

  m_AtlasImage = atlasRescaler->GetOutput();
  m_AtlasImage->DisconnectPipeline();
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::RigidRegistration()
{
  // perform intial rigid alignment of atlas with patient image

  //  std::cout << "Doing initial rigid mask alignment" << std::endl;

  using TransformType = itk::VersorRigid3DTransform<double>;
  using OptimizerType = itk::VersorRigid3DTransformOptimizer;
  using MetricType = itk::MattesMutualInformationImageToImageMetric<TImageType, TAtlasImageType>;
  using MultiResRegistrationType = itk::MultiResolutionImageRegistrationMethod<TImageType, TAtlasImageType>;
  using LinearInterpolatorType = itk::LinearInterpolateImageFunction<TAtlasImageType, double>;
  using NNInterpolatorType = itk::NearestNeighborInterpolateImageFunction<TAtlasLabelType, double>;
  //  using NNInterpolatorType = itk::LabelImageGaussianInterpolateImageFunction<TAtlasLabelType, double>;

  typename TransformType::Pointer            transform = TransformType::New();
  typename OptimizerType::Pointer            optimizer = OptimizerType::New();
  typename MetricType::Pointer               metric = MetricType::New();
  typename MultiResRegistrationType::Pointer registration = MultiResRegistrationType::New();
  typename LinearInterpolatorType::Pointer   linearInterpolator = LinearInterpolatorType::New();
  typename NNInterpolatorType::Pointer       nnInterpolator = NNInterpolatorType::New();

  metric->SetNumberOfHistogramBins(64);
  metric->SetNumberOfSpatialSamples(100000); // default number is too small

  registration->SetMetric(metric);
  registration->SetOptimizer(optimizer);
  registration->SetInterpolator(linearInterpolator);
  // registration->SetNumberOfLevels( 3 );

  // perform registration only on subsampled image for speed gains
  typename MultiResRegistrationType::ScheduleType schedule;
  schedule.SetSize(2, 3);
  schedule[0][0] = 4;
  schedule[0][1] = 4;
  schedule[0][2] = 4;
  schedule[1][0] = 2;
  schedule[1][1] = 2;
  schedule[1][2] = 2;
  registration->SetSchedules(schedule, schedule);

  registration->SetFixedImageRegion(m_PatientImage->GetBufferedRegion());

  registration->SetTransform(transform);

  registration->SetFixedImage(m_PatientImage);
  registration->SetMovingImage(m_AtlasImage);

  // transform initialization
  using TransformInitializerType = itk::CenteredTransformInitializer<TransformType, TImageType, TAtlasImageType>;
  typename TransformInitializerType::Pointer initializer = TransformInitializerType::New();

  initializer->SetTransform(transform);
  initializer->SetFixedImage(m_PatientImage);
  initializer->SetMovingImage(m_AtlasImage);

  initializer->GeometryOn(); // geometry initialization because of multimodality

  try
  {
    initializer->InitializeTransform();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "Exception caught ! " << std::endl;
    std::cerr << &exception << std::endl;
  }

  registration->SetInitialTransformParameters(transform->GetParameters());

  using OptimizerScalesType = OptimizerType::ScalesType;
  OptimizerScalesType optimizerScales(transform->GetNumberOfParameters());
  constexpr double    scale = 1.0;
  const double        translationScale = scale / 2500;

  optimizerScales[0] = scale;
  optimizerScales[1] = scale;
  optimizerScales[2] = scale;
  optimizerScales[3] = translationScale;
  optimizerScales[4] = translationScale;
  optimizerScales[5] = translationScale;

  optimizer->SetScales(optimizerScales);
  optimizer->SetMaximumStepLength(0.05);
  optimizer->SetMinimumStepLength(0.001);
  optimizer->SetNumberOfIterations(250);
  optimizer->MinimizeOn();

  try
  {
    m_Progress->RegisterInternalFilter(registration, 0.29f);
    registration->Update();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  typename OptimizerType::ParametersType finalParameters = registration->GetLastTransformParameters();

  transform->SetParameters(finalParameters);

  // resample atlas image
  using ResampleImageFilterType = itk::ResampleImageFilter<TAtlasImageType, TAtlasImageType>;
  typename ResampleImageFilterType::Pointer imageResampler = ResampleImageFilterType::New();

  typename TransformType::Pointer finalTransform = TransformType::New();
  finalTransform->SetCenter(transform->GetCenter());
  finalTransform->SetParameters(finalParameters);


  imageResampler->SetTransform(finalTransform);
  imageResampler->SetInterpolator(linearInterpolator);

  imageResampler->SetSize(m_PatientImage->GetLargestPossibleRegion().GetSize());
  imageResampler->SetOutputOrigin(m_PatientImage->GetOrigin());
  imageResampler->SetOutputSpacing(m_PatientImage->GetSpacing());
  imageResampler->SetOutputDirection(m_PatientImage->GetDirection());
  imageResampler->SetDefaultPixelValue(0);

  imageResampler->SetInput(m_AtlasImage);
  try
  {
    m_Progress->RegisterInternalFilter(imageResampler, 0.24f);
    imageResampler->Update();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  m_AtlasImage = imageResampler->GetOutput();
  m_AtlasImage->DisconnectPipeline();

  // resample atlas mask
  using ResampleLabelFilterType = itk::ResampleImageFilter<TAtlasLabelType, TAtlasLabelType>;
  typename ResampleLabelFilterType::Pointer labelResampler = ResampleLabelFilterType::New();

  labelResampler->SetTransform(finalTransform);
  labelResampler->SetInterpolator(nnInterpolator);

  labelResampler->SetSize(m_PatientImage->GetLargestPossibleRegion().GetSize());
  labelResampler->SetOutputOrigin(m_PatientImage->GetOrigin());
  labelResampler->SetOutputSpacing(m_PatientImage->GetSpacing());
  labelResampler->SetOutputDirection(m_PatientImage->GetDirection());
  labelResampler->SetDefaultPixelValue(0);

  labelResampler->SetInput(m_AtlasLabels);
  try
  {
    m_Progress->RegisterInternalFilter(labelResampler, 0.01f);
    labelResampler->Update();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  m_AtlasLabels = labelResampler->GetOutput();
  m_AtlasLabels->DisconnectPipeline();
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::AffineRegistration()
{
  // perform refined affine alignment of atlas with patient image

  //  std::cout << "Doing affine mask alignment" << std::endl;

  using TransformType = itk::AffineTransform<double, 3>;
  using OptimizerType = itk::RegularStepGradientDescentOptimizer;
  using MetricType = itk::MattesMutualInformationImageToImageMetric<TImageType, TAtlasImageType>;
  using MultiResRegistrationType = itk::MultiResolutionImageRegistrationMethod<TImageType, TAtlasImageType>;
  using LinearInterpolatorType = itk::LinearInterpolateImageFunction<TAtlasImageType, double>;
  using NNInterpolatorType = itk::NearestNeighborInterpolateImageFunction<TAtlasLabelType, double>;

  typename TransformType::Pointer            transform = TransformType::New();
  typename OptimizerType::Pointer            optimizer = OptimizerType::New();
  typename MetricType::Pointer               metric = MetricType::New();
  typename MultiResRegistrationType::Pointer registration = MultiResRegistrationType::New();
  typename LinearInterpolatorType::Pointer   linearInterpolator = LinearInterpolatorType::New();
  typename NNInterpolatorType::Pointer       nnInterpolator = NNInterpolatorType::New();

  metric->SetNumberOfHistogramBins(64);
  metric->SetNumberOfSpatialSamples(100000); // default number is too small

  registration->SetMetric(metric);
  registration->SetOptimizer(optimizer);
  registration->SetInterpolator(linearInterpolator);
  // registration->SetNumberOfLevels( 3 );

  // perform registration only on subsampled image for speed gains
  typename MultiResRegistrationType::ScheduleType schedule;
  schedule.SetSize(2, 3);
  schedule[0][0] = 4;
  schedule[0][1] = 4;
  schedule[0][2] = 4;
  schedule[1][0] = 2;
  schedule[1][1] = 2;
  schedule[1][2] = 2;
  registration->SetSchedules(schedule, schedule);

  registration->SetFixedImageRegion(m_PatientImage->GetBufferedRegion());

  registration->SetTransform(transform);

  registration->SetFixedImage(m_PatientImage);
  registration->SetMovingImage(m_AtlasImage);

  transform->SetIdentity();
  registration->SetInitialTransformParameters(transform->GetParameters());

  using OptimizerScalesType = OptimizerType::ScalesType;
  OptimizerScalesType optimizerScales(transform->GetNumberOfParameters());
  constexpr double    matrixScale = 1.0;
  const double        translationScale = matrixScale / 200;

  optimizerScales[0] = matrixScale;
  optimizerScales[1] = matrixScale;
  optimizerScales[2] = matrixScale;
  optimizerScales[3] = matrixScale;
  optimizerScales[4] = matrixScale;
  optimizerScales[5] = matrixScale;
  optimizerScales[6] = matrixScale;
  optimizerScales[7] = matrixScale;
  optimizerScales[8] = matrixScale;
  optimizerScales[9] = translationScale;
  optimizerScales[10] = translationScale;
  optimizerScales[11] = translationScale;

  optimizer->SetScales(optimizerScales);
  optimizer->SetMaximumStepLength(0.05);
  optimizer->SetMinimumStepLength(0.001);
  optimizer->SetNumberOfIterations(200);
  optimizer->MinimizeOn();

  try
  {
    m_Progress->RegisterInternalFilter(registration, 0.24f);
    registration->Update();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  typename OptimizerType::ParametersType finalParameters = registration->GetLastTransformParameters();

  transform->SetParameters(finalParameters);

  // resample atlas image
  using ResampleImageFilterType = itk::ResampleImageFilter<TAtlasImageType, TAtlasImageType>;
  typename ResampleImageFilterType::Pointer imageResampler = ResampleImageFilterType::New();

  typename TransformType::Pointer finalTransform = TransformType::New();
  finalTransform->SetCenter(transform->GetCenter());
  finalTransform->SetParameters(finalParameters);


  imageResampler->SetTransform(finalTransform);
  imageResampler->SetInterpolator(linearInterpolator);

  imageResampler->SetSize(m_PatientImage->GetLargestPossibleRegion().GetSize());
  imageResampler->SetOutputOrigin(m_PatientImage->GetOrigin());
  imageResampler->SetOutputSpacing(m_PatientImage->GetSpacing());
  imageResampler->SetOutputDirection(m_PatientImage->GetDirection());
  imageResampler->SetDefaultPixelValue(0);

  imageResampler->SetInput(m_AtlasImage);
  try
  {
    m_Progress->RegisterInternalFilter(imageResampler, 0.01f);
    imageResampler->Update();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  m_AtlasImage = imageResampler->GetOutput();
  m_AtlasImage->DisconnectPipeline();

  // resample atlas mask
  using ResampleLabelFilterType = itk::ResampleImageFilter<TAtlasLabelType, TAtlasLabelType>;
  typename ResampleLabelFilterType::Pointer labelResampler = ResampleLabelFilterType::New();

  labelResampler->SetTransform(finalTransform);
  labelResampler->SetInterpolator(nnInterpolator);

  labelResampler->SetSize(m_PatientImage->GetLargestPossibleRegion().GetSize());
  labelResampler->SetOutputOrigin(m_PatientImage->GetOrigin());
  labelResampler->SetOutputSpacing(m_PatientImage->GetSpacing());
  labelResampler->SetOutputDirection(m_PatientImage->GetDirection());
  labelResampler->SetDefaultPixelValue(0);

  labelResampler->SetInput(m_AtlasLabels);
  try
  {
    m_Progress->RegisterInternalFilter(labelResampler, 0.01f);
    labelResampler->Update();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  m_AtlasLabels = labelResampler->GetOutput();
  m_AtlasLabels->DisconnectPipeline();
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::BinaryErosion()
{
  //  std::cout << "Eroding aligned mask" << std::endl;

  // make sure mask is binary
  itk::ImageRegionIterator<AtlasLabelType> iterLabel(m_AtlasLabels, m_AtlasLabels->GetLargestPossibleRegion());

  for (iterLabel.GoToBegin(); !iterLabel.IsAtEnd(); ++iterLabel)
  {
    if (iterLabel.Get() != 0)
    {
      iterLabel.Set(1);
    }
  }

  // erode binary mask
  using StructuringElementType = itk::BinaryBallStructuringElement<typename AtlasLabelType::PixelType, 3>;
  using ErodeFilterType = itk::BinaryErodeImageFilter<AtlasLabelType, AtlasLabelType, StructuringElementType>;
  StructuringElementType            structuringElement;
  typename ErodeFilterType::Pointer eroder = ErodeFilterType::New();

  structuringElement.SetRadius(3);
  structuringElement.SetRadius(3);
  structuringElement.CreateStructuringElement();

  eroder->SetKernel(structuringElement);
  eroder->SetInput(m_AtlasLabels);
  eroder->SetErodeValue(1);
  eroder->SetBackgroundValue(0);

  try
  {
    m_Progress->RegisterInternalFilter(eroder, 0.02f);
    eroder->Update();
  }
  catch (itk::ExceptionObject & err)
  {
    std::cerr << "ExceptionObject caught while dilating mask!" << std::endl;
    std::cerr << err << std::endl;
  }

  m_AtlasLabels = eroder->GetOutput();
  m_AtlasLabels->DisconnectPipeline();
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::MultiResLevelSet()
{
  // level set refinement of brain mask in two resolution levels

  //  std::cout << "Level set refinement of brain mask" << std::endl;

  // coarse (2mm isotropic resolution)
  //  std::cout << "...coarse" << std::endl;
  PyramidFilter(2);
  LevelSetRefinement(2);

  // fine (1mm isotropic resolution)
  //  std::cout << "...fine" << std::endl;
  PyramidFilter(1);
  LevelSetRefinement(1);
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::PyramidFilter(int isoSpacing)
{
  // resample to isoSpacing before applying level set

  // resample patient image
  using ImageResamplerType = itk::ResampleImageFilter<ImageType, ImageType>;
  typename ImageResamplerType::Pointer imageResampler = ImageResamplerType::New();

  using TransformType = itk::IdentityTransform<double, 3>;
  typename TransformType::Pointer transform = TransformType::New();

  using LinearInterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;
  using NNInterpolatorType = itk::NearestNeighborInterpolateImageFunction<AtlasLabelType, double>;
  typename LinearInterpolatorType::Pointer linearInterpolator = LinearInterpolatorType::New();
  typename NNInterpolatorType::Pointer     nnInterpolator = NNInterpolatorType::New();

  transform->SetIdentity();

  imageResampler->SetTransform(transform);
  imageResampler->SetInput(this->GetInput());

  typename ImageType::SpacingType imageSpacing;
  typename ImageType::SizeType    imageSize;
  imageSpacing[0] = isoSpacing;
  imageSpacing[1] = isoSpacing;
  imageSpacing[2] = isoSpacing;
  imageSize[0] =
    (this->GetInput()->GetLargestPossibleRegion().GetSize()[0]) * (this->GetInput()->GetSpacing()[0]) / imageSpacing[0];
  imageSize[1] =
    (this->GetInput()->GetLargestPossibleRegion().GetSize()[1]) * (this->GetInput()->GetSpacing()[1]) / imageSpacing[1];
  imageSize[2] =
    (this->GetInput()->GetLargestPossibleRegion().GetSize()[2]) * (this->GetInput()->GetSpacing()[2]) / imageSpacing[2];

  imageResampler->SetInterpolator(linearInterpolator);
  imageResampler->SetSize(imageSize);
  imageResampler->SetOutputSpacing(imageSpacing);
  imageResampler->SetOutputOrigin(this->GetInput()->GetOrigin());
  imageResampler->SetOutputDirection(this->GetInput()->GetDirection());
  imageResampler->SetDefaultPixelValue(0);

  try
  {
    m_Progress->RegisterInternalFilter(imageResampler, 0.01f);
    m_Timer.Start("6a) Image Resampler");
    imageResampler->Update();
    m_Timer.Stop("6a) Image Resampler");
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  m_PatientImage = imageResampler->GetOutput();
  m_PatientImage->DisconnectPipeline();


  // resample mask
  using LabelResamplerType = itk::ResampleImageFilter<AtlasLabelType, AtlasLabelType>;
  typename LabelResamplerType::Pointer labelResampler = LabelResamplerType::New();

  labelResampler->SetTransform(transform);
  labelResampler->SetInput(m_AtlasLabels);

  typename AtlasLabelType::SpacingType labelSpacing;
  typename AtlasLabelType::SizeType    labelSize;
  labelSpacing[0] = isoSpacing;
  labelSpacing[1] = isoSpacing;
  labelSpacing[2] = isoSpacing;
  labelSize[0] =
    (this->GetInput()->GetLargestPossibleRegion().GetSize()[0]) * (this->GetInput()->GetSpacing()[0]) / labelSpacing[0];
  labelSize[1] =
    (this->GetInput()->GetLargestPossibleRegion().GetSize()[1]) * (this->GetInput()->GetSpacing()[1]) / labelSpacing[1];
  labelSize[2] =
    (this->GetInput()->GetLargestPossibleRegion().GetSize()[2]) * (this->GetInput()->GetSpacing()[2]) / labelSpacing[2];

  labelResampler->SetInterpolator(nnInterpolator);
  labelResampler->SetSize(labelSize);
  labelResampler->SetOutputSpacing(labelSpacing);
  labelResampler->SetOutputOrigin(this->GetInput()->GetOrigin());
  labelResampler->SetOutputDirection(this->GetInput()->GetDirection());
  labelResampler->SetDefaultPixelValue(0);

  try
  {
    m_Progress->RegisterInternalFilter(labelResampler, 0.01f);
    m_Timer.Start("6b) Label Resampler");
    labelResampler->Update();
    m_Timer.Stop("6b) Label Resampler");
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  m_AtlasLabels = labelResampler->GetOutput();
  m_AtlasLabels->DisconnectPipeline();
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::LevelSetRefinement(int isoSpacing)
{
  // refine brain mask using geodesic active contour level set evolution

  // have to cast images to float first for level-set
  using FloatImageType = itk::Image<float, 3>;

  using ImageCasterType = itk::CastImageFilter<ImageType, FloatImageType>;
  typename ImageCasterType::Pointer imageCaster = ImageCasterType::New();

  using LabelCasterType = itk::CastImageFilter<AtlasLabelType, FloatImageType>;
  typename LabelCasterType::Pointer labelCaster = LabelCasterType::New();

  imageCaster->SetInput(m_PatientImage);
  labelCaster->SetInput(m_AtlasLabels);
  try
  {
    m_Timer.Start("6c) Image Caster");
    imageCaster->Update();
    m_Timer.Stop("6c) Image Caster");
    m_Timer.Start("6d) Label Caster");
    labelCaster->Update();
    m_Timer.Stop("6d) Label Caster");
  }
  catch (itk::ExceptionObject & excep)
  {
    std::cerr << "Exception caught while doing GeodesicActiveContours !" << std::endl;
    std::cerr << excep << std::endl;
  }

  // Geodesic Active Contour level set settings
  using SmoothingFilterType = itk::GradientAnisotropicDiffusionImageFilter<FloatImageType, FloatImageType>;
  using GradientMagFilterType = itk::GradientMagnitudeRecursiveGaussianImageFilter<FloatImageType, FloatImageType>;
  using RescalerType = itk::RescaleIntensityImageFilter<FloatImageType, FloatImageType>;
  using SigmoidFilterType = itk::SigmoidImageFilter<FloatImageType, FloatImageType>;
  using GeodesicActiveContourFilterType = itk::GeodesicActiveContourLevelSetImageFilter<FloatImageType, FloatImageType>;
  typename SmoothingFilterType::Pointer             smoothingFilter = SmoothingFilterType::New();
  typename GradientMagFilterType::Pointer           gradientMagnitude = GradientMagFilterType::New();
  typename RescalerType::Pointer                    rescaler = RescalerType::New();
  typename SigmoidFilterType::Pointer               sigmoid = SigmoidFilterType::New();
  typename GeodesicActiveContourFilterType::Pointer geodesicActiveContour = GeodesicActiveContourFilterType::New();

  smoothingFilter->SetTimeStep(0.0625);
  smoothingFilter->SetNumberOfIterations(5);
  smoothingFilter->SetConductanceParameter(2.0);

  gradientMagnitude->SetSigma(1.0);

  rescaler->SetOutputMinimum(0);
  rescaler->SetOutputMaximum(255);

  sigmoid->SetOutputMinimum(0.0);
  sigmoid->SetOutputMaximum(1.0);


  geodesicActiveContour->SetIsoSurfaceValue(0.5);
  geodesicActiveContour->SetUseImageSpacing(true);

  // set parameters depending on coarse or fine isotropic resolution
  if (isoSpacing == 2)
  {
    sigmoid->SetAlpha(-2.0);
    sigmoid->SetBeta(12.0);

    geodesicActiveContour->SetMaximumRMSError(0.01);
    geodesicActiveContour->SetPropagationScaling(-2.0);
    geodesicActiveContour->SetCurvatureScaling(10.0);
    geodesicActiveContour->SetAdvectionScaling(2.0);
    geodesicActiveContour->SetNumberOfIterations(100);
  }
  if (isoSpacing == 1)
  {
    sigmoid->SetAlpha(-2.0);
    sigmoid->SetBeta(12.0);

    geodesicActiveContour->SetMaximumRMSError(0.001);
    geodesicActiveContour->SetPropagationScaling(-1.0);
    geodesicActiveContour->SetCurvatureScaling(20.0);
    geodesicActiveContour->SetAdvectionScaling(5.0);
    geodesicActiveContour->SetNumberOfIterations(120);
  }

  m_Timer.Start("6e) Gradient AD");
  smoothingFilter->SetInput(imageCaster->GetOutput());
  smoothingFilter->Update();
  m_Timer.Stop("6e) Gradient AD");

  m_Timer.Start("6f) Gradient Mag");
  gradientMagnitude->SetInput(smoothingFilter->GetOutput());
  gradientMagnitude->Update();
  m_Timer.Stop("6f) Gradient Mag");

  m_Timer.Start("6g) Rescaler");
  rescaler->SetInput(gradientMagnitude->GetOutput());
  rescaler->Update();
  m_Timer.Stop("6g) Rescaler");

  m_Timer.Start("6h) Sigmoid");
  sigmoid->SetInput(rescaler->GetOutput());
  sigmoid->Update();
  m_Timer.Stop("6h) Sigmoid");

  m_Timer.Start("6i) Geodesic");
  geodesicActiveContour->SetInput(labelCaster->GetOutput());
  geodesicActiveContour->SetFeatureImage(sigmoid->GetOutput());
  geodesicActiveContour->Update();
  m_Timer.Stop("6i) Geodesic");

  // threshold level set output
  using ThresholdFilterType = itk::BinaryThresholdImageFilter<FloatImageType, FloatImageType>;
  typename ThresholdFilterType::Pointer thresholder = ThresholdFilterType::New();

  thresholder->SetUpperThreshold(0.0);
  thresholder->SetLowerThreshold(-1000.0);
  thresholder->SetOutsideValue(1);
  thresholder->SetInsideValue(0);

  thresholder->SetInput(geodesicActiveContour->GetOutput());
  try
  {
    m_Progress->RegisterInternalFilter(thresholder, 0.01f);
    m_Timer.Start("6j) Thresholder");
    thresholder->Update();
    m_Timer.Stop("6j) Thresholder");
  }
  catch (itk::ExceptionObject & excep)
  {
    std::cerr << "Exception caught while doing GeodesicActiveContours !" << std::endl;
    std::cerr << excep << std::endl;
  }


  // cast back mask from float to char
  using LabelReCasterType = itk::CastImageFilter<FloatImageType, AtlasLabelType>;
  typename LabelReCasterType::Pointer labelReCaster = LabelReCasterType::New();

  labelReCaster->SetInput(thresholder->GetOutput());
  try
  {
    m_Timer.Start("6k) Label Caster");
    labelReCaster->Update();
    m_Timer.Stop("6k) Label Caster");
  }
  catch (itk::ExceptionObject & excep)
  {
    std::cerr << "Exception caught while doing GeodesicActiveContours !" << std::endl;
    std::cerr << excep << std::endl;
  }

  m_AtlasLabels = labelReCaster->GetOutput();
  m_AtlasLabels->DisconnectPipeline();
}


template <class TImageType, class TAtlasImageType, class TAtlasLabelType>
void
StripTsImageFilter<TImageType, TAtlasImageType, TAtlasLabelType>::UpsampleLabels()
{
  // upsample atlas label image to original resolution

  //  std::cout << "Generating final brain mask" << std::endl;

  using ResamplerType = itk::ResampleImageFilter<TAtlasLabelType, TAtlasLabelType>;
  typename ResamplerType::Pointer resampler = ResamplerType::New();

  using TransformType = itk::IdentityTransform<double, 3>;
  typename TransformType::Pointer transform = TransformType::New();

  using NNInterpolatorType = itk::NearestNeighborInterpolateImageFunction<TAtlasLabelType, double>;
  typename NNInterpolatorType::Pointer nnInterp = NNInterpolatorType::New();

  transform->SetIdentity();

  resampler->SetTransform(transform);
  resampler->SetInput(m_AtlasLabels);

  resampler->SetInterpolator(nnInterp);
  resampler->SetSize(this->GetInput()->GetLargestPossibleRegion().GetSize());
  resampler->SetOutputSpacing(this->GetInput()->GetSpacing());
  resampler->SetOutputOrigin(this->GetInput()->GetOrigin());
  resampler->SetOutputDirection(this->GetInput()->GetDirection());
  resampler->SetDefaultPixelValue(0);

  try
  {
    m_Progress->RegisterInternalFilter(resampler, 0.01f);
    resampler->Update();
  }
  catch (itk::ExceptionObject & exception)
  {
    std::cerr << "ExceptionObject caught !" << std::endl;
    std::cerr << exception << std::endl;
  }

  m_AtlasLabels = resampler->GetOutput();
  m_AtlasLabels->DisconnectPipeline();
}

} // end namespace itk

#endif
