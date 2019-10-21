//
//
/*
  Copyright (C) 2004- 2009-11-03, Hammersmith Imanet
  Copyright (C) 2011-07-01 - 2011, Kris Thielemans
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
  \brief Implementation of stir::ScatterEstimationByBin::single_scatter_estimate_for_one_scatter_point

  \author Charalampos Tsoumpas
  \author Pablo Aguiar
  \author Kris Thielemans


*/
#include "stir/scatter/SingleScatterSimulation.h"
#include "stir/scatter/ScatterSimulation.h"
#ifndef NDEBUG
// currently necessary for assert below
#include "stir/VoxelsOnCartesianGrid.h"
#endif

#include "stir/round.h"
#include <math.h>
using namespace std;
START_NAMESPACE_STIR

static const float total_Compton_cross_section_511keV = 
ScatterSimulation::
  total_Compton_cross_section(511.F); 

float
SingleScatterSimulation::
 simulate_for_one_scatter_point(
          const std::size_t scatter_point_num, 
          const unsigned det_num_A, 
          const unsigned det_num_B)
{       




 // The code now supports more than one energy window: the low energy threshold has to correspond to lowest window.   
    
    int low = 0;

 // TODO: check that the selected windows don't overcome the max num windows
    
    if (this->template_exam_info_sptr->get_num_energy_windows()>1)

    {

        int first_window=this->template_exam_info_sptr->get_energy_window_pair().first-1;
        int second_window=this->template_exam_info_sptr->get_energy_window_pair().second-1;

        if(this->template_exam_info_sptr->get_low_energy_thres(first_window) <= this->template_exam_info_sptr->get_low_energy_thres(second_window) )

        {
             low = first_window;
        }
        else if(this->template_exam_info_sptr->get_low_energy_thres(first_window) >= this->template_exam_info_sptr->get_low_energy_thres(second_window) )

        {
             low = second_window;
        }
        //std::cerr << "low:= "<< low<< '\n';

    }



  static const float max_single_scatter_cos_angle=max_cos_angle(this->template_exam_info_sptr->get_low_energy_thres(low),
                                                                2.f,
                                                                this->proj_data_info_cyl_noarc_cor_sptr->get_scanner_ptr()->get_energy_resolution());

  //static const float min_energy=energy_lower_limit(lower_energy_threshold,2.,energy_resolution);

  const CartesianCoordinate3D<float>& scatter_point =
    this->scatt_points_vector[scatter_point_num].coord;
  const CartesianCoordinate3D<float>& detector_coord_A =
    this->detection_points_vector[det_num_A];
  const CartesianCoordinate3D<float>& detector_coord_B =
    this->detection_points_vector[det_num_B];
  // note: costheta is -cos_angle such that it is 1 for zero scatter angle
  const float costheta = static_cast<float>(
    -cos_angle(detector_coord_A - scatter_point,
               detector_coord_B - scatter_point));
  // note: costheta is identical for scatter to A or scatter to B
  // Hence, the Compton_cross_section and energy are identical for both cases as well.
  if(max_single_scatter_cos_angle>costheta)
   return 0;
  const float new_energy =
    photon_energy_after_Compton_scatter_511keV(costheta);




// The detection efficiency varies with respect to the energy window.
//The code can now compute the scatter for a combination of two windows X and Y
//Default: one window -> The code will combine the window with itself

std::vector<float>detection_efficiency_scattered;
std::vector<float>detection_efficiency_unscattered;


detection_efficiency_scattered.push_back(0);
detection_efficiency_unscattered.push_back(0);



//detection efficiency of each window for that energy
  for (int i = 0; i < this->template_exam_info_sptr->get_num_energy_windows(); ++i)
  {
      detection_efficiency_scattered[i] = detection_efficiency(new_energy,i);
      detection_efficiency_unscattered[i] = detection_efficiency(511.F,i);
  }


    int index0 = 0;
    int index1 = 0;

    if (this->template_exam_info_sptr->get_num_energy_windows()>1)
    {
        index0 = this->template_exam_info_sptr->get_energy_window_pair().first-1;
        index1 = this->template_exam_info_sptr->get_energy_window_pair().second-1;

    }


    float detection_probability_XY=detection_efficiency_scattered[index0]*detection_efficiency_unscattered[index1];
    float detection_probability_YX=detection_efficiency_scattered[index1]*detection_efficiency_unscattered[index0];

  if ((detection_probability_XY==0)&&(detection_probability_YX==0))
      return 0;


  const float emiss_to_detA =
    cached_integral_over_activity_image_between_scattpoint_det
    (static_cast<unsigned int> (scatter_point_num),
     det_num_A);                
  const float emiss_to_detB = 
    cached_integral_over_activity_image_between_scattpoint_det
    (static_cast<unsigned int> (scatter_point_num),
     det_num_B);
  if (emiss_to_detA==0 && emiss_to_detB==0)
    return 0;   
  const float atten_to_detA = 
    cached_exp_integral_over_attenuation_image_between_scattpoint_det
    (scatter_point_num, 
     det_num_A);
  const float atten_to_detB = 
    cached_exp_integral_over_attenuation_image_between_scattpoint_det
    (scatter_point_num, 
     det_num_B);

  const float dif_Compton_cross_section_value =
    dif_Compton_cross_section(costheta, 511.F); 
        
  const float rA_squared=static_cast<float>(norm_squared(scatter_point-detector_coord_A));
  const float rB_squared=static_cast<float>(norm_squared(scatter_point-detector_coord_B));

  const float scatter_point_mu=
    scatt_points_vector[scatter_point_num].mu_value;

  const CartesianCoordinate3D<float>
    detA_to_ring_center(0,-detector_coord_A[2],-detector_coord_A[3]);
  const CartesianCoordinate3D<float>
    detB_to_ring_center(0,-detector_coord_B[2],-detector_coord_B[3]);
  const float cos_incident_angle_AS = static_cast<float>(
    cos_angle(scatter_point - detector_coord_A,
              detA_to_ring_center)) ;

  const float cos_incident_angle_BS = static_cast<float>(
    cos_angle(scatter_point - detector_coord_B,
              detB_to_ring_center)) ;
  if (cos_incident_angle_AS*cos_incident_angle_BS<0)
      return 0;
#ifndef NDEBUG
  {  
    // check if mu-value ok
    // currently terribly shift needed as in sample_scatter_points (TODO)
    const VoxelsOnCartesianGrid<float>& image =
      dynamic_cast<const VoxelsOnCartesianGrid<float>&>(*this->density_image_for_scatter_points_sptr);
    const CartesianCoordinate3D<float> voxel_size = image.get_voxel_size();       
    const float z_to_middle =
    (image.get_max_index() + image.get_min_index())*voxel_size.z()/2.F;
    CartesianCoordinate3D<float> shifted=scatter_point;
    shifted.z() += z_to_middle;
    assert(scatter_point_mu==
	   (*this->density_image_for_scatter_points_sptr)[this->density_image_for_scatter_points_sptr->get_indices_closest_to_physical_coordinates(shifted)]);
  }
#endif


  //normalisation
  // we will divide by the solid angle factors for unscattered photons
  // (computed with the same detection model as used in the scatter code)
  // the energy dependency is left out

    const double common_factor = scatter_volume/total_Compton_cross_section_511keV;


  float scatter_ratio=0 ;


  scatter_ratio=
          (detection_probability_XY*emiss_to_detA*(1.F/rB_squared)*pow(atten_to_detB,total_Compton_cross_section_relative_to_511keV(new_energy)-1)
           +detection_probability_YX*emiss_to_detB*(1.F/rA_squared)*pow(atten_to_detA,total_Compton_cross_section_relative_to_511keV(new_energy)-1))
           *atten_to_detB
           *atten_to_detA
           *scatter_point_mu
           *cos_incident_angle_AS
           *cos_incident_angle_BS
           *dif_Compton_cross_section_value
           *common_factor;


  return scatter_ratio;


          
}

END_NAMESPACE_STIR
