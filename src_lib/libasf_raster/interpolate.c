/*******************************************************************
FUNCTION NAME:   interpolate - performs the interpolation to calculate
                 the pixel value e.g. during resampling

INPUT: interpolation  - interpolation method (nearest neighbor, bilinear, sinc)
       inbuf          - input image buffer
       nLines         - number of lines (image buffer)
       nSamples       - number of samples per line (image buffer)
       xLine          - line of interest
       xSample        - sample position within line
       weighting      - weighting function to be applied (Kaiser, Hamming)
       nKernel        - kernel size for sinc function
*******************************************************************/
#include <assert.h>
#include <math.h>

#include <glib.h>

#include "asf.h"
#include "asf_raster.h"

#define SQR(X) ((X)*(X))

// For sinc interpolation, the goal is to weight source values using a
// sinc function.  We want to position the peak of the sinc at the
// point of interest.  The sinc filter itself will interpolate a small
// number of point, perhaps 8 or 16, for example.  In order to achieve
// the desired location of the peak, we want to introduce a shift in
// the sinc function computed.  However, we don't want to recompute
// the sinc function for every point to be interpolated!  The solution
// is to precompute an array of slightly shifted sinc functions, and
// then choose the appropriate one for the point we are interpolating.
// For example, if we have known values for 1 and 2, and we want to
// interpolate a value for 1.2, we choose the (1.2 - 1.0) * (2 - 1) *
// NUM_SINCS sinc function to evaluate the result.
#define NUM_SINCS 512 // Number of since functions to compute.

// Fetch pixel ii, jj from inbuf, where inbuf is taken to be an image
// nSamples wide and nLines high.  If ii, jj is outside the image, a
// "reflected" value is returned.  For example, these two calls will
// return the same pixel:
//
//     get_pixel_with_reflection (inbuf, sample_count, line_count, -1, -2) 
//     get_pixel_with_reflection (inbuf, sample_count, line_count, 1, 2);
//
// If the reflected indicies are still outside the image, an exception
// is triggered.
static float
get_pixel_with_reflection (float *inbuf, int nSamples, int nLines, int sample,
			   int line)
{
  int ii = sample;		// Convenience alias.
  int jj = line;		// Convenience alias.
  // Handle reflection at image edges.
  if ( G_UNLIKELY (ii < 0) ) { 
    ii = -ii; 
  }
  else if ( G_UNLIKELY (ii >= nSamples) ) { 
    ii = ii - (ii - nSamples + 1) - 1; 
  }
  if ( G_UNLIKELY (jj < 0) ) { 
    jj = -jj; 
  }
  else if ( G_UNLIKELY (jj >= nLines) ) { 
    jj = jj - (jj - nLines + 1) - 1; 
  }
  // Now we better be in the image.
  assert (ii >= 0 && ii < nSamples);
  assert (jj >= 0 && jj < nLines);

  return inbuf[nSamples * jj + ii];
}

float interpolate(interpolate_type_t interpolation, float *inbuf, int nLines, 
		  int nSamples, float yLine, float xSample, 
		  weighting_type_t weighting, int sinc_points)
{
  int ix, iy, base;
  float a00, a10, a01, a11;
  float value;
             
  /* Get closed pixel position to start from */

  switch ( interpolation ) {
    case NEAREST:
      ix = (int) (xSample + 0.5);
      iy = (int) (yLine + 0.5);
      base = iy*nSamples + ix;
      value = inbuf[base];
      break;

    case BILINEAR:
      assert (xSample >= 0.0);
      assert (yLine >= 0.0);
      assert (xSample <= nSamples - 1);
      assert (yLine <= nLines - 1);
      // If we fall on the right or lower edge of the image, we cheat
      // by nudging the requested line sample down just slightly, to
      // keep the below interpolation algorithm simple.
      //      const float cheat_nudge = 1e-6;
      //      if ( xSample >= nSamples - 1 ) { xSample - cheat_nudge ;}
      //      if ( xLine >=  nLines - 1 ) { xLine - cheat_nudge; }
      ix = floor(xSample);
      iy = floor(yLine);
      
      base = iy*nSamples + ix;
      a00 = inbuf[base];
      a10 = inbuf[base+1] - inbuf[base];
      a01 = inbuf[base+nSamples] - inbuf[base];
      a11 = (inbuf[base] - inbuf[base+1] - inbuf[base+nSamples] 
	     + inbuf[base+nSamples+1]);
      value = (a00 + a10 * (xSample - ix) + a01 * (yLine - iy) 
	       + a11 * (xSample - ix) * (yLine - iy));
      break;

    case BICUBIC:
      assert (FALSE);		/* Not implemented yet.  */
      break;

    case SINC:
      {
	// Initialize array of shifted sinc functions and weights on 
	// first time through.
	static float *sinc_funcs = NULL;
	static float *weights = NULL;
        float alpha = 5;
	if ( sinc_funcs == NULL ) {
	  sinc_funcs = MALLOC (sinc_points * NUM_SINCS * sizeof (float));
          weights = MALLOC (sinc_points * NUM_SINCS * sizeof(float));
	  int ii;
	  for ( ii = 0 ; ii < NUM_SINCS ; ii++ ) {
	    double shift = (double)ii / NUM_SINCS;
	    float *current_sinc = &sinc_funcs[ii * sinc_points];
	    float sinc_weight = 0.0;
	    int jj;

	    for ( jj = 0 ; jj < sinc_points/2 ; jj++ ) {

	      // Assigning weights
	      switch ( weighting ) {
	      case NO_WEIGHT:
		weights[jj] = 1.0;
		weights[sinc_points-jj] = 1.0;
		break;
	      case KAISER:
                // optimum alpha value to be determined - for the moment set to 5
                // gsl_sf_bessel_I0 - zeroth order modified Bessel function
                weights[jj] = gsl_sf_bessel_I0(alpha*sqrt(1.0 - SQR(jj/sinc_points))) 
                  / gsl_sf_bessel_I0(alpha);
                weights[sinc_points-jj] = gsl_sf_bessel_I0(alpha*sqrt(1.0 - SQR(jj/sinc_points))) 
                  / gsl_sf_bessel_I0(alpha);
	      case HAMMING:
		weights[jj] = 0.54 + 0.46 * cos(M_PI*(jj-sinc_points) / sinc_points);
		weights[sinc_points-jj] = 0.54 + 0.46 * cos(M_PI*(jj-sinc_points) / sinc_points);
		break;
              case LANCZOS:
                weights[jj] = sin(M_PI*(jj-sinc_points) / sinc_points) 
                  / (M_PI*(jj-sinc_points)/sinc_points);
                weights[sinc_points-jj] = sin(M_PI*(jj-sinc_points) / sinc_points) 
                  / (M_PI*(jj-sinc_points)/sinc_points);
                break;
	      default:
		assert (FALSE);    /* should not be here! */
		break;
	      }
	    }

	    for ( jj = 0 ; jj < sinc_points ; jj++ ) {

	      // Argument of the sinc function for this shifted sinc function.
	      double sinc_arg = jj - sinc_points / 2.0 + 1.0 - shift;
	      if ( sinc_arg == 0.0 ) {
		current_sinc[jj] = 1.0;
	      }
	      else {
		current_sinc[jj] = 
		  sin (M_PI * sinc_arg) / (M_PI * sinc_arg) * weights[jj];
	      }

	      sinc_weight += current_sinc[jj];
	    }

	    // Normalize the sinc weighting.
	    sinc_weight = 1.0 / sinc_weight;
	    for ( jj = 0 ; jj < sinc_points ; jj++ ) {
	      current_sinc[jj] *= sinc_weight;
	    }
	  }
	}
	 
	// Apply sinc interpolation in two dimensions.
	value = 0.0;
	// Integer parts of x and y to interpolate.
	int ix = floor (xSample), iy = floor (yLine);
	float x_fraction = xSample - ix, y_fraction = yLine - iy;
	float *x_sinc_func 
	  = &sinc_funcs[((int)(x_fraction * NUM_SINCS)) * sinc_points];
	float *y_sinc_func
	  = &sinc_funcs[((int)(y_fraction * NUM_SINCS)) * sinc_points];
	int x_start_sinc = ix + 1 - sinc_points / 2;
	int y_start_sinc = iy + 1 - sinc_points / 2;
	int x_end_sinc = x_start_sinc + sinc_points;
	int y_end_sinc = y_start_sinc + sinc_points;
	int ii;
	for ( ii = y_start_sinc ; ii < y_end_sinc ; ii++ ) {
	  int jj;
	  for ( jj = x_start_sinc ; jj < x_end_sinc ; jj++ ) {
	    value += x_sinc_func[jj - x_start_sinc] 
	      * y_sinc_func[ii - y_start_sinc] 
	      * get_pixel_with_reflection (inbuf, nSamples, nLines, ii, jj);
	  }
	}
      }
      break;
    default:
      assert (FALSE);
      break;
  }

  return value;
}
