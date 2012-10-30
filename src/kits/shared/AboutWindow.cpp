/*
 * Copyright 2007-2009 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ryan Leavengood, leavengood@gmail.com
 */

#include <AboutWindow.h>

#include <stdarg.h>
#include <time.h>

#include <Alert.h>
#include <Font.h>
#include <String.h>
#include <SystemCatalog.h>
#include <TextView.h>

using BPrivate::gSystemCatalog;


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AboutWindow"


BAboutWindow::BAboutWindow(const char *appName, int32 firstCopyrightYear,
	const char **authors, const char *extraInfo)
{
	fAppName = new BString(appName);

	const char* copyright = gSystemCatalog.GetString(
		B_TRANSLATE_MARK("Copyright " B_UTF8_COPYRIGHT " %years% Haiku, Inc."),
		"AboutWindow");
	const char* writtenBy = gSystemCatalog.GetString(
		B_TRANSLATE_MARK("Written by:"), "AboutWindow");

	// Get current year
	time_t tp;
	time(&tp);
	char currentYear[5];
	strftime(currentYear, 5, "%Y", localtime(&tp));
	BString copyrightYears;
	copyrightYears << firstCopyrightYear;
	if (copyrightYears != currentYear)
		copyrightYears << "-" << currentYear;

	// Build the text to display
	BString text(appName);
	text << "\n\n";
	text << copyright;
	text << "\n\n";
	text.ReplaceAll("%years%", copyrightYears.String());
	text << writtenBy << "\n";
	for (int32 i = 0; authors[i]; i++) {
		text << "    " << authors[i] << "\n";
	}

	// The extra information is optional
	if (extraInfo != NULL) {
		text << "\n" << extraInfo << "\n";
	}

	fText = new BString(text);
}


BAboutWindow::~BAboutWindow()
{
	delete fText;
	delete fAppName;
}


void
BAboutWindow::Show()
{
	const char* aboutTitle = gSystemCatalog.GetString(
		B_TRANSLATE_MARK("About" B_UTF8_ELLIPSIS), "AboutWindow");
	const char* closeLabel = gSystemCatalog.GetString(
		B_TRANSLATE_MARK("Close"), "AboutWindow");

	BAlert *alert = new BAlert(aboutTitle, fText->String(), closeLabel);
	BTextView *view = alert->TextView();
	BFont font;
	view->SetStylable(true);
	view->GetFont(&font);
	font.SetFace(B_BOLD_FACE);
	font.SetSize(font.Size() * 1.7);
	view->SetFontAndColor(0, fAppName->Length(), &font);
	alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
	alert->Go();
}

