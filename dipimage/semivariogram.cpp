/*
 * DIPimage 3.0
 * This MEX-file implements the `semivariogram` function
 *
 * (c)2018, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 * Based on original DIPimage code: (c)1999-2014, Delft University of Technology.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dip_matlab_interface.h"
#include "diplib/analysis.h"

void mexFunction( int /*nlhs*/, mxArray* plhs[], int nrhs, const mxArray* prhs[] ) {
   try {

      DML_MIN_ARGS( 1 );
      DML_MAX_ARGS( 5 );

      dip::Image const in = dml::GetImage( prhs[ 0 ] );
      dip::Image mask;
      if( nrhs > 1 ) {
         mask = dml::GetImage( prhs[ 1 ] );
      }

      dip::uint probes = 1000000;
      if( nrhs > 2 ) {
         probes = dml::GetUnsigned( prhs[ 2 ] );
      }

      dip::uint length = 100;
      if( nrhs > 3 ) {
         length = dml::GetUnsigned( prhs[ 3 ] );
      }

      dip::String estimator = dip::S::RANDOM;
      if( nrhs > 4 ) {
         estimator = dml::GetString( prhs[ 4 ] );
      }

      dip::Distribution out = dip::Semivariogram( in, mask, probes, length, estimator );

      plhs[ 0 ] = dml::GetArray( out );

   } DML_CATCH
}
