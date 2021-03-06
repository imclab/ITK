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

#include "itkImageRegistrationMethodv4.h"

#include "itkAffineTransform.h"
#include "itkANTSNeighborhoodCorrelationImageToImageMetricv4.h"
#include "itkComposeDisplacementFieldsImageFilter.h"
#include "itkGaussianExponentialDiffeomorphicTransform.h"
#include "itkGaussianExponentialDiffeomorphicTransformParametersAdaptor.h"
#include "itkVectorMagnitudeImageFilter.h"
#include "itkStatisticsImageFilter.h"

#include "itkTimeProbesCollectorBase.h"

template<typename TFilter>
class CommandIterationUpdate : public itk::Command
{
public:
  typedef CommandIterationUpdate   Self;
  typedef itk::Command             Superclass;
  typedef itk::SmartPointer<Self>  Pointer;
  itkNewMacro( Self );

protected:
  CommandIterationUpdate() {};

public:

  void Execute(itk::Object *caller, const itk::EventObject & event)
    {
    Execute( (const itk::Object *) caller, event);
    }

  void Execute(const itk::Object * object, const itk::EventObject & event)
    {
    const TFilter * filter =
      dynamic_cast< const TFilter * >( object );
    if( typeid( event ) != typeid( itk::IterationEvent ) )
      { return; }

    unsigned int currentLevel = filter->GetCurrentLevel();
    typename TFilter::ShrinkFactorsPerDimensionContainerType shrinkFactors = filter->GetShrinkFactorsPerDimension( currentLevel );
    typename TFilter::SmoothingSigmasArrayType smoothingSigmas = filter->GetSmoothingSigmasPerLevel();
    typename TFilter::TransformParametersAdaptorsContainerType adaptors = filter->GetTransformParametersAdaptorsPerLevel();

    typename itk::ObjectToObjectOptimizerBase::Pointer optimizerBase = (const_cast<TFilter*>(filter))->GetModifiableOptimizer();
    typedef itk::GradientDescentOptimizerv4 GradientDescentOptimizerv4Type;
    typename GradientDescentOptimizerv4Type::Pointer optimizer = dynamic_cast<GradientDescentOptimizerv4Type *>(optimizerBase.GetPointer());
    if( !optimizer )
      {
      itkGenericExceptionMacro( "Error dynamic_cast failed" );
      }
    typename GradientDescentOptimizerv4Type::DerivativeType gradient = optimizer->GetGradient();

    /* orig
    std::cout << "  Current level = " << currentLevel << std::endl;
    std::cout << "    shrink factor = " << shrinkFactors[currentLevel] << std::endl;
    std::cout << "    smoothing sigma = " << smoothingSigmas[currentLevel] << std::endl;
    std::cout << "    required fixed parameters = " << adaptors[currentLevel]->GetRequiredFixedParameters() << std::endl;
    */

    //debug:
    std::cout << "  CL Current level:           " << currentLevel << std::endl;
    std::cout << "   SF Shrink factor:          " << shrinkFactors << std::endl;
    std::cout << "   SS Smoothing sigma:        " << smoothingSigmas[currentLevel] << std::endl;
    std::cout << "   RFP Required fixed params: " << adaptors[currentLevel]->GetRequiredFixedParameters() << std::endl;
    std::cout << "   LR Final learning rate:    " << optimizer->GetLearningRate() << std::endl;
    std::cout << "   FM Final metric value:     " << optimizer->GetCurrentMetricValue() << std::endl;
    std::cout << "   SC Optimizer scales:       " << optimizer->GetScales() << std::endl;
    std::cout << "   FG Final metric gradient (sample of values): ";
    if( gradient.GetSize() < 10 )
      {
      std::cout << gradient;
      }
    else
      {
      for( itk::SizeValueType i = 0; i < gradient.GetSize(); i += (gradient.GetSize() / 16) )
        {
        std::cout << gradient[i] << " ";
        }
      }
    std::cout << std::endl;
    }
};

template <unsigned int VImageDimension>
int PerformExpImageRegistration( int argc, char *argv[] )
{
  if( argc < 6 )
    {
    std::cout << argv[0] << " imageDimension fixedImage movingImage outputImage numberOfAffineIterations numberOfDeformableIterations" << std::endl;
    exit( 1 );
    }

  itk::TimeProbesCollectorBase timer;
  typedef double                                 PixelType;
  typedef itk::Image<PixelType, VImageDimension> FixedImageType;
  typedef itk::Image<PixelType, VImageDimension> MovingImageType;

  typedef itk::ImageFileReader<FixedImageType> ImageReaderType;

  typename ImageReaderType::Pointer fixedImageReader = ImageReaderType::New();
  fixedImageReader->SetFileName( argv[2] );
  timer.Start("0 fixedImageReader");
  fixedImageReader->Update();
  timer.Stop("0 fixedImageReader");
  typename FixedImageType::Pointer fixedImage = fixedImageReader->GetOutput();
  timer.Start("1 fixedImage");
  fixedImage->Update();
  timer.Stop("1 fixedImage");
  fixedImage->DisconnectPipeline();

  typename ImageReaderType::Pointer movingImageReader = ImageReaderType::New();
  movingImageReader->SetFileName( argv[3] );
  timer.Start("2 movingImageReader");
  movingImageReader->Update();
  timer.Stop("2 movingImageReader");
  typename MovingImageType::Pointer movingImage = movingImageReader->GetOutput();
  timer.Start("3 movingImage");
  movingImage->Update();
  timer.Stop("3 movingImage");
  movingImage->DisconnectPipeline();

  typedef itk::AffineTransform<double, VImageDimension> AffineTransformType;
  typedef itk::ImageRegistrationMethodv4<FixedImageType, MovingImageType, AffineTransformType> AffineRegistrationType;
  typedef itk::GradientDescentOptimizerv4 GradientDescentOptimizerv4Type;
  typename AffineRegistrationType::Pointer affineSimple = AffineRegistrationType::New();
  affineSimple->SetFixedImage( fixedImage );
  affineSimple->SetMovingImage( movingImage );

  // Smooth by specified gaussian sigmas for each level.  These values are specified in
  // physical units. Sigmas of zero cause inconsistency between some platforms.
  {
  typename AffineRegistrationType::SmoothingSigmasArrayType smoothingSigmasPerLevel;
  smoothingSigmasPerLevel.SetSize( 3 );
  smoothingSigmasPerLevel[0] = 2;
  smoothingSigmasPerLevel[1] = 1;
  smoothingSigmasPerLevel[2] = 1; //0;
  affineSimple->SetSmoothingSigmasPerLevel( smoothingSigmasPerLevel );
  }

  typedef itk::GradientDescentOptimizerv4 GradientDescentOptimizerv4Type;
  typename GradientDescentOptimizerv4Type::Pointer affineOptimizer =
    dynamic_cast<GradientDescentOptimizerv4Type * >( affineSimple->GetModifiableOptimizer() );
  if( !affineOptimizer )
    {
    itkGenericExceptionMacro( "Error dynamic_cast failed" );
    }
#ifdef NDEBUG
  affineOptimizer->SetNumberOfIterations( atoi( argv[5] ) );
#else
  affineOptimizer->SetNumberOfIterations( 1 );
#endif
  affineOptimizer->SetDoEstimateLearningRateOnce( false ); //true by default
  affineOptimizer->SetDoEstimateLearningRateAtEachIteration( true );

  typedef CommandIterationUpdate<AffineRegistrationType> AffineCommandType;
  typename AffineCommandType::Pointer affineObserver = AffineCommandType::New();
  affineSimple->AddObserver( itk::IterationEvent(), affineObserver );

  {
  typedef itk::ImageToImageMetricv4<FixedImageType, MovingImageType> ImageMetricType;
  typename ImageMetricType::Pointer imageMetric = dynamic_cast<ImageMetricType*>( affineSimple->GetModifiableMetric() );
  //imageMetric->SetUseFloatingPointCorrection(true);
  imageMetric->SetFloatingPointCorrectionResolution(1e4);
  }

  try
    {
    std::cout << "Affine txf:" << std::endl;
    timer.Start("4 affineSimple");
    affineSimple->Update();
    timer.Stop("4 affineSimple");
    }
  catch( itk::ExceptionObject &e )
    {
    std::cerr << "Exception caught: " << e << std::endl;
    return EXIT_FAILURE;
    }

  {
  typedef itk::ImageToImageMetricv4<FixedImageType, MovingImageType> ImageMetricType;
  typename ImageMetricType::Pointer imageMetric = dynamic_cast<ImageMetricType*>( affineOptimizer->GetModifiableMetric() );
  std::cout << "Affine parameters after registration: " << std::endl
            << affineOptimizer->GetCurrentPosition() << std::endl
            << "Last LearningRate: " << affineOptimizer->GetLearningRate() << std::endl
            << "Use FltPtCorrex: " << imageMetric->GetUseFloatingPointCorrection() << std::endl
            << "FltPtCorrexRes: " << imageMetric->GetFloatingPointCorrectionResolution() << std::endl
            << "Number of threads used: metric: " << imageMetric->GetNumberOfThreadsUsed()
            << std::endl << " optimizer: " << affineOptimizer->GetNumberOfThreads() << std::endl;
  }
  //
  // Now do the displacement field transform with gaussian smoothing using
  // the composite transform.
  //

  typedef typename AffineRegistrationType::RealType RealType;

  typedef itk::CompositeTransform<RealType, VImageDimension> CompositeTransformType;
  typename CompositeTransformType::Pointer compositeTransform = CompositeTransformType::New();
  compositeTransform->AddTransform( const_cast<typename AffineRegistrationType::OutputTransformType *>( affineSimple->GetOutput()->Get() ) );

  typedef itk::Vector<RealType, VImageDimension> VectorType;
  VectorType zeroVector( 0.0 );
  typedef itk::Image<VectorType, VImageDimension> DisplacementFieldType;
  typedef itk::Image<VectorType, VImageDimension> ConstantVelocityFieldType;
  typename ConstantVelocityFieldType::Pointer displacementField = ConstantVelocityFieldType::New();
  displacementField->CopyInformation( fixedImage );
  displacementField->SetRegions( fixedImage->GetBufferedRegion() );
  displacementField->Allocate();
  displacementField->FillBuffer( zeroVector );

  typedef itk::GaussianExponentialDiffeomorphicTransform<RealType, VImageDimension> ConstantVelocityFieldTransformType;

  typedef itk::ImageRegistrationMethodv4<FixedImageType, MovingImageType, ConstantVelocityFieldTransformType> DisplacementFieldRegistrationType;
  typename DisplacementFieldRegistrationType::Pointer displacementFieldSimple = DisplacementFieldRegistrationType::New();

  typename ConstantVelocityFieldTransformType::Pointer fieldTransform = const_cast<ConstantVelocityFieldTransformType *>( displacementFieldSimple->GetOutput()->Get() );
  fieldTransform->SetGaussianSmoothingVarianceForTheUpdateField( 0.75 );
  fieldTransform->SetGaussianSmoothingVarianceForTheConstantVelocityField( 1.5 );
  fieldTransform->SetConstantVelocityField( displacementField );
  fieldTransform->SetCalculateNumberOfIntegrationStepsAutomatically( true );

  typedef itk::ANTSNeighborhoodCorrelationImageToImageMetricv4<FixedImageType, MovingImageType> CorrelationMetricType;
  typename CorrelationMetricType::Pointer correlationMetric = CorrelationMetricType::New();
  typename CorrelationMetricType::RadiusType radius;
  radius.Fill( 4 );
  correlationMetric->SetRadius( radius );
  correlationMetric->SetUseMovingImageGradientFilter( false );
  correlationMetric->SetUseFixedImageGradientFilter( false );

  //correlationMetric->SetUseFloatingPointCorrection(true);
  //correlationMetric->SetFloatingPointCorrectionResolution(1e4);

  typedef itk::RegistrationParameterScalesFromPhysicalShift<CorrelationMetricType> ScalesEstimatorType;
  typename ScalesEstimatorType::Pointer scalesEstimator = ScalesEstimatorType::New();
  scalesEstimator->SetMetric( correlationMetric );
  scalesEstimator->SetTransformForward( true );
  scalesEstimator->SetSmallParameterVariation( 1.0 );

  typename GradientDescentOptimizerv4Type::Pointer optimizer = GradientDescentOptimizerv4Type::New();
  optimizer->SetLearningRate( 1.0 );
#ifdef NDEBUG
  optimizer->SetNumberOfIterations( atoi( argv[6] ) );
#else
  optimizer->SetNumberOfIterations( 1 );
#endif
  optimizer->SetScalesEstimator( NULL );
  optimizer->SetDoEstimateLearningRateOnce( false ); //true by default
  optimizer->SetDoEstimateLearningRateAtEachIteration( true );

  displacementFieldSimple->SetFixedImage( fixedImage );
  displacementFieldSimple->SetMovingImage( movingImage );
  displacementFieldSimple->SetNumberOfLevels( 3 );
  displacementFieldSimple->SetMovingInitialTransform( compositeTransform );
  displacementFieldSimple->SetMetric( correlationMetric );
  displacementFieldSimple->SetOptimizer( optimizer );

  // Shrink the virtual domain by specified factors for each level.  See documentation
  // for the itkShrinkImageFilter for more detailed behavior.
  typename DisplacementFieldRegistrationType::ShrinkFactorsArrayType shrinkFactorsPerLevel;
  shrinkFactorsPerLevel.SetSize( 3 );
  shrinkFactorsPerLevel[0] = 3;
  shrinkFactorsPerLevel[1] = 2;
  shrinkFactorsPerLevel[2] = 1;
  displacementFieldSimple->SetShrinkFactorsPerLevel( shrinkFactorsPerLevel );

  // Smooth by specified gaussian sigmas for each level.  These values are specified in
  // physical units.
  typename DisplacementFieldRegistrationType::SmoothingSigmasArrayType smoothingSigmasPerLevel;
  smoothingSigmasPerLevel.SetSize( 3 );
  smoothingSigmasPerLevel[0] = 2;
  smoothingSigmasPerLevel[1] = 1;
  smoothingSigmasPerLevel[2] = 1;
  displacementFieldSimple->SetSmoothingSigmasPerLevel( smoothingSigmasPerLevel );

  typedef itk::GaussianExponentialDiffeomorphicTransformParametersAdaptor<ConstantVelocityFieldTransformType> VelocityFieldTransformAdaptorType;

  typename DisplacementFieldRegistrationType::TransformParametersAdaptorsContainerType adaptors;

  for( unsigned int level = 0; level < shrinkFactorsPerLevel.Size(); level++ )
    {
    // We use the shrink image filter to calculate the fixed parameters of the virtual
    // domain at each level.  To speed up calculation and avoid unnecessary memory
    // usage, we could calculate these fixed parameters directly.

    typedef itk::ShrinkImageFilter<ConstantVelocityFieldType, ConstantVelocityFieldType> ShrinkFilterType;
    typename ShrinkFilterType::Pointer shrinkFilter = ShrinkFilterType::New();
    shrinkFilter->SetShrinkFactors( shrinkFactorsPerLevel[level] );
    shrinkFilter->SetInput( fieldTransform->GetConstantVelocityField() );
    timer.Start("5 shrink");
    shrinkFilter->Update();
    timer.Stop("5 shrink");

    typename VelocityFieldTransformAdaptorType::Pointer fieldTransformAdaptor = VelocityFieldTransformAdaptorType::New();
    fieldTransformAdaptor->SetRequiredSpacing( shrinkFilter->GetOutput()->GetSpacing() );
    fieldTransformAdaptor->SetRequiredSize( shrinkFilter->GetOutput()->GetBufferedRegion().GetSize() );
    fieldTransformAdaptor->SetRequiredDirection( shrinkFilter->GetOutput()->GetDirection() );
    fieldTransformAdaptor->SetRequiredOrigin( shrinkFilter->GetOutput()->GetOrigin() );

    adaptors.push_back( fieldTransformAdaptor.GetPointer() );
    }
  displacementFieldSimple->SetTransformParametersAdaptorsPerLevel( adaptors );

  typedef CommandIterationUpdate<DisplacementFieldRegistrationType> DisplacementFieldRegistrationCommandType;
  typename DisplacementFieldRegistrationCommandType::Pointer displacementFieldObserver = DisplacementFieldRegistrationCommandType::New();
  displacementFieldSimple->AddObserver( itk::IterationEvent(), displacementFieldObserver );

  try
    {
    std::cout << "Displ. txf - gauss update" << std::endl;
    timer.Start("6 displacementFieldSimple");
    displacementFieldSimple->Update();
    timer.Stop("6 displacementFieldSimple");
    }
  catch( itk::ExceptionObject &e )
    {
    std::cerr << "Exception caught: " << e << std::endl;
    return EXIT_FAILURE;
    }

  compositeTransform->AddTransform( const_cast<ConstantVelocityFieldTransformType *>( displacementFieldSimple->GetOutput()->Get() ) );

  std::cout << "After displacement registration: " << std::endl
            << "Last LearningRate: " << optimizer->GetLearningRate() << std::endl
            << "Use FltPtCorrex: " << correlationMetric->GetUseFloatingPointCorrection() << std::endl
            << "FltPtCorrexRes: " << correlationMetric->GetFloatingPointCorrectionResolution() << std::endl
            << "Number of threads used: metric: " << correlationMetric->GetNumberOfThreadsUsed()
            << "Number of threads used: metric: " << correlationMetric->GetNumberOfThreadsUsed()
            << " optimizer: " << displacementFieldSimple->GetOptimizer()->GetNumberOfThreads() << std::endl;

  typedef itk::ResampleImageFilter<MovingImageType, FixedImageType> ResampleFilterType;
  typename ResampleFilterType::Pointer resampler = ResampleFilterType::New();
  resampler->SetTransform( compositeTransform );
  resampler->SetInput( movingImage );
  resampler->SetSize( fixedImage->GetLargestPossibleRegion().GetSize() );
  resampler->SetOutputOrigin( fixedImage->GetOrigin() );
  resampler->SetOutputSpacing( fixedImage->GetSpacing() );
  resampler->SetOutputDirection( fixedImage->GetDirection() );
  resampler->SetDefaultPixelValue( 0 );
  timer.Start("7 resampler");
  resampler->Update();
  timer.Stop("7 resampler");

  typedef itk::ImageFileWriter<FixedImageType> WriterType;
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName( argv[4] );
  writer->SetInput( resampler->GetOutput() );
  timer.Start("8 writer");
  writer->Update();
  timer.Stop("8 writer");

  // Check identity of forward and inverse transforms

  typedef itk::ComposeDisplacementFieldsImageFilter<DisplacementFieldType, DisplacementFieldType> ComposerType;
  typename ComposerType::Pointer composer = ComposerType::New();
  composer->SetDisplacementField( fieldTransform->GetDisplacementField() );
  composer->SetWarpingField( fieldTransform->GetInverseDisplacementField() );
  timer.Start("8 composer");
  composer->Update();
  timer.Stop("8 composer");

  typedef itk::VectorMagnitudeImageFilter<DisplacementFieldType, MovingImageType> MagnituderType;
  typename MagnituderType::Pointer magnituder = MagnituderType::New();
  magnituder->SetInput( composer->GetOutput() );
  timer.Start("9 magnituder");
  magnituder->Update();
  timer.Stop("9 magnituder");

  typedef itk::StatisticsImageFilter<MovingImageType> StatisticsImageFilterType;
  typename StatisticsImageFilterType::Pointer stats = StatisticsImageFilterType::New();
  stats->SetInput( magnituder->GetOutput() );
  timer.Start("10 stats");
  stats->Update();
  timer.Stop("10 stats");

  std::cout << "Identity check:" << std::endl;
  std::cout << "  Min:  " << stats->GetMinimum() << std::endl;
  std::cout << "  Max:  " << stats->GetMaximum() << std::endl;
  std::cout << "  Mean:  " << stats->GetMean() << std::endl;
  std::cout << "  Variance:  " << stats->GetVariance() << std::endl;

  if( stats->GetMean() > 0.1 )
    {
    std::cerr << "Identity test failed." << std::endl;
    }

  timer.Report(std::cout);

  return EXIT_SUCCESS;
}

int itkExponentialImageRegistrationTest( int argc, char *argv[] )
{
  if( argc < 6 )
    {
    std::cout << argv[0] << " imageDimension fixedImage movingImage outputImage numberOfAffineIterations numberOfDeformableIterations" << std::endl;
    exit( 1 );
    }

  switch( atoi( argv[1] ) )
   {
   case 2:
     PerformExpImageRegistration<2>( argc, argv );
     break;
   case 3:
     PerformExpImageRegistration<3>( argc, argv );
     break;
   default:
      std::cerr << "Unsupported dimension" << std::endl;
      exit( EXIT_FAILURE );
   }
  return EXIT_SUCCESS;
}
