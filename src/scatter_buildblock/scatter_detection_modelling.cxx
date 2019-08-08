//
//
/*
  Copyright (C) 2004- 2009, Hammersmith Imanet
  This file is part of STIR.

  This file is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This file is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details. 

  See STIR/LICENSE.txt for details
*/
/*!
  \file
  \ingroup scatter
  \brief Implementations of detection modelling in stir::ScatterEstimationByBin

  \author Charalampos Tsoumpas
  \author Pablo Aguiar
  \author Nikolaos Dikaios
  \author Kris Thielemans

*/

#include "stir/scatter/ScatterSimulation.h"
#include "stir/ProjDataInfoCylindricalNoArcCorr.h"
#include "stir/numerics/erf.h"
#include "stir/info.h"
#include "stir/CPUTimer.h"
#include <iostream>

START_NAMESPACE_STIR
unsigned 
ScatterSimulation::
find_in_detection_points_vector(const CartesianCoordinate3D<float>& coord) const
{
  unsigned int ret_value = 0;
#pragma omp critical(SCATTERESTIMATIONFINDDETECTIONPOINTS)
  {
  std::vector<CartesianCoordinate3D<float> >::const_iterator iter=
    std::find(detection_points_vector.begin(),
              detection_points_vector.end(),
              coord);
  if (iter != detection_points_vector.end())
    {
      ret_value = iter-detection_points_vector.begin();
    }
  else
    {
      if (detection_points_vector.size()==static_cast<std::size_t>(this->total_detectors))
        error("More detection points than we think there are!\n");

      detection_points_vector.push_back(coord);
      ret_value = detection_points_vector.size()-1;
    }
  }
  return ret_value;
}

void
ScatterSimulation::
find_detectors(unsigned& det_num_A, unsigned& det_num_B, const Bin& bin) const
{
  CartesianCoordinate3D<float> detector_coord_A, detector_coord_B;
  this->proj_data_info_cyl_noarc_cor_sptr->
    find_cartesian_coordinates_of_detection(
                                            detector_coord_A,detector_coord_B,bin);
  det_num_A =
    this->find_in_detection_points_vector(detector_coord_A + 
                                          this->shift_detector_coordinates_to_origin);
  det_num_B =
    this->find_in_detection_points_vector(detector_coord_B + 
                                          this->shift_detector_coordinates_to_origin);
}

float
ScatterSimulation::
compute_emis_to_det_points_solid_angle_factor(
                                              const CartesianCoordinate3D<float>& emis_point,
                                              const CartesianCoordinate3D<float>& detector_coord)
{
  
  const CartesianCoordinate3D<float> dist_vector = emis_point - detector_coord ;
 

  const float dist_emis_det_squared = norm_squared(dist_vector);

  const float emis_det_solid_angle_factor = 1.F/ dist_emis_det_squared ;

  return emis_det_solid_angle_factor ;
}

float
ScatterSimulation::
detection_efficiency(const float energy, const int en_window) const
{
  // factor 2.35482 is used to convert FWHM to sigma
  const float sigma_times_sqrt2= 
    sqrt(2.*energy*this->proj_data_info_cyl_noarc_cor_sptr->get_scanner_ptr()->get_reference_energy())*
          this->proj_data_info_cyl_noarc_cor_sptr->get_scanner_ptr()->get_energy_resolution()/2.35482f;  // 2.35482=2 * sqrt( 2 * ( log(2) )
  
  // sigma_times_sqrt2= sqrt(2) * sigma   // resolution proportional to FWHM    
  
  const float efficiency =
    0.5f*( erf((this->template_exam_info_sptr->get_high_energy_thres(en_window)-energy)/sigma_times_sqrt2)
          - erf((this->template_exam_info_sptr->get_low_energy_thres(en_window)-energy)/sigma_times_sqrt2 ));
  /* Maximum efficiency is 1.*/

  return efficiency;
}

std::vector<float>
ScatterSimulation::detection_efficiency(const float LLD, const float HLD, const float size, const float incoming_photon_energy) const
{

    std::vector<float> energy_range(size);
    std::vector<float> out(size);
    float increment_x = (HLD - LLD)/size;
    float increment_theta = (2*M_PI)/size;
    std::vector<float> theta(size);
    for(int i = 0 ; i< size; ++i)
    {
        energy_range[i]+= LLD+i*increment_x;
        theta[i]+=i*increment_theta;
        out[i]+=0;
    }

    for(int j = 0; j < size; ++j)
    {
        out[j]+= detection_model_with_fitted_parameters(energy_range[j], theta[j], incoming_photon_energy);
    }

    return out;
}

float
ScatterSimulation::detection_efficiency_full(const float incoming_photon_energy, const int en_window) const
{

    const float HLD = this->template_exam_info_sptr->get_high_energy_thres(en_window);
    const float LLD = this->template_exam_info_sptr->get_low_energy_thres(en_window);
    float sum = 0;
    const int size = HLD-LLD;
    const float increment_x = (HLD - LLD)/size;
    const float increment_theta = (2*M_PI)/size;
    for(int i = 0 ; i< size; ++i)
    {
        const float energy_range = LLD+i*increment_x;
        const float theta=i*increment_theta;
        sum+= detection_model_with_fitted_parameters(energy_range, theta, incoming_photon_energy);
    }

    sum*=increment_x;
    return sum;
}

float
ScatterSimulation::
detection_model_with_fitted_parameters(const float x, const float theta, const float energy) const
{
  //! Brief
  //! All the parameters are obtained by fitting the model to the energy spectrum obtained with GATE.
  //! The crystal used here is LSO, the one for the Siemens mMR (atomic number Z = 66).
  //! We consider to have four terms: (i) gaussian model for the photopeak, (ii) compton plateau, (iii) flat continuum, (iv) exponential tale
  //! The model has be trained with 511 keV and tested with 370 keV.

  const int Z = 66; // atomic number of LSO
  const float H_1 = pow(Z,5)/energy; //the height of the photopeak is prop. to the photoelectric cross section
  const float H_2 = 9.27*pow(10,25)*total_Compton_cross_section(energy)*Z; // the eight of the compton plateau is proportional to the compton cross section
  const float H_3 = 7; //fitting parameter
  const float H_4 = 26.0; //fitting parameter
  const float beta = -0.817; //fitting parameter
  const float global_scale = 2.33*1e-07; //fitting parameter
  const float fwhm = this->proj_data_info_cyl_noarc_cor_sptr->get_scanner_ptr()->get_energy_resolution();
  const float std_peak = energy*fwhm/2.35482;
  const float scaling_std_compton = 28.8; //fitting parameter
  const float shift_compton = 0.6; //fitting parameter
  const float f1 = photoelectric(H_1, std_peak, x, energy);
  const float f2 = compton_plateau(H_2, std_peak, x, energy,theta, scaling_std_compton,shift_compton);
  const float f3 = flat_continuum(H_3,std_peak, x, energy);
  const float f4 = exponential_tail(H_4,std_peak, x, energy,beta);

  return global_scale*(f1+f2+f3+f4);
}

float
ScatterSimulation::
photoelectric(const float K, const float std_peak, const float x, const float energy) const
{
  const float diff = x - energy;
  const float pow_diff = pow(diff,2);
  const float pow_std_peak = pow(std_peak,2);
  return  K/(std_peak*sqrt(2*M_PI))*exp(-pow_diff/(2*pow_std_peak));
}

float
ScatterSimulation::
compton_plateau(const float K, const float std_peak, const float x, const float energy, const float theta, const float scaling_std_compton,const float shift_compton) const
{
    const float m_0_c_2 = 511.F;
    const float alpha = energy/m_0_c_2;
    const float E_1 = energy/(1+alpha*(1-cos(theta)));
    const float mean = energy*shift_compton;
    return ((energy/E_1)+(E_1/energy)-1+cos(theta))*(K*exp(-(pow((x - mean),2))/(4*scaling_std_compton*std_peak)));
}
float
ScatterSimulation::
flat_continuum(const float K, const float std_peak, const float x, const float energy) const
{
    const float den = sqrt(2)*std_peak;
    float f = 0;
        if (x<=energy)
            f = K* erfc((x-energy)/den);
        else
            f = 0;
     return f;
}

float
ScatterSimulation::
exponential_tail(const float K, const float std_peak, const float x, const float energy, const float beta) const
{
    const float den1 = sqrt(2)*M_PI*std_peak*beta;
    const float den2 = sqrt(2)*std_peak;
    const float den3 = 2*beta;
    float f;
    if (x > 100)
        f = K * exp((x-energy)/den1)*erfc((x-energy)/den2+1/den3);
    else
        f = 0;
    return f;
}

float
ScatterSimulation::
max_cos_angle(const float low, const float approx, const float resolution_at_511keV)
{
  return
    2.f - (8176.*log(2.))/(square(approx*resolution_at_511keV)*(511. + (16.*low*log(2.))/square(approx*resolution_at_511keV) -
                                                               sqrt(511.)*sqrt(511. + (32.*low*log(2.))/square(approx*resolution_at_511keV)))) ;
}


float
ScatterSimulation::
energy_lower_limit(const float low, const float approx, const float resolution_at_511keV)
{
  return
  low + (approx*resolution_at_511keV)*(approx*resolution_at_511keV)*(46.0761 - 2.03829*sqrt(22.1807*low/square(approx*resolution_at_511keV)+511.));
}

double
ScatterSimulation::
detection_efficiency_no_scatter(const unsigned det_num_A, 
                                const unsigned det_num_B) const
{
  // TODO: slightly dangerous to use a static here
  // it would give wrong results when the energy_thresholds are changed...
    static const float detector_efficiency_no_scatter = 1; //here we remove this, it will be multiplied by the unscattered counts

  const CartesianCoordinate3D<float>& detector_coord_A =
    detection_points_vector[det_num_A];
  const CartesianCoordinate3D<float>& detector_coord_B =
    detection_points_vector[det_num_B];
  const CartesianCoordinate3D<float> 
    detA_to_ring_center(0,-detector_coord_A[2],-detector_coord_A[3]);
  const CartesianCoordinate3D<float> 
    detB_to_ring_center(0,-detector_coord_B[2],-detector_coord_B[3]);
  const float rAB_squared=static_cast<float>(norm_squared(detector_coord_A-detector_coord_B));
  const float cos_incident_angle_A = static_cast<float>(
    cos_angle(detector_coord_B - detector_coord_A,
              detA_to_ring_center)) ;
  const float cos_incident_angle_B = static_cast<float>(
    cos_angle(detector_coord_A - detector_coord_B,
              detB_to_ring_center)) ;

  //0.75 is due to the volume of the pyramid approximation!
  return
    1./(  0.75/2./_PI *
    rAB_squared
    /pow(detector_efficiency_no_scatter,2.0)/
    (cos_incident_angle_A*
     cos_incident_angle_B));
}

END_NAMESPACE_STIR
