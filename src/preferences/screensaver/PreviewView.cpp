/*
 * Copyright 2003-2013 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Michael Phipps
 *		Jérôme Duval, jerome.duval@free.fr
 */


#include "PreviewView.h"
#include "Constants.h"
#include "Utility.h"

#include <Point.h>
#include <Rect.h>
#include <Screen.h>
#include <ScreenSaver.h>
#include <Shape.h>
#include <String.h>

#include <iostream>


static float sampleX[]
	= { 0, 0.05, 0.15, 0.7, 0.725, 0.8, 0.825, 0.85, 0.950, 1.0 };
static float sampleY[] = { 0, 0.05, 0.90, 0.95, 0.966, 0.975, 1.0 };


inline BPoint
scale2(int x, int y, BRect area)
{
	return scale_direct(sampleX[x], sampleY[y], area);
}


inline BRect
scale2(int x1, int x2, int y1, int y2, BRect area)
{
	return scale_direct(sampleX[x1], sampleX[x2], sampleY[y1], sampleY[y2],
		area);
}


PreviewView::PreviewView(const char* name)
	:
	BView(name, B_WILL_DRAW),
	fSaverView(NULL)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	float aspectRatio = 4.0f / 3.0f;
		// 4:3 monitor
	float previewWidth = 160.0f;
	float previewHeight = ceilf(previewWidth / aspectRatio);

	SetExplicitSize(BSize(previewWidth, previewHeight));
}


PreviewView::~PreviewView()
{
}


BView*
PreviewView::AddPreview()
{
	BRect rect = scale2(1, 8, 1, 2, Bounds());
	rect.InsetBy(1, 1);
	fSaverView = new BView(rect, "preview", B_FOLLOW_NONE, B_WILL_DRAW);
	fSaverView->SetViewColor(0, 0, 0);
	AddChild(fSaverView);

	return fSaverView;
}


BView*
PreviewView::RemovePreview()
{
	if (fSaverView != NULL)
		RemoveChild(fSaverView);

	BView* saverView = fSaverView;
	fSaverView = NULL;
	return saverView;
}


void
PreviewView::Draw(BRect update)
{
	SetHighColor(184, 184, 184);
	FillRoundRect(scale2(0, 9, 0, 3, Bounds()), 4, 4);
		// Outer shape
	FillRoundRect(scale2(2, 7, 3, 6, Bounds()), 2, 2);
		// control console outline

	SetHighColor(96, 96, 96);
	StrokeRoundRect(scale2(2, 7, 3, 6, Bounds()), 2, 2);
		// control console outline
	StrokeRoundRect(scale2(0, 9, 0, 3, Bounds()), 4, 4);
		// Outline outer shape

	SetHighColor(kBlack);
	FillRect(scale2(1, 8, 1, 2, Bounds()));

	SetHighColor(184, 184, 184);
	BRect outerShape = scale2(2, 7, 2, 6, Bounds());
	outerShape.InsetBy(1, 1);
	FillRoundRect(outerShape, 4, 4);
		// Outer shape

	SetHighColor(0, 255, 0);
	FillRect(scale2(3, 4, 4, 5, Bounds()));
	SetHighColor(96, 96, 96);
	FillRect(scale2(5, 6, 4, 5, Bounds()));
}
