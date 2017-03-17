/*
 * DIPlib 3.0
 * This file contains definitions of functions that implement the basic morphological operators.
 *
 * (c)2017, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
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

#include "diplib.h"
#include "diplib/pixel_table.h"
#include "diplib/framework.h"
#include "diplib/overload.h"
#include "diplib/saturated_arithmetic.h"
#include "diplib/morphology.h"

namespace dip {

namespace {

// "texture", "object", "both", "dynamic"=="both"
enum class EdgeType {
      TEXTURE,
      OBJECT,
      BOTH
};

EdgeType GetEdgeType( String const& edgeType ) {
   if( edgeType == "texture" ) {
      return EdgeType::TEXTURE;
   } else if( edgeType == "object" ) {
      return EdgeType::OBJECT;
   } else if(( edgeType == "both" ) || ( edgeType == "dynamic" )) {
      return EdgeType::BOTH;
   } else {
      DIP_THROW( E::INVALID_FLAG );
   }
}

} // namespace


void Tophat(
      Image const& in,
      Image& out,
      StructuringElement const& se,
      String const& edgeType,
      String const& polarity,
      StringArray const& boundaryCondition
) {
   bool white = true;
   if( polarity == "black" ) {
      white = false;
   } else if( polarity != "white" ) {
      DIP_THROW( E::INVALID_FLAG );
   }
   DIP_START_STACK_TRACE
      EdgeType decodedEdgeType = GetEdgeType( edgeType );
      switch( decodedEdgeType ) {
         default:
         //case EdgeType::BOTH: // Annoying: CLion complains that the default case is useless, GCC complains that there's no default case.
            if( white ) {
               Image c_in = in.QuickCopy();
               Erosion( c_in, out, se, boundaryCondition );
               Subtract( c_in, out, out, out.DataType() );
            } else {
               Image c_in = in.QuickCopy();
               Dilation( c_in, out, se, boundaryCondition );
               Subtract( out, c_in, out, out.DataType() );
            }
            break;
         case EdgeType::TEXTURE:
            if( white ) {
               Image c_in = in.QuickCopy();
               Opening( c_in, out, se, boundaryCondition );
               Subtract( c_in, out, out, out.DataType() );
            } else {
               Image c_in = in.QuickCopy();
               Closing( c_in, out, se, boundaryCondition );
               Subtract( out, c_in, out, out.DataType() );
            }
            break;
         case EdgeType::OBJECT:
            if( white ) {
               Image tmp;
               Erosion( in, tmp, se, boundaryCondition );
               Dilation( tmp, out, se, boundaryCondition );
               Subtract( out, tmp, out, out.DataType() );
            } else {
               Image tmp;
               Dilation( in, tmp, se, boundaryCondition );
               Erosion( tmp, out, se, boundaryCondition );
               Subtract( tmp, out, out, out.DataType() );
            }
            break;
      }
   DIP_END_STACK_TRACE
}

void MorphologicalThreshold(
      Image const& in,
      Image& out,
      StructuringElement const& se,
      String const& edgeType,
      StringArray const& boundaryCondition
) {
   DIP_START_STACK_TRACE
      EdgeType decodedEdgeType = GetEdgeType( edgeType );
      Image tmp;
      switch( decodedEdgeType ) {
         default:
         //case EdgeType::BOTH: // Annoying: CLion complains that the default case is useless, GCC complains that there's no default case.
            Dilation( in, tmp, se, boundaryCondition );
            Erosion( in, out, se, boundaryCondition );
            Add( out, tmp, out, out.DataType() );
            Divide( out, 2, out, out.DataType() );
            break;
         case EdgeType::TEXTURE:
            Closing( in, tmp, se, boundaryCondition );
            Opening( in, out, se, boundaryCondition );
            Add( out, tmp, out, out.DataType() );
            Divide( out, 2, out, out.DataType() );
            break;
         case EdgeType::OBJECT: {
            Image c_in = in.QuickCopy();
            Dilation( c_in, tmp, se, boundaryCondition );
            Erosion( tmp, out, se, boundaryCondition );
            Subtract( tmp, out, out, out.DataType() );
            Erosion( c_in, tmp, se, boundaryCondition );
            Add( out, tmp, out, out.DataType() );
            Dilation( tmp, tmp, se, boundaryCondition );
            Subtract( out, tmp, out, out.DataType() );
            Divide( out, 2, out, out.DataType() );
            Add( c_in, out, out, out.DataType() );
            break;
         }
      }
   DIP_END_STACK_TRACE
}

void MorphologicalGist(
      Image const& in,
      Image& out,
      StructuringElement const& se,
      String const& edgeType,
      StringArray const& boundaryCondition
) {
   DIP_START_STACK_TRACE
      EdgeType decodedEdgeType = GetEdgeType( edgeType );
      Image tmp;
      Image c_in = in.QuickCopy();
      switch( decodedEdgeType ) {
         default:
         //case EdgeType::BOTH: // Annoying: CLion complains that the default case is useless, GCC complains that there's no default case.
            Dilation( c_in, tmp, se, boundaryCondition );
            Erosion( c_in, out, se, boundaryCondition );
            Add( tmp, out, out, out.DataType() );
            Divide( out, 2, out, out.DataType() );
            Subtract( c_in, out, out, out.DataType() );
            break;
         case EdgeType::TEXTURE:
            Closing( c_in, tmp, se, boundaryCondition );
            Opening( c_in, out, se, boundaryCondition );
            Add( tmp, out, out, out.DataType() );
            Divide( out, 2, out, out.DataType() );
            Subtract( c_in, out, out, out.DataType() );
            break;
         case EdgeType::OBJECT:
            Dilation( c_in, tmp, se, boundaryCondition );
            Erosion( tmp, out, se, boundaryCondition );
            Subtract( out, tmp, out, out.DataType() );
            Erosion( c_in, tmp, se, boundaryCondition );
            Subtract( out, tmp, out, out.DataType() );
            Dilation( tmp, tmp, se, boundaryCondition );
            Add( out, tmp, out, out.DataType() );
            Divide( out, 2, out, out.DataType() );
            break;
      }
   DIP_END_STACK_TRACE
}

void MorphologicalRange(
      Image const& in,
      Image& out,
      StructuringElement const& se,
      String const& edgeType,
      StringArray const& boundaryCondition
) {
   DIP_START_STACK_TRACE
      EdgeType decodedEdgeType = GetEdgeType( edgeType );
      Image tmp;
      switch( decodedEdgeType ) {
         default:
         //case EdgeType::BOTH: // Annoying: CLion complains that the default case is useless, GCC complains that there's no default case.
            Dilation( in, tmp, se, boundaryCondition );
            Erosion( in, out, se, boundaryCondition );
            Subtract( tmp, out, out, out.DataType() );
            break;
         case EdgeType::TEXTURE:
            Closing( in, tmp, se, boundaryCondition );
            Opening( in, out, se, boundaryCondition );
            Subtract( tmp, out, out, out.DataType() );
            break;
         case EdgeType::OBJECT: {
            Image c_in = in.QuickCopy();
            Dilation( c_in, tmp, se, boundaryCondition );
            Erosion( tmp, out, se, boundaryCondition );
            Subtract( tmp, out, out, out.DataType() );
            Erosion( c_in, tmp, se, boundaryCondition );
            Subtract( out, tmp, out, out.DataType() );
            Dilation( tmp, tmp, se, boundaryCondition );
            Add( out, tmp, out, out.DataType() );
            break;
         }
      }
   DIP_END_STACK_TRACE
}

void Lee(
      Image const& in,
      Image& out,
      StructuringElement const& se,
      String const& edgeType,
      String const& sign,
      StringArray const& boundaryCondition
) {
   Image out2;
   DIP_START_STACK_TRACE
      EdgeType decodedEdgeType = GetEdgeType( edgeType );
      Image c_in = in.QuickCopy();
      switch( decodedEdgeType ) {
         default:
         //case EdgeType::BOTH: // Annoying: CLion complains that the default case is useless, GCC complains that there's no default case.
            Dilation( c_in, out, se, boundaryCondition );
            Subtract( out, c_in, out, out.DataType() );
            Erosion( c_in, out2, se, boundaryCondition );
            Subtract( c_in, out2, out2, out2.DataType() );
            break;
         case EdgeType::TEXTURE:
            Closing( c_in, out, se, boundaryCondition );
            Subtract( out, c_in, out, out.DataType() );
            Opening( c_in, out2, se, boundaryCondition );
            Subtract( c_in, out2, out2, out2.DataType() );
            break;
         case EdgeType::OBJECT: {
            Image tmp;
            Dilation( c_in, tmp, se, boundaryCondition );
            Erosion( tmp, out, se, boundaryCondition );
            Subtract( tmp, out, out, out.DataType() );
            Erosion( c_in, tmp, se, boundaryCondition );
            Dilation( tmp, out2, se, boundaryCondition );
            Subtract( out2, tmp, out2, out2.DataType() );
            break;
         }
      }
   DIP_END_STACK_TRACE
   if( sign == "unsigned" ) {
      //Min( out, out2, out ); // TODO: implement Min
   } else if( sign == "signed" ) {
      //SignedMinimum( out, out2, out ); // TODO: implement SignedMinimum
   } else {
      DIP_THROW( E::INVALID_FLAG );
   }
}

void MorphologicalSmoothing(
      Image const& in,
      Image& out,
      StructuringElement const& se,
      String const& mode,
      StringArray const& boundaryCondition
) {
   if( mode == "open-close" ) {
      Opening( in, out, se, boundaryCondition );
      Closing( out, out, se, boundaryCondition );
   } else if( mode == "close-open" ) {
      Closing( in, out, se, boundaryCondition );
      Opening( out, out, se, boundaryCondition );
   } else if( mode == "average" ) {
      Image tmp;
      Opening( in, tmp, se, boundaryCondition );
      Closing( tmp, tmp, se, boundaryCondition );
      Closing( in, out, se, boundaryCondition );
      Opening( out, out, se, boundaryCondition );
      Add( tmp, out, out, out.DataType() );
      Divide( out, 2, out, out.DataType() );
   } else {
      DIP_THROW( E::INVALID_FLAG );
   }
}

void MultiScaleMorphologicalGradient(
      Image const& in,
      Image& out,
      dip::uint upperSize,
      dip::uint lowerSize,
      String const& shape,
      StringArray const& boundaryCondition
) {
   DIP_THROW_IF( lowerSize < 0.0, "lowerSize out-of-range" );
   DIP_THROW_IF( upperSize < 0.0, "upperSize out-of-range" );
   DIP_THROW_IF( lowerSize > upperSize, "lowerSize > upperSize" );
   bool first = true;
   Image dila;
   Image eros;
   for( dip::uint ii = lowerSize; ii <= upperSize; ++ii ) {
      StructuringElement se1( { 2.0 * ii + 1.0 }, shape );
      StructuringElement se2( { 2.0 * ( ii - 1 ) + 1.0 }, shape );
      Dilation( in, dila, se1, boundaryCondition );
      Erosion( in, eros, se1, boundaryCondition );
      Subtract( dila, eros, eros, dila.DataType() );
      if( first ) {
         Erosion( eros, out, se2, boundaryCondition );
         first = false;
      } else {
         Erosion( eros, eros, se2, boundaryCondition );
         Add( eros, out, out, out.DataType() );
      }
   }
   Divide( out, static_cast< uint32 >( upperSize - lowerSize + 1 ), out.DataType() );
   // About the cast to uint32: we can't make an Image with a dip::uint data type, so we need to cast to a posisble type.
   // It doesn't really matter which type we pick, as long as there's no loss. We could pick uint8 here too, as it's
   // unlikely that anyone would use a filtersize larger than 255. But uint32 is the safest option, and it won't
   // affect performance, it's just a 0D image.
}

void MorphologicalLaplace(
      Image const& in,
      Image& out,
      StructuringElement const& se,
      StringArray const& boundaryCondition
) {
   Image c_in = in;
   Image tmp;
   Dilation( c_in, tmp, se, boundaryCondition );
   Erosion( c_in, out, se, boundaryCondition );
   Add( tmp, out, out, out.DataType() );
   Divide( out, 2, out, out.DataType() );
   Subtract( out, c_in, out, out.DataType() );
}

} // namespace dip
