/*
 * DIPlib 3.0
 * This file contains definitions for the Image class and related functions.
 *
 * (c)2014-2016, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 */


//
// NOTE!
// This file is included through diplib.h -- no need to include directly
//


#ifndef DIP_IMAGE_H
#define DIP_IMAGE_H

#include <memory>
#include <functional>
#include <utility>

#include "diplib/datatype.h"
#include "diplib/tensor.h"
#include "diplib/physdims.h"
#include "diplib/clamp_cast.h"

#include <iostream>

/// \file
/// Defines the dip::Image class and support functions. This file is always included through diplib.h.


/// The dip namespace contains all the library functionality.
namespace dip {


//
// Support for external interfaces
//

/// Support for external interfaces. Software using DIPlib might want to
/// control how the image data is allocated. Such software should derive
/// a class from this one, and assign a pointer to it into each of the
/// images that it creates, through Image::SetExternalInterface().
/// The caller will maintain ownership of the interface.
///
/// See dip_matlab_interface.h for an example of how to create an ExternalInterface.
class ExternalInterface {
   public:
      /// Allocates the data for an image. The function is free to modify
      /// `strides` and `tstride` if desired, though they will be set
      /// to the normal values by the calling function.
      virtual std::shared_ptr< void > AllocateData(
            UnsignedArray const& sizes,
            IntegerArray& strides,
            dip::Tensor const& tensor,
            dip::sint& tstride,
            dip::DataType datatype
      ) = 0;
};


//
// Functor that converts indices or offsets to coordinates.
//

/// Objects of this class are returned by dip::Image::OffsetToCoordinatesComputer
/// and dip::Image::IndexToCoordinatesComputer, and act as functors.
/// Call it with an offset or index (depending on which function created the
/// functor), and it will return the coordinates:
///
///     auto coordComp = img.OffsetToCoordinatesComputer();
///     auto coords1 = coordComp( offset1 );
///     auto coords2 = coordComp( offset2 );
///     auto coords3 = coordComp( offset3 );
///
/// Note that the coordinates must be inside the image domain, if the offset given
/// does not correspond to one of the image's pixels, the result is meaningless.
class CoordinatesComputer {
   public:
      CoordinatesComputer( UnsignedArray const& sizes, IntegerArray const& strides );

      UnsignedArray operator()( dip::sint offset ) const;

   private:
      IntegerArray strides_; // a copy of the image's strides array, but with all positive values
      IntegerArray sizes_;   // a copy of the image's sizes array, but with negative values where the strides are negative
      UnsignedArray index_;  // sorted indices to the strides array (largest to smallest)
      dip::sint offset_;     // offset needed to handle negative strides
};


//
// The Image class
//

class Image;

/// An array of images
using ImageArray = std::vector< Image >;

/// An array of const images
using ConstImageArray = std::vector< Image const >;

/// An array of image references
using ImageRefArray = std::vector< std::reference_wrapper< Image >>;

/// An array of const image references
using ImageConstRefArray = std::vector< std::reference_wrapper< Image const >>;

// The class is documented in the file src/documentation/image.md
class Image {

   public:

      //
      // Default constructor
      //

      /// The default-initialized image is 0D (an empty sizes array), one tensor element, dip::DT_SFLOAT,
      /// and raw (it has no data segment).
      Image() {}

      //
      // Other constructors
      //

      /// Forged image of given sizes and data type.
      explicit Image( UnsignedArray const& sizes, dip::uint tensorElems = 1, dip::DataType dt = DT_SFLOAT ) :
            dataType_( dt ),
            sizes_( sizes ),
            tensor_( tensorElems ) {
         Forge();
      }

      /// Move constructor moves all data from `src` to `*this`.
      Image( Image&& src ) = default;

      /// Forged image similar to `src`; the data is not copied.
      /// This constructor is explicit, meaning that it is called for the syntax
      ///
      ///     Image newimg( img );
      ///
      /// but not for the syntax
      ///
      ///     Image newimg = img;
      ///
      /// This latter syntax is therefore invalid, unfortunately. Instead, use
      ///
      ///     Image newimg;
      ///     newimg = img;
      ///
      /// to invoke the copy assignment operator.
      // NOTE: this is the copy constructor. Because we change the definition here, we also need
      // to define the move constructor, the copy assignment operator, and the move assignment operator.
      explicit Image( Image const& src ) :
            dataType_( src.dataType_ ),
            sizes_( src.sizes_ ),
            strides_( src.strides_ ),
            tensor_( src.tensor_ ),
            tensorStride_( src.tensorStride_ ),
            colorSpace_( src.colorSpace_ ),
            pixelSize_( src.pixelSize_ ),
            externalInterface_( src.externalInterface_ ) {
         Forge();
      }

      /// Forged image similar to `src`, but with different data type; the data is not copied.
      Image( Image const& src, dip::DataType dt ) :
            dataType_( dt ),
            sizes_( src.sizes_ ),
            strides_( src.strides_ ),
            tensor_( src.tensor_ ),
            tensorStride_( src.tensorStride_ ),
            colorSpace_( src.colorSpace_ ),
            pixelSize_( src.pixelSize_ ),
            externalInterface_( src.externalInterface_ ) {
         Forge();
      }

      /// Create a 0-D image with the value and data type of `p`.
      template< typename T >
      explicit Image( T p ) {
         dataType_ = dip::DataType( p );
         Forge();       // sizes is empty by default
         * static_cast< T* >( origin_ ) = p;
      }

      /// Create a 0-D image with the value of `p` and the given data type.
      template< typename T >
      Image( T p, dip::DataType dt ) {
         dataType_ = dt;
         Forge();       // sizes is empty by default
         switch( dataType_ ) {
            case dip::DT_BIN:
               * static_cast< bin* >( origin_ ) = clamp_cast< bin >( p );
               break;
            case dip::DT_UINT8:
               * static_cast< uint8* >( origin_ ) = clamp_cast< uint8 >( p );
               break;
            case dip::DT_UINT16:
               * static_cast< uint16* >( origin_ ) = clamp_cast< uint16 >( p );
               break;
            case dip::DT_UINT32:
               * static_cast< uint32* >( origin_ ) = clamp_cast< uint32 >( p );
               break;
            case dip::DT_SINT8:
               * static_cast< sint8* >( origin_ ) = clamp_cast< sint8 >( p );
               break;
            case dip::DT_SINT16:
               * static_cast< sint16* >( origin_ ) = clamp_cast< sint16 >( p );
               break;
            case dip::DT_SINT32:
               * static_cast< sint32* >( origin_ ) = clamp_cast< sint32 >( p );
               break;
            case dip::DT_SFLOAT:
               * static_cast< sfloat* >( origin_ ) = clamp_cast< sfloat >( p );
               break;
            case dip::DT_DFLOAT:
               * static_cast< dfloat* >( origin_ ) = clamp_cast< dfloat >( p );
               break;
            case dip::DT_SCOMPLEX:
               * static_cast< scomplex* >( origin_ ) = clamp_cast< scomplex >( p );
               break;
            case dip::DT_DCOMPLEX:
               * static_cast< dcomplex* >( origin_ ) = clamp_cast< dcomplex >( p );
               break;
            default: dip_Throw( dip::E::DATA_TYPE_NOT_SUPPORTED );
         }
      }

      /// Create an image around existing data.
      Image(
            std::shared_ptr< void > data,        // points at the data block, not necessarily the origin!
            dip::DataType dataType,
            UnsignedArray const& sizes,
            IntegerArray const& strides,
            dip::Tensor const& tensor,
            dip::sint tensorStride,
            ExternalInterface* externalInterface = nullptr
      ) :
            dataType_( dataType ),
            sizes_( sizes ),
            strides_( strides ),
            tensor_( tensor ),
            tensorStride_( tensorStride ),
            dataBlock_( data ),
            externalInterface_( externalInterface ) {
         dip::uint size;
         dip::sint start;
         GetDataBlockSizeAndStartWithTensor( size, start );
         origin_ = static_cast< uint8* >( dataBlock_.get() ) + start * dataType_.SizeOf();
      }

      //
      // Copy and move assignment operators
      //

      /// Move assignment has `*this` steal all data from `src`.
      Image& operator=( Image&& src ) = default;

      /// Copy assignment makes `*this` into an identical copy of `src`, with shared data.
      // NOTE: we need to explicitly define this because the copy constructor does something different...
      Image& operator=( Image const& src ) {
         dataType_ = src.dataType_;
         sizes_ = src.sizes_;
         strides_ = src.strides_;
         tensor_ = src.tensor_;
         tensorStride_ = src.tensorStride_;
         protect_ = src.protect_;
         colorSpace_ = src.colorSpace_;
         pixelSize_ = src.pixelSize_;
         dataBlock_ = src.dataBlock_;
         origin_ = src.origin_;
         externalInterface_ = src.externalInterface_;
         return *this;
      }

      /// Swaps `*this` and `other`.
      void swap( Image& other ) {
         using std::swap;
         swap( dataType_, other.dataType_ );
         swap( sizes_, other.sizes_ );
         swap( strides_, other.strides_ );
         swap( tensor_, other.tensor_ );
         swap( tensorStride_, other.tensorStride_ );
         swap( protect_, other.protect_ );
         swap( colorSpace_, other.colorSpace_ );
         swap( pixelSize_, other.pixelSize_ );
         swap( dataBlock_, other.dataBlock_ );
         swap( origin_, other.origin_ );
         swap( externalInterface_, other.externalInterface_ );
      }

      //
      // Sizes
      //

      /// Get the number of spatial dimensions.
      dip::uint Dimensionality() const {
         return sizes_.size();
      }

      /// Get a const reference to the sizes array (image size).
      UnsignedArray const& Sizes() const {
         return sizes_;
      }

      /// Get the image size along a specific dimension.
      dip::uint Size( dip::uint dim ) const {
         return sizes_[ dim ];
      }

      /// Get the number of pixels.
      dip::uint NumberOfPixels() const {
         return sizes_.product();
      }

      /// Get the number of samples.
      dip::uint NumberOfSamples() const {
         return NumberOfPixels() * TensorElements();
      }

      /// Set the image sizes; the image must be raw.
      void SetSizes( UnsignedArray const& d ) {
         dip_ThrowIf( IsForged(), E::IMAGE_NOT_RAW );
         sizes_ = d;
      }

      //
      // Strides
      //

      /// Get a const reference to the strides array.
      IntegerArray const& Strides() const {
         return strides_;
      }

      /// Get the stride along a specific dimension.
      dip::sint Stride( dip::uint dim ) const {
         return strides_[ dim ];
      }

      /// Get the tensor stride.
      dip::sint TensorStride() const {
         return tensorStride_;
      }

      /// Set the strides array; the image must be raw.
      void SetStrides( IntegerArray const& s ) {
         dip_ThrowIf( IsForged(), E::IMAGE_NOT_RAW );
         strides_ = s;
      }

      /// Set the tensor stride; the image must be raw.
      void SetTensorStride( dip::sint ts ) {
         dip_ThrowIf( IsForged(), E::IMAGE_NOT_RAW );
         tensorStride_ = ts;
      }

      /// Test if all the pixels are contiguous.
      /// If all pixels are contiguous, you can traverse the whole image,
      /// accessing each of the pixles, using a single stride with a value
      /// of 1. To do so, you don't necessarily start at the origin; if any
      /// of the strides is negative, the origin of the contiguous data will
      /// be elsewhere.
      /// Use GetSimpleStrideAndOrigin to get a pointer to the origin
      /// of the contiguous data.
      ///
      /// The image must be forged.
      /// \see GetSimpleStrideAndOrigin, HasSimpleStride, HasNormalStrides, Strides, TensorStride.
      bool HasContiguousData() const {
         dip_ThrowIf( !IsForged(), E::IMAGE_NOT_FORGED );
         dip::uint size = NumberOfPixels() * TensorElements();
         dip::sint start;
         dip::uint sz;
         GetDataBlockSizeAndStartWithTensor( sz, start );
         return sz == size;
      }

      /// Test if strides are as by default; the image must be forged.
      bool HasNormalStrides() const;

      /// Test if the whole image can be traversed with a single stride
      /// value. This is similar to HasContiguousData, but the stride
      /// value can be larger than 1.
      /// Use GetSimpleStrideAndOrigin to get a pointer to the origin
      /// of the contiguous data. Note that this only tests spatial
      /// dimesions, the tensor dimension must still be accessed separately.
      ///
      /// The image must be forged.
      /// \see GetSimpleStrideAndOrigin, HasContiguousData, HasNormalStrides, Strides, TensorStride.
      bool HasSimpleStride() const {
         void* p;
         dip::uint s;
         GetSimpleStrideAndOrigin( s, p );
         return p != nullptr;
      }

      /// Return a pointer to the start of the data and a single stride to
      /// walk through all pixels. If this is not possible, the function
      /// sets `porigin==nullptr`. Note that this only tests spatial dimesions,
      /// the tensor dimension must still be accessed separately.
      ///
      /// The image must be forged.
      /// \see HasSimpleStride, HasContiguousData, HasNormalStrides, Strides, TensorStride, Data.
      void GetSimpleStrideAndOrigin( dip::uint& stride, void*& origin ) const;

      /// Checks to see if `other` and `this` have their dimensions ordered in
      /// the same way. Traversing more than one image using simple strides is only
      /// possible if they have their dimensions ordered in the same way, otherwise
      /// the simple stride does not visit the pixels in the same order in the
      /// various images.
      ///
      /// The images must be forged.
      /// \see HasSimpleStride, GetSimpleStrideAndOrigin, HasContiguousData.
      bool HasSameDimensionOrder( Image const& other ) const;

      //
      // Tensor
      //

      /// Get the tensor sizes; the array returned can have 0, 1 or
      /// 2 elements, as those are the allowed tensor dimensionalities.
      UnsignedArray TensorSizes() const {
         return tensor_.Sizes();
      }

      /// Get the number of tensor elements, the product of the elements
      /// in the array returned by TensorSizes.
      dip::uint TensorElements() const {
         return tensor_.Elements();
      }

      /// Get the number of tensor columns.
      dip::uint TensorColumns() const {
         return tensor_.Columns();
      }

      /// Get the number of tensor rows.
      dip::uint TensorRows() const {
         return tensor_.Rows();
      }

      /// Get the tensor shape.
      enum dip::Tensor::Shape TensorShape() const {
         return tensor_.Shape();
      }

      // Note: This function is the reason we refer to the Tensor class as
      // dip::Tensor everywhere in this file.

      /// Get the tensor shape.
      dip::Tensor const& Tensor() const {
         return tensor_;
      }

      /// True for non-tensor (grey-value) images.
      bool IsScalar() const {
         return tensor_.IsScalar();
      }

      /// True for vector images, where the tensor is one-dimensional.
      bool IsVector() const {
         return tensor_.IsVector();
      }

      /// Set tensor sizes; the image must be raw.
      void SetTensorSizes( UnsignedArray const& tdims ) {
         dip_ThrowIf( IsForged(), E::IMAGE_NOT_RAW );
         tensor_.SetSizes( tdims );
      }

      /// Set tensor sizes; the image must be raw.
      void SetTensorSizes( dip::uint nelems ) {
         dip_ThrowIf( IsForged(), E::IMAGE_NOT_RAW );
         tensor_.SetVector( nelems );
      }

      //
      // Data Type
      //

      // Note: This function is the reason we refer to the DataType class as
      // dip::DataType everywhere in this file.

      /// Get the image's data type.
      dip::DataType DataType() const {
         return dataType_;
      }

      /// Set the image's data type; the image must be raw.
      void SetDataType( dip::DataType dt ) {
         dip_ThrowIf( IsForged(), E::IMAGE_NOT_RAW );
         dataType_ = dt;
      }

      //
      // Color space
      //

      /// Get the image's color space name.
      String const& ColorSpace() const { return colorSpace_; }

      /// Returns true if the image is in color, false if the image is grey-valued.
      bool IsColor() const { return !colorSpace_.empty(); }

      /// Sets the image's color space name; this causes the image to be a color
      /// image, but will cause errors to occur if the number of tensor elements
      /// does not match the expected number of channels for the given color space.
      void SetColorSpace( String const& cs ) { colorSpace_ = cs; }

      /// Resets the image's color space information, turning the image into a non-color image.
      void ResetColorSpace() { colorSpace_.clear(); }

      //
      // Pixel size
      //

      // Note: This function is the reason we refer to the PixelSize class as
      // dip::PixelSize everywhere in this file.

      /// Get the pixels's size in physical units, by reference, allowing to modify it at will.
      dip::PixelSize& PixelSize() { return pixelSize_; }

      /// Get the pixels's size in physical units.
      dip::PixelSize const& PixelSize() const { return pixelSize_; }

      /// Set the pixels's size.
      void SetPixelSize( dip::PixelSize const& ps ) {
         pixelSize_ = ps;
      }

      /// Returns true if the pixel has physical dimensions.
      bool HasPixelSize() const { return pixelSize_.IsDefined(); }

      /// Returns true if the pixel has the same size in all dimensions.
      bool IsIsotropic() const { return pixelSize_.IsIsotropic(); }

      /// Converts a size in pixels to a size in phyical units.
      PhysicalQuantityArray PixelsToPhysical( FloatArray const& in ) const { return pixelSize_.ToPhysical( in ); }

      /// Converts a size in physical units to a size in pixels.
      FloatArray PhysicalToPixels( PhysicalQuantityArray const& in ) const { return pixelSize_.ToPixels( in ); }

      //
      // Utility functions
      //

      /// Compare properties of an image against a template, either
      /// returns true/false or throws an error.
      bool CompareProperties(
            Image const& src,
            Option::CmpProps cmpProps,
            Option::ThrowException throwException = Option::ThrowException::doThrow
      ) const;

      /// Check image properties, either returns true/false or throws an error.
      ///
      bool CheckProperties(
            dip::uint ndims,
            dip::DataType::Classes dts,
            Option::ThrowException throwException = Option::ThrowException::doThrow
      ) const;

      /// Check image properties, either returns true/false or throws an error.
      bool CheckProperties(
            UnsignedArray const& sizes,
            dip::DataType::Classes dts,
            Option::ThrowException throwException = Option::ThrowException::doThrow
      ) const;

      /// Check image properties, either returns true/false or throws an error.
      bool CheckProperties(
            UnsignedArray const& sizes,
            dip::uint tensorElements,
            dip::DataType::Classes dts,
            Option::ThrowException throwException = Option::ThrowException::doThrow
      ) const;

      /// Copy all image properties from `src`; the image must be raw.
      void CopyProperties( Image const& src ) {
         dip_ThrowIf( IsForged(), E::IMAGE_NOT_RAW );
         dataType_ = src.dataType_;
         sizes_ = src.sizes_;
         strides_ = src.strides_;
         tensor_ = src.tensor_;
         tensorStride_ = src.tensorStride_;
         colorSpace_ = src.colorSpace_;
         pixelSize_ = src.pixelSize_;
         if( !externalInterface_ ) {
            externalInterface_ = src.externalInterface_;
         }
      }

      //
      // Data
      // Defined in src/library/image_data.cpp
      //

      /// Get pointer to the data segment. This is useful to identify
      /// the data segment, but not to access the pixel data stored in
      /// it. Use dip::Image::Origin instead. The image must be forged.
      /// \see Origin, IsShared, ShareCount, SharesData.
      void* Data() const {
         dip_ThrowIf( !IsForged(), E::IMAGE_NOT_FORGED );
         return dataBlock_.get();
      }

      /// Check to see if the data segment is shared with other images.
      /// The image must be forged.
      /// \see Data, ShareCount, SharesData.
      bool IsShared() const {
         dip_ThrowIf( !IsForged(), E::IMAGE_NOT_FORGED );
         return !dataBlock_.unique();
      }

      /// Get the number of images that share their data with this image.
      /// The count is always at least 1. If the count is 1, dip::Image::IsShared is
      /// false. The image must be forged.
      /// \see Data, IsShared, SharesData.
      dip::uint ShareCount() const {
         dip_ThrowIf( !IsForged(), E::IMAGE_NOT_FORGED );
         return static_cast< dip::uint >( dataBlock_.use_count() );
      }

      /// Determine if this image shares its data pointer with `other`.
      /// Both images must be forged.
      ///
      /// Note that sharing the data pointer
      /// does not imply that the two images share any pixel data, as it
      /// is possible for the two images to represent disjoint windows
      /// into the same data block. To determine if any pixels are shared,
      /// use Aliases.
      ///
      /// \see Aliases, IsIdenticalView, IsOverlappingView, Data, IsShared, ShareCount.
      bool SharesData( Image const& other ) const {
         dip_ThrowIf( !IsForged(), E::IMAGE_NOT_FORGED );
         dip_ThrowIf( !other.IsForged(), E::IMAGE_NOT_FORGED );
         return dataBlock_ == other.dataBlock_;
      }

      /// Determine if this image shares any samples with `other`.
      /// If `true`, writing into this image will change the data in
      /// `other`, and vice-versa.
      ///
      /// Both images must be forged.
      /// \see SharesData, IsIdenticalView, IsOverlappingView, Alias.
      bool Aliases( Image const& other ) const;

      /// Determine if this image and `other` offer an identical view of the
      /// same set of pixels. If `true`, changing one sample in this image will
      /// change the same sample in `other`.
      ///
      /// Both images must be forged.
      /// \see SharesData, Aliases, IsOverlappingView.
      bool IsIdenticalView( Image const& other ) const {
         dip_ThrowIf( !IsForged(), E::IMAGE_NOT_FORGED );
         dip_ThrowIf( !other.IsForged(), E::IMAGE_NOT_FORGED );
         // We don't need to check dataBlock_ here, as origin_ is a pointer, not an offset.
         return ( origin_ == other.origin_ ) &&
                ( dataType_ == other.dataType_ ) &&
                ( strides_ == other.strides_ ) &&
                ( tensorStride_ == other.tensorStride_ );
      }

      /// Determine if this image and `other` offer different views of the
      /// same data segment, and share at least one sample. If `true`, changing one
      /// sample in this image might change a different sample in `other`.
      /// An image with an overlapping view of an input image cannot be used as output to a
      /// filter, as it might change input data that still needs to be used. Use this function
      /// to test whether to use the existing data segment or allocate a new one.
      ///
      /// Both images must be forged.
      /// \see SharesData, Aliases, IsIdenticalView.
      bool IsOverlappingView( Image const& other ) const {
         // Aliases checks for both images to be forged.
         return Aliases( other ) && !IsIdenticalView( other );
      }

      /// Determine if this image and any of those in `other` offer different views of the
      /// same data segment, and share at least one sample. If `true`, changing one
      /// sample in this image might change a different sample in at least one image in `other`.
      /// An image with an overlapping view of an input image cannot be used as output to a
      /// filter, as it might change input data that still needs to be used. Use this function
      /// to test whether to use the existing data segment or allocate a new one.
      ///
      /// `*this` must be forged.
      /// \see SharesData, Aliases, IsIdenticalView.
      bool IsOverlappingView( ImageConstRefArray const& other ) const {
         for( dip::uint ii = 0; ii < other.size(); ++ii ) {
            Image const& tmp = other[ ii ].get();
            if( tmp.IsForged() && IsOverlappingView( tmp )) {
               return true;
            }
         }
         return false;
      }

      /// Determine if this image and any of those in `other` offer different views of the
      /// same data segment, and share at least one sample. If `true`, changing one
      /// sample in this image might change a different sample in at least one image in `other`.
      /// An image with an overlapping view of an input image cannot be used as output to a
      /// filter, as it might change input data that still needs to be used. Use this function
      /// to test whether to use the existing data segment or allocate a new one.
      ///
      /// `*this` must be forged.
      /// \see SharesData, Aliases, IsIdenticalView.
      bool IsOverlappingView( ImageArray const& other ) const {
         for( dip::uint ii = 0; ii < other.size(); ++ii ) {
            Image const& tmp = other[ ii ];
            if( tmp.IsForged() && IsOverlappingView( tmp )) {
               return true;
            }
         }
         return false;
      }

      /// Allocate data segment. This function allocates a memory block
      /// to hold the pixel data. If the stride array is consistent with
      /// size array, and leads to a compact data segment, it is honored.
      /// Otherwise, it is ignored and a new stride array is created that
      /// leads to an image that dip::Image::HasNormalStrides. If an
      /// external interface is registered for this image, the resulting
      /// strides might be different from normal, and the exising stride
      /// array need not be honored even if it would yield a compact
      /// data segment.
      void Forge();

      /// Modify image properties and forge the image. ReForge has three
      /// signatures that match three image constructors. ReForge will try
      /// to avoid freeing the current data segment and allocating a new one.
      /// This version will cause `*this` to be an identical copy of `src`,
      /// but with uninitialized data.
      void ReForge( Image const& src ) {
         ReForge( src, src.dataType_ );
      }

      /// Modify image properties and forge the image. ReForge has three
      /// signatures that match three image constructors. ReForge will try
      /// to avoid freeing the current data segment and allocating a new one.
      /// This version will cause `*this` to be an identical copy of `src`,
      /// but with a different data type and uninitialized data.
      void ReForge( Image const& src, dip::DataType dt ) {
         ReForge( src.sizes_, src.tensor_.Elements(), dt );
         tensor_ = src.tensor_;
         colorSpace_ = src.colorSpace_;
         pixelSize_ = src.pixelSize_;
      }

      /// Modify image properties and forge the image. ReForge has three
      /// signatures that match three image constructors. ReForge will try
      /// to avoid freeing the current data segment and allocating a new one.
      /// This version will cause `*this` to be of the requested size and
      /// data type.
      void ReForge( UnsignedArray const& sizes, dip::uint tensorElems = 1, dip::DataType dt = DT_SFLOAT );

      /// Dissasociate the data segment from the image. If there are no
      /// other images using the same data segment, it will be freed.
      void Strip() {
         if( IsForged() ) {
            dip_ThrowIf( IsProtected(), "Image is protected" );
            dataBlock_ = nullptr; // Automatically frees old memory if no other pointers to it exist.
            origin_ = nullptr;    // Keep this one in sync!
         }
      }

      /// Test if forged.
      bool IsForged() const {
         return origin_ != nullptr;
      }

      /// Set protection flag.
      void Protect( bool set = true ) {
         protect_ = set;
      }

      /// Test if protected.
      bool IsProtected() const {
         return protect_;
      }

      /// Set external interface pointer; the image must be raw.
      void SetExternalInterface( ExternalInterface* ei ) {
         dip_ThrowIf( IsForged(), E::IMAGE_NOT_RAW );
         externalInterface_ = ei;
      }

      //
      // Pointers, Offsets, Indices
      // Defined in src/library/image_data.cpp
      //

      /// Get pointer to the first sample in the image, the first tensor
      /// element at coordinates (0,0,0,...); the image must be forged.
      void* Origin() const {
         dip_ThrowIf( !IsForged(), E::IMAGE_NOT_FORGED );
         return origin_;
      }

      /// Get a pointer to the pixel given by the offset. Cast the pointer
      /// to the right type before use. No check is made on the index.
      ///
      /// \see Origin, Offset, OffsetToCoordinates
      void* Pointer( dip::sint offset ) const {
         return static_cast< uint8* >( origin_ ) + offset * dataType_.SizeOf();
      }

      /// Get a pointer to the pixel given by the coordinates index. Cast the
      /// pointer to the right type before use. This is not the most efficient
      /// way of indexing many pixels in the image.
      ///
      /// If `coords` is not within the image domain, an exception is thrown.
      ///
      /// The image must be forged.
      /// \see Origin, Offset, OffsetToCoordinates
      void* Pointer( UnsignedArray const& coords ) const {
         return Pointer( Offset( coords ));
      }

      /// Get a pointer to the pixel given by the coordinates index. Cast the
      /// pointer to the right type before use. This is not the most efficient
      /// way of indexing many pixels in the image.
      ///
      /// `coords` can be outside the image domain.
      ///
      /// The image must be forged.
      /// \see Origin, Offset, OffsetToCoordinates
      void* Pointer( IntegerArray const& coords ) const {
         return Pointer( Offset( coords ));
      }

      /// Compute offset given coordinates. The offset needs to be multiplied
      /// by the number of bytes of each sample to become a memory offset
      /// within the image.
      ///
      /// If `coords` is not within the image domain, an exception is thrown.
      ///
      /// The image must be forged.
      /// \see Origin, Pointer, OffsetToCoordinates
      dip::sint Offset( UnsignedArray const& coords ) const;

      /// Compute offset given coordinates. The offset needs to be multiplied
      /// by the number of bytes of each sample to become a memory offset
      /// within the image.
      ///
      /// `coords` can be outside the image domain.
      ///
      /// The image must be forged.
      /// \see Origin, Pointer, OffsetToCoordinates
      dip::sint Offset( IntegerArray const& coords ) const;

      /// Compute coordinates given an offset. If the image has any singleton-expanded
      /// dimensions, the computed coordinate along that dimension will always be 0.
      /// This is an expensive operation, use dip::Image::OffsetToCoordinatesComputer to make it
      /// more efficient when performing multiple computations in sequence.
      ///
      /// Note that the coordinates must be inside the image domain, if the offset given
      /// does not correspond to one of the image's pixels, the result is meaningless.
      ///
      /// The image must be forged.
      /// \see Offset, OffsetToCoordinatesComputer, IndexToCoordinates
      UnsignedArray OffsetToCoordinates( dip::sint offset ) const;

      /// Returns a functor that computes coordinates given an offset. This is
      /// more efficient than using dip::Image::OffsetToCoordinates when repeatedly
      /// computing offsets, but still requires complex calculations.
      ///
      /// The image must be forged.
      /// \see Offset, OffsetToCoordinates, IndexToCoordinates, IndexToCoordinatesComputer
      CoordinatesComputer OffsetToCoordinatesComputer() const;

      /// Compute linear index (not offset) given coordinates. This index is not
      /// related to the position of the pixel in memory, and should not be used
      /// to index many pixels in sequence.
      ///
      /// The image must be forged.
      /// \see IndexToCoordinates, Offset
      dip::uint Index( UnsignedArray const& coords ) const;

      /// Compute coordinates given a linear index. If the image has any singleton-expanded
      /// dimensions, the computed coordinate along that dimension will always be 0.
      /// This is an expensive operation, use dip::Image::IndexToCoordinatesComputer to make it
      /// more efficient when performing multiple computations in sequence.
      ///
      /// Note that the coordinates must be inside the image domain, if the index given
      /// does not correspond to one of the image's pixels, the result is meaningless.
      ///
      /// The image must be forged.
      /// \see Index, Offset, IndexToCoordinatesComputer, OffsetToCoordinates
      UnsignedArray IndexToCoordinates( dip::uint index ) const;

      /// Returns a functor that computes coordinates given a linear index. This is
      /// more efficient than using dip::Image::IndexToCoordinates, when repeatedly
      /// computing indices, but still requires complex calculations.
      ///
      /// The image must be forged.
      /// \see Index, Offset, IndexToCoordinates, OffsetToCoordinates, OffsetToCoordinatesComputer
      CoordinatesComputer IndexToCoordinatesComputer() const;

      //
      // Modifying geometry of a forged image without data copy
      // Defined in src/library/image_manip.cpp
      //

      /// Permute dimensions. This function allows to re-arrange the dimensions
      /// of the image in any order. It also allows to remove singleton dimensions
      /// (but not to add them, should we add that? how?). For example, given
      /// an image with size `{ 30, 1, 50 }`, and an `order` array of
      /// `{ 2, 0 }`, the image will be modified to have size `{ 50, 30 }`.
      /// Dimension number 1 is not referenced, and was removed (this can only
      /// happen if the dimension has size 1, otherwise an exception will be
      /// thrown!). Dimension 2 was placed first, and dimension 0 was placed second.
      ///
      /// The image must be forged. If it is not, you can simply assign any
      /// new sizes array through Image::SetSizes. The data will never
      /// be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see SwapDimensions, Squeeze, AddSingleton, ExpandDimensionality, Flatten.
      Image& PermuteDimensions( UnsignedArray const& order );

      /// Swap dimensions d1 and d2. This is a simplified version of the
      /// PermuteDimensions.
      ///
      /// The image must be forged, and the data will never
      /// be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see PermuteDimensions.
      Image& SwapDimensions( dip::uint dim1, dip::uint dim2 );

      /// Make image 1D. The image must be forged. If HasSimpleStride,
      /// this is a quick and cheap operation, but if not, the data segment
      /// will be copied. Note that the order of the pixels in the
      /// resulting image depend on the strides, and do not necessarily
      /// follow the same order as linear indices.
      ///
      /// \see PermuteDimensions, ExpandDimensionality.
      Image& Flatten();

      /// Remove singleton dimensions (dimensions with size==1).
      /// The image must be forged, and the data will never
      /// be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see AddSingleton, ExpandDimensionality, PermuteDimensions.
      Image& Squeeze();

      /// Add a singleton dimension (with size==1) to the image.
      /// Dimensions `dim` to last are shifted up, dimension `dim` will
      /// have a size of 1.
      ///
      /// The image must be forged, and the data will never
      /// be copied (i.e. this is a quick and cheap operation).
      ///
      /// Example: to an image with sizes `{ 4, 5, 6 }` we add a
      /// singleton dimension `dim == 1`. The image will now have
      /// sizes `{ 4, 1, 5, 6 }`.
      ///
      /// \see Squeeze, ExpandDimensionality, PermuteDimensions.
      Image& AddSingleton( dip::uint dim );

      /// Append singleton dimensions to increase the image dimensionality.
      /// The image will have `n` dimensions. However, if the image already
      /// has `n` or more dimensions, nothing happens.
      ///
      /// The image must be forged, and the data will never
      /// be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see AddSingleton, ExpandSingletonDimension, Squeeze, PermuteDimensions, Flatten.
      Image& ExpandDimensionality( dip::uint dim );

      /// Expand singleton dimension `dim` to `sz` pixels, setting the corresponding
      /// stride to 0. If `dim` is not a singleton dimension (size==1), an
      /// exception is thrown.
      ///
      /// The image must be forged, and the data will never
      /// be copied (i.e. this is a quick and cheap operation).
      ///
      /// \see AddSingleton, ExpandDimensionality.
      Image& ExpandSingletonDimension( dip::uint dim, dip::uint sz );

      /// Mirror de image about selected axes.
      /// The image must be forged, and the data will never
      /// be copied (i.e. this is a quick and cheap operation).
      Image& Mirror( BooleanArray const& process );

      /// Change the tensor shape, without changing the number of tensor elements.
      Image& ReshapeTensor( dip::uint rows, dip::uint cols ) {
         dip_ThrowIf( tensor_.Elements() != rows * cols, "Cannot reshape tensor to requested sizes." );
         tensor_.ChangeShape( rows );
         return *this;
      }

      /// Change the tensor shape, without changing the number of tensor elements.
      Image& ReshapeTensor( dip::Tensor const& other ) {
         tensor_.ChangeShape( other );
         return *this;
      }

      /// Change the tensor to a vector, without changing the number of tensor elements.
      Image& ReshapeTensorAsVector() {
         tensor_.ChangeShape();
         return *this;
      }

      /// Change the tensor to a diagonal matrix, without changing the number of tensor elements.
      Image& ReshapeTensorAsDiagonal() {
         dip::Tensor other{ dip::Tensor::Shape::DIAGONAL_MATRIX, tensor_.Elements(), tensor_.Elements() };
         tensor_.ChangeShape( other );
         return *this;
      }

      /// Transpose the tensor.
      Image& Transpose() {
         tensor_.Transpose();
         return *this;
      }

      /// Convert tensor dimensions to spatial dimension.
      /// Works even for scalar images, creating a singleton dimension. `dim`
      /// defines the new dimension, subsequent dimensions will be shifted over.
      /// `dim` should not be larger than the number of dimensions. If `dim`
      /// is negative, the new dimension will be the last one. The image must
      /// be forged.
      Image& TensorToSpatial( dip::sint dim = -1 );

      /// Convert spatial dimension to tensor dimensions. The image must be scalar.
      /// If `rows` or `cols` is zero, its size is computed from the size of the
      /// image along dimension `dim`. If both are zero, a default column tensor
      /// is created. If `dim` is negative, the last dimension is used. The
      /// image must be forged.
      Image& SpatialToTensor( dip::sint dim = -1, dip::uint rows = 0, dip::uint cols = 0 );

      /// Split the two values in a complex sample into separate samples,
      /// creating a new spatial dimension of size 2. `dim` defines the new
      /// dimension, subsequent dimensions will be shifted over. `dim` should
      /// not be larger than the number of dimensions. If `dim` is negative,
      /// the new dimension will be the last one. The image must be forged.
      Image& SplitComplex( dip::sint dim = -1 );

      /// Merge the two samples along dimension `dim` into a single complex-valued sample.
      /// Dimension `dim` must have size 2 and a stride of 1. If `dim` is negative, the last
      /// dimension is used. The image must be forged.
      Image& MergeComplex( dip::sint dim = -1 );

      /// Split the two values in a complex sample into separate samples of
      /// a tensor. The image must be scalar and forged.
      Image& SplitComplexToTensor();

      /// Merge the two samples in the tensor into a single complex-valued sample.
      /// The image must have two tensor elements, a tensor stride of 1, and be forged.
      Image& MergeTensorToComplex();

      //
      // Creating views of the data -- indexing without data copy
      // Defined in src/library/image_indexing.cpp
      //

      /// Extract a tensor element, `indices` must have one or two elements; the image must be forged.
      Image operator[]( UnsignedArray const& indices ) const;

      /// Extract a tensor element using linear indexing; the image must be forged.
      Image operator[]( dip::uint index ) const;

      /// Extracts the tensor elements along the diagonal; the image must be forged.
      Image Diagonal() const;

      /// Extracts the pixel at the given coordinates; the image must be forged.
      Image At( UnsignedArray const& coords ) const;

      /// Extracts the pixel at the given linear index (inneficient!); the image must be forged.
      Image At( dip::uint index ) const;

      /// Extracts a subset of pixels from a 1D image; the image must be forged.
      Image At( Range x_range ) const;

      /// Extracts a subset of pixels from a 2D image; the image must be forged.
      Image At( Range x_range, Range y_range ) const;

      /// Extracts a subset of pixels from a 3D image; the image must be forged.
      Image At( Range x_range, Range y_range, Range z_range ) const;

      /// Extracts a subset of pixels from an image; the image must be forged.
      Image At( RangeArray ranges ) const;

      /// Extracts the real component of a complex-typed image; the image must be forged.
      Image Real() const;

      /// Extracts the imaginary component of a complex-typed image; the image must be forged.
      Image Imaginary() const;

      /// Quick copy, returns a new image that points at the same data as `this`,
      /// and has mostly the same properties. The color space and pixel size
      /// information are not copied, and the protect flag is reset.
      /// This function is mostly meant for use in functions that need to
      /// modify some properties of the input images, without actually modifying
      /// the input images.
      Image QuickCopy() const {
         Image out;
         out.dataType_ = dataType_;
         out.sizes_ = sizes_;
         out.strides_ = strides_;
         out.tensor_ = tensor_;
         out.tensorStride_ = tensorStride_;
         out.dataBlock_ = dataBlock_;
         out.origin_ = origin_;
         out.externalInterface_ = externalInterface_;
         return out;
      }

      //
      // Getting/setting pixel values
      // Defined in src/library/image_data.cpp
      //

      /// Deep copy, `this` will become a copy of `src` with its own data.
      ///
      /// If `this` is forged, then `src` is expected to have the same sizes
      /// and number of tensor elements, and the data is copied over from `src`
      /// to `this`. The copy will apply data type conversion, where values are
      /// clipped to the target range and/or truncated, as applicable. Complex
      /// values are converted to non-complex values by taking the absolute
      /// value.
      ///
      /// If `this` is not forged, then all the properties of `src` will be
      /// copied to `this`, `this` will be forged, and the data from `src` will
      /// be copied over.
      ///
      /// `src` must be forged.
      void Copy( Image const& src );

      /// Converts the image to another data type. The data segment is replaced
      /// by a new one, unless the old and new data types have the same size and
      /// is not shared with other images.
      /// If the data segment is replaced, strides are set to normal.
      void Convert( dip::DataType dt );

      /// Sets all samples in the image to the value `v`. The function is defined
      /// for values `v` of type dip::sint, dip::dfloat, and dip::dcomplex. The
      /// value will be clipped to the target range and/or truncated, as applicable.
      /// The image must be forged.
      void Fill( dip::sint v );

      /// Sets all samples in the image to the value `v`. The function is defined
      /// for values `v` of type dip::sint, dip::dfloat, and dip::dcomplex. The
      /// value will be clipped to the target range and/or truncated, as applicable.
      /// The image must be forged.
      void Fill( dfloat v );

      /// Sets all samples in the image to the value `v`. The function is defined
      /// for values `v` of type dip::sint, dip::dfloat, and dip::dcomplex. The
      /// value will be clipped to the target range and/or truncated, as applicable.
      /// The image must be forged.
      void Fill( dcomplex v );

      /// Extracts the fist sample in the first pixel (At(0,0)[0]), casted
      /// to a signed integer of maximum width; for complex values
      /// returns the absolute value.
      explicit operator dip::sint() const;

      /// Extracts the fist sample in the first pixel (At(0,0)[0]), casted
      /// to a double-precision floating-point value; for complex values
      /// returns the absolute value.
      explicit operator dfloat() const;

      /// Extracts the fist sample in the first pixel (At(0,0)[0]), casted
      /// to a double-precision complex floating-point value.
      explicit operator dcomplex() const;


   private:

      //
      // Implementation
      //

      dip::DataType dataType_ = DT_SFLOAT;
      UnsignedArray sizes_;               // sizes_.size() == ndims (if forged)
      IntegerArray strides_;              // strides_.size() == ndims (if forged)
      dip::Tensor tensor_;
      dip::sint tensorStride_ = 0;
      bool protect_ = false;              // When set, don't strip image
      String colorSpace_;
      dip::PixelSize pixelSize_;
      std::shared_ptr< void > dataBlock_; // Holds the pixel data. Data block will be freed when last image that uses it is destroyed.
      void* origin_ = nullptr;            // Points to the origin ( pixel (0,0) ), not necessarily the first pixel of the data block.
      ExternalInterface* externalInterface_ = nullptr; // A function that will be called instead of the default forge function.

      //
      // Some private functions
      //

      bool HasValidStrides() const;       // Are the strides such that no two samples are in the same memory cell?

      void SetNormalStrides();            // Fill in all strides.

      void GetDataBlockSizeAndStart( dip::uint& size, dip::sint& start ) const;

      void GetDataBlockSizeAndStartWithTensor( dip::uint& size, dip::sint& start ) const;
      // size is the distance between top left and bottom right corners.
      // start is the distance between top left corner and origin
      // (will be <0 if any strides[ii] < 0). All measured in samples.

}; // class Image


//
// Overloaded operators
//

/// You can output an Image to `std::cout` or any other stream; some
/// information about the image is printed.
std::ostream& operator<<( std::ostream& os, Image const& img );

//
// Utility functions
//

inline void swap( Image& v1, Image& v2 ) {
   v1.swap( v2 );
}

/// Calls `img1.Aliases( img2 )`; see Image::Aliases.
inline bool Alias( Image const& img1, Image const& img2 ) {
   return img1.Aliases( img2 );
}

/// Makes a new image object pointing to same pixel data as `src`, but
/// with different origin, strides and size (backwards compatibility
/// function, we recommend the Image::At function instead).
void DefineROI(
      Image const& src,
      Image& dest,
      UnsignedArray const& origin,
      UnsignedArray const& sizes,
      IntegerArray const& spacing
);

inline Image DefineROI(
      Image const& src,
      UnsignedArray const& origin,
      UnsignedArray const& sizes,
      IntegerArray const& spacing
) {
   Image dest;
   DefineROI( src, dest, origin, sizes, spacing );
   return dest;
}

/// Copies samples over from `src` to `dest`, identical to the dip::Image::Copy method.
inline void Copy(
      Image const& src,
      Image& dest
) {
   dest.Copy( src );
}

inline Image Copy(
      Image const& src
) {
   Image dest;
   dest.Copy( src );
   return dest;
}

/// Copies samples over from `src` to `dest`, with data type conversion. If `dest` is forged,
/// has the same size as number of tensor elements as `src`, and has data type `dt`, then
/// its data segment is reused. If `src` and `dest` are the same object, its dip::Image::Convert
/// method is called instead.
inline void Convert(
      Image const& src,
      Image& dest,
      dip::DataType dt
) {
   if( &src == &dest ) {
      dest.Convert( dt );
   } else {
      dest.ReForge( src, dt );
      dest.Copy( src );
   }
}

inline Image Convert(
      Image const& src,
      dip::DataType dt
) {
   Image dest( src, dt );
   dest.Copy( src );
   return dest;
}


} // namespace dip

#endif // DIP_IMAGE_H