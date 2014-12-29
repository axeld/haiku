/*
 * Copyright 2004-2011 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 */

#include <stdlib.h>

#include "NetworkSetupProfile.h"


NetworkSetupProfile::NetworkSetupProfile()
	:
	fRoot(new BEntry()),
	fPath(new BPath()),
	fIsDefault(false),
	fIsCurrent(false),
	fName(NULL)
{
}


NetworkSetupProfile::NetworkSetupProfile(const char* path)
	:
	fRoot(NULL),
	fPath(NULL),
	fIsDefault(false),
	fIsCurrent(false)
{
	SetTo(path);
}


NetworkSetupProfile::NetworkSetupProfile(const entry_ref* ref)
	:
	fRoot(NULL),
	fPath(NULL),
	fIsDefault(false),
	fIsCurrent(false)
{
	SetTo(ref);
}


NetworkSetupProfile::NetworkSetupProfile(BEntry* entry)
	:
	fRoot(NULL),
	fPath(NULL),
	fIsDefault(false),
	fIsCurrent(false)
{
	SetTo(entry);
}


NetworkSetupProfile::~NetworkSetupProfile()
{
	delete fRoot;
	delete fPath;
}


status_t
NetworkSetupProfile::SetTo(const char* path)
{
	SetTo(new BEntry(path));
	return B_OK;
}


status_t
NetworkSetupProfile::SetTo(const entry_ref* ref)
{
	SetTo(new BEntry(ref));
	return B_OK;
}


status_t
NetworkSetupProfile::SetTo(BEntry *entry)
{
	delete fRoot;
	delete fPath;
	fRoot = entry;
	fPath = NULL;
	fName = NULL;
	return B_OK;
}


const char*
NetworkSetupProfile::Name()
{
	if (!fName) {
		fRoot->GetPath(fPath);
		fName = fPath->Leaf();
	}

	return fName;
}


status_t 
NetworkSetupProfile::SetName(const char* name)
{
	return B_OK;
}


bool
NetworkSetupProfile::Exists()
{
	return fRoot->Exists();
}


status_t
NetworkSetupProfile::Delete()
{
	return B_ERROR;
}


bool
NetworkSetupProfile::IsDefault()
{
	return fIsDefault;
}


bool
NetworkSetupProfile::IsCurrent()
{
	return fIsCurrent;
}


status_t
NetworkSetupProfile::MakeCurrent()
{
	return B_ERROR;
}


// #pragma mark -
NetworkSetupProfile*
NetworkSetupProfile::Default()
{
	return NULL;
}


NetworkSetupProfile*
NetworkSetupProfile::Current()
{
	return NULL;
}
