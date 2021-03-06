/*
    Copyright (C) 2016 University College London
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
  \brief Definition of class stir::SingleScatterLikelihoodAndGradient

  \author Ludovica Brusaferri
*/

#ifndef __stir_scatter_SingleScatterLikelihoodAndGradient_H__
#define __stir_scatter_SingleScatterLikelihoodAndGradient_H__

#include "stir/Succeeded.h"
#include "stir/scatter/ScatterSimulation.h"
#include "stir/scatter/SingleScatterSimulation.h"
#include "stir/RegisteredParsingObject.h"
#include "stir/zoom.h"

START_NAMESPACE_STIR

class SingleScatterLikelihoodAndGradient : public
        RegisteredParsingObject<
        SingleScatterLikelihoodAndGradient,
        SingleScatterSimulation,
        SingleScatterSimulation>
{
private:
    typedef RegisteredParsingObject<
    SingleScatterLikelihoodAndGradient,
    SingleScatterSimulation,
    SingleScatterSimulation> base_type;
public:



    //! Name which will be used when parsing a ScatterSimulation object
    static const char * const registered_name;

    //! Default constructor
    SingleScatterLikelihoodAndGradient();

    //! Constructor with initialisation from parameter file

    explicit
    SingleScatterLikelihoodAndGradient(const std::string& parameter_filename);

    virtual ~SingleScatterLikelihoodAndGradient();


    double L_G_function(const ProjData& data,VoxelsOnCartesianGrid<float>& gradient_image, const bool compute_gradient = true ,const bool isgradient_mu = true,const float rescale = 1.F);
    double L_G_function(const ProjData& data,const ProjData &add_sino,VoxelsOnCartesianGrid<float>& gradient_image,const bool compute_gradient = true ,const bool isgradient_mu = true,const float rescale = 1.F);
    ProjDataInMemory  likelihood_and_gradient_scatter(const ProjData &projdata,const ProjData& norm, const ProjData &add_projdata, VoxelsOnCartesianGrid<float>& gradient_image_HR, VoxelsOnCartesianGrid<float>& gradient_image_LR,const bool compute_gradient, const bool isgradient_mu);
    ProjDataInMemory get_jacobian(std::vector<VoxelsOnCartesianGrid<float> >  &gradient_image_array,const bool compute_gradient, const bool isgradient_mu);
    ProjDataInMemory get_ratio(const ProjData& projdata,const ProjData& norm,const ProjData &add_projdata, const ProjData &est_projdata, std::vector<float> &ratio_vector);
    protected:

    void
    line_contribution(VoxelsOnCartesianGrid<float>& gradient_image,const float scale,
                                  const CartesianCoordinate3D<float>& scatter_point,
                                  const CartesianCoordinate3D<float>& detector_coord,
        						  const float C);

    void
    line_contribution_act(VoxelsOnCartesianGrid<float>& gradient_image,
                                  const CartesianCoordinate3D<float>& scatter_point,
                                  const CartesianCoordinate3D<float>& detector_coord,
                                  const float C);

    void
    s_contribution(VoxelsOnCartesianGrid<float>& gradient_image,
        		const CartesianCoordinate3D<float>& scatter_point,
        						  const float D);
    float
    L_G_for_one_scatter_point(VoxelsOnCartesianGrid<float>& gradient,
             const std::size_t scatter_point_num,
             const unsigned det_num_A,
             const unsigned det_num_B,
             const bool compute_gradient,
             const bool isgradient_mu);

    double L_G_estimate(VoxelsOnCartesianGrid<float>& gradient_image_bin,const Bin bin, const bool compute_gradient, const bool isgradient_mu);
    double L_G_for_view_segment_number(const ProjData&data,const ProjData&add_sino,VoxelsOnCartesianGrid<float>& gradient_image,const ViewSegmentNumbers& vs_num, const float rescale, const bool compute_gradient,const bool isgradient_mu);
    double L_G_for_viewgram(const Viewgram<float>& viewgram,const Viewgram<float>& v_add, Viewgram<float>& v_est,VoxelsOnCartesianGrid<float>& gradient_image,const float rescale, const bool compute_gradient,const bool isgradient_mu);
    void get_jacobian_for_view_segment_number(std::vector<VoxelsOnCartesianGrid<float> > &gradient_image_array,ProjData &est_data,const ViewSegmentNumbers& vs_num, const bool compute_gradient,const bool isgradient_mu);
    void get_jacobian_for_viewgram(Viewgram<float>& v_est, std::vector<VoxelsOnCartesianGrid<float> >  &gradient_image_array,const bool compute_gradient, const bool isgradient_mu);

};

END_NAMESPACE_STIR

#endif
