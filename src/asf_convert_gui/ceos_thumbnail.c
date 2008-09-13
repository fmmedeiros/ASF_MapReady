#include "asf.h"
#include "asf_nan.h"
#include "asf_tiff.h"
#include <geotiff_support.h>

#include <unistd.h>
#include <asf_meta.h>
#include <ceos_io.h>
#include <float_image.h>
#include <math.h>

#include "ceos_thumbnail.h"
#include "asf_convert_gui.h"
#include "get_ceos_names.h"
#include "asf_import.h"
#include "asf_endian.h"

// Prototypes
int get_geotiff_float_line(TIFF *fpIn, meta_parameters *meta, long row, int band, float *line);

// Implementations
static void destroy_pb_data(guchar *pixels, gpointer data)
{
    g_free(pixels);
}

static meta_parameters * silent_meta_create(const char *filename)
{
    report_level_t prev = g_report_level;
    g_report_level = REPORT_LEVEL_NONE;

    meta_parameters *ret = meta_create(filename);

    g_report_level = prev;
    return ret;
}

static meta_parameters * silent_meta_read(const char *filename)
{
    report_level_t prev = g_report_level;
    g_report_level = REPORT_LEVEL_NONE;

    meta_parameters *ret = meta_read(filename);

    g_report_level = prev;
    return ret;
}

static GdkPixbuf *
make_geotiff_thumb(const char *input_metadata, const char *input_data,
                   size_t max_thumbnail_dimension)
{
    TIFF *fpIn;
    int ignore[MAX_BANDS];

    meta_parameters *meta = read_generic_geotiff_metadata(input_metadata, ignore, NULL);

    fpIn = XTIFFOpen(input_data, "rb");
    if (!fpIn) {
        meta_free(meta);
        return NULL;
    }
    short sample_format;
    short bits_per_sample;
    short planar_config;
    int is_scanline_format;
    int is_palette_color_tiff;
    data_type_t data_type;
    short num_bands;
    get_tiff_data_config(fpIn,
                         &sample_format,
                         &bits_per_sample,
                         &planar_config,
                         &data_type,
                         &num_bands,
                         &is_scanline_format,
                         &is_palette_color_tiff,
                         REPORT_LEVEL_WARNING);
    tiff_type_t tiffInfo;
    get_tiff_type(fpIn, &tiffInfo);
    if (tiffInfo.imageCount > 1) {
        meta_free(meta);
        return NULL;
    }
    if (tiffInfo.imageCount < 1) {
        meta_free(meta);
        return NULL;
    }
    if (tiffInfo.format != SCANLINE_TIFF &&
        tiffInfo.format != STRIP_TIFF    &&
        tiffInfo.format != TILED_TIFF)
    {
        meta_free(meta);
        return NULL;
    }
    if (tiffInfo.volume_tiff) {
        meta_free(meta);
        return NULL;
    }
    if (num_bands > 1 &&
        planar_config != PLANARCONFIG_CONTIG &&
        planar_config != PLANARCONFIG_SEPARATE)
    {
        meta_free(meta);
        return NULL;
    }
    if (num_bands < 1) {
        meta_free(meta);
        return NULL;
    }

    // use a larger dimension at first, for our crude scaling.  We will
    // use a better scaling method later, from GdbPixbuf
    int larger_dim = 1024;

    // Vertical and horizontal scale factors required to meet the
    // max_thumbnail_dimension part of the interface contract.
    int vsf = ceil (meta->general->line_count / larger_dim);
    int hsf = ceil (meta->general->sample_count / larger_dim);
    // Overall scale factor to use is the greater of vsf and hsf.
    int sf = (hsf > vsf ? hsf : vsf);

    // Thumbnail image sizes.
    size_t tsx = meta->general->sample_count / sf;
    size_t tsy = meta->general->line_count / sf;


    // Form the thumbnail image by grabbing individual pixels.
    //
    // Keep track of the average pixel value, so later we can do a 2-sigma
    // scaling - makes the thumbnail look a little nicer and more like what
    // they'd get if they did the default jpeg export.
//    double avg = 0.0;
    size_t ii, jj;
    int ret = 0;
    int band;
    double min[num_bands];
    double max[num_bands];
    guchar *data = g_new(guchar, 3*tsx*tsy);
    float **line = g_new(float *, num_bands);
    float **fdata = g_new(float *, num_bands);
    for (band = 0; band < num_bands; band++) {
        line[band] = g_new (float, meta->general->sample_count);
        fdata[band] = g_new(float, 3*tsx*tsy);
        min[band] = DBL_MAX;
        max[band] = DBL_MIN;
    }
    for (band = 0; band < num_bands; band++) {
        for ( ii = 0 ; ii < tsy ; ii++ ) {
            ret = get_geotiff_float_line(fpIn, meta, ii*sf, band, line[band]);
            if (!ret) break;

            for (jj = 0; jj < tsx; ++jj) {
                fdata[band][jj + ii*tsx] = line[band][jj*sf];
                min[band] = (line[band][jj*sf] < min[band]) ? line[band][jj*sf] : min[band];
                max[band] = (line[band][jj*sf] > max[band]) ? line[band][jj*sf] : max[band];
                //avg += line[jj*sf];
            }
        }
        if (!ret) break;
    }
    for (band = 0; band < num_bands; band++) {
        g_free(line[band]);
        if (!ret) {
            g_free(fdata[band]);
        }
    }
    g_free(line);
    meta_free(meta);
    XTIFFClose(fpIn);
    if (!ret) {
        g_free(fdata);
        return NULL;
    }

    // Compute the std devation
//    avg /= tsx*tsy;
//    double stddev = 0.0;
//    for (ii = 0; ii < tsx*tsy; ++ii)
//        stddev += ((double)fdata[ii] - avg) * ((double)fdata[ii] - avg);
//    stddev = sqrt(stddev / (tsx*tsy));

    // Set the limits of the scaling - 2-sigma on either side of the mean
    //double lmin = avg - 2*stddev;
    //double lmax = avg + 2*stddev;

    // Now actually scale the data, and convert to bytes.
    // Note that we need 3 values, one for each of the RGB channels.
    for (ii = 0; ii < tsx*tsy; ++ii) {
        float max_red,   min_red;
        float max_green, min_green;
        float max_blue,  min_blue;
        float rval;
        float gval;
        float bval;
        if (num_bands <= 2) {
            // If we don't have enough bands to do a 3-color image, just make
            // a grayscale thumbnail
            rval = fdata[0][ii];
            gval = fdata[0][ii];
            bval = fdata[0][ii];
            min_red = min_green = min_blue = min[0];
            max_red = max_green = max_blue = max[0];
        }
        else if (num_bands >= 3) {
            // If we have 3 or more bands, use the first 3 to make a color
            // thumbnail (without further information about which band represents
            // what color or wavelengths, this is the best we can do)
            rval = fdata[0][ii];
            gval = fdata[1][ii];
            bval = fdata[2][ii];
            min_red   = min[0];
            max_red   = max[0];
            min_green = min[1];
            max_green = max[1];
            min_blue  = min[2];
            max_blue  = max[2];
        }
        guchar ruval, guval, buval;
        if (rval < min_red) {
            ruval = 0;
        }
        else if (rval > max_red) {
            ruval = 255;
        }
        else {
            ruval = (guchar) round(((rval - min_red) / (max_red - min_red)) * 255);
        }
        if (gval < min_green) {
            guval = 0;
        }
        else if (gval > max_green) {
            guval = 255;
        }
        else {
            guval = (guchar) round(((gval - min_green) / (max_green - min_green)) * 255);
        }
        if (bval < min_blue) {
            buval = 0;
        }
        else if (bval > max_blue) {
            buval = 255;
        }
        else {
            buval = (guchar) round(((bval - min_blue) / (max_blue - min_blue)) * 255);
        }

        int n = 3*ii;
        data[n]   = ruval;
        data[n+1] = guval;
        data[n+2] = buval;
    }

    // Create the pixbuf
    GdkPixbuf *pb =
            gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, FALSE,
                                     8, tsx, tsy, tsx*3, destroy_pb_data, NULL);
    for (band = 0; band < num_bands; band++) {
        g_free(fdata[band]);
    }
    g_free(fdata);

    if (!pb) {
        asfPrintWarning("Failed to create the thumbnail pixbuf: %s\n", input_data);
        return NULL;
    }

    // Scale down to the size we actually want, using the built-in Gdk
    // scaling method, much nicer than what we did above

    // Must ensure we scale the same in each direction
    double scale_y = tsy / max_thumbnail_dimension;
    double scale_x = tsx / max_thumbnail_dimension;
    double scale = scale_y > scale_x ? scale_y : scale_x;
    int x_dim = tsx / scale;
    int y_dim = tsy / scale;

    GdkPixbuf *pb_s =
            gdk_pixbuf_scale_simple(pb, x_dim, y_dim, GDK_INTERP_BILINEAR);
    gdk_pixbuf_unref(pb);

    if (!pb_s) {
        asfPrintWarning("Failed to allocate scaled thumbnail pixbuf: %s\n", input_data);
    }

    return pb_s;
}

static GdkPixbuf *
make_airsar_thumb(const char *input_metadata, const char *input_data,
                  size_t max_thumbnail_dimension)
{
    // the airsar metadata importer wants just the basename
    char *airsar_basename = STRDUP(input_metadata);
    char *p = strstr(airsar_basename, "_meta.airsar");
    if (!p) {
      p = strstr(airsar_basename, "_META.AIRSAR");
      if (!p) {
        p = strstr(airsar_basename, "_meta.AIRSAR");
        if (!p) {
          p = strstr(airsar_basename, "_META.airsar");
          if (!p) { free(airsar_basename); return NULL; }
        }
      }
    }
    *p = '\0';

    meta_parameters *meta = import_airsar_meta(input_data, airsar_basename);
    meta->general->data_type = INTEGER16;

    char *filename = MALLOC(sizeof(char)*(20+strlen(airsar_basename)));

    // try C-band file first, if that doesn't exists, then L-band
    sprintf(filename, "%s_c.vvi2", airsar_basename);

    if (!fileExists(filename)) {
      sprintf(filename, "%s_l.vvi2", airsar_basename);
      if (!fileExists(filename)) {
        free(airsar_basename);
        free(filename);
        meta_free(meta);
        return NULL;
      }
    }

    FILE *fpIn = fopen(filename, "rb");
    if (!fpIn) {
      free(airsar_basename);
      free(filename);
      meta_free(meta);
      return NULL;
    }

    // use a larger dimension at first, for our crude scaling.  We will
    // use a better scaling method later, from GdbPixbuf
    int larger_dim = 1024;

    // Vertical and horizontal scale factors required to meet the
    // max_thumbnail_dimension part of the interface contract.
    int vsf = ceil (meta->general->line_count / larger_dim);
    int hsf = ceil (meta->general->sample_count / larger_dim);
    // Overall scale factor to use is the greater of vsf and hsf.
    int sf = (hsf > vsf ? hsf : vsf);

    // Thumbnail image sizes.
    size_t tsx = meta->general->sample_count / sf;
    size_t tsy = meta->general->line_count / sf;

    guchar *data = g_new(guchar, 3*tsx*tsy);
    float *fdata = g_new(float, 3*tsx*tsy);

    // Form the thumbnail image by grabbing individual pixels.
    size_t ii, jj;
    float *line = g_new (float, meta->general->sample_count);

    // Keep track of the average pixel value, so later we can do a 2-sigma
    // scaling - makes the thumbnail look a little nicer and more like what
    // they'd get if they did the default jpeg export.
    double avg = 0.0;
    for ( ii = 0 ; ii < tsy ; ii++ ) {

        get_float_line(fpIn, meta, ii*sf, line);

        for (jj = 0; jj < tsx; ++jj) {
            fdata[jj + ii*tsx] = line[jj*sf];
            avg += line[jj*sf];
        }
    }
    g_free (line);
    fclose(fpIn);

    // Compute the std devation
    avg /= tsx*tsy;
    double stddev = 0.0;
    for (ii = 0; ii < tsx*tsy; ++ii)
        stddev += ((double)fdata[ii] - avg) * ((double)fdata[ii] - avg);
    stddev = sqrt(stddev / (tsx*tsy));

    // Set the limits of the scaling - 2-sigma on either side of the mean
    double lmin = avg - 2*stddev;
    double lmax = avg + 2*stddev;

    // Now actually scale the data, and convert to bytes.
    // Note that we need 3 values, one for each of the RGB channels.
    for (ii = 0; ii < tsx*tsy; ++ii) {
        float val = fdata[ii];
        guchar uval;
        if (val < lmin)
            uval = 0;
        else if (val > lmax)
            uval = 255;
        else
            uval = (guchar) round(((val - lmin) / (lmax - lmin)) * 255);

        int n = 3*ii;
        data[n] = uval;
        data[n+1] = uval;
        data[n+2] = uval;
    }

    g_free(fdata);

    // Create the pixbuf
    GdkPixbuf *pb =
        gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, FALSE,
                                 8, tsx, tsy, tsx*3, destroy_pb_data, NULL);

    if (!pb) {
        printf("Failed to create the thumbnail pixbuf: %s\n", input_data);
        meta_free(meta);
        g_free(data);
        return NULL;
    }

    // Scale down to the size we actually want, using the built-in Gdk
    // scaling method, much nicer than what we did above

    // Must ensure we scale the same in each direction
    double scale_y = tsy / max_thumbnail_dimension;
    double scale_x = tsx / max_thumbnail_dimension;
    double scale = scale_y > scale_x ? scale_y : scale_x;
    int x_dim = tsx / scale;
    int y_dim = tsy / scale;

    GdkPixbuf *pb_s =
        gdk_pixbuf_scale_simple(pb, x_dim, y_dim, GDK_INTERP_BILINEAR);
    gdk_pixbuf_unref(pb);

    if (!pb_s)
        printf("Failed to allocate scaled thumbnail pixbuf: %s\n", input_data);

    meta_free(meta);
    return pb_s;
}

static GdkPixbuf *
make_asf_internal_thumb(const char *input_metadata, const char *input_data,
                        size_t max_thumbnail_dimension)
{
    FILE *fpIn = fopen(input_data, "rb");
    if (!fpIn)
        return NULL;

    meta_parameters *meta = silent_meta_read(input_metadata);

    // use a larger dimension at first, for our crude scaling.  We will
    // use a better scaling method later, from GdbPixbuf
    int larger_dim = 1024;

    // Vertical and horizontal scale factors required to meet the
    // max_thumbnail_dimension part of the interface contract.
    int vsf = ceil (meta->general->line_count / larger_dim);
    int hsf = ceil (meta->general->sample_count / larger_dim);
    // Overall scale factor to use is the greater of vsf and hsf.
    int sf = (hsf > vsf ? hsf : vsf);

    // Thumbnail image sizes.
    size_t tsx = meta->general->sample_count / sf;
    size_t tsy = meta->general->line_count / sf;

    guchar *data = g_new(guchar, 3*tsx*tsy);
    float *fdata = g_new(float, 3*tsx*tsy);

    // Form the thumbnail image by grabbing individual pixels.
    size_t ii, jj;
    float *line = g_new (float, meta->general->sample_count);

    // Keep track of the average pixel value, so later we can do a 2-sigma
    // scaling - makes the thumbnail look a little nicer and more like what
    // they'd get if they did the default jpeg export.
    double avg = 0.0;
    for ( ii = 0 ; ii < tsy ; ii++ ) {

        get_float_line(fpIn, meta, ii*sf, line);

        for (jj = 0; jj < tsx; ++jj) {
            fdata[jj + ii*tsx] = line[jj*sf];
            avg += line[jj*sf];
        }
    }
    g_free (line);
    fclose(fpIn);

    // Compute the std devation
    avg /= tsx*tsy;
    double stddev = 0.0;
    for (ii = 0; ii < tsx*tsy; ++ii)
        stddev += ((double)fdata[ii] - avg) * ((double)fdata[ii] - avg);
    stddev = sqrt(stddev / (tsx*tsy));

    // Set the limits of the scaling - 2-sigma on either side of the mean
    double lmin = avg - 2*stddev;
    double lmax = avg + 2*stddev;

    // Now actually scale the data, and convert to bytes.
    // Note that we need 3 values, one for each of the RGB channels.
    for (ii = 0; ii < tsx*tsy; ++ii) {
        float val = fdata[ii];
        guchar uval;
        if (val < lmin)
            uval = 0;
        else if (val > lmax)
            uval = 255;
        else
            uval = (guchar) round(((val - lmin) / (lmax - lmin)) * 255);

        int n = 3*ii;
        data[n] = uval;
        data[n+1] = uval;
        data[n+2] = uval;
    }

    g_free(fdata);

    // Create the pixbuf
    GdkPixbuf *pb =
        gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, FALSE,
                                 8, tsx, tsy, tsx*3, destroy_pb_data, NULL);

    if (!pb) {
        printf("Failed to create the thumbnail pixbuf: %s\n", input_data);
        meta_free(meta);
        g_free(data);
        return NULL;
    }

    // Scale down to the size we actually want, using the built-in Gdk
    // scaling method, much nicer than what we did above

    // Must ensure we scale the same in each direction
    double scale_y = tsy / max_thumbnail_dimension;
    double scale_x = tsx / max_thumbnail_dimension;
    double scale = scale_y > scale_x ? scale_y : scale_x;
    int x_dim = tsx / scale;
    int y_dim = tsy / scale;

    GdkPixbuf *pb_s =
        gdk_pixbuf_scale_simple(pb, x_dim, y_dim, GDK_INTERP_BILINEAR);
    gdk_pixbuf_unref(pb);

    if (!pb_s)
        printf("Failed to allocate scaled thumbnail pixbuf: %s\n", input_data);

    meta_free(meta);
    return pb_s;
}

GdkPixbuf *
make_complex_thumb(meta_parameters* imd,
                   char *meta_name, char *data_name,
                   size_t max_thumbnail_dimension)
{
    FILE *fpIn = fopen(data_name, "rb");
    if (!fpIn)
    {
        // failed for some reason, quit without thumbnailing
        meta_free(imd);
        return NULL;
    }

    struct IOF_VFDR image_fdr;                /* CEOS File Descriptor Record */
    get_ifiledr(meta_name, &image_fdr);
    int leftFill = image_fdr.lbrdrpxl;
    int rightFill = image_fdr.rbrdrpxl;
    int headerBytes = firstRecordLen(data_name) +
        (image_fdr.reclen - (imd->general->sample_count + leftFill + rightFill)
         * image_fdr.bytgroup);

    int larger_dim = 512;
    int ns = imd->general->sample_count;
    int nl = imd->general->line_count;
    int lc = imd->sar->look_count;
    int ii, kk;

    // Vertical and horizontal scale factors required to meet the
    // max_thumbnail_dimension part of the interface contract.
    int vsf = ceil (nl / lc / larger_dim);
    int hsf = ceil (ns / larger_dim);

    // Overall scale factor to use is the greater of vsf and hsf.
    int sf = (hsf > vsf ? hsf : vsf);

    // Thumbnail image sizes.
    int tsx = ns / sf;
    int tsy = nl / lc / sf;

    float *fdata = MALLOC(sizeof(float)*tsx*tsy); // raw data, prior to scaling
    guchar *ucdata = g_new(guchar, 3*tsx*tsy);    // RGB data, after scaling

    // These are the input arrays, directly from the file (complex)
    // only one of these is actually allocated, depends on input data type
    unsigned char *chars=NULL;
    short *shorts=NULL;
    unsigned int *ints=NULL;
    float *floats=NULL;

    // This is the array of multilooked & converted to amplitude
    float *amp_line=MALLOC(sizeof(float)*ns);

    // allocate the right input array
    switch (imd->general->data_type) {
      case COMPLEX_BYTE:
        chars = MALLOC(sizeof(unsigned char)*ns*2*lc);
        break;
      case COMPLEX_INTEGER16:
        shorts = MALLOC(sizeof(short)*ns*2*lc);
        break;
      case COMPLEX_INTEGER32:
        ints = MALLOC(sizeof(unsigned int)*ns*2*lc);
        break;
      case COMPLEX_REAL32:
        floats = MALLOC(sizeof(float)*ns*2*lc);
        break;
      default:
        asfPrintWarning("Invalid data type for complex thumbnail: %d\n",
                        imd->general->data_type);
        return NULL;
    }

    // now read in "look_count" lines at a time, skipping ahead in the file
    // according to our thumbnail downsizing.  however, the multilooking is
    // always done on consecutive lines
    float avg = 0.0;
    for ( ii = 0 ; ii < tsy ; ii++ ) {
        int jj;

        //printf("line %d nl=%d\n", ii*sf*lc, nl);
        if (imd->general->data_type == COMPLEX_INTEGER16)
        {
            for (kk=0; kk<lc; ++kk) {
                assert(shorts);
                int line = ii*sf*lc + kk;
                //printf("reading line %d\n", line);

                // if we would read past the end of the file, just read the
                // last line multiple times.  the last line in the thumb might
                // look weird, but hey it is just a thumbnail.  if this code
                // is pulled over to create_thumbs, then we should check for
                // this possiblity ahead of time, and back up to start the
                // process at an earlier line
                if (line>=nl)
                  line=nl-1;

                long long offset =
                  (long long)headerBytes+line*(long long)image_fdr.reclen;
                FSEEK64(fpIn, offset, SEEK_SET);
                FREAD(shorts + kk*ns*2, sizeof(short), ns*2, fpIn);
            }

            // proper endianness for all those int16s
            for (jj=0; jj<ns*2*lc; ++jj)
                big16(shorts[jj]);

            // now multilook and convert to amplitude
            for (jj=0; jj<ns; ++jj) {
                complexFloat c;
                c.real = c.imag = 0.0;
                for (kk=0; kk<lc; ++kk) {
                    c.real += (float)shorts[kk*ns*2 + jj*2];
                    c.imag += (float)shorts[kk*ns*2 + jj*2 + 1];
                }
                if (lc>1) {
                    c.real /= (float)lc;
                    c.imag /= (float)lc;
                }
                amp_line[jj] = sqrt(c.real*c.real + c.imag*c.imag);
            }
        }
        else if (imd->general->data_type == COMPLEX_BYTE)
        {
            for (kk=0; kk<lc; ++kk) {
                int line = ii*sf*lc + kk;
                if (line>=nl) line=nl-1;
                long long offset =
                  (long long)headerBytes+line*(long long)image_fdr.reclen;
                FSEEK64(fpIn, offset, SEEK_SET);
                FREAD(chars + kk*ns*2, sizeof(unsigned char), ns*2, fpIn);
            }

            // now multilook and convert to amplitude
            for (jj=0; jj<ns; ++jj) {
                complexFloat c;
                c.real = c.imag = 0.0;
                for (kk=0; kk<lc; ++kk) {
                    c.real += (float)chars[kk*ns*2 + jj*2];
                    c.imag += (float)chars[kk*ns*2 + jj*2 + 1];
                }
                c.real /= (float)lc;
                c.imag /= (float)lc;
                amp_line[jj] = sqrt(c.real*c.real + c.imag*c.imag);
            }
        }
        else if (imd->general->data_type == COMPLEX_REAL32)
        {
            for (kk=0; kk<lc; ++kk) {
                int line = ii*sf*lc + kk;
                if (line>=nl) line=nl-1;
                long long offset =
                  (long long)headerBytes+line*(long long)image_fdr.reclen;
                FSEEK64(fpIn, offset, SEEK_SET);
                FREAD(floats + kk*ns*2, sizeof(float), ns*2, fpIn);
            }

            // proper endianness for all those real32s
            for (jj = 0; jj < ns*2*lc; ++jj)
                ieee_big32(floats[jj]);

            // now multilook and convert to amplitude
            for (jj=0; jj<ns; ++jj) {
                complexFloat c;
                c.real = c.imag = 0.0;
                for (kk=0; kk<lc; ++kk) {
                    c.real += floats[kk*ns*2 + jj*2];
                    c.imag += floats[kk*ns*2 + jj*2 + 1];
                }
                c.real /= (float)lc;
                c.imag /= (float)lc;
                amp_line[jj] = sqrt(c.real*c.real + c.imag*c.imag);
            }
        }

        for ( jj = 0 ; jj < tsx ; jj++ ) {
            // Current sampled value.
            float csv;

            // We will average a couple pixels together.
            if ( jj * sf < ns - 1 ) {
                csv = (amp_line[jj*sf] + amp_line[jj*sf+1]) * 0.5;
            }
            else {
                csv = (amp_line[jj*sf] + amp_line[jj*sf-1]) * 0.5;
            }

            fdata[ii*tsx + jj] = csv;
            avg += csv;
        }
    }
    FREE(chars);
    FREE(ints);
    FREE(floats);
    FREE(shorts);
    fclose(fpIn);

    // Compute the std devation
    avg /= tsx*tsy;
    float stddev = 0.0;
    for (ii = 0; ii < tsx*tsy; ++ii)
        stddev += (fdata[ii] - avg) * (fdata[ii] - avg);
    stddev = sqrt(stddev / (tsx*tsy));

    // Set the limits of the scaling - 2-sigma on either side of the mean
    float lmin = avg - 2*stddev;
    float lmax = avg + 2*stddev;

    // Now actually scale the data, and convert to bytes.
    // Note that we need 3 values, one for each of the RGB channels.
    for (ii = 0; ii < tsx*tsy; ++ii) {
        float val = fdata[ii];
        guchar uval;
        if (val < lmin)
            uval = 0;
        else if (val > lmax)
            uval = 255;
        else
            uval = (unsigned char) round(((val - lmin) / (lmax - lmin)) * 255);

        int n = 3*ii;
        ucdata[n] = uval;
        ucdata[n+1] = uval;
        ucdata[n+2] = uval;
    }

    FREE(fdata);

    // Create the pixbuf
    GdkPixbuf *pb =
        gdk_pixbuf_new_from_data(ucdata, GDK_COLORSPACE_RGB, FALSE,
                                 8, tsx, tsy, tsx*3, destroy_pb_data, NULL);

    if (!pb) {
        printf("Failed to create the thumbnail pixbuf: %s\n", data_name);
        meta_free(imd);
        g_free(ucdata);
        return NULL;
    }

    // Scale down to the size we actually want, using the built-in Gdk
    // scaling method, much nicer than what we did above

    // Must ensure we scale the same in each direction
    double scale_y = tsy / max_thumbnail_dimension;
    double scale_x = tsx / max_thumbnail_dimension;
    double scale = scale_y > scale_x ? scale_y : scale_x;
    int x_dim = tsx / scale;
    int y_dim = tsy / scale;

    GdkPixbuf *pb_s =
        gdk_pixbuf_scale_simple(pb, x_dim, y_dim, GDK_INTERP_BILINEAR);
    gdk_pixbuf_unref(pb);

    if (!pb_s) {
        printf("Failed to allocate scaled thumbnail pixbuf: %s\n", data_name);
        meta_free(imd);
        return NULL;
    }

    meta_free(imd);
    FREE(data_name);
    FREE(meta_name);
    return pb_s;
}

GdkPixbuf *
make_input_image_thumbnail_pixbuf (const char *input_metadata,
                                   const char *input_data,
                                   size_t max_thumbnail_dimension)
{
    /* This can happen if we don't get around to drawing the thumbnail
       until the file has already been processes & cleaned up, don't want
       to crash in that case. */
    if (!fileExists(input_metadata))
        return NULL;

    char *ext = findExt(input_data);

    // don't have support for thumbnails of geotiffs yet
    if (ext && (strcmp_case(ext, ".tif")==0 || strcmp_case(ext, ".tiff")==0))
        return make_geotiff_thumb(input_metadata, input_data,
                                  max_thumbnail_dimension);

    // no point in a thumbnail of Level 0 data
    if (ext && strcmp_case(ext, ".raw") == 0)
        return NULL;

    // split some cases into their own funcs
    if (ext && strcmp_case(ext, ".img") == 0)
        return make_asf_internal_thumb(input_metadata, input_data,
                                       max_thumbnail_dimension);

    // for airsar, need to check the metadata extension, that's more reliable
    char *meta_ext = findExt(input_metadata);
    if (meta_ext && strcmp_case(meta_ext, ".airsar") == 0)
        return make_airsar_thumb(input_metadata, input_data,
                                 max_thumbnail_dimension);

    // Input metadata
    meta_parameters *imd;
    char *data_name, *met, **dataName;
    int nBands = 1;

    int pre = has_prepension(input_metadata);
    if (pre > 0)
    {
        char *baseName;
        baseName = MALLOC(sizeof(char)*255);
        char filename[255], dirname[255];
        split_dir_and_file(input_metadata, dirname, filename);
        met = MALLOC(sizeof(char)*(strlen(input_metadata)+1));
        sprintf(met, "%s%s", dirname, filename + pre);

        get_ceos_data_name(met, baseName, &dataName, &nBands);

        imd = silent_meta_create(met);
        data_name = STRDUP(dataName[0]);

        free(baseName);
    }
    else
    {
        imd = silent_meta_create (input_metadata);
        data_name = STRDUP(input_data);
        met = STRDUP(input_metadata);
    }

    if (imd->general->data_type >= COMPLEX_BYTE &&
        imd->general->data_type <= COMPLEX_REAL32)
    {
        return make_complex_thumb(imd, met, data_name,
                                  max_thumbnail_dimension);
    }
    else if (imd->general->data_type != BYTE &&
             imd->general->data_type != INTEGER16)
// Turning off support for these guys for now.
//        imd->general->data_type != INTEGER32 &&
//        imd->general->data_type != REAL32 &&
//        imd->general->data_type != REAL64)
    {
        /* don't know how to make a thumbnail for this type ... */
        printf("Cannot make thumbnail for: %d\n", imd->general->data_type);
        return NULL;
    }

    guchar *data=NULL;
    int tsx=-1, tsy=-1;
    int kk;
    for (kk=0; kk<nBands; kk++) {
      if (nBands > 1 && strcmp_case(imd->general->sensor_name, "PRISM") != 0) {
        data_name = STRDUP(dataName[kk]);
      }
      FILE *fpIn = fopen(data_name, "rb");
      if (!fpIn)
      {
        // failed for some reason, quit without thumbnailing
        meta_free(imd);
        return NULL;
      }

      struct IOF_VFDR image_fdr;              /* CEOS File Descriptor Record */
      get_ifiledr(met, &image_fdr);
      int leftFill = image_fdr.lbrdrpxl;
      int rightFill = image_fdr.rbrdrpxl;
      int headerBytes = firstRecordLen(data_name) +
        (image_fdr.reclen - (imd->general->sample_count + leftFill + rightFill)
         * image_fdr.bytgroup);

      // use a larger dimension at first, for our crude scaling.  We will
      // use a better scaling method later, from GdbPixbuf
      int larger_dim = 512;

      // Vertical and horizontal scale factors required to meet the
      // max_thumbnail_dimension part of the interface contract.
      int vsf = ceil (imd->general->line_count / larger_dim);
      int hsf = ceil (imd->general->sample_count / larger_dim);
      // Overall scale factor to use is the greater of vsf and hsf.
      int sf = (hsf > vsf ? hsf : vsf);

      // Thumbnail image sizes.
      tsx = imd->general->sample_count / sf;
      tsy = imd->general->line_count / sf;

      // Thumbnail image buffers - 'idata' is the temporary data prior
      // to scaling to 2-sigma, 'data' is the byte buffer used to create the
      // pixbuf, it will need 3 bytes per value, all equal, since the pixbuf
      // wants an RGB value.
      int *idata = g_new(int, tsx*tsy);
      if (kk == 0)
        data = g_new(guchar, 3*tsx*tsy);

      // Form the thumbnail image by grabbing individual pixels.  FIXME:
      // Might be better to do some averaging or interpolating.
      size_t ii;
      unsigned short *line = g_new (unsigned short, imd->general->sample_count);
      unsigned char *bytes = g_new (unsigned char, imd->general->sample_count);

      // Keep track of the average pixel value, so later we can do a 2-sigma
      // scaling - makes the thumbnail look a little nicer and more like what
      // they'd get if they did the default jpeg export.
      double avg = 0.0;
      for ( ii = 0 ; ii < tsy ; ii++ ) {

        size_t jj;
        long long offset =
          (long long)headerBytes+ii*sf*(long long)image_fdr.reclen;

        FSEEK64(fpIn, offset, SEEK_SET);
        if (imd->general->data_type == INTEGER16)
        {
          FREAD(line, sizeof(unsigned short), imd->general->sample_count,
                fpIn);

          for (jj = 0; jj < imd->general->sample_count; ++jj)
            big16(line[jj]);
        }
        else if (imd->general->data_type == BYTE)
        {
          FREAD(bytes, sizeof(unsigned char), imd->general->sample_count,
                fpIn);

          for (jj = 0; jj < imd->general->sample_count; ++jj)
            line[jj] = (unsigned short)bytes[jj];
        }

        for ( jj = 0 ; jj < tsx ; jj++ ) {
          // Current sampled value.
          double csv;

          // We will average a couple pixels together.
          if ( jj * sf < imd->general->line_count - 1 ) {
            csv = (line[jj * sf] + line[jj * sf + 1]) / 2;
          }
          else {
            csv = (line[jj * sf] + line[jj * sf - 1]) / 2;
          }

          idata[ii*tsx + jj] = (int)csv;
          avg += csv;
        }
      }
      g_free (line);
      g_free (bytes);
      fclose(fpIn);

      // Compute the std devation
      avg /= tsx*tsy;
      double stddev = 0.0;
      for (ii = 0; ii < tsx*tsy; ++ii)
        stddev += ((double)idata[ii] - avg) * ((double)idata[ii] - avg);
      stddev = sqrt(stddev / (tsx*tsy));

      // Set the limits of the scaling - 2-sigma on either side of the mean
      double lmin = avg - 2*stddev;
      double lmax = avg + 2*stddev;

      // Now actually scale the data, and convert to bytes.
      // Note that we need 3 values, one for each of the RGB channels.
      for (ii = 0; ii < tsx*tsy; ++ii) {
        int val = idata[ii];
        guchar uval;
        if (val < lmin)
            uval = 0;
        else if (val > lmax)
            uval = 255;
        else
            uval = (guchar) round(((val - lmin) / (lmax - lmin)) * 255);

        int n = 3*ii;
    // Single-band image
    if (nBands == 1) {
      data[n] = uval;
      data[n+1] = uval;
      data[n+2] = uval;
    }
    // Multi-band image
    else {
      if (strcmp_case(imd->general->sensor_name, "PRISM") == 0) {
        data[n] = uval;
        data[n+1] = uval;
        data[n+2] = uval;
      }
      else if (strcmp_case(imd->general->sensor_name, "AVNIR") == 0) {
        if (kk == 1)
          data[n+2] = uval;
        else if (kk == 2)
          data[n+1] = uval;
        else if (kk == 3)
          data[n] = uval;
      }
      else if (strcmp_case(imd->general->sensor_name, "SAR") == 0) {
        // Keep all SAR images grayscale, reading the first band
        if (kk == 0) {
          data[n] = uval;
          data[n+1] = uval;
          data[n+2] = uval;
        }
        /*
        // Dual-pol data
        if (nBands == 2) {
          if (kk == 0)
        data[n] = uval;
          else if (kk == 1) {
        data[n+1] = uval;
        data[n+2] = 0;
          }
        }
        // Quad-pol data
        else if (nBands == 4) {
          if (kk == 0)
        data[n] = uval;
          else if (kk == 1)
        data[n+1] = uval;
          else if (kk == 3)
        data[n+2] = uval;
        }
        */
      }
    }
      }

      g_free(idata);
    }

    // Create the pixbuf
    GdkPixbuf *pb =
      gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, FALSE,
                               8, tsx, tsy, tsx*3, destroy_pb_data, NULL);

    if (!pb) {
        printf("Failed to create the thumbnail pixbuf: %s\n", data_name);
        meta_free(imd);
        g_free(data);
        return NULL;
    }

    // Scale down to the size we actually want, using the built-in Gdk
    // scaling method, much nicer than what we did above

    // Must ensure we scale the same in each direction
    double scale_y = tsy / max_thumbnail_dimension;
    double scale_x = tsx / max_thumbnail_dimension;
    double scale = scale_y > scale_x ? scale_y : scale_x;
    int x_dim = tsx / scale;
    int y_dim = tsy / scale;

    GdkPixbuf *pb_s =
        gdk_pixbuf_scale_simple(pb, x_dim, y_dim, GDK_INTERP_BILINEAR);
    gdk_pixbuf_unref(pb);

    if (!pb_s) {
        printf("Failed to allocate scaled thumbnail pixbuf: %s\n", data_name);
        meta_free(imd);
        return NULL;
    }

    meta_free(imd);
    FREE(data_name);
    FREE(met);
    return pb_s;
}

int get_geotiff_float_line(TIFF *fpIn, meta_parameters *meta, long row, int band, float *line)
{
    short sample_format;
    short bits_per_sample;
    short planar_config;
    int is_scanline_format;
    int is_palette_color_tiff;
    data_type_t data_type;
    short num_bands;
    int ret = get_tiff_data_config(fpIn,
                                   &sample_format,
                                   &bits_per_sample,
                                   &planar_config,
                                   &data_type,
                                   &num_bands,
                                   &is_scanline_format,
                                   &is_palette_color_tiff,
                                   REPORT_LEVEL_WARNING);
    if (ret) {
        // Unfortunately, get_tiff_data_config() returns 0 on success, -1 on failure
        return 0;
    }
    tiff_type_t tiffInfo;
    get_tiff_type(fpIn, &tiffInfo);

    tsize_t scanlineSize = TIFFScanlineSize(fpIn);
    if (scanlineSize <= 0) {
        return 0;
    }
    tdata_t *tif_buf = _TIFFmalloc(scanlineSize);
    if (!tif_buf) {
        return 0;
    }
    switch (tiffInfo.format) {
        case SCANLINE_TIFF:
            if (planar_config == PLANARCONFIG_CONTIG || num_bands == 1) {
                TIFFReadScanline(fpIn, tif_buf, row, 0);
            }
            else {
                // Planar configuration is band-sequential
                TIFFReadScanline(fpIn, tif_buf, row, band);
            }
            break;
        case STRIP_TIFF:
            ReadScanline_from_TIFF_Strip(fpIn, tif_buf, row, band);
            break;
        case TILED_TIFF:
            // Planar configuration is band-sequential
            ReadScanline_from_TIFF_TileRow(fpIn, tif_buf, row, band);
            break;
        default:
            return 0;
            break;
    }
    int col;
    for (col=0; col < meta->general->sample_count; col++) {
        switch (bits_per_sample) {
            case 8:
                switch(sample_format) {
                    case SAMPLEFORMAT_UINT:
                        ((float*)line)[col] = (float)(((uint8*)tif_buf)[col]);
                        break;
                    case SAMPLEFORMAT_INT:
                        ((float*)line)[col] = (float)(((int8*)tif_buf)[col]);
                        break;
                    default:
                        // No such thing as an 8-bit IEEE float
                        return 0;
                        break;
                }
                break;
            case 16:
                switch(sample_format) {
                    case SAMPLEFORMAT_UINT:
                        ((float*)line)[col] = (float)(((uint16*)tif_buf)[col]);
                        break;
                    case SAMPLEFORMAT_INT:
                        ((float*)line)[col] = (float)(((int16*)tif_buf)[col]);
                        break;
                    default:
                        // No such thing as an 16-bit IEEE float
                        meta_free(meta);
                        return 0;
                        break;
                }
                break;
            case 32:
                switch(sample_format) {
                    case SAMPLEFORMAT_UINT:
                        ((float*)line)[col] = (float)(((uint32*)tif_buf)[col]);
                        break;
                    case SAMPLEFORMAT_INT:
                        ((float*)line)[col] = (float)(((long*)tif_buf)[col]);
                        break;
                    case SAMPLEFORMAT_IEEEFP:
                        ((float*)line)[col] = (float)(((float*)tif_buf)[col]);
                        break;
                    default:
                        return 0;
                        break;
                }
                break;
            default:
                return 0;
                break;
        }
    }

    return 1;
}

