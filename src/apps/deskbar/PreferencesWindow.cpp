/*
 * Copyright 2009-2011 Haiku, Inc.
 * All Rights Reserved. Distributed under the terms of the MIT License.
 *
 * Authors:
 *		John Scipione, jscipione@gmail.com
 *		Jonas Sundström, jonas@kirilla.com
 */


#include "PreferencesWindow.h"

#include <ctype.h>

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <ControlLook.h>
#include <FormattingConventions.h>
#include <GroupLayout.h>
#include <Locale.h>
#include <LayoutBuilder.h>
#include <OpenWithTracker.h>
#include <RadioButton.h>
#include <Roster.h>
#include <SeparatorView.h>
#include <Slider.h>
#include <StringView.h>
#include <TextControl.h>
#include <View.h>

#include "BarApp.h"
#include "StatusView.h"


static const float kIndentSpacing
	= be_control_look->DefaultItemSpacing() * 2.3;


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PreferencesWindow"


PreferencesWindow::PreferencesWindow(BRect frame)
	:
	BWindow(frame, B_TRANSLATE("Deskbar preferences"), B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE)
{
	// Menu controls
	fMenuRecentDocuments = new BCheckBox(B_TRANSLATE("Recent documents:"),
		new BMessage(kUpdateRecentCounts));
	fMenuRecentApplications = new BCheckBox(B_TRANSLATE("Recent applications:"),
		new BMessage(kUpdateRecentCounts));
	fMenuRecentFolders = new BCheckBox(B_TRANSLATE("Recent folders:"),
		new BMessage(kUpdateRecentCounts));

	fMenuRecentDocumentCount = new BTextControl(NULL, NULL,
		new BMessage(kUpdateRecentCounts));
	fMenuRecentApplicationCount = new BTextControl(NULL, NULL,
		new BMessage(kUpdateRecentCounts));
	fMenuRecentFolderCount = new BTextControl(NULL, NULL,
		new BMessage(kUpdateRecentCounts));

	// Applications controls
	fAppsSort = new BCheckBox(B_TRANSLATE("Sort running applications"),
		new BMessage(kSortRunningApps));
	fAppsSortTrackerFirst = new BCheckBox(B_TRANSLATE("Tracker always first"),
		new BMessage(kTrackerFirst));
	fAppsShowExpanders = new BCheckBox(B_TRANSLATE("Show application expander"),
		new BMessage(kSuperExpando));
	fAppsExpandNew = new BCheckBox(B_TRANSLATE("Expand new applications"),
		new BMessage(kExpandNewTeams));
	fAppsHideLabels = new BCheckBox(B_TRANSLATE("Hide application names"),
		new BMessage(kHideLabels));
	fAppsIconSizeSlider = new BSlider("icon_size", B_TRANSLATE("Icon size"),
		NULL, kMinimumIconSize / kIconSizeInterval,
		kMaximumIconSize / kIconSizeInterval, B_HORIZONTAL);
	fAppsIconSizeSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fAppsIconSizeSlider->SetHashMarkCount((kMaximumIconSize - kMinimumIconSize)
		/ kIconSizeInterval + 1);
	fAppsIconSizeSlider->SetLimitLabels(B_TRANSLATE("Small"),
		B_TRANSLATE("Large"));
	fAppsIconSizeSlider->SetModificationMessage(new BMessage(kResizeTeamIcons));

	// Window controls
	fWindowAlwaysOnTop = new BCheckBox(B_TRANSLATE("Always on top"),
		new BMessage(kAlwaysTop));
	fWindowAutoRaise = new BCheckBox(B_TRANSLATE("Auto-raise"),
		new BMessage(kAutoRaise));
	fWindowAutoHide = new BCheckBox(B_TRANSLATE("Auto-hide"),
		new BMessage(kAutoHide));

	// Clock controls
	fShowSeconds = new BCheckBox(B_TRANSLATE("Show seconds"),
		new BMessage(kShowSeconds));
	fShowDayOfWeek = new BCheckBox(B_TRANSLATE("Show day of week"),
		new BMessage(kShowDayOfWeek));

	// Get settings from BarApp
	TBarApp* barApp = static_cast<TBarApp*>(be_app);
	desk_settings* settings = barApp->Settings();

	// Menu settings
	BTextView* docTextView = fMenuRecentDocumentCount->TextView();
	BTextView* appTextView = fMenuRecentApplicationCount->TextView();
	BTextView* folderTextView = fMenuRecentFolderCount->TextView();

	for (int32 i = 0; i < 256; i++) {
		if (!isdigit(i)) {
			docTextView->DisallowChar(i);
			appTextView->DisallowChar(i);
			folderTextView->DisallowChar(i);
		}
	}

	docTextView->SetMaxBytes(4);
	appTextView->SetMaxBytes(4);
	folderTextView->SetMaxBytes(4);

	int32 docCount = settings->recentDocsCount;
	int32 appCount = settings->recentAppsCount;
	int32 folderCount = settings->recentFoldersCount;

	fMenuRecentDocuments->SetValue(settings->recentDocsEnabled);
	fMenuRecentDocumentCount->SetEnabled(settings->recentDocsEnabled);

	fMenuRecentApplications->SetValue(settings->recentAppsEnabled);
	fMenuRecentApplicationCount->SetEnabled(settings->recentAppsEnabled);

	fMenuRecentFolders->SetValue(settings->recentFoldersEnabled);
	fMenuRecentFolderCount->SetEnabled(settings->recentFoldersEnabled);

	BString docString;
	BString appString;
	BString folderString;

	docString << docCount;
	appString << appCount;
	folderString << folderCount;

	fMenuRecentDocumentCount->SetText(docString.String());
	fMenuRecentApplicationCount->SetText(appString.String());
	fMenuRecentFolderCount->SetText(folderString.String());

	// Applications settings
	fAppsSort->SetValue(settings->sortRunningApps);
	fAppsSortTrackerFirst->SetValue(settings->trackerAlwaysFirst);
	fAppsShowExpanders->SetValue(settings->superExpando);
	fAppsExpandNew->SetValue(settings->expandNewTeams);
	fAppsHideLabels->SetValue(settings->hideLabels);
	fAppsIconSizeSlider->SetValue(settings->iconSize / kIconSizeInterval);

	// Window settings
	fWindowAlwaysOnTop->SetValue(settings->alwaysOnTop);
	fWindowAutoRaise->SetValue(settings->autoRaise);
	fWindowAutoHide->SetValue(settings->autoHide);

	// Clock settings
	TReplicantTray* replicantTray = barApp->BarView()->ReplicantTray();
	if (replicantTray->Time() != NULL) {
		fShowSeconds->SetValue(replicantTray->Time()->ShowSeconds());
		fShowDayOfWeek->SetValue(replicantTray->Time()->ShowDayOfWeek());
	} else {
		fShowSeconds->SetValue(settings->showSeconds);
		fShowDayOfWeek->SetValue(settings->showDayOfWeek);
	}

	EnableDisableDependentItems();

	// Targets
	fAppsSort->SetTarget(be_app);
	fAppsSortTrackerFirst->SetTarget(be_app);
	fAppsExpandNew->SetTarget(be_app);
	fAppsHideLabels->SetTarget(be_app);
	fAppsIconSizeSlider->SetTarget(be_app);

	fWindowAlwaysOnTop->SetTarget(be_app);
	fWindowAutoRaise->SetTarget(be_app);
	fWindowAutoHide->SetTarget(be_app);

	fShowSeconds->SetTarget(replicantTray);
	fShowDayOfWeek->SetTarget(replicantTray);

	// Layout
	fMenuBox = new BBox("fMenuBox");
	fAppsBox = new BBox("fAppsBox");
	fWindowBox = new BBox("fWindowBox");
	fClockBox = new BBox("fClockBox");

	fMenuBox->SetLabel(B_TRANSLATE("Menu"));
	fAppsBox->SetLabel(B_TRANSLATE("Applications"));
	fWindowBox->SetLabel(B_TRANSLATE("Window"));
	fClockBox->SetLabel(B_TRANSLATE("Clock"));

	BView* view;
	view = BLayoutBuilder::Group<>()
		.AddGroup(B_VERTICAL, 0)
			.AddGroup(B_HORIZONTAL, 0)
				.AddGroup(B_VERTICAL, 0)
					.Add(fMenuRecentDocuments)
					.Add(fMenuRecentFolders)
					.Add(fMenuRecentApplications)
					.End()
				.AddGroup(B_VERTICAL, 0)
					.Add(fMenuRecentDocumentCount)
					.Add(fMenuRecentFolderCount)
					.Add(fMenuRecentApplicationCount)
					.End()
				.End()
			.AddGroup(B_VERTICAL, 0)
				.SetInsets(0, B_USE_DEFAULT_SPACING, 0, 0)
				.Add(new BButton(B_TRANSLATE("Edit menu" B_UTF8_ELLIPSIS),
					new BMessage(kEditMenuInTracker)))
				.End()
			.SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING,
				B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
			.End()
		.View();
	fMenuBox->AddChild(view);

	view = BLayoutBuilder::Group<>()
		.AddGroup(B_VERTICAL, 0)
			.Add(fAppsSort)
			.Add(fAppsSortTrackerFirst)
			.Add(fAppsShowExpanders)
			.AddGroup(B_HORIZONTAL, 0)
				.SetInsets(kIndentSpacing, 0, 0, 0)
				.Add(fAppsExpandNew)
				.End()
			.Add(fAppsHideLabels)
			.AddGroup(B_HORIZONTAL, 0)
				.SetInsets(0, B_USE_DEFAULT_SPACING, 0, 0)
				.Add(fAppsIconSizeSlider)
				.End()
			.AddGlue()
			.SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING,
				B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
			.End()
		.View();
	fAppsBox->AddChild(view);

	view = BLayoutBuilder::Group<>()
		.AddGroup(B_VERTICAL, 0)
			.Add(fWindowAlwaysOnTop)
			.Add(fWindowAutoRaise)
			.Add(fWindowAutoHide)
			.AddGlue()
			.SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING,
				B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
			.End()
		.View();
	fWindowBox->AddChild(view);

	view = BLayoutBuilder::Group<>()
		.AddGroup(B_VERTICAL, 0)
			.Add(fShowSeconds)
			.Add(fShowDayOfWeek)
			.AddGlue()
			.SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING,
				B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
			.End()
		.View();
	fClockBox->AddChild(view);

	BLayoutBuilder::Group<>(this)
		.AddGrid(5, 5)
			.Add(fMenuBox, 0, 0)
			.Add(fWindowBox, 1, 0)
			.Add(fAppsBox, 0, 1)
			.Add(fClockBox, 1, 1)
			.SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING,
				B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
			.End()
		.End();

	CenterOnScreen();
}


PreferencesWindow::~PreferencesWindow()
{
	UpdateRecentCounts();
	be_app->PostMessage(kConfigClose);
}


void
PreferencesWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kEditMenuInTracker:
			OpenWithTracker(B_USER_DESKBAR_DIRECTORY);
			break;

		case kUpdateRecentCounts:
			UpdateRecentCounts();
			break;

		case kSuperExpando:
			EnableDisableDependentItems();
			be_app->PostMessage(message);
			break;

		case kStateChanged:
			EnableDisableDependentItems();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
PreferencesWindow::WindowActivated(bool active)
{
	if (!active && IsMinimized())
		PostMessage(B_QUIT_REQUESTED);
}


void
PreferencesWindow::UpdateRecentCounts()
{
	BMessage message(kUpdateRecentCounts);

	int32 docCount = atoi(fMenuRecentDocumentCount->Text());
	int32 appCount = atoi(fMenuRecentApplicationCount->Text());
	int32 folderCount = atoi(fMenuRecentFolderCount->Text());

	message.AddInt32("documents", max_c(0, docCount));
	message.AddInt32("applications", max_c(0, appCount));
	message.AddInt32("folders", max_c(0, folderCount));

	message.AddBool("documentsEnabled", fMenuRecentDocuments->Value());
	message.AddBool("applicationsEnabled", fMenuRecentApplications->Value());
	message.AddBool("foldersEnabled", fMenuRecentFolders->Value());

	be_app->PostMessage(&message);

	EnableDisableDependentItems();
}


void
PreferencesWindow::EnableDisableDependentItems()
{
	TBarApp* barApp = static_cast<TBarApp*>(be_app);
	if (barApp->BarView()->Vertical()
		&& barApp->BarView()->ExpandoState()) {
		fAppsShowExpanders->SetEnabled(true);
		fAppsExpandNew->SetEnabled(fAppsShowExpanders->Value());
	} else {
		fAppsShowExpanders->SetEnabled(false);
		fAppsExpandNew->SetEnabled(false);
	}

	fMenuRecentDocumentCount->SetEnabled(
		fMenuRecentDocuments->Value() != B_CONTROL_OFF);
	fMenuRecentApplicationCount->SetEnabled(
		fMenuRecentApplications->Value() != B_CONTROL_OFF);
	fMenuRecentFolderCount->SetEnabled(
		fMenuRecentFolders->Value() != B_CONTROL_OFF);

	fWindowAutoRaise->SetEnabled(
		fWindowAlwaysOnTop->Value() == B_CONTROL_OFF);
}
