// A rectangular block of data of type float.  A transparent block
// cache is used for each instance of this class, so the image can be
// much bigger than will fit in memory, and pixels can still be
// accessed and written efficiently, provided subsequent accesses are
// spatially correlated.  A variety of useful methods are implemented
// (filtering, subsetting, interpolating, etc.)
//
// Don't try to access the same instance concurrently.  Split your
// images up into separate instances if you must parallelize things.
//
// For many methods, arguments of type ssize_t are used, but are not
// allowed to be negative.  This is to help prevent people from
// shooting themselves in the foor by accidently passing negative
// values which don't yield warnings and can't be caught with
// assertions.


#ifndef FLOAT_IMAGE_H
#define FLOAT_IMAGE_H

#include <stdio.h>
#include <sys/types.h>

#include <glib.h>
#include <gsl/gsl_matrix.h>

// Instance structure.  Everything here is private and need not be
// used or understood by client code, except for the size_x and size_y
// fields.
typedef struct {
  size_t size_x, size_y;	// Image dimensions.
  size_t cache_space;		// Memory cache space in bytes.
  size_t cache_area;		// Memory cache area in pixels.
  size_t tile_size;		// Tile size in pixels on a side.
  size_t cache_size_in_tiles;	// Number of tiles in cache.
  size_t tile_count_x;		// Number of tiles in image in x direction.
  size_t tile_count_y;		// Number of tiles in image in y direction.
  size_t tile_count;		// Total number of tiles in image.
  size_t tile_area;             // Area of a tile, in pixels.
  float *cache;                 // Memory cache.
  float **tile_addresses;	// Addresss of individual tiles in the cache.
  GQueue *tile_queue;		// Queue of tile offsets kept in load order.
  FILE *tile_file;              // File with tiles stored contiguously.
} FloatImage;

///////////////////////////////////////////////////////////////////////////////
//
// Creating New Instances
//
///////////////////////////////////////////////////////////////////////////////

// Create a new image filled with zero pixel values.
FloatImage *
float_image_new (ssize_t size_x, ssize_t size_y);

// Create a new image with pixels initialized to value.
FloatImage *
float_image_new_with_value (ssize_t size_x, ssize_t size_y, float value);

// Create a new image from memory.  This pixels are assumed to be
// layed out in memory in the usual way, i.e. contiguous rows of
// pixels in the x direction are contiguous in memory.
FloatImage *
float_image_new_from_memory (ssize_t size_x, ssize_t size_y, float *buffer);

// Type used to specify whether disk files should be in big or little
// endian byte order.
typedef enum {
  FLOAT_IMAGE_BYTE_ORDER_LITTLE_ENDIAN,
  FLOAT_IMAGE_BYTE_ORDER_BIG_ENDIAN
} float_image_byte_order_t;

// Form reduced resolution version of the model.  The scale_factor
// must be positive and odd.  The new image will be
// ceil (model->size_x / scale_factor) pixels by 
// ceil (model->size_y / scale_factor) pixels.  Scaling is performed by 
// averaging blocks of pixels together, using odd pixel reflection
// around the image edges (see the description of the apply_kernel
// method).
FloatImage *
float_image_new_from_model_scaled (FloatImage *model, ssize_t scale_factor);

// Create a new image from data at byte offset in file.  The pixel
// layout in the file is assumed to be the same as for the
// float_image_new_from_memory method.  The byte order of individual
// pixels in the file should be byte_order.
FloatImage *
float_image_new_from_file (ssize_t size_x, ssize_t size_y, const char *file,
			   off_t offset, float_image_byte_order_t byte_order);

// The method is like new_from_file, but takes a file pointer instead
// of a file name, and the offset argument is with respect to the
// current position in the file_pointer stream.
FloatImage *
float_image_new_from_file_pointer (ssize_t size_x, ssize_t size_y, 
				   FILE *file_pointer, off_t offset,
				   float_image_byte_order_t byte_order);

// Form a low quality reduced resolution version of the
// original_size_x by original_size_y image in file.  The new image
// will be size_x by size_y pixels.  This method is like new_from_file
// method, but gets its data by sampling in each dimension using
// bilinear interpolation.  This is a decent way of forming quick
// thumbnails of images, but not much else.
FloatImage *
float_image_new_from_file_scaled (ssize_t size_x, ssize_t size_y, 
				  ssize_t original_size_x, 
				  ssize_t original_size_y,
				  const char *file, off_t offset, 
				  float_image_byte_order_t byte_order);

///////////////////////////////////////////////////////////////////////////////
//
// Getting and Setting Image Pixels and Regions
//
///////////////////////////////////////////////////////////////////////////////

// Get pixel at 0-indexed position x, y.  A cache is used so pixel
// access does not usually involve the hard disk, provided subsequent
// pixel lookups are spatially close together.  You can probably get
// away with trating this function just as if everything was in
// memory.  For details, see the cache control methods below.
//
// The x and y arguments should always be positive, ssize_t is used
// only so people can't as easily shoot themselves in the foot by
// accidently supplying negative arguments which get promoted in some
// strange way.
float
float_image_get_pixel (FloatImage *self, ssize_t x, ssize_t y);

// Set pixel at 0-indexed position x, y to value.  A cache is used to
// make this fast, as for the float_image_get_pixel method.
void
float_image_set_pixel (FloatImage *self, ssize_t x, ssize_t y, float value);

// Get rectangular image region of size_x, size_y having upper left
// corner at x, y and copy it into already allocated buffer.  There is
// not necessarily any caching help for this method, i.e. it may
// always involve disk access and always be slow.
void
float_image_get_region (FloatImage *self, ssize_t x, ssize_t y, 
			ssize_t size_x, ssize_t size_y, float *buffer);

// This method is analogous to float_image_get_region.
void
float_image_set_region (FloatImage *self, size_t x, size_t y, size_t size_x, 
			size_t size_y, float *buffer);

// Get a full row of pixels, copying the data into already allocated
// buffer.  This method will be fast on the average for calls with
// sequential row numbers.
void
float_image_get_row (FloatImage *self, size_t row, float *buffer);

// Get a pixel, performing odd reflection at image edges if the pixel
// indicies fall outside the image.  See the description of the
// apply_kernel method for an explanation of reflection.
float
float_image_get_pixel_with_reflection (FloatImage *self, ssize_t x, ssize_t y);

///////////////////////////////////////////////////////////////////////////////
//
// Image Analysis and Statistics
//
///////////////////////////////////////////////////////////////////////////////

// Finds the minimum and maximum pixel values in the image, and the
// mean and standard deviation of all pixels.  This function sucks
// every pixel in the image into memory at the moment (this needs
// fixing).
void
float_image_statistics (FloatImage *self, float *min, float *max, float *mean, 
			float *standard_deviation);

// Compute an efficient estimate of the mean and standard deviation of
// the pixels in the image, by sampling every stride th pixel in each
// dimension.  The whole array of sample pixels ends up in memory, so
// stride probably needs to be something other than one or some other
// low number for large images.
void
float_image_approximate_statistics (FloatImage *self, size_t stride, 
				    float *mean, float *standard_deviation);

///////////////////////////////////////////////////////////////////////////////
//
// Kernels, Interpolation, and Sampling
//
///////////////////////////////////////////////////////////////////////////////

// Apply kernel centerd at pixel x, y and return the value.  The
// kernel matrix must be have equal odd dimensions.  The values in the
// kernel are multiplied by the pixels, and the sum of the products
// returned.  When part of the kernel would fall outside the image
// extents, the values used for the out-of-image pixels are the mirror
// images of the corresponding in-image pixels, with the edge pixels
// not duplicated, i.e. reflection about the middle of the edge pixels
// is used.
float
float_image_apply_kernel (FloatImage *self, ssize_t x, ssize_t y, 
			  gsl_matrix_float *kernel);

// Type used to specify whether disk files should be in big or little
// endian byte order.

// Sample method types.  These dictate how nearby pixels are
// considered when we want to find the approximate value for a point
// which falls between pixel indicies.
typedef enum {
  // Nearest pixel.
  FLOAT_IMAGE_SAMPLE_METHOD_NEAREST_NEIGHBOR,
  // Linearly weited average of four nearest pixels
  FLOAT_IMAGE_SAMPLE_METHOD_BILINEAR,
  // Bicubic spline interpolation (which consideres the nearest 16 pixels).
  FLOAT_IMAGE_SAMPLE_METHOD_BICUBIC
} float_image_sample_method_t;

float
float_image_sample (FloatImage *self, float x, float y, 
		    float_image_sample_method_t sample_method);

///////////////////////////////////////////////////////////////////////////////
//
// Storing Images in Files
//
///////////////////////////////////////////////////////////////////////////////

// Store image in file.  The image is stored in the usual order,
// i.e. contiguous rows of pixels in the x direction are stored
// contiguously in memory.  Individual pixels are stored in byte order
// byte_order.  Returns 0 on success, nonzero on error.
int
float_image_store (FloatImage *self, const char *file, 
		   float_image_byte_order_t byte_order);

///////////////////////////////////////////////////////////////////////////////
//
// Exporting Images in Various Image File Formats
//
///////////////////////////////////////////////////////////////////////////////

// Export image to fila as a gray scaled jpeg image, with largest
// dimension no larger than max_dimension.  The max_dimension argument
// must be less than or equal to the largest dimension of the image.
// The image may be scaled st its largest dimension is considerably
// less than max_dimension.  Scaling is performed by averaging blocks
// of pixels together, using odd pixel reflection around the image
// edges (see the description of the apply_kernel method).  The JPEG
// format uses byte-valued gray scale samples, so the dynamic range of
// gray scale output pixels is limited to [0, 255].  Image pixel
// values inside two standard deviations of the mean pixel value are
// mapped linearly into this range; image pixel values outside two
// standard are clamped at the appropriate limit.  If all image pixels
// have the same value, the output is made black if the pixels have
// value 0.0, and white otherwise.  This routine slurps the whole
// image into memory, so beware.  Returns 0 on success, nonzero on
// error.
int
float_image_export_as_jpeg (FloatImage *self, const char *file, 
			    size_t max_dimension);

///////////////////////////////////////////////////////////////////////////////
//
// Controlling Image Data Caching
//
// It probably isn't necessary to use these methods.  They are
// provided largely to make it clear how the cache works.  The major
// tunable parameter is the size of the in-memory cache to use.
//
// When a new image is created, the following steps are performed:
//
//      1. The image is divided up into square tiles st two full rows
//         or columns of tiles will fit in the memory cache.
//
//      2. A copy of the image is created on disk with the memory
//         layout rearranged st individual tiles are contiguous in
//         memory.  This allows tiles to be quickly retrieved later.
//
// When a pixel is accessed (read or set), the following happens:
//
//      1. If the pixel is in a tile already loaded into the cache,
//         it is simply fetched or set.
//
//      2. Otherwise, the tile containing the pixel is loaded,
//         possibly displacing an already loaded tile, and then the
//         pixel is fetched or set.  The tile displaced is the one
//         loaded longest ago (there is no most-recently-accessed
//         heuristic, as this would make pixel access too slow).
//
// Thus, using a larger memory cache will result in larger tiles being
// used, and fewer tile loads being needed.  In general, the default
// behavior is pretty good, but if you know will be performing lots of
// widely (but not too widely) scattered accesses, you might want to
// make it bigger.
//
///////////////////////////////////////////////////////////////////////////////

// Get the image memory cache size setting, in bytes.  Note that this
// is the memory cache used per image, not the class-wide cache usage.
// If you will have a lot of objects instantiated simultaneously, you
// may find it necessary to use a smaller cache for each image.
size_t
float_image_get_cache_size (FloatImage *self);

// Set the image memory cache to size bytes.  Changing the cache size
// requires the tiling to be recomputed, the on-disk tile cache to be
// regenerated, and the in memory cache to be flushed, so its slow.
void
float_image_set_cache_size (FloatImage *self, size_t size);

///////////////////////////////////////////////////////////////////////////////
//
// Freeing Instances
//
///////////////////////////////////////////////////////////////////////////////

// Destroy self.
void
float_image_free (FloatImage *self);

#endif // FLOAT_IMAGE_H
