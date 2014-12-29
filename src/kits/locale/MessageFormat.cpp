/*
 * Copyright 2014, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */
#include <MessageFormat.h>

#include <Autolock.h>
#include <FormattingConventionsPrivate.h>
#include <LanguagePrivate.h>

#include <ICUWrapper.h>

#include <unicode/msgfmt.h>


BMessageFormat::BMessageFormat(const BLanguage& language, const BString pattern)
	: BFormat(language, BFormattingConventions())
{
	_Initialize(UnicodeString::fromUTF8(pattern.String()));
}


BMessageFormat::BMessageFormat(const BString pattern)
	: BFormat()
{
	_Initialize(UnicodeString::fromUTF8(pattern.String()));
}


BMessageFormat::~BMessageFormat()
{
	delete fFormatter;
}


status_t
BMessageFormat::InitCheck()
{
	return fInitStatus;
}


status_t
BMessageFormat::Format(BString& output, const int32 arg) const
{
	if (fInitStatus != B_OK)
		return fInitStatus;

	UnicodeString buffer;
	UErrorCode error = U_ZERO_ERROR;

	Formattable arguments[] = {
		(int32_t)arg
	};

	FieldPosition pos;
	buffer = fFormatter->format(arguments, 1, buffer, pos, error);
	if (!U_SUCCESS(error))
		return B_ERROR;

	BStringByteSink byteSink(&output);
	buffer.toUTF8(byteSink);

	return B_OK;
}


status_t
BMessageFormat::_Initialize(const UnicodeString& pattern)
{
	fInitStatus = B_OK;
	UErrorCode error = U_ZERO_ERROR;
	Locale* icuLocale
		= BLanguage::Private(&fLanguage).ICULocale();

	fFormatter = new MessageFormat(pattern, *icuLocale, error);

	if (fFormatter == NULL)
		fInitStatus = B_NO_MEMORY;

	if (!U_SUCCESS(error)) {
		delete fFormatter;
		fInitStatus = B_ERROR;
		fFormatter = NULL;
	}

	return fInitStatus;
}
