/*=========================================================================

  Program: GDCM (Grassroots DICOM). A DICOM library
  Module:  $URL$

  Copyright (c) 2006-2008 Mathieu Malaterre
  All rights reserved.
  See Copyright.txt or http://gdcm.sourceforge.net/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMyGDCMPolyDataReader.h"
#include "vtkGDCMImageReader.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkFloatArray.h"
#include "vtkStringArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMedicalImageProperties.h"
#include "vtkObjectFactory.h"
#include "vtkPolyData.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkSmartPointer.h"

#include "vtkImageData.h"
#include "vtkMatrix4x4.h"
#include "vtkPointData.h"

#include "gdcmAttribute.h"
#include "gdcmIPPSorter.h"
#include "gdcmImage.h"
#include "gdcmImageReader.h"
#include "gdcmPhotometricInterpretation.h"
#include "gdcmReader.h"
#include "gdcmSequenceOfItems.h"
#include "gdcmSmartPointer.h"

vtkStandardNewMacro(vtkMyGDCMPolyDataReader)

//----------------------------------------------------------------------------
vtkMyGDCMPolyDataReader::vtkMyGDCMPolyDataReader()
{
	this->FileName = NULL;
	this->SetNumberOfInputPorts(0);
	this->MedicalImageProperties = vtkMedicalImageProperties::New();
}

//----------------------------------------------------------------------------
vtkMyGDCMPolyDataReader::~vtkMyGDCMPolyDataReader()
{
	this->SetFileName(0);
	this->MedicalImageProperties->Delete();
}

//----------------------------------------------------------------------------
// inline keyword is needed since GetStringValueFromTag was copy/paste from vtkGDCMImageReader
// FIXME: need to restructure code to avoid copy/paste
inline const char* GetStringValueFromTag(const gdcm::Tag& t, const gdcm::DataSet& ds)
{
	static std::string buffer;
	buffer = ""; // cleanup previous call

	if (ds.FindDataElement(t))
	{
		const gdcm::DataElement& de = ds.GetDataElement(t);
		const gdcm::ByteValue* bv = de.GetByteValue();
		if (bv) // Can be Type 2
		{
			buffer = std::string(bv->GetPointer(), bv->GetLength());
			// Will be padded with at least one \0
		}
	}

	// Since return is a const char* the very first \0 will be considered
	return buffer.c_str();
}

//----------------------------------------------------------------------------
void vtkMyGDCMPolyDataReader::FillMedicalImageInformation(const gdcm::Reader& reader)
{
	const gdcm::File& file = reader.GetFile();
	const gdcm::DataSet& ds = file.GetDataSet();

	// $ grep "vtkSetString\|DICOM" vtkMedicalImageProperties.h
	// For ex: DICOM (0010,0010) = DOE,JOHN
	this->MedicalImageProperties->SetPatientName(GetStringValueFromTag(gdcm::Tag(0x0010, 0x0010), ds));
	// For ex: DICOM (0010,0020) = 1933197
	this->MedicalImageProperties->SetPatientID(GetStringValueFromTag(gdcm::Tag(0x0010, 0x0020), ds));
	// For ex: DICOM (0010,1010) = 031Y
	this->MedicalImageProperties->SetPatientAge(GetStringValueFromTag(gdcm::Tag(0x0010, 0x1010), ds));
	// For ex: DICOM (0010,0040) = M
	this->MedicalImageProperties->SetPatientSex(GetStringValueFromTag(gdcm::Tag(0x0010, 0x0040), ds));
	// For ex: DICOM (0010,0030) = 19680427
	this->MedicalImageProperties->SetPatientBirthDate(GetStringValueFromTag(gdcm::Tag(0x0010, 0x0030), ds));
#if (VTK_MAJOR_VERSION == 5 && VTK_MINOR_VERSION > 0)
	// For ex: DICOM (0008,0020) = 20030617
	this->MedicalImageProperties->SetStudyDate(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0020), ds));
#endif
	// For ex: DICOM (0008,0022) = 20030617
	this->MedicalImageProperties->SetAcquisitionDate(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0022), ds));
#if (VTK_MAJOR_VERSION == 5 && VTK_MINOR_VERSION > 0)
	// For ex: DICOM (0008,0030) = 162552.0705 or 230012, or 0012
	this->MedicalImageProperties->SetStudyTime(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0030), ds));
#endif
	// For ex: DICOM (0008,0032) = 162552.0705 or 230012, or 0012
	this->MedicalImageProperties->SetAcquisitionTime(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0032), ds));
	// For ex: DICOM (0008,0023) = 20030617
	this->MedicalImageProperties->SetImageDate(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0023), ds));
	// For ex: DICOM (0008,0033) = 162552.0705 or 230012, or 0012
	this->MedicalImageProperties->SetImageTime(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0033), ds));
	// For ex: DICOM (0020,0013) = 1
	this->MedicalImageProperties->SetImageNumber(GetStringValueFromTag(gdcm::Tag(0x0020, 0x0013), ds));
	// For ex: DICOM (0020,0011) = 902
	this->MedicalImageProperties->SetSeriesNumber(GetStringValueFromTag(gdcm::Tag(0x0020, 0x0011), ds));
	// For ex: DICOM (0008,103e) = SCOUT
	this->MedicalImageProperties->SetSeriesDescription(GetStringValueFromTag(gdcm::Tag(0x0008, 0x103e), ds));
	// For ex: DICOM (0020,0010) = 37481
	this->MedicalImageProperties->SetStudyID(GetStringValueFromTag(gdcm::Tag(0x0020, 0x0010), ds));
	// For ex: DICOM (0008,1030) = BRAIN/C-SP/FACIAL
	this->MedicalImageProperties->SetStudyDescription(GetStringValueFromTag(gdcm::Tag(0x0008, 0x1030), ds));
	// For ex: DICOM (0008,0060)= CT
	this->MedicalImageProperties->SetModality(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0060), ds));
	// For ex: DICOM (0008,0070) = Siemens
	this->MedicalImageProperties->SetManufacturer(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0070), ds));
	// For ex: DICOM (0008,1090) = LightSpeed QX/i
	this->MedicalImageProperties->SetManufacturerModelName(GetStringValueFromTag(gdcm::Tag(0x0008, 0x1090), ds));
	// For ex: DICOM (0008,1010) = LSPD_OC8
	this->MedicalImageProperties->SetStationName(GetStringValueFromTag(gdcm::Tag(0x0008, 0x1010), ds));
	// For ex: DICOM (0008,0080) = FooCity Medical Center
	this->MedicalImageProperties->SetInstitutionName(GetStringValueFromTag(gdcm::Tag(0x0008, 0x0080), ds));
	// For ex: DICOM (0018,1210) = Bone
	this->MedicalImageProperties->SetConvolutionKernel(GetStringValueFromTag(gdcm::Tag(0x0018, 0x1210), ds));
	// For ex: DICOM (0018,0050) = 0.273438
	this->MedicalImageProperties->SetSliceThickness(GetStringValueFromTag(gdcm::Tag(0x0018, 0x0050), ds));
	// For ex: DICOM (0018,0060) = 120
	this->MedicalImageProperties->SetKVP(GetStringValueFromTag(gdcm::Tag(0x0018, 0x0060), ds));
	// For ex: DICOM (0018,1120) = 15
	this->MedicalImageProperties->SetGantryTilt(GetStringValueFromTag(gdcm::Tag(0x0018, 0x1120), ds));
	// For ex: DICOM (0018,0081) = 105
	this->MedicalImageProperties->SetEchoTime(GetStringValueFromTag(gdcm::Tag(0x0018, 0x0081), ds));
	// For ex: DICOM (0018,0091) = 35
	this->MedicalImageProperties->SetEchoTrainLength(GetStringValueFromTag(gdcm::Tag(0x0018, 0x0091), ds));
	// For ex: DICOM (0018,0080) = 2040
	this->MedicalImageProperties->SetRepetitionTime(GetStringValueFromTag(gdcm::Tag(0x0018, 0x0080), ds));
	// For ex: DICOM (0018,1150) = 5
	this->MedicalImageProperties->SetExposureTime(GetStringValueFromTag(gdcm::Tag(0x0018, 0x1150), ds));
	// For ex: DICOM (0018,1151) = 400
	this->MedicalImageProperties->SetXRayTubeCurrent(GetStringValueFromTag(gdcm::Tag(0x0018, 0x1151), ds));
	// For ex: DICOM (0018,1152) = 114
	this->MedicalImageProperties->SetExposure(GetStringValueFromTag(gdcm::Tag(0x0018, 0x1152), ds));

	// virtual void AddWindowLevelPreset(double w, double l);
	// (0028,1050) DS [   498\  498]                           #  12, 2 WindowCenter
	// (0028,1051) DS [  1063\ 1063]                           #  12, 2 WindowWidth
	gdcm::Tag twindowcenter(0x0028, 0x1050);
	gdcm::Tag twindowwidth(0x0028, 0x1051);
	if (ds.FindDataElement(twindowcenter) && ds.FindDataElement(twindowwidth))
	{
		const gdcm::DataElement& windowcenter = ds.GetDataElement(twindowcenter);
		const gdcm::DataElement& windowwidth = ds.GetDataElement(twindowwidth);
		const gdcm::ByteValue* bvwc = windowcenter.GetByteValue();
		const gdcm::ByteValue* bvww = windowwidth.GetByteValue();
		if (bvwc && bvww) // Can be Type 2
		{
			//gdcm::Attributes<0x0028,0x1050> at;
			gdcm::Element<gdcm::VR::DS, gdcm::VM::VM1_n> elwc;
			std::stringstream ss1;
			std::string swc = std::string(bvwc->GetPointer(), bvwc->GetLength());
			ss1.str(swc);
			gdcm::VR vr = gdcm::VR::DS;
			unsigned int vrsize = vr.GetSizeof();
			unsigned int count = gdcm::VM::GetNumberOfElementsFromArray(swc.c_str(), static_cast<unsigned>(swc.size()));
			elwc.SetLength(count * vrsize);
			elwc.Read(ss1);
			std::stringstream ss2;
			std::string sww = std::string(bvww->GetPointer(), bvww->GetLength());
			ss2.str(sww);
			gdcm::Element<gdcm::VR::DS, gdcm::VM::VM1_n> elww;
			elww.SetLength(count * vrsize);
			elww.Read(ss2);
			for (unsigned int i = 0; i < elwc.GetLength(); ++i)
			{
				this->MedicalImageProperties->AddWindowLevelPreset(elww.GetValue(i), elwc.GetValue(i));
			}
		}
	}
	gdcm::Tag twindowexplanation(0x0028, 0x1055);
	if (ds.FindDataElement(twindowexplanation))
	{
		const gdcm::DataElement& windowexplanation = ds.GetDataElement(twindowexplanation);
		const gdcm::ByteValue* bvwe = windowexplanation.GetByteValue();
		if (bvwe) // Can be Type 2
		{
			int n = this->MedicalImageProperties->GetNumberOfWindowLevelPresets();
			gdcm::Element<gdcm::VR::LO, gdcm::VM::VM1_n> elwe; // window explanation
			gdcm::VR vr = gdcm::VR::LO;
			std::stringstream ss;
			ss.str("");
			std::string swe = std::string(bvwe->GetPointer(), bvwe->GetLength());
			unsigned int count = gdcm::VM::GetNumberOfElementsFromArray(swe.c_str(), static_cast<unsigned>(swe.size()));

			// I found a case with only one W/L but two comments: WINDOW1\WINDOW2
			// SIEMENS-IncompletePixelData.dcm
			elwe.SetLength(/*count*/ n * vr.GetSizeof());
			ss.str(swe);
			elwe.Read(ss);
			for (int i = 0; i < n; ++i)
			{
				this->MedicalImageProperties->SetNthWindowLevelPresetComment(i, elwe.GetValue(i).c_str());
			}
		}
	}

#if 0
  // gdcmData/JDDICOM_Sample4.dcm 
  // -> (0008,0060) CS [DM  Digital microscopy]                 #  24, 1 Modality
  gdcm::MediaStorage ms1 = gdcm::MediaStorage::SecondaryCaptureImageStorage;
  ms1.GuessFromModality( this->MedicalImageProperties->GetModality(), this->FileDimensionality );
  gdcm::MediaStorage ms2;
  ms2.SetFromFile( reader.GetFile() );
  if( ms2 != ms1 && ms2 != gdcm::MediaStorage::SecondaryCaptureImageStorage )
    {
    vtkWarningMacro( "SHOULD NOT HAPPEN. Unrecognized Modality: " << this->MedicalImageProperties->GetModality() 
      << " Will be set instead to the known one: " << ms2.GetModality() )
    this->MedicalImageProperties->SetModality( ms2.GetModality() );
    }
#endif

	// Add more info:
}

//----------------------------------------------------------------------------
int vtkMyGDCMPolyDataReader::RequestData_RTStructureSetStorage(gdcm::Reader const& reader,
		vtkInformationVector* outputVector)
{
	// This is done in RequestInformation
	//  gdcm::MediaStorage ms;
	//  ms.SetFromFile( reader.GetFile() );
	//  //std::cout << ms << std::endl;
	//  if( ms != gdcm::MediaStorage::RTStructureSetStorage )
	//    {
	//    return 0;
	//    }

	const gdcm::DataSet& ds = reader.GetFile().GetDataSet();
	// (3006,0020) SQ (Sequence with explicit length #=4)      # 370, 1 StructureSetROISequence
	// (3006,0039) SQ (Sequence with explicit length #=4)      # 24216, 1 ROIContourSequence
	gdcm::Tag troicsq(0x3006, 0x0039);
	if (!ds.FindDataElement(troicsq))
	{
		return 0;
	}
	
	gdcm::Tag tssroisq(0x3006, 0x0020);
	if (!ds.FindDataElement(tssroisq))
	{
		return 0;
	}

	const gdcm::DataElement& roicsq = ds.GetDataElement(troicsq);
	//const gdcm::SequenceOfItems *sqi = roicsq.GetSequenceOfItems();
	gdcm::SmartPointer<gdcm::SequenceOfItems> sqi = roicsq.GetValueAsSQ();
	if (!sqi || !sqi->GetNumberOfItems())
	{
		return 0;
	}
	const gdcm::DataElement& ssroisq = ds.GetDataElement(tssroisq);
	//const gdcm::SequenceOfItems *ssqi = ssroisq.GetSequenceOfItems();
	gdcm::SmartPointer<gdcm::SequenceOfItems> ssqi = ssroisq.GetValueAsSQ();
	if (!ssqi || !ssqi->GetNumberOfItems())
	{
		return 0;
	}

	// For each Item in the DataSet create a vtkPolyData
	for (unsigned int pd = 0; pd < sqi->GetNumberOfItems(); ++pd)
	{
		// get the ouptut
		auto outInfo1 = outputVector->GetInformationObject(pd);
		auto output = vtkPolyData::SafeDownCast(outInfo1->Get(vtkDataObject::DATA_OBJECT()));

		const gdcm::Item& item = sqi->GetItem(pd + 1); // Item start at #1
		const gdcm::Item& sitem = ssqi->GetItem(pd + 1); // Item start at #1
		const gdcm::DataSet& snestedds = sitem.GetNestedDataSet();

		// (3006,0026) ?? (LO) [date]                                    # 4,1 ROI Name
		gdcm::Tag stcsq(0x3006, 0x0026);
		if (!snestedds.FindDataElement(stcsq))
		{
			return 0;
		}
		const gdcm::DataElement& sde = snestedds.GetDataElement(stcsq);
		const gdcm::DataSet& nestedds = item.GetNestedDataSet();

		//(3006,002a) IS [255\192\96]                              # 10,3 ROI Display Color
		gdcm::Tag troidc(0x3006, 0x002a);
		gdcm::Attribute<0x3006, 0x002a> color = {};
		if (nestedds.FindDataElement(troidc))
		{
			const gdcm::DataElement& decolor = nestedds.GetDataElement(troidc);
			color.SetFromDataElement(decolor);
		}

		//(3006,0040) SQ (Sequence with explicit length #=8)      # 4326, 1 ContourSequence
		gdcm::Tag tcsq(0x3006, 0x0040);
		if (!nestedds.FindDataElement(tcsq))
		{
			continue;
		}
		const gdcm::DataElement& csq = nestedds.GetDataElement(tcsq);

		gdcm::SmartPointer<gdcm::SequenceOfItems> sqi2 = csq.GetValueAsSQ();
		if (!sqi2)// || !sqi2->GetNumberOfItems())
		{
			continue;
		}
		unsigned int nitems = static_cast<unsigned>(sqi2->GetNumberOfItems());

		auto scalars = vtkSmartPointer<vtkFloatArray>::New();
		scalars->SetNumberOfComponents(3);
		// In VTK there is no API to specify the name of a vtkPolyData, you can only specify Name
		// for the scalars (pointdata or celldata), so let's do that...
		std::string s(sde.GetByteValue()->GetPointer(), sde.GetByteValue()->GetLength());
		scalars->SetName(s.c_str());
		auto newPts = vtkSmartPointer<vtkPoints>::New();
		auto polys = vtkSmartPointer<vtkCellArray>::New();
		for (unsigned int i = 0; i < nitems; ++i)
		{
			const gdcm::Item& item2 = sqi2->GetItem(i + 1); // Item start at #1
			const gdcm::DataSet& nestedds2 = item2.GetNestedDataSet();

			// (3006,0050) DS [43.57636\65.52504\-10.0\46.043102\62.564945\-10.0\49.126537\60.714... # 398,48 ContourData
			gdcm::Tag tcontourdata(0x3006, 0x0050);
			const gdcm::DataElement& contourdata = nestedds2.GetDataElement(tcontourdata);

			gdcm::Attribute<0x3006, 0x0050> at;
			at.SetFromDataElement(contourdata);

			const double* pts = at.GetValues();
			unsigned int npts = at.GetNumberOfValues() / 3;
			std::vector<vtkIdType> ptIds(npts);

			for (unsigned int i = 0; i < npts * 3; i += 3)
			{
				auto ptId = newPts->InsertNextPoint(pts[i + 0], pts[i + 1], pts[i + 2]);
				ptIds[i / 3] = ptId;
			}
			// Each Contour Data is in fact a Cell:
			auto cellId = polys->InsertNextCell(npts, ptIds.data());
			scalars->InsertTuple3(cellId, (double)color[0] / 255.0, (double)color[1] / 255.0, (double)color[2] / 255.0);
		}
		output->SetPoints(newPts);
		output->SetPolys(polys);
		output->GetCellData()->SetScalars(scalars);
	}

	return 1;
}

//----------------------------------------------------------------------------
int vtkMyGDCMPolyDataReader::RequestData_HemodynamicWaveformStorage(gdcm::Reader const& reader,
		vtkInformationVector* outputVector)
{
	const gdcm::DataSet& ds = reader.GetFile().GetDataSet();
	// (5400,0100) SQ (Sequence with undefined length #=1)     # u/l, 1 WaveformSequence
	gdcm::Tag twsq(0x5400, 0x0100);
	if (!ds.FindDataElement(twsq))
	{
		return 0;
	}
	const gdcm::DataElement& wsq = ds.GetDataElement(twsq);
	gdcm::SmartPointer<gdcm::SequenceOfItems> sqi = wsq.GetValueAsSQ();
	if (!sqi || !sqi->GetNumberOfItems())
	{
		return 0;
	}

	const gdcm::Item& item = sqi->GetItem(1); // Item start at #1
	const gdcm::DataSet& nestedds = item.GetNestedDataSet();

	// (5400,1004) US 16                                       #   2, 1 WaveformBitsAllocated
	gdcm::Tag twba(0x5400, 0x1004);
	if (!nestedds.FindDataElement(twba))
	{
		return 0;
	}
	const gdcm::DataElement& wba = nestedds.GetDataElement(twba);

	//  (5400,1006) CS [SS]                                     #   2, 1 WaveformSampleInterpretation
	// (5400,1010) OW 00ba\0030\ff76\ff8b\00a2\ffd3\ffae\ff50\0062\00c4\011e\00c2\00ba... # 57600, 1 WaveformData
	gdcm::Tag twd(0x5400, 0x1010);
	if (!nestedds.FindDataElement(twd))
	{
		return 0;
	}
	const gdcm::DataElement& wd = nestedds.GetDataElement(twd);
	const gdcm::ByteValue* bv = wd.GetByteValue();
	size_t len = bv->GetLength();
	int16_t* p = (int16_t*)bv;

	// get the ouptut
	int pd = 0;
	auto outInfo1 = outputVector->GetInformationObject(pd);
	auto output = vtkPolyData::SafeDownCast(outInfo1->Get(vtkDataObject::DATA_OBJECT()));

	auto newPts = vtkSmartPointer<vtkPoints>::New();
	size_t npts = len / 2;
	for (size_t i = 0; i < npts; ++i)
	{
		float x[3];
		x[0] = (float)p[i] / 8800; // ?
		x[1] = (float)i;
		x[2] = 0;
		vtkIdType ptId = newPts->InsertNextPoint(x);
	}
	output->SetPoints(newPts);

	auto lines = vtkSmartPointer<vtkCellArray>::New();
	for (int i = 0; i < newPts->GetNumberOfPoints() - 1; ++i)
	{
		vtkIdType topol[2];
		topol[0] = i;
		topol[1] = i + 1;
		lines->InsertNextCell(2, topol);
	}

	output->SetLines(lines);
	output->BuildCells();

	return 1;
}

//----------------------------------------------------------------------------
int vtkMyGDCMPolyDataReader::RequestData(
	vtkInformation* vtkNotUsed(request),
	vtkInformationVector** vtkNotUsed(inputVector),
	vtkInformationVector* outputVector)
{
	auto outInfo = outputVector->GetInformationObject(0);
	if (outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER()) > 0)
	{
		return 0;
	}

	if (!this->FileName || !*this->FileName)
	{
		vtkErrorMacro(<< "A FileName must be specified.");
		return 0;
	}

	gdcm::Reader reader;
	reader.SetFileName(this->FileName);
	if (!reader.Read())
	{
		return 0;
	}

	gdcm::MediaStorage ms;
	ms.SetFromFile(reader.GetFile());

	int ret;
	if (ms == gdcm::MediaStorage::RTStructureSetStorage)
	{
		ret = this->RequestData_RTStructureSetStorage(reader, outputVector);
	}
	else if (ms == gdcm::MediaStorage::HemodynamicWaveformStorage)
	{
		ret = this->RequestData_HemodynamicWaveformStorage(reader, outputVector);
	}
	else
	{
		// not handled assume error
		ret = 0;
	}

	return ret;
}

//----------------------------------------------------------------------------
int vtkMyGDCMPolyDataReader::RequestInformation_RTStructureSetStorage(gdcm::Reader const& reader)
{
	const gdcm::DataSet& ds = reader.GetFile().GetDataSet();
	// (3006,0020) SQ (Sequence with explicit length #=4)      # 370, 1 StructureSetROISequence
	gdcm::Tag tssroisq(0x3006, 0x0020);
	if (!ds.FindDataElement(tssroisq))
	{
		return 0;
	}

	const gdcm::DataElement& ssroisq = ds.GetDataElement(tssroisq);
	//const gdcm::SequenceOfItems *sqi = ssroisq.GetSequenceOfItems();
	gdcm::SmartPointer<gdcm::SequenceOfItems> sqi = ssroisq.GetValueAsSQ();
	if (!sqi || !sqi->GetNumberOfItems())
	{
		return 0;
	}
	unsigned int npds = static_cast<unsigned>(sqi->GetNumberOfItems());

	//std::cout << "Nb pd:" << npds << std::endl;
	this->SetNumberOfOutputPorts(npds);

	// Allocate
	for (unsigned int i = 1; i < npds; ++i) // first output is allocated for us
	{
		auto output2 = vtkSmartPointer<vtkPolyData>::New();
		this->GetExecutive()->SetOutputData(i, output2);
	}
	return 1;
}

//----------------------------------------------------------------------------
int vtkMyGDCMPolyDataReader::RequestInformation_HemodynamicWaveformStorage(gdcm::Reader const& reader)
{
	return 1;
}

//----------------------------------------------------------------------------
int vtkMyGDCMPolyDataReader::RequestInformation(
	vtkInformation* vtkNotUsed(request),
	vtkInformationVector** vtkNotUsed(inputVector),
	vtkInformationVector* outputVector)
{
	gdcm::Reader reader;
	reader.SetFileName(this->FileName);
	if (!reader.Read())
	{
		return 0;
	}

	gdcm::MediaStorage ms;
	ms.SetFromFile(reader.GetFile());

	int ret;
	if (ms == gdcm::MediaStorage::RTStructureSetStorage)
	{
		ret = this->RequestInformation_RTStructureSetStorage(reader);
	}
	else if (ms == gdcm::MediaStorage::HemodynamicWaveformStorage)
	{
		ret = this->RequestInformation_HemodynamicWaveformStorage(reader);
	}
	else
	{
		// not handled assume error
		ret = 0;
	}

	if (ret)
	{
		// Ok let's fill in the 'extra' info:
		this->FillMedicalImageInformation(reader);
	}

	return ret;
}

//----------------------------------------------------------------------------
void vtkMyGDCMPolyDataReader::PrintSelf(ostream& os, vtkIndent indent)
{
	this->Superclass::PrintSelf(os, indent);

	os << indent << "File Name: "
	   << (this->FileName ? this->FileName : "(none)") << "\n";
}

//----------------------------------------------------------------------------
int gdcmvtk_rtstruct::RequestData_RTStructureSetStorage(const char* filename, tissuevec& tissues)
{
	gdcm::Reader reader;
	reader.SetFileName(filename);
	if (!reader.Read())
	{
		return 0;
	}

	gdcm::MediaStorage ms;
	ms.SetFromFile(reader.GetFile());
	if (ms != gdcm::MediaStorage::RTStructureSetStorage)
	{
		return 0;
	}

	const gdcm::DataSet& ds = reader.GetFile().GetDataSet();
	// (3006,0020) SQ (Sequence with explicit length #=4)      # 370, 1 StructureSetROISequence
	// (3006,0039) SQ (Sequence with explicit length #=4)      # 24216, 1 ROIContourSequence
	gdcm::Tag troicsq(0x3006, 0x0039);
	if (!ds.FindDataElement(troicsq))
	{
		return 0;
	}
	gdcm::Tag tssroisq(0x3006, 0x0020);
	if (!ds.FindDataElement(tssroisq))
	{
		return 0;
	}

	const gdcm::DataElement& roicsq = ds.GetDataElement(troicsq);
	//const gdcm::SequenceOfItems *sqi = roicsq.GetSequenceOfItems();
	gdcm::SmartPointer<gdcm::SequenceOfItems> sqi = roicsq.GetValueAsSQ();
	if (!sqi || !sqi->GetNumberOfItems())
	{
		return 0;
	}
	const gdcm::DataElement& ssroisq = ds.GetDataElement(tssroisq);
	//const gdcm::SequenceOfItems *ssqi = ssroisq.GetSequenceOfItems();
	gdcm::SmartPointer<gdcm::SequenceOfItems> ssqi = ssroisq.GetValueAsSQ();
	if (!ssqi || !ssqi->GetNumberOfItems())
	{
		return 0;
	}

	// For each Item in the DataSet create a vtkPolyData
	for (unsigned int pd = 0; pd < sqi->GetNumberOfItems(); ++pd)
	{
		const gdcm::Item& item = sqi->GetItem(pd + 1); // Item start at #1
		const gdcm::Item& sitem = ssqi->GetItem(pd + 1); // Item start at #1

		const gdcm::DataSet& snestedds = sitem.GetNestedDataSet();
		// (3006,0026) ?? (LO) [date]                                    # 4,1 ROI Name
		gdcm::Tag stcsq(0x3006, 0x0026);
		if (!snestedds.FindDataElement(stcsq))
		{
			continue;
		}

		gdcm::SmartPointer<gdcm::SequenceOfItems> sqi2;
		const gdcm::DataSet& nestedds = item.GetNestedDataSet();
		//(3006,0040) SQ (Sequence with explicit length #=8)      # 4326, 1 ContourSequence
		gdcm::Tag tcsq(0x3006, 0x0040);
		if (!nestedds.FindDataElement(tcsq))
		{
			sqi2 = 0;
		}
		else
		{
			const gdcm::DataElement& csq = nestedds.GetDataElement(tcsq);
			sqi2 = csq.GetValueAsSQ();
		}

		auto tissue = new tissuestruct;
		tissues.push_back(tissue);

		//(3006,002a) IS [255\192\96]                              # 10,3 ROI Display Color
		gdcm::Tag troidc(0x3006, 0x002a);
		gdcm::Attribute<0x3006, 0x002a> color = {};
		if (nestedds.FindDataElement(troidc))
		{
			const gdcm::DataElement& decolor = nestedds.GetDataElement(troidc);
			color.SetFromDataElement(decolor);
			tissue->color[0] = (float)color[0] / 255.0f;
			tissue->color[1] = (float)color[1] / 255.0f;
			tissue->color[2] = (float)color[2] / 255.0f;
		}
		else
		{
			tissue->color[0] = 1.0f;
			tissue->color[1] = 0.0f;
			tissue->color[2] = 0.0f;
		}

		unsigned int nitems = (sqi2==0) ? 0 : static_cast<unsigned>(sqi2->GetNumberOfItems());

		const gdcm::DataElement& sde = snestedds.GetDataElement(stcsq);
		std::string s(sde.GetByteValue()->GetPointer(), sde.GetByteValue()->GetLength());
		// In VTK there is no API to specify the name of a vtkPolyData, you can only specify Name
		// for the scalars (pointdata or celldata), so let's do that...

		//Check if the tissues has a contour sequence. If it doesn't, add the " (empty)" suffix.
		if (nitems == 0) s += " (empty)";
		tissue->name = s;

		for (unsigned int i = 0; i < nitems; ++i)
		{
			const gdcm::Item& item2 = sqi2->GetItem(i + 1); // Item start at #1

			const gdcm::DataSet& nestedds2 = item2.GetNestedDataSet();
			// (3006,0050) DS [43.57636\65.52504\-10.0\46.043102\62.564945\-10.0\49.126537\60.714... # 398,48 ContourData
			gdcm::Tag tcontourdata(0x3006, 0x0050);
			const gdcm::DataElement& contourdata = nestedds2.GetDataElement(tcontourdata);

			gdcm::Attribute<0x3006, 0x0050> at;
			at.SetFromDataElement(contourdata);

			const double* pts = at.GetValues();
			unsigned int npts = at.GetNumberOfValues() / 3;
			tissue->outlinelength.push_back(npts);
			size_t cur_length = tissue->points.size();
			tissue->points.resize(cur_length + 3 * npts);
			for (unsigned int i = 0; i < npts * 3; i++, cur_length++)
			{
				tissue->points[cur_length] = static_cast<float>(pts[i]);
			}
		}
	}

	//Sort the list of tissues giving to the smallest tissue the highest priority and to the biggest tissue the lowest
	tissuevec tissuesAux(tissues);
	tissuesAux.clear();

	for (int i = 0; i < tissuesAux.size(); i++)
	{
		int pos = 0;
		for (int j = 0; j < tissues.size(); j++)
		{
			if (tissues[j]->points.size() > tissuesAux[i]->points.size())
				pos++;
		}

		if (pos == tissues.size())
			tissues.push_back(tissuesAux[i]);
		else
			tissues.insert(tissues.begin() + pos, tissuesAux[i]);
	}
	tissuesAux.clear();

	return 1;
}

template<typename T>
void _CopyArray(const T* data, float* bits, int xm, int xp, int ym, int yp)
{
	size_t pos = 0;
	for (int j = ym; j <= yp; j++)
	{
		for (int i = xm; i <= xp; i++, pos++)
		{
			bits[pos] = static_cast<float>(data[pos]);
		}
	}
}

//----------------------------------------------------------------------------
bool gdcmvtk_rtstruct::GetDicomUsingGDCM(const char* filename, float* bits, unsigned short& w, unsigned short& h)
{
	auto reader = vtkSmartPointer<vtkGDCMImageReader>::New();
	if (reader->CanReadFile(filename) == 0)
	{
		return false;
	}
	reader->SetFileName(filename);
	reader->Update();
	int xm, xp, ym, yp, zm, zp;
	reader->GetDataExtent(xm, xp, ym, yp, zm, zp);
	h = yp - ym + 1;
	w = xp - xm + 1;

	vtkImageData* img = reader->GetOutput();

	std::string className = img->GetPointData()->GetScalars()->GetClassName();

	unsigned long long pos = 0;
	if (className == "vtkUnsignedCharArray")
	{
		unsigned char* ptr = (unsigned char*)img->GetScalarPointer(xm, ym, zm);
		_CopyArray(ptr, bits, xm, xp, ym, xp);
	}
	else if (className == "vtkFloatArray")
	{
		float* ptr = (float*)img->GetScalarPointer(xm, ym, zm);
		_CopyArray(ptr, bits, xm, xp, ym, xp);
	}
	else if (className == "vtkDoubleArray")
	{
		double* ptr = (double*)img->GetScalarPointer(xm, ym, zm);
		_CopyArray(ptr, bits, xm, xp, ym, xp);
	}
	else if (className == "vtkShortArray")
	{
		short* ptr = (short*)img->GetScalarPointer(xm, ym, zm);
		_CopyArray(ptr, bits, xm, xp, ym, xp);
	}
	else
	{
		for (int j = ym; j <= yp; j++)
		{
			for (int i = xm; i <= xp; i++, pos++)
			{
				bits[pos] = reader->GetOutput()->GetScalarComponentAsFloat(i, j, zm, 0);
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------
bool gdcmvtk_rtstruct::GetDicomUsingGDCM(const char* filename, float* bits, unsigned short& w, unsigned short& h, unsigned short& nrslices)
{
	auto reader = vtkSmartPointer<vtkGDCMImageReader>::New();
	if (reader->CanReadFile(filename) == 0)
	{
		return false;
	}
	reader->SetFileName(filename);
	reader->Update();
	int xm, xp, ym, yp, zm, zp;
	reader->GetDataExtent(xm, xp, ym, yp, zm, zp);
	h = yp - ym + 1;
	w = xp - xm + 1;
	nrslices = zp - zm + 1;

	vtkImageData* img = reader->GetOutput();

	std::string className = img->GetPointData()->GetScalars()->GetClassName();

	unsigned long long pos = 0;
	if (className == "vtkUnsignedCharArray")
	{
		for (int k = zm; k <= zp; k++)
		{
			unsigned char* ptr = (unsigned char*)img->GetScalarPointer(xm, ym, k);
			unsigned long long pos1 = 0;
			for (int j = ym; j <= yp; j++)
			{
				for (int i = xm; i <= xp; i++, pos++, pos1++)
				{
					bits[pos] = (float)ptr[pos1];
				}
			}
		}
	}
	else if (className == "vtkFloatArray")
	{
		for (int k = zm; k <= zp; k++)
		{
			float* ptr = (float*)img->GetScalarPointer(xm, ym, k);
			unsigned long long pos1 = 0;
			for (int j = ym; j <= yp; j++)
			{
				for (int i = xm; i <= xp; i++, pos++, pos1++)
				{
					bits[pos] = ptr[pos1];
				}
			}
		}
	}
	else if (className == "vtkDoubleArray")
	{
		for (int k = zm; k <= zp; k++)
		{
			double* ptr = (double*)img->GetScalarPointer(xm, ym, k);
			unsigned long long pos1 = 0;
			for (int j = ym; j <= yp; j++)
			{
				for (int i = xm; i <= xp; i++, pos++, pos1++)
				{
					bits[pos] = (float)ptr[pos1];
				}
			}
		}
	}
	else if (className == "vtkShortArray")
	{
		for (int k = zm; k <= zp; k++)
		{
			short* ptr = (short*)img->GetScalarPointer(xm, ym, k);
			unsigned long long pos1 = 0;
			for (int j = ym; j <= yp; j++)
			{
				for (int i = xm; i <= xp; i++, pos++, pos1++)
				{
					bits[pos] = (float)ptr[pos1];
				}
			}
		}
	}
	else
	{
		for (int k = zm; k <= zp; k++)
		{
			for (int j = ym; j <= yp; j++)
			{
				for (int i = xm; i <= xp; i++, pos++)
				{
					bits[pos] = reader->GetOutput()->GetScalarComponentAsFloat(i, j, k, 0);
				}
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------
bool gdcmvtk_rtstruct::GetSizeUsingGDCM(const char* filename, unsigned short& w, unsigned short& h, unsigned short& nrslices, float& dx, float& dy, float& dz, float* disp, float* dc)
{
	float c1[3], c2[3], c3[3];
	if (GetSizeUsingGDCM({std::string(filename)}, w, h, nrslices, dx, dy, dz, disp, c1, c2, c3))
	{
		for (unsigned short i = 0; i < 3; ++i)
		{
			dc[i] = c1[i];
			dc[i + 3] = c2[i];
		}
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------
bool gdcmvtk_rtstruct::GetSizeUsingGDCM(const std::vector<std::string>& filenames, unsigned short& w, unsigned short& h, unsigned short& nrslices, float& dx, float& dy, float& dz, float* disp, float* c1, float* c2, float* c3)
{
	auto reader = vtkSmartPointer<vtkGDCMImageReader>::New();
	if (reader->CanReadFile(filenames[0].c_str()) == 0)
	{
		return false;
	}
	if (filenames.size() == 1)
	{
		reader->SetFileName(filenames[0].c_str());
	}
	else 
	{
		auto files = vtkSmartPointer<vtkStringArray>::New();
		for (const auto& f : filenames)
		{
			files->InsertNextValue(f);
			std::cerr << f << "\n";
		}
		reader->SetFileNames(files);
	}
	int xm, xp, ym, yp, zm, zp;
	reader->Update();
	reader->GetDataExtent(xm, xp, ym, yp, zm, zp);
	h = yp - ym + 1;
	w = xp - xm + 1;
	nrslices = zp - zm + 1;
	double a[3];
	reader->GetOutput()->GetSpacing(a);//GetDataSpacing(a);
	dx = a[0];
	dy = a[1];
	dz = a[2];
	double b[3];
	reader->GetDataOrigin(b);
	//std::cerr << "Dicom origin: " << b[0] << ", " << b[1] << ", " << b[2] << "\n";
	reader->GetImagePositionPatient(b);
	//std::cerr << "Dicom patient position: " << b[0] << ", " << b[1] << ", " << b[2] << "\n";
	disp[0] = b[0];
	disp[1] = b[1];
	disp[2] = b[2];
	vtkMatrix4x4* mat = reader->GetDirectionCosines();
	for (unsigned short i = 0; i < 3; ++i)
	{
		c1[i] = mat->GetElement(i, 0);
		c2[i] = mat->GetElement(i, 1);
		c3[i] = mat->GetElement(i, 2);
	}

	return true;
}

//----------------------------------------------------------------------------
double gdcmvtk_rtstruct::GetZSPacing(const std::vector<std::string>& files)
{
	std::vector<std::string> filenames(files.size());
	std::copy(files.cbegin(), files.cend(), filenames.begin());

	gdcm::IPPSorter s;
	s.SetComputeZSpacing(true);
	s.SetZSpacingTolerance(1e-3);
	s.Sort(filenames);

	return s.GetZSpacing();
}

//----------------------------------------------------------------------------
gdcmvtk_rtstruct::tissuevec::~tissuevec()
{
	for (auto it = begin(); it != end(); it++)
		delete *it;
}
