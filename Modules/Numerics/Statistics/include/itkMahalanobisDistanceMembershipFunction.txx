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
#ifndef __itkMahalanobisDistanceMembershipFunction_txx
#define __itkMahalanobisDistanceMembershipFunction_txx

#include "itkMahalanobisDistanceMembershipFunction.h"

#include "vnl/vnl_vector.h"
#include "vnl/vnl_matrix.h"
#include "vnl/algo/vnl_matrix_inverse.h"

namespace itk
{
namespace Statistics
{
template< class TVector >
MahalanobisDistanceMembershipFunction< TVector >
::MahalanobisDistanceMembershipFunction()
{
  NumericTraits<MeanVectorType>::SetLength(m_Mean, this->GetMeasurementVectorSize());
  m_Mean.Fill(0.0f);

  m_Covariance.SetSize(this->GetMeasurementVectorSize(), this->GetMeasurementVectorSize());
  m_Covariance.SetIdentity();

  m_InverseCovariance = m_Covariance;

  m_DeterminantOK = true;
}

template< class TVector >
void
MahalanobisDistanceMembershipFunction< TVector >
::SetMean(const MeanVectorType & mean)
{
  if ( this->GetMeasurementVectorSize() )
    {
    MeasurementVectorTraits::Assert(mean,
                                    this->GetMeasurementVectorSize(),
                                    "GaussianMembershipFunction::SetMean(): Size of mean vector specified does not match the size of a measurement vector.");
    }
  else
    {
    // not already set, cache the size
    this->SetMeasurementVectorSize( mean.Size() );
    }

  if ( m_Mean != mean )
    {
    m_Mean = mean;
    this->Modified();
    }
}

template< class TVector >
void
MahalanobisDistanceMembershipFunction< TVector >
::SetCovariance(const CovarianceMatrixType & cov)
{
  // Sanity check
  if ( cov.GetVnlMatrix().rows() != cov.GetVnlMatrix().cols() )
    {
    itkExceptionMacro(<< "Covariance matrix must be square");
    }
  if ( this->GetMeasurementVectorSize() )
    {
    if ( cov.GetVnlMatrix().rows() != this->GetMeasurementVectorSize() )
      {
      itkExceptionMacro(<< "Length of measurement vectors must be"
                        << " the same as the size of the covariance.");
      }
    }
  else
    {
    // not already set, cache the size
    this->SetMeasurementVectorSize( cov.GetVnlMatrix().rows() );
    }

  if (m_Covariance == cov)
    {
    // no need to copy the matrix, compute the inverse, or the normalization
    return;
    }

  m_Covariance = cov;

  // the inverse of the covariance matrix is first computed by SVD
  vnl_matrix_inverse< double > inv_cov( m_Covariance.GetVnlMatrix() );

  // the determinant is then costless this way
  double det = inv_cov.determinant_magnitude();

  if( det < 0.)
    {
    itkExceptionMacro( << "det( m_Covariance ) < 0" );
    }

  // 1e-6 is an arbitrary value!!!
  double m_DeterminantOK = ( det > 1e-6 );

  if( m_DeterminantOK )
    {
    // allocate the memory for m_InverseCovariance matrix
    m_InverseCovariance.GetVnlMatrix() = inv_cov.inverse();
    }

  this->Modified();
}

template< class TVector >
double
MahalanobisDistanceMembershipFunction< TVector >
::Evaluate(const MeasurementVectorType & measurement) const
{
  const MeasurementVectorSizeType measurementVectorSize =
    this->GetMeasurementVectorSize();

  //if ( !m_IsCovarianceZero )
  if( m_DeterminantOK )
    {
    // Compute ( y - mean )
    vnl_vector< double > tempVector( measurementVectorSize );

    for ( MeasurementVectorSizeType i = 0; i < measurementVectorSize; ++i )
      {
      tempVector[i] = measurement[i] - m_Mean[i];
      }

    // temp = ( y - mean )^t * InverseCovariance * ( y - mean )
    double temp = dot_product( tempVector,
                               m_InverseCovariance.GetVnlMatrix() * tempVector );

    return temp;
    }
  else
    {
    for ( MeasurementVectorSizeType i = 0; i < measurementVectorSize; ++i )
      {
      if ( m_Mean[i] != static_cast< double >( measurement[i] ) )
        {
        return 0.;
        }
      }
    return NumericTraits< double >::max();
    }
}

template< class TVector >
void
MahalanobisDistanceMembershipFunction< TVector >
::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "Mean: " << m_Mean << std::endl;
  os << indent << "Covariance: " << std::endl;
  os << m_Covariance.GetVnlMatrix();
  os << indent << "InverseCovariance: " << std::endl;
  os << indent << m_InverseCovariance.GetVnlMatrix();
  os << indent << "Non-null covariance: " <<
    (m_DeterminantOK ? "true" : "false") << std::endl;
}

template< class TVector >
typename MahalanobisDistanceMembershipFunction< TVector >::MembershipFunctionPointer
MahalanobisDistanceMembershipFunction< TVector >
::Clone() const
{
  Pointer membershipFunction = MahalanobisDistanceMembershipFunction< TVector >::New();

  membershipFunction->SetMeasurementVectorSize( this->GetMeasurementVectorSize() );
  membershipFunction->SetMean( this->GetMean() );
  membershipFunction->SetCovariance( this->GetCovariance() );

  MembershipFunctionPointer sptr = membershipFunction.GetPointer();
  return sptr;
}

} // end namespace Statistics
} // end of namespace itk

#endif