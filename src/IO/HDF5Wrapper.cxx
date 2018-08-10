#include "stir/IO/HDF5Wrapper.h"

START_NAMESPACE_STIR

bool HDF5Wrapper::check_GE_signature(const std::string filename)
{
    H5::H5File file;
    file.openFile( filename, H5F_ACC_RDONLY );
    H5::StrType  vlst(0,37);  // 37 here is the length of the string (I got it from the text file generated by list2txt with the LIST000_decomp.BLF

    std::string read_str_scanner;
    std::string read_str_manufacturer;

    H5::DataSet dataset = file.openDataSet("/HeaderData/ExamData/scannerDesc");
    dataset.read(read_str_scanner,vlst);

    H5::DataSet dataset2= file.openDataSet("/HeaderData/ExamData/manufacturer");
    dataset2.read(read_str_manufacturer,vlst);

    if(read_str_scanner == "SIGNA PET/MR" &&
            read_str_manufacturer == "GE MEDICAL SYSTEMS")
        return true;

    return false;
}

HDF5Wrapper::HDF5Wrapper()
{

}

HDF5Wrapper::HDF5Wrapper(const std::string& filename)
{
    open(filename);

}

shared_ptr<Scanner>
HDF5Wrapper::get_scanner_sptr() const
{
    return this->scanner_sptr;
}

shared_ptr<ExamInfo>
HDF5Wrapper::get_exam_info_sptr() const
{
//    return this->exam_info_sptr;
}

H5::DataSet* HDF5Wrapper::get_listmode_data_ptr() const
{
    return dataset_list_sptr.get();
}

hsize_t HDF5Wrapper::get_listmode_size() const
{
    return m_list_size;
}

Succeeded
HDF5Wrapper::open(const std::string& filename)
{
    file.openFile( filename, H5F_ACC_RDONLY );

    initialise_exam_info();

    if(HDF5Wrapper::check_GE_signature(filename))
    {
        warning("CListModeDataGESigna: "
                "Probably this is GESigna, but couldn't find scan start time etc."
                "The scanner is initialised from library instread from HDF5 header.");
        is_signa = true;

        this->scanner_sptr.reset(new Scanner(Scanner::PETMR_Signa));

        return Succeeded::yes;
    }
    else
    {
        // Read from HDF5 header ...
        return initialise_scanner_from_HDF5();
    }
}

Succeeded HDF5Wrapper::initialise_scanner_from_HDF5()
{
    int num_transaxial_blocks_per_bucket = 0;
    int num_axial_blocks_per_bucket = 0;
    int axial_blocks_per_unit = 0;
    int radial_blocks_per_unit = 0;
    int axial_units_per_module = 0;
    int radial_units_per_module = 0;
    int axial_modules_per_system = 0;
    int radial_modules_per_system = 0;
    int max_num_non_arccorrected_bins = 0;
    int num_transaxial_crystals_per_block = 0;
    int num_axial_crystals_per_block = 0 ;
    float inner_ring_diameter = 0.0;
    float detector_axial_size = 0.0;
    float intrinsic_tilt = 0.0;

    int num_detector_layers = 1;
    float energy_resolution = -1.0f;
    float reference_energy = -1.0f;

    H5::DataSet str_inner_ring_diameter = file.openDataSet("/HeaderData/SystemGeometry/effectiveRingDiameter");
    H5::DataSet str_axial_blocks_per_module = file.openDataSet("/HeaderData/SystemGeometry/axialBlocksPerModule");
    H5::DataSet str_radial_blocks_per_module = file.openDataSet("/HeaderData/SystemGeometry/radialBlocksPerModule");
    H5::DataSet str_axial_blocks_per_unit = file.openDataSet("/HeaderData/SystemGeometry/axialBlocksPerUnit");
    H5::DataSet str_radial_blocks_per_unit = file.openDataSet("/HeaderData/SystemGeometry/radialBlocksPerUnit");
    H5::DataSet str_axial_units_per_module = file.openDataSet("/HeaderData/SystemGeometry/axialUnitsPerModule");
    H5::DataSet str_radial_units_per_module = file.openDataSet("/HeaderData/SystemGeometry/radialUnitsPerModule");
    H5::DataSet str_axial_modules_per_system = file.openDataSet("/HeaderData/SystemGeometry/axialModulesPerSystem");
    H5::DataSet str_radial_modules_per_system = file.openDataSet("/HeaderData/SystemGeometry/radialModulesPerSystem");
    //! \todo P.W: Find the crystal gaps and other info missing.
    H5::DataSet str_detector_axial_size = file.openDataSet("/HeaderData/SystemGeometry/detectorAxialSize");
    H5::DataSet str_intrinsic_tilt = file.openDataSet("/HeaderData/SystemGeometry/transaxial_crystal_0_offset");
    H5::DataSet str_max_number_of_non_arc_corrected_bins = file.openDataSet("/HeaderData/Sorter/dimension1Size");
    H5::DataSet str_axial_crystals_per_block = file.openDataSet("/HeaderData/SystemGeometry/axialCrystalsPerBlock");
    H5::DataSet str_radial_crystals_per_block = file.openDataSet("/HeaderData/SystemGeometry/radialCrystalsPerBlock");
    //! \todo Convert to numbers.

    str_radial_blocks_per_module.read(&num_transaxial_blocks_per_bucket, H5T_INTEGER);
    str_axial_blocks_per_module.read(&num_axial_blocks_per_bucket, H5T_INTEGER);
    str_axial_blocks_per_unit.read(&axial_blocks_per_unit, H5T_INTEGER);
    str_radial_blocks_per_unit.read(&radial_blocks_per_unit, H5T_INTEGER);
    str_axial_units_per_module.read(&axial_units_per_module, H5T_INTEGER);
    str_radial_units_per_module.read(&radial_units_per_module, H5T_INTEGER);
    str_axial_modules_per_system.read(&axial_modules_per_system, H5T_INTEGER);
    str_radial_modules_per_system.read(&radial_modules_per_system, H5T_INTEGER);
    str_inner_ring_diameter.read(&inner_ring_diameter, H5T_FLOAT);
    str_detector_axial_size.read(&detector_axial_size, H5T_FLOAT);
    str_intrinsic_tilt.read(&intrinsic_tilt, H5T_FLOAT);
    str_max_number_of_non_arc_corrected_bins.read(&max_num_non_arccorrected_bins, H5T_INTEGER);
    str_radial_crystals_per_block.read(&num_transaxial_crystals_per_block, H5T_INTEGER);
    str_axial_crystals_per_block.read(&num_axial_crystals_per_block, H5T_INTEGER);

    int num_rings  = num_axial_blocks_per_bucket*num_axial_crystals_per_block*axial_modules_per_system;
    int num_detectors_per_ring = num_transaxial_blocks_per_bucket*num_transaxial_crystals_per_block*radial_modules_per_system;
    int default_num_arccorrected_bins = max_num_non_arccorrected_bins;
    float inner_ring_radius = 0.5f*inner_ring_diameter;
    float average_depth_of_interaction = 0.5f; // Assuming this to be constant. Although this will change depending on scanner.
    float ring_spacing = detector_axial_size/num_rings;

    //! \todo : bin_size
    float bin_size = max_num_non_arccorrected_bins/inner_ring_radius;
    int num_axial_crystals_per_singles_unit =1;
    int num_transaxial_crystals_per_singles_unit =1;



//    scanner_sptr.reset(new Scanner(  ,
//    int num_rings,
//    int max_num_non_arccorrected_bins,
//    int num_detectors_per_ring,
//    float inner_ring_radius,
//    float average_depth_of_interaction,
//    float ring_spacing,
//    float bin_size, float intrinsic_tilt,
//    int num_axial_blocks_per_bucket, int num_transaxial_blocks_per_bucket,
//    int num_axial_crystals_per_block, int num_transaxial_crystals_per_block,
//    int num_axial_crystals_per_singles_unit,
//    int num_transaxial_crystals_per_singles_unit,
//    int num_detector_layers,
//    float energy_resolution = -1.0f,
//    float reference_energy = -1.0f));


    return Succeeded::yes;
}

Succeeded HDF5Wrapper::initialise_exam_info()
{
    this->exam_info_sptr.reset(new ExamInfo());

//    exam_info_sptr->set_high_energy_thres();
//    exam_info_sptr->set_low_energy_thres();

//    exam_info_sptr->set_time_frame_definitions();

    return Succeeded::yes;
}

Succeeded HDF5Wrapper::initialise_listmode_data(const std::string &path)
{
    if(path.size() == 0)
    {
        if(is_signa)
        {
            m_listmode_address = "/ListData/listData";
            //! \todo Get these numbers from the HDF5 file
            {
            m_size_of_record_signature = 6;
            m_max_size_of_record = 16;
            }
        }
        else
            return Succeeded::no;
    }
    else
        m_listmode_address = path;

    dataset_list_sptr.reset(new H5::DataSet(file.openDataSet(m_listmode_address)));

    m_dataspace = dataset_list_sptr->getSpace();
    int dataset_list_Ndims = m_dataspace.getSimpleExtentNdims();

    hsize_t dims_out[dataset_list_Ndims];
    m_dataspace.getSimpleExtentDims( dims_out, NULL);
    m_list_size=dims_out[0];
    const long long unsigned int tmp_size_of_record_signature = m_size_of_record_signature;
    m_memspace_ptr = new H5::DataSpace( dataset_list_Ndims,
                            &tmp_size_of_record_signature);


    return Succeeded::yes;
}


Succeeded HDF5Wrapper::get_next(std::streampos& current_offset, shared_ptr<char>& data_sptr)
{

    hsize_t pos = static_cast<hsize_t>(current_offset);
    m_dataspace.selectHyperslab( H5S_SELECT_SET, &m_size_of_record_signature, &pos );
    dataset_list_sptr->read( data_sptr.get(), H5::PredType::STD_U8LE, *m_memspace_ptr, m_dataspace );
    current_offset += static_cast<std::streampos>(m_size_of_record_signature);

    //  // TODO error checking
    return Succeeded::yes;
}

//stir::Array<2, float>* data_sptr;
//shared_ptr<char> HDF5Wrapper::get_next_viewgram()
//{

//}

//stir::Array<2, float>* data_sptr;
//shared_ptr<char> HDF5Wrapper::get_next_sinogram()
//{

//}

//stir::Array<3, float>* data_sptr;
//shared_ptr<char> HDF5Wrapper::get_next_segment_by_sinogram()
//{

//}

//stir::Array<3, float>* data_sptr;
//shared_ptr<char> HDF5Wrapper::get_next_segment_by_viewgram()
//{

//}

// initialise_singles
// Can be used for normalisation, too.
// Can be used for nonTOF sinograms.
//Succeeded HDF5Wrapper::initialise_list_2D_arrays(const std::string &path)
//{
//    if(path.size() == 0)
//    {
//        if(is_signa)
//        {
//            m_listmode_address = "/ListData/listData";
//        }
//        else
//            return Succeeded::no;
//    }
//    else
//        m_listmode_address = path;

//    dataset_list_sptr.reset(new H5::DataSet(file.openDataSet(m_listmode_address)));

//    H5::DataSpace dataspace = dataset_list_sptr->getSpace();
//    dataset_list_Ndims = dataspace.getSimpleExtentNdims() ;


//    return Succeeded::yes;
//}



END_NAMESPACE_STIR

