/*
 * Copyright (c) 2018 The Foundation for Research on Information Technologies in Society (IT'IS).
 * 
 * This file is part of iSEG
 * (see https://github.com/ITISFoundation/osparc-iseg).
 * 
 * This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 */
#include "Precompiled.h"

#include "SlicesHandler.h"
#include "SmoothingWidget.h"
#include "bmp_read_1.h"

#include "../Data/SlicesHandlerITKInterface.h"

#include <itkBinaryThresholdImageFilter.h>
#include <itkMeanImageFilter.h>
#include <itkSliceBySliceImageFilter.h>

#include <q3vbox.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qdialog.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qsize.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qwidget.h>

#include <algorithm>

namespace iseg {

SmoothingWidget::SmoothingWidget(SlicesHandler* hand3D, QWidget* parent,
		const char* name, Qt::WindowFlags wFlags)
		: WidgetInterface(parent, name, wFlags), handler3D(hand3D)
{
	setToolTip(Format("Smoothing and noise removal filters."));

	activeslice = handler3D->active_slice();
	bmphand = handler3D->get_activebmphandler();

	hboxoverall = new Q3HBox(this);
	hboxoverall->setMargin(8);
	vboxmethods = new Q3VBox(hboxoverall);
	vbox1 = new Q3VBox(hboxoverall);
	hbox1 = new Q3HBox(vbox1);
	hbox2 = new Q3HBox(vbox1);
	vbox2 = new Q3VBox(vbox1);
	hbox3 = new Q3HBox(vbox2);
	hbox5 = new Q3HBox(vbox2);
	hbox4 = new Q3HBox(vbox1);
	allslices = new QCheckBox(QString("Apply to all slices"), vbox1);
	target = new QCheckBox(QString("Target"), vbox1);
	pushexec = new QPushButton("Execute", vbox1);
	contdiff = new QPushButton("Cont. Diffusion", vbox1);

	txt_n = new QLabel("n: ", hbox1);
	sb_n = new QSpinBox(1, 11, 2, hbox1);
	sb_n->setValue(5);
	sb_n->setToolTip("'n' is the width of the kernel in pixels.");

	txt_sigma1 = new QLabel("Sigma: 0 ", hbox2);
	sl_sigma = new QSlider(Qt::Horizontal, hbox2);
	sl_sigma->setRange(1, 100);
	sl_sigma->setValue(20);
	sl_sigma->setToolTip(
			"Sigma gives the radius of the smoothing filter. Larger "
			"values remove more details.");
	txt_sigma2 = new QLabel(" 5", hbox2);

	txt_dt = new QLabel("dt: 1    ", hbox3);
	txt_iter = new QLabel("Iterations: ", hbox3);
	sb_iter = new QSpinBox(1, 100, 1, hbox3);
	sb_iter->setValue(20);

	txt_k = new QLabel("Sigma: 0 ", hbox4);
	sl_k = new QSlider(Qt::Horizontal, hbox4);
	sl_k->setRange(0, 100);
	sl_k->setValue(50);
	sl_k->setToolTip(
			"Together with value on the right, defines the Sigma of the "
			"smoothing filter.");
	sb_kmax = new QSpinBox(1, 100, 1, hbox4);
	sb_kmax->setValue(10);
	sb_kmax->setToolTip("Gives the max. radius of the smoothing filter. This "
											"value defines the scale used in the slider bar.");

	txt_restrain1 = new QLabel("Restraint: 0 ", hbox5);
	sl_restrain = new QSlider(Qt::Horizontal, hbox5);
	sl_restrain->setRange(0, 100);
	sl_restrain->setValue(0);
	txt_restrain2 = new QLabel(" 1", hbox5);

	rb_gaussian = new QRadioButton(QString("Gaussian"), vboxmethods);
	rb_gaussian->setToolTip("Gaussian smoothing blurs the image and can remove "
													"noise and details (high frequency).");
	rb_average = new QRadioButton(QString("Average"), vboxmethods);
	rb_average->setToolTip(
			"Mean smoothing blurs the image and can remove noise and details.");
	rb_median = new QRadioButton(QString("Median"), vboxmethods);
	rb_median->setToolTip(
			"Median filtering can remove (speckle or salt-and-pepper) noise.");
	rb_sigmafilter = new QRadioButton(QString("Sigma Filter"), vboxmethods);
	rb_sigmafilter->setToolTip(
			"Sigma filtering is a mixture between Gaussian and Average filtering. "
			"It "
			"preserves edges better than Average filtering.");
	rb_anisodiff =
			new QRadioButton(QString("Anisotropic Diffusion"), vboxmethods);
	rb_anisodiff->setToolTip("Anisotropic diffusion can remove noise, while "
													 "preserving significant details, such as edges.");

	modegroup = new QButtonGroup(this);
	modegroup->insert(rb_gaussian);
	modegroup->insert(rb_average);
	modegroup->insert(rb_median);
	modegroup->insert(rb_sigmafilter);
	modegroup->insert(rb_anisodiff);
	rb_gaussian->setChecked(TRUE);

	sl_restrain->setFixedWidth(300);
	sl_k->setFixedWidth(300);
	sl_sigma->setFixedWidth(300);

	vboxmethods->setMargin(5);
	vbox1->setMargin(5);
	vboxmethods->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
	vboxmethods->setLineWidth(1);

	hbox1->setFixedSize(hbox1->sizeHint());
	hbox2->setFixedSize(hbox2->sizeHint());
	hbox3->setFixedSize(hbox3->sizeHint());
	hbox4->setFixedSize(hbox4->sizeHint());
	hbox5->setFixedSize(hbox5->sizeHint());
	vbox2->setFixedSize(vbox2->sizeHint());
	vbox1->setFixedSize(vbox1->sizeHint());

	method_changed(0);

	connect(modegroup, SIGNAL(buttonClicked(int)), this, SLOT(method_changed(int)));
	connect(target, SIGNAL(clicked()), this, SLOT(input_changed()));
	connect(pushexec, SIGNAL(clicked()), this, SLOT(execute()));
	connect(contdiff, SIGNAL(clicked()), this, SLOT(continue_diff()));
	connect(sl_sigma, SIGNAL(valueChanged(int)), this, SLOT(sigmaslider_changed(int)));
	connect(sl_sigma, SIGNAL(sliderPressed()), this, SLOT(slider_pressed()));
	connect(sl_sigma, SIGNAL(sliderReleased()), this, SLOT(slider_released()));
	connect(sl_k, SIGNAL(valueChanged(int)), this, SLOT(kslider_changed(int)));
	connect(sl_k, SIGNAL(sliderPressed()), this, SLOT(slider_pressed()));
	connect(sl_k, SIGNAL(sliderReleased()), this, SLOT(slider_released()));
	connect(sb_n, SIGNAL(valueChanged(int)), this, SLOT(n_changed(int)));
	connect(sb_kmax, SIGNAL(valueChanged(int)), this, SLOT(kmax_changed(int)));
}

SmoothingWidget::~SmoothingWidget()
{

}

void SmoothingWidget::execute()
{
	iseg::DataSelection dataSelection;
	dataSelection.allSlices = allslices->isChecked();
	dataSelection.sliceNr = handler3D->active_slice();
	dataSelection.work = true;
	emit begin_datachange(dataSelection, this);

	if (target->isCheckable())
	{
		using slice_image = itk::Image<float, 2>;
		using threshold_filter = itk::BinaryThresholdImageFilter<slice_image, slice_image>;
		using mean_filter = itk::MeanImageFilter<slice_image, slice_image>;

		auto threshold = threshold_filter::New();
		threshold->SetLowerThreshold(0.001f); // intensity threshold
		threshold->SetInsideValue(1);
		threshold->SetOutsideValue(0);

		slice_image::SizeType radius;
		radius[0] = 1; // radius along x
		radius[1] = 1; // radius along y

		auto mean = mean_filter::New();
		mean->SetInput(threshold->GetOutput());
		mean->SetRadius(radius);

		auto threshold2 = threshold_filter::New();
		threshold2->SetInput(mean->GetOutput());
		threshold2->SetLowerThreshold(0.5f); // 50% above threshold
		threshold2->SetInsideValue(255);
		threshold2->SetOutsideValue(0);

		SlicesHandlerITKInterface wrapper(handler3D);
		if (allslices->isChecked())
		{
			using input_image_type = itk::SliceContiguousImage<float>;
			using image_type = itk::Image<float, 3>;
			using slice_filter_type = itk::SliceBySliceImageFilter<input_image_type, image_type, threshold_filter, threshold_filter>;

			auto target = wrapper.GetTarget(true);

			auto slice_executor = slice_filter_type::New();
			slice_executor->SetInput(target);
			slice_executor->SetInputFilter(threshold);
			slice_executor->SetOutputFilter(threshold2);

			slice_executor->Update();
			// copy back to target
		}
		else
		{
			auto target = wrapper.GetTargetSlice();

			threshold->SetInput(target);

			threshold2->Update();
			// copy back to target
		}
	}
	else
	{
		if (allslices->isChecked())
		{
			if (rb_gaussian->isChecked())
			{
				handler3D->gaussian(sl_sigma->value() * 0.05f);
			}
			else if (rb_average->isChecked())
			{
				handler3D->average((short unsigned)sb_n->value());
			}
			else if (rb_median->isChecked())
			{
				handler3D->median_interquartile(true);
			}
			else if (rb_sigmafilter->isChecked())
			{
				handler3D->sigmafilter(
					(sl_k->value() + 1) * 0.01f * sb_kmax->value(),
					(short unsigned)sb_n->value(), (short unsigned)sb_n->value());
			}
			else
			{
				handler3D->aniso_diff(1.0f, sb_iter->value(), f2,
					sl_k->value() * 0.01f * sb_kmax->value(),
					sl_restrain->value() * 0.01f);
			}
		}
		else // current slice
		{
			if (rb_gaussian->isChecked())
			{
				bmphand->gaussian(sl_sigma->value() * 0.05f);
			}
			else if (rb_average->isChecked())
			{
				bmphand->average((short unsigned)sb_n->value());
			}
			else if (rb_median->isChecked())
			{
				bmphand->median_interquartile(true);
			}
			else if (rb_sigmafilter->isChecked())
			{
				bmphand->sigmafilter((sl_k->value() + 1) * 0.01f * sb_kmax->value(),
					(short unsigned)sb_n->value(),
					(short unsigned)sb_n->value());
			}
			else
			{
				bmphand->aniso_diff(1.0f, sb_iter->value(), f2,
					sl_k->value() * 0.01f * sb_kmax->value(),
					sl_restrain->value() * 0.01f);
			}
		}
	}
	emit end_datachange(this);
}

void SmoothingWidget::method_changed(int)
{
	if (rb_gaussian->isChecked())
	{
		if (hideparams)
			hbox2->hide();
		else
			hbox2->show();
		hbox1->hide();
		hbox4->hide();
		vbox2->hide();
		contdiff->hide();
	}
	else if (rb_average->isChecked())
	{
		if (hideparams)
			hbox1->hide();
		else
			hbox1->show();
		hbox2->hide();
		hbox4->hide();
		vbox2->hide();
		contdiff->hide();
	}
	else if (rb_median->isChecked())
	{
		hbox1->hide();
		hbox2->hide();
		hbox4->hide();
		vbox2->hide();
		contdiff->hide();
	}
	else if (rb_sigmafilter->isChecked())
	{
		if (hideparams)
			hbox1->hide();
		else
			hbox1->show();
		//		hbox2->show();
		hbox2->hide();
		vbox2->hide();
		if (hideparams)
			hbox4->hide();
		else
			hbox4->show();
		contdiff->hide();
		txt_k->setText("Sigma: 0 ");
	}
	else
	{
		hbox1->hide();
		hbox2->hide();
		txt_k->setText("k: 0 ");
		if (hideparams)
			hbox4->hide();
		else
			hbox4->show();
		if (hideparams)
			vbox2->hide();
		else
			vbox2->show();
		if (hideparams)
			contdiff->hide();
		else
			contdiff->show();
	}
}

void SmoothingWidget::input_changed()
{
	// if target, we only allow median
	if (target->isChecked())
	{
		rb_median->setChecked(true);

		// update params
		method_changed(0);
	}

	// TODO disable modegroup
}

void SmoothingWidget::continue_diff()
{
	if (!rb_anisodiff->isChecked())
	{
		return;
	}

	iseg::DataSelection dataSelection;
	dataSelection.allSlices = allslices->isChecked();
	dataSelection.sliceNr = handler3D->active_slice();
	dataSelection.work = true;
	emit begin_datachange(dataSelection, this);

	if (allslices->isChecked())
	{
		handler3D->cont_anisodiff(1.0f, sb_iter->value(), f2,
				sl_k->value() * 0.01f * sb_kmax->value(),
				sl_restrain->value() * 0.01f);
	}
	else
	{
		bmphand->cont_anisodiff(1.0f, sb_iter->value(), f2,
				sl_k->value() * 0.01f * sb_kmax->value(),
				sl_restrain->value() * 0.01f);
	}

	emit end_datachange(this);
}

void SmoothingWidget::sigmaslider_changed(int /* newval */)
{
	if (rb_gaussian->isChecked())
	{
		if (allslices->isChecked())
			handler3D->gaussian(sl_sigma->value() * 0.05f);
		else
			bmphand->gaussian(sl_sigma->value() * 0.05f);
		emit end_datachange(this, iseg::NoUndo);
	}

	return;
}

void SmoothingWidget::kslider_changed(int /* newval */)
{
	if (rb_sigmafilter->isChecked())
	{
		if (allslices->isChecked())
			handler3D->sigmafilter(
					(sl_k->value() + 1) * 0.01f * sb_kmax->value(),
					(short unsigned)sb_n->value(), (short unsigned)sb_n->value());
		else
			bmphand->sigmafilter((sl_k->value() + 1) * 0.01f * sb_kmax->value(),
					(short unsigned)sb_n->value(),
					(short unsigned)sb_n->value());
		emit end_datachange(this, iseg::NoUndo);
	}

	return;
}

void SmoothingWidget::n_changed(int /* newval */)
{
	iseg::DataSelection dataSelection;
	dataSelection.allSlices = allslices->isChecked();
	dataSelection.sliceNr = handler3D->active_slice();
	dataSelection.work = true;
	emit begin_datachange(dataSelection, this);

	if (rb_sigmafilter->isChecked())
	{
		if (allslices->isChecked())
		{
			handler3D->sigmafilter(
					(sl_k->value() + 1) * 0.01f * sb_kmax->value(),
					(short unsigned)sb_n->value(), (short unsigned)sb_n->value());
		}
		else
		{
			bmphand->sigmafilter((sl_k->value() + 1) * 0.01f * sb_kmax->value(),
					(short unsigned)sb_n->value(),
					(short unsigned)sb_n->value());
		}
	}
	else if (rb_average)
	{
		if (allslices->isChecked())
		{
			handler3D->average((short unsigned)sb_n->value());
		}
		else
		{
			bmphand->average((short unsigned)sb_n->value());
		}
	}
	emit end_datachange(this);
}

void SmoothingWidget::kmax_changed(int /* newval */)
{
	iseg::DataSelection dataSelection;
	dataSelection.allSlices = allslices->isChecked();
	dataSelection.sliceNr = handler3D->active_slice();
	dataSelection.work = true;
	emit begin_datachange(dataSelection, this);

	if (rb_sigmafilter->isChecked())
	{
		if (allslices->isChecked())
		{
			handler3D->sigmafilter(
					(sl_k->value() + 1) * 0.01f * sb_kmax->value(),
					(short unsigned)sb_n->value(), (short unsigned)sb_n->value());
		}
		else
		{
			bmphand->sigmafilter((sl_k->value() + 1) * 0.01f * sb_kmax->value(),
					(short unsigned)sb_n->value(),
					(short unsigned)sb_n->value());
		}
	}
	emit end_datachange(this);
}

QSize SmoothingWidget::sizeHint() const { return vbox1->sizeHint(); }

void SmoothingWidget::on_slicenr_changed()
{
	activeslice = handler3D->active_slice();
	bmphand_changed(handler3D->get_activebmphandler());
}

void SmoothingWidget::bmphand_changed(bmphandler* bmph)
{
	bmphand = bmph;
}

void SmoothingWidget::init()
{
	on_slicenr_changed();
	hideparams_changed();
}

void SmoothingWidget::newloaded()
{
	activeslice = handler3D->active_slice();
	bmphand = handler3D->get_activebmphandler();
}

void SmoothingWidget::slider_pressed()
{
	if ((rb_gaussian->isChecked() || rb_sigmafilter->isChecked()))
	{
		iseg::DataSelection dataSelection;
		dataSelection.allSlices = allslices->isChecked();
		dataSelection.sliceNr = handler3D->active_slice();
		dataSelection.work = true;
		emit begin_datachange(dataSelection, this);
	}
}

void SmoothingWidget::slider_released()
{
	if (rb_gaussian->isChecked() || rb_sigmafilter->isChecked())
	{
		emit end_datachange(this);
	}
}

FILE* SmoothingWidget::SaveParams(FILE* fp, int version)
{
	if (version >= 2)
	{
		int dummy;
		dummy = sl_sigma->value();
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = sl_k->value();
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = sl_restrain->value();
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = sb_n->value();
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = sb_iter->value();
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = sb_kmax->value();
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = (int)(rb_gaussian->isChecked());
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = (int)(rb_average->isChecked());
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = (int)(rb_median->isChecked());
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = (int)(rb_sigmafilter->isChecked());
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = (int)(rb_anisodiff->isChecked());
		fwrite(&(dummy), 1, sizeof(int), fp);
		dummy = (int)(allslices->isChecked());
		fwrite(&(dummy), 1, sizeof(int), fp);
	}

	return fp;
}

FILE* SmoothingWidget::LoadParams(FILE* fp, int version)
{
	if (version >= 2)
	{
		disconnect(modegroup, SIGNAL(buttonClicked(int)), this,
				SLOT(method_changed(int)));
		disconnect(sl_sigma, SIGNAL(valueChanged(int)), this,
				SLOT(sigmaslider_changed(int)));
		disconnect(sl_k, SIGNAL(valueChanged(int)), this,
				SLOT(kslider_changed(int)));
		disconnect(sb_n, SIGNAL(valueChanged(int)), this,
				SLOT(n_changed(int)));
		disconnect(sb_kmax, SIGNAL(valueChanged(int)), this,
				SLOT(kmax_changed(int)));

		int dummy;
		fread(&dummy, sizeof(int), 1, fp);
		sl_sigma->setValue(dummy);
		fread(&dummy, sizeof(int), 1, fp);
		sl_k->setValue(dummy);
		fread(&dummy, sizeof(int), 1, fp);
		sl_restrain->setValue(dummy);
		fread(&dummy, sizeof(int), 1, fp);
		sb_n->setValue(dummy);
		fread(&dummy, sizeof(int), 1, fp);
		sb_iter->setValue(dummy);
		fread(&dummy, sizeof(int), 1, fp);
		sb_kmax->setValue(dummy);
		fread(&dummy, sizeof(int), 1, fp);
		rb_gaussian->setChecked(dummy > 0);
		fread(&dummy, sizeof(int), 1, fp);
		rb_average->setChecked(dummy > 0);
		fread(&dummy, sizeof(int), 1, fp);
		rb_median->setChecked(dummy > 0);
		fread(&dummy, sizeof(int), 1, fp);
		rb_sigmafilter->setChecked(dummy > 0);
		fread(&dummy, sizeof(int), 1, fp);
		rb_anisodiff->setChecked(dummy > 0);
		fread(&dummy, sizeof(int), 1, fp);
		allslices->setChecked(dummy > 0);

		method_changed(0);

		connect(modegroup, SIGNAL(buttonClicked(int)), this,
				SLOT(method_changed(int)));
		connect(sl_sigma, SIGNAL(valueChanged(int)), this,
				SLOT(sigmaslider_changed(int)));
		connect(sl_k, SIGNAL(valueChanged(int)), this,
				SLOT(kslider_changed(int)));
		connect(sb_n, SIGNAL(valueChanged(int)), this,
				SLOT(n_changed(int)));
		connect(sb_kmax, SIGNAL(valueChanged(int)), this,
				SLOT(kmax_changed(int)));
	}
	return fp;
}

void SmoothingWidget::hideparams_changed() { method_changed(0); }

} // namespace iseg
