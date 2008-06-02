// __BEGIN_LICENSE__
// 
// Copyright (C) 2006 United States Government as represented by the
// Administrator of the National Aeronautics and Space Administration
// (NASA).  All Rights Reserved.
// 
// Copyright 2006 Carnegie Mellon University. All rights reserved.
// 
// This software is distributed under the NASA Open Source Agreement
// (NOSA), version 1.3.  The NOSA has been approved by the Open Source
// Initiative.  See the file COPYING at the top of the distribution
// directory tree for the complete NOSA document.
// 
// THE SUBJECT SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY OF ANY
// KIND, EITHER EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT NOT
// LIMITED TO, ANY WARRANTY THAT THE SUBJECT SOFTWARE WILL CONFORM TO
// SPECIFICATIONS, ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
// A PARTICULAR PURPOSE, OR FREEDOM FROM INFRINGEMENT, ANY WARRANTY THAT
// THE SUBJECT SOFTWARE WILL BE ERROR FREE, OR ANY WARRANTY THAT
// DOCUMENTATION, IF PROVIDED, WILL CONFORM TO THE SUBJECT SOFTWARE.
// 
// __END_LICENSE__

#include <vw/Camera/Exif.h>

#include <math.h>

// --------------------------------------------------------------
//                   ExifView
// --------------------------------------------------------------

vw::camera::ExifView::ExifView(std::string const& filename) {
  if (!m_data.import_data(filename)) {
    vw_throw(ExifErr() << "Could not parse EXIF data out of \"" << filename << "\".");
  }
}

// Query the data by tag ID (common tags are enumerated at the top if
// Exif.h)
void vw::camera::ExifView::query_by_tag(const uint16 tag, int& value) const {
  bool success = m_data.get_tag_value(tag, value);
  if (!success) vw_throw(ExifErr() << "Could not read EXIF tag " << tag << ".");
}

// Query the data by tag ID (common tags are enumerated at the top if
// Exif.h)
void vw::camera::ExifView::query_by_tag(const uint16 tag, double& value) const {
  bool success = m_data.get_tag_value(tag, value);
  if (!success) vw_throw(ExifErr() << "Could not read EXIF tag: " << tag << ".");
}

// Query the data by tag ID (common tags are enumerated at the top if
// Exif.h)
void vw::camera::ExifView::query_by_tag(const uint16 tag, std::string& value) const {
  bool success = m_data.get_tag_value(tag, value);
  if (!success) vw_throw(ExifErr() << "Could not read EXIF tag: " << tag << ".");
}

// Camera info
std::string vw::camera::ExifView::get_make() const {
  std::string make;
  query_by_tag(EXIF_Make, make);
  return make;
}

std::string vw::camera::ExifView::get_model() const {
  std::string model;
  query_by_tag(EXIF_Model, model);
  return model;
}

// Camera settings
double vw::camera::ExifView::get_f_number() const {
  double value;
  try { 
    query_by_tag(EXIF_FNumber, value); 
    return value;
  } catch (ExifErr &e) {
    query_by_tag(EXIF_ApertureValue, value); 
    return pow(2.0, value * 0.5);
  }
}

double vw::camera::ExifView::get_exposure_time() const {
  double value;
  try { 
    query_by_tag(EXIF_ExposureTime, value); 
    return value;
  } catch (ExifErr &e) {
    query_by_tag(EXIF_ShutterSpeedValue, value); 
    return pow(2.0, -value);
  }
}

double vw::camera::ExifView::get_iso() const {
  double value;
  try { 
    query_by_tag(EXIF_ISOSpeedRatings, value); 
    return value;
  } catch (ExifErr &e) {
    query_by_tag(EXIF_ExposureIndex, value); 
    return value;
  }
  // otherwise probably have to dig through MakerNote
}

// Returns focal length of camera in mm, as if image sensor were 36mm x 24mm
double vw::camera::ExifView::get_focal_length_35mm_equiv() const {
  double value;
  try { 
    query_by_tag(EXIF_FocalLengthIn35mmFilm, value); // 0 if unknown
    if (value > 0) return value;
  } catch (ExifErr &e) {}

  // Compute from various other statistics
  double focal_length;
  double pixel_x_dimension, focal_plane_x_resolution;
  double pixel_y_dimension, focal_plane_y_resolution;
  query_by_tag(EXIF_FocalLength, focal_length);
  query_by_tag(EXIF_PixelXDimension, pixel_x_dimension);
  query_by_tag(EXIF_PixelYDimension, pixel_y_dimension);
  query_by_tag(EXIF_FocalPlaneXResolution, focal_plane_x_resolution);
  if (focal_plane_x_resolution <= 0) vw_throw(ExifErr() << "Illegal value for FocalPlaneXResolution");
  query_by_tag(EXIF_FocalPlaneYResolution, focal_plane_y_resolution);
  if (focal_plane_y_resolution <= 0) vw_throw(ExifErr() << "Illegal value for FocalPlaneYResolution");
  int focal_plane_resolution_unit = 2;
  try { query_by_tag(EXIF_FocalPlaneResolutionUnit, focal_plane_resolution_unit); } catch (ExifErr) {}
  double focal_plane_resolution_unit_in_mm;
  switch (focal_plane_resolution_unit) {
  case 2: // inch
    focal_plane_resolution_unit_in_mm = 25.4;
    break;
  case 3: // cm
    focal_plane_resolution_unit_in_mm = 10.;
    break;
  default:
    vw_throw(ExifErr() << "Illegal value for FocalPlaneResolutionUnit");
  }
  double x_pixel_size_in_mm = focal_plane_resolution_unit_in_mm / focal_plane_x_resolution;
  double y_pixel_size_in_mm = focal_plane_resolution_unit_in_mm / focal_plane_y_resolution;
  double sensor_width_in_mm = x_pixel_size_in_mm * pixel_x_dimension;
  double sensor_height_in_mm = y_pixel_size_in_mm * pixel_y_dimension;
  double sensor_diagonal_in_mm = hypot(sensor_width_in_mm, sensor_height_in_mm);
  if (sensor_diagonal_in_mm == 0) vw_throw(ExifErr() << "Illegal value while computing 35mm equiv focal length");
  return focal_length * sqrt(36.*36.+24.*24.) / sensor_diagonal_in_mm;
}

// FIXME: report in some logical way when value doesn't exist
double vw::camera::ExifView::get_aperture_value() const {
  double value;
  try { 
    query_by_tag(EXIF_ApertureValue, value); 
    return value;
  } catch (ExifErr &e) {
    query_by_tag(EXIF_FNumber, value); 
    return 2 * log(value)/log(2.);  // log2(value) = log(value)/log(2)
  }
}

double vw::camera::ExifView::get_time_value() const {
  double value;
  try { 
    query_by_tag(EXIF_ShutterSpeedValue, value); 
    return value;
  } catch (ExifErr &e) {
    query_by_tag(EXIF_ExposureTime, value); 
    return log(1/value)/log(2.); // log2(value) = log(value)/log(2)
  }
}

double vw::camera::ExifView::get_exposure_value() const {
  return get_time_value() + get_aperture_value();
}

// Film speed value is log_2(N * iso)
// 
// N is a constant that establishes the relationship between the ASA
// arithmetic film speed and the ASA speed value. The value of N
// 1/3.125, as defined by the EXIF 2.2 spec.  The constant K is the
// reflected light meter calibration constant.  See
// http://en.wikipedia.org/wiki/Light_meter#Exposure_meter_calibration
//
double vw::camera::ExifView::get_film_speed_value() const {
  double iso = (double)get_iso();
  const double N = 1/3.125;
  //  const double K = 12.5; 
  return log(iso * N)/log(2.); // log2(value) = log(value)/log(2)
}

double vw::camera::ExifView::get_luminance_value() const {
  double Bv;
  try{
    query_by_tag(EXIF_BrightnessValue, Bv);
    return Bv;
  } catch (ExifErr &e) { 
    try {
      double Av = get_aperture_value();
      double Tv = get_time_value();
      double Sv = get_film_speed_value();
      return (Av + Tv - Sv);
    } catch (ExifErr &e) {
      vw_throw(ExifErr() << "Insufficient EXIF information to compute brightness value.");
      return 0; // never reached
    }
  }
}

// Film speed value is log_2(N * iso)
// 
// N is a constant that establishes the relationship between the ASA
// arithmetic film speed and the ASA speed value. The value of N
// 1/3.125, as defined by the EXIF 2.2 spec.  The constant K is the
// reflected light meter calibration constant.  See
// http://en.wikipedia.org/wiki/Light_meter#Exposure_meter_calibration
//
double vw::camera::ExifView::get_average_luminance() const {
  // const double N = 1/3.125;
  const double K = 12.5; 

  try {
    double A = get_f_number();
    double T = get_exposure_time();
    double S = get_iso();
    double B = (A*A*K)/(T*S);
    return B;
  } catch (ExifErr &e) {
    vw_throw(ExifErr() << "Insufficient EXIF information to compute average scene luminance.");
    return 0; // never reached
  }
}

size_t vw::camera::ExifView::get_thumbnail_location() const {
  int offset;
  // query_by_tag throws if tag isn't found
  query_by_tag(vw::camera::EXIF_ThumbnailOffset, offset);
  return offset + m_data.get_exif_location();
}
