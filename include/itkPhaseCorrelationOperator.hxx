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
#ifndef itkPhaseCorrelationOperator_hxx
#define itkPhaseCorrelationOperator_hxx

#include "itkPhaseCorrelationOperator.h"
#include "itkImageRegionIterator.h"
#include "itkImageScanlineIterator.h"
#include "itkObjectFactory.h"
#include "itkProgressReporter.h"
#include "itkMetaDataObject.h"

namespace itk
{

/*
 * \author Jakub Bican, jakub.bican@matfyz.cz, Department of Image Processing,
 *         Institute of Information Theory and Automation,
 *         Academy of Sciences of the Czech Republic.
 *
 */


template < typename TRealPixel, unsigned int VImageDimension >
PhaseCorrelationOperator< TRealPixel, VImageDimension >
::PhaseCorrelationOperator()
{
  this->SetNumberOfRequiredInputs( 2 );
  m_BandPassControlPoints[0] = 0.05;
  m_BandPassControlPoints[1] = 0.1;
  m_BandPassControlPoints[2] = 0.5;
  m_BandPassControlPoints[3] = 0.9;
}


template < typename TRealPixel, unsigned int VImageDimension >
void
PhaseCorrelationOperator< TRealPixel, VImageDimension >
::PrintSelf(std::ostream& os, Indent indent) const
{
  Superclass::PrintSelf(os,indent);
}


template < typename TRealPixel, unsigned int VImageDimension >
void
PhaseCorrelationOperator< TRealPixel, VImageDimension >
::SetFixedImage( ImageType * fixedImage )
{
  this->SetNthInput(0, const_cast<ImageType *>( fixedImage ));
}


template < typename TRealPixel, unsigned int VImageDimension >
void
PhaseCorrelationOperator< TRealPixel, VImageDimension >
::SetMovingImage( ImageType * movingImage )
{
  this->SetNthInput(1, const_cast<ImageType *>( movingImage ));
}


template<typename TRealPixel, unsigned int VImageDimension>
void
PhaseCorrelationOperator<TRealPixel, VImageDimension>
::SetBandPassControlPoints(const BandPassPointsType& points)
{
  if (this->m_BandPassControlPoints != points)
    {
    if (points[0] < 0.0)
      {
      itkExceptionMacro("Control point 0 must be greater than or equal to 0.0!");
      }
    if (points[3] > 1.0)
      {
      itkExceptionMacro("Control point 3 must be less than or equal to 1.0!");
      }
    if (points[0] >= points[1])
      {
      itkExceptionMacro("Control point 0 must be strictly less than control point 1!");
      }
    if (points[1] >= points[2])
      {
      itkExceptionMacro("Control point 1 must be strictly less than control point 2!");
      }
    if (points[2] >= points[3])
      {
      itkExceptionMacro("Control point 2 must be strictly less than control point 3!");
      }
    this->m_BandPassControlPoints = points;
    this->Modified();
    }
}


template < typename TRealPixel, unsigned int VImageDimension >
void
PhaseCorrelationOperator< TRealPixel, VImageDimension >
::DynamicThreadedGenerateData(const OutputImageRegionType& outputRegionForThread)
{
  // Get the input and output pointers
  ImageConstPointer  fixed     = this->GetInput(0);
  ImageConstPointer  moving    = this->GetInput(1);
  ImagePointer       output    = this->GetOutput();

  // Define an iterator that will walk the output region for this thread.
  using InputIterator = ImageScanlineConstIterator<ImageType>;
  using OutputIterator = ImageScanlineIterator<ImageType>;
  InputIterator fixedIt(fixed, outputRegionForThread);
  InputIterator movingIt(moving, outputRegionForThread);
  OutputIterator outIt(output, outputRegionForThread);

  typename ImageType::SizeType size = output->GetLargestPossibleRegion().GetSize();
  PixelType maxDist = size[0] * size[0]; //first dimension is halved
  for (unsigned d = 1; d < VImageDimension; d++)
    {
    maxDist += size[d] * size[d] / 4.0;
    }
  maxDist = std::sqrt(maxDist);
  PixelType c0 = m_BandPassControlPoints[0] * maxDist;
  PixelType c1 = m_BandPassControlPoints[1] * maxDist;
  PixelType c2 = m_BandPassControlPoints[2] * maxDist;
  PixelType c3 = m_BandPassControlPoints[3] * maxDist;
  PixelType oneOverC1minusC0 = 1.0 / (c1 - c0); //saves per pixel computation
  PixelType oneOverC3minusC2 = 1.0 / (c3 - c2); //saves per pixel computation
  typename ImageType::IndexType ind0 = output->GetLargestPossibleRegion().GetIndex();

  itkDebugMacro("computing correlation surface");
  // walk the output region, and sample the input image
  while ( !outIt.IsAtEnd() )
    {
    while ( !outIt.IsAtEndOfLine() )
      {
      typename ImageType::IndexType ind = fixedIt.GetIndex();
      PixelType distFrom0 = (ind[0] - ind0[0])*(ind[0] - ind0[0]); //first dimension is halved
      for (unsigned d = 1; d < VImageDimension; d++) //higher dimensions wrap around
        {
        IndexValueType dInd = ind[d] - ind0[d];
        if (dInd >= IndexValueType(size[d] / 2))
          {
          dInd = size[d] - (ind[d] - ind0[d]);
          }
        distFrom0 += dInd * dInd;
        }
      distFrom0 = std::sqrt(distFrom0);

      // compute the phase correlation
      const PixelType real = fixedIt.Value().real() * movingIt.Value().real() +
                             fixedIt.Value().imag() * movingIt.Value().imag();
      const PixelType imag = fixedIt.Value().imag() * movingIt.Value().real() -
                             fixedIt.Value().real() * movingIt.Value().imag();
      PixelType magn = std::sqrt( real*real + imag*imag );

      PixelType factor = 1;
      if (distFrom0 < c0)
        {
        factor = 0;
        }
      else if (distFrom0 >= c0 && distFrom0 < c1)
        {
        factor = (distFrom0 - c0) * oneOverC1minusC0;
        }
      else if (distFrom0 >= c1 && distFrom0 <= c2)
        {
        factor = 1;
        }
      else if (distFrom0 > c2 && distFrom0 <= c3)
        {
        factor = (c3 - distFrom0) * oneOverC3minusC2;
        }
      else //distFrom0 > c3
        {
        factor = 0;
        }

      if (magn != 0 )
        {
        outIt.Set(ComplexType(factor*real / magn, factor*imag / magn));
        }
      else
        {
        outIt.Set( ComplexType( 0, 0 ) );
        }

      ++fixedIt;
      ++movingIt;
      ++outIt;
      }
    fixedIt.NextLine();
    movingIt.NextLine();
    outIt.NextLine();
    }
}


template < typename TRealPixel, unsigned int VImageDimension >
void
PhaseCorrelationOperator< TRealPixel, VImageDimension >
::GenerateInputRequestedRegion()
{
  /**
   *  Request all available data. This filter is cropping from the center.
   */

  // call the superclass' implementation of this method
  Superclass::GenerateInputRequestedRegion();

  // get pointers to the inputs
  ImagePointer fixed  = const_cast<ImageType *> (this->GetInput(0));
  ImagePointer moving = const_cast<ImageType *> (this->GetInput(1));

  if ( !fixed || !moving )
    {
    return;
    }

  fixed->SetRequestedRegion(  fixed->GetLargestPossibleRegion()  );
  moving->SetRequestedRegion( moving->GetLargestPossibleRegion() );
}


template < typename TRealPixel, unsigned int VImageDimension >
void
PhaseCorrelationOperator< TRealPixel, VImageDimension >
::GenerateOutputInformation()
{
  /**
   *  The output will have the lower size of the two input images in all
   *  dimensions.
   */

  // call the superclass' implementation of this method
  Superclass::GenerateOutputInformation();

  // get pointers to the inputs and output
  ImageConstPointer fixed  = this->GetInput(0);
  ImageConstPointer moving = this->GetInput(1);
  ImagePointer      output = this->GetOutput();

  if ( !fixed || !moving || !output )
    {
    return;
    }

  itkDebugMacro( "adjusting size of output image" );
  // we need to compute the output spacing, the output image size, and the
  // output image start index
  unsigned int i;
  const typename ImageType::SpacingType&
    fixedSpacing     =  fixed->GetSpacing(),
    movingSpacing    = moving->GetSpacing();
  const typename ImageType::SizeType&
    fixedSize        =  fixed->GetLargestPossibleRegion().GetSize(),
    movingSize       = moving->GetLargestPossibleRegion().GetSize();
  const typename ImageType::IndexType&
    fixedStartIndex  =  fixed->GetLargestPossibleRegion().GetIndex();

  typename ImageType::SpacingType  outputSpacing;
  typename ImageType::SizeType     outputSize;
  typename ImageType::IndexType    outputStartIndex;

  for (i = 0; i < ImageType::ImageDimension; i++)
    {
    outputSpacing[i]    = fixedSpacing[i] >= movingSpacing[i] ?
                                            fixedSpacing[i] : movingSpacing[i];
    outputSize[i]       = fixedSize[i] <= movingSize[i] ?
                                            fixedSize[i] : movingSize[i];
    outputStartIndex[i] = fixedStartIndex[i];
    }

  // additionally adjust the data size
  this->AdjustOutputInformation( outputSpacing, outputStartIndex, outputSize );

  output->SetSpacing( outputSpacing );

  typename ImageType::RegionType outputLargestPossibleRegion;
  outputLargestPossibleRegion.SetSize( outputSize );
  outputLargestPossibleRegion.SetIndex( outputStartIndex );

  output->SetLargestPossibleRegion( outputLargestPossibleRegion );

  //
  // Pass the metadata with the actual size of the image.
  // The size must be adjusted according to the cropping and scaling
  // that will be made on the image!
  itkDebugMacro( "storing size of pre-FFT image in MetaData" );
  using SizeScalarType = typename ImageType::SizeValueType;

  SizeScalarType fixedX = NumericTraits< SizeScalarType >::Zero;
  SizeScalarType movingX = NumericTraits< SizeScalarType >::Zero;
  SizeScalarType outputX = NumericTraits< SizeScalarType >::Zero;

  MetaDataDictionary &fixedDic  = const_cast<MetaDataDictionary &>(fixed->GetMetaDataDictionary());
  MetaDataDictionary &movingDic = const_cast<MetaDataDictionary &>(moving->GetMetaDataDictionary());
  MetaDataDictionary &outputDic = const_cast<MetaDataDictionary &>(output->GetMetaDataDictionary());

  if(ExposeMetaData < SizeScalarType >
          (fixedDic,std::string("FFT_Actual_RealImage_Size"),fixedX)
     &&
     ExposeMetaData < SizeScalarType >
          (movingDic,std::string("FFT_Actual_RealImage_Size"),movingX))
    {
    outputX = fixedX > movingX ? movingX : fixedX;

    EncapsulateMetaData<SizeScalarType>(outputDic,
                                        std::string("FFT_Actual_RealImage_Size"),
                                        outputX);
    }
}


template < typename TRealPixel, unsigned int VImageDimension >
void
PhaseCorrelationOperator< TRealPixel, VImageDimension >
::EnlargeOutputRequestedRegion(DataObject *output)
{
  Superclass::EnlargeOutputRequestedRegion(output);
  output->SetRequestedRegionToLargestPossibleRegion();
}

} //end namespace itk

#endif
