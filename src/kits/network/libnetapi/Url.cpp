/*
 * Copyright 2010 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christophe Huriaux, c.huriaux@gmail.com
 */


#include <Url.h>

#include <ctype.h>
#include <cstdio>
#include <cstdlib>
#include <new>

#include <RegExp.h>


static const char* kArchivedUrl = "be:url string";


BUrl::BUrl(const char* url)
	: 
	fUrlString(),
	fProtocol(),
	fUser(),
	fPassword(),
	fHost(),
	fPort(0),
	fPath(),
	fRequest(),
	fHasAuthority(false)
{
	SetUrlString(url);
}


BUrl::BUrl(BMessage* archive)
	: 
	fUrlString(),
	fProtocol(),
	fUser(),
	fPassword(),
	fHost(),
	fPort(0),
	fPath(),
	fRequest(),
	fHasAuthority(false)
{
	BString url;

	if (archive->FindString(kArchivedUrl, &url) == B_OK)
		SetUrlString(url);
	else
		_ResetFields();
}


BUrl::BUrl(const BUrl& other)
	: 
	BArchivable(),
	fUrlString(),
	fProtocol(),
	fUser(),
	fPassword(),
	fHost(),
	fPort(0),
	fPath(),
	fRequest(),
	fHasAuthority(false)
{
	*this = other;
}


BUrl::BUrl(const BUrl& base, const BString& location)
	: 
	fUrlString(),
	fProtocol(),
	fUser(),
	fPassword(),
	fHost(),
	fPort(0),
	fPath(),
	fRequest(),
	fHasAuthority(false)
{
	// This implements the algorithm in RFC3986, Section 5.2.

	BUrl relative(location);
	if(relative.HasProtocol()) {
		SetProtocol(relative.Protocol());
		SetAuthority(relative.Authority());
		SetPath(relative.Path()); // TODO _RemoveDotSegments()
		SetRequest(relative.Request());
	} else {
		if(relative.HasAuthority()) {
			SetAuthority(relative.Authority());
			SetPath(relative.Path()); // TODO _RemoveDotSegments()
			SetRequest(relative.Request());
		} else {
			if(relative.Path().IsEmpty()) {
				SetPath(base.Path());
				if(relative.HasRequest())
					SetRequest(relative.Request());
				else
					SetRequest(Request());
			} else {
				if (relative.Path()[0] == '/')
					SetPath(relative.Path());
				else {
					BString path = base.Path();
					// Remove last part of path (the file, if any) so we get the
					// "current directory"
					path.Truncate(path.FindLast('/') + 1);
					path += relative.Path();
					// TODO _RemoveDotSegments()
					SetPath(path);
				}
				SetRequest(relative.Request());
			}
			SetAuthority(base.Authority());
		}
		SetProtocol(base.Protocol());
	}

	SetFragment(relative.Fragment());
}


BUrl::BUrl()
	:
	fUrlString(),
	fProtocol(),
	fUser(),
	fPassword(),
	fHost(),
	fPort(0),
	fPath(),
	fRequest(),
	fHasAuthority(false)
{
	_ResetFields();
}


BUrl::~BUrl()
{
}


// #pragma mark URL fields modifiers


BUrl&
BUrl::SetUrlString(const BString& url)
{
	_ExplodeUrlString(url);
	return *this;
}


BUrl&
BUrl::SetProtocol(const BString& protocol)
{
	fProtocol = protocol;
	fHasProtocol = !fProtocol.IsEmpty();
	fUrlStringValid = false;
	return *this;
}


BUrl&
BUrl::SetUserName(const BString& user)
{
	fUser = user;
	fHasUserName = !fUser.IsEmpty();
	fUrlStringValid = false;
	fAuthorityValid = false;
	fUserInfoValid = false;
	return *this;
}


BUrl&
BUrl::SetPassword(const BString& password)
{
	fPassword = password;
	fHasPassword = !fPassword.IsEmpty();
	fUrlStringValid = false;
	fAuthorityValid = false;
	fUserInfoValid = false;
	return *this;
}


BUrl&
BUrl::SetHost(const BString& host)
{
	fHost = host;
	fHasHost = !fHost.IsEmpty();
	fUrlStringValid = false;
	fAuthorityValid = false;
	return *this;
}


BUrl&
BUrl::SetPort(int port)
{
	fPort = port;
	fHasPort = (port != 0);
	fUrlStringValid = false;
	fAuthorityValid = false;
	return *this;
}


BUrl&
BUrl::SetPath(const BString& path)
{
	fPath = path;
	fHasPath = true; // RFC says an empty path is still a path
	fUrlStringValid = false;
	return *this;
}


BUrl&
BUrl::SetRequest(const BString& request)
{
	fRequest = request;
	fHasRequest = !fRequest.IsEmpty();
	fUrlStringValid = false;
	return *this;
}


BUrl&
BUrl::SetFragment(const BString& fragment)
{
	fFragment = fragment;
	fHasFragment = !fFragment.IsEmpty();
	fUrlStringValid = false;
	return *this;
}


// #pragma mark URL fields access


const BString&
BUrl::UrlString() const
{
	if (!fUrlStringValid) {
		fUrlString.Truncate(0);

		if (HasProtocol()) {
			fUrlString << fProtocol << ':';
			if (HasAuthority())
				fUrlString << "//";
		}

		fUrlString << Authority();
		fUrlString << Path();

		if (HasRequest())
			fUrlString << '?' << fRequest;

		if (HasFragment())
			fUrlString << '#' << fFragment;

		fUrlStringValid = true;
	}

	return fUrlString;
}


const BString&
BUrl::Protocol() const
{
	return fProtocol;
}


const BString&
BUrl::UserName() const
{
	return fUser;
}


const BString&
BUrl::Password() const
{
	return fPassword;
}


const BString&
BUrl::UserInfo() const
{
	if (!fUserInfoValid) {
		fUserInfo = fUser;

		if (HasPassword())
			fUserInfo << ':' << fPassword;

		fUserInfoValid = true;
	}

	return fUserInfo;
}


const BString&
BUrl::Host() const
{
	return fHost;
}


int
BUrl::Port() const
{
	return fPort;
}


const BString&
BUrl::Authority() const
{
	if (!fAuthorityValid) {
		fAuthority.Truncate(0);

		if (HasUserInfo())
			fAuthority << UserInfo() << '@';
		fAuthority << Host();

		if (HasPort())
			fAuthority << ':' << fPort;
			
		fAuthorityValid = true;
	}
	return fAuthority;
}


const BString&
BUrl::Path() const
{
	return fPath;
}


const BString&
BUrl::Request() const
{
	return fRequest;
}


const BString&
BUrl::Fragment() const
{
	return fFragment;
}


// #pragma mark URL fields tests


bool
BUrl::IsValid() const
{
	// TODO
	return false;
}


bool
BUrl::HasProtocol() const
{
	return fHasProtocol;
}


bool
BUrl::HasAuthority() const
{
	return fHasAuthority;
}


bool
BUrl::HasUserName() const
{
	return fHasUserName;
}


bool
BUrl::HasPassword() const
{
	return fHasPassword;
}


bool
BUrl::HasUserInfo() const
{
	return fHasUserInfo;
}


bool
BUrl::HasHost() const
{
	return fHasHost;
}


bool
BUrl::HasPort() const
{
	return fHasPort;
}


bool
BUrl::HasPath() const
{
	return fHasPath;
}


bool
BUrl::HasRequest() const
{
	return fHasRequest;
}


bool
BUrl::HasFragment() const
{
	return fHasFragment;
}


// #pragma mark URL encoding/decoding of needed fields


void
BUrl::UrlEncode(bool strict)
{
	fUser = _DoUrlEncodeChunk(fUser, strict);
	fPassword = _DoUrlEncodeChunk(fPassword, strict);
	fHost = _DoUrlEncodeChunk(fHost, strict);
	fFragment = _DoUrlEncodeChunk(fFragment, strict);
	fPath = _DoUrlEncodeChunk(fPath, strict, true);
}


void
BUrl::UrlDecode(bool strict)
{
	fUser = _DoUrlDecodeChunk(fUser, strict);
	fPassword = _DoUrlDecodeChunk(fPassword, strict);
	fHost = _DoUrlDecodeChunk(fHost, strict);
	fFragment = _DoUrlDecodeChunk(fFragment, strict);
	fPath = _DoUrlDecodeChunk(fPath, strict);
}


// #pragma mark Url encoding/decoding of string


/*static*/ BString
BUrl::UrlEncode(const BString& url, bool strict, bool directory)
{
	return _DoUrlEncodeChunk(url, strict, directory);
}


/*static*/ BString
BUrl::UrlDecode(const BString& url, bool strict)
{
	return _DoUrlDecodeChunk(url, strict);
}


// #pragma mark BArchivable members


status_t
BUrl::Archive(BMessage* into, bool deep) const
{
	status_t ret = BArchivable::Archive(into, deep);

	if (ret == B_OK)
		ret = into->AddString(kArchivedUrl, UrlString());
	
	return ret;
}


/*static*/ BArchivable*
BUrl::Instantiate(BMessage* archive)
{
	if (validate_instantiation(archive, "BUrl"))
		return new(std::nothrow) BUrl(archive);
	return NULL;
}


// #pragma mark URL comparison


bool
BUrl::operator==(BUrl& other) const
{
	UrlString();
	other.UrlString();
	
	return fUrlString == other.fUrlString;
}


bool
BUrl::operator!=(BUrl& other) const
{
	return !(*this == other);
}


// #pragma mark URL assignment


const BUrl&
BUrl::operator=(const BUrl& other)
{
	fUrlStringValid		= other.fUrlStringValid;
	if (fUrlStringValid)
		fUrlString			= other.fUrlString;
	
	fAuthorityValid		= other.fAuthorityValid;
	if (fAuthorityValid)
		fAuthority			= other.fAuthority;
	
	fUserInfoValid		= other.fUserInfoValid;
	if (fUserInfoValid)
		fUserInfo			= other.fUserInfo;
	
	fProtocol			= other.fProtocol;
	fUser				= other.fUser;
	fPassword			= other.fPassword;
	fHost				= other.fHost;
	fPort				= other.fPort;
	fPath				= other.fPath;
	fRequest			= other.fRequest;
	fFragment			= other.fFragment;
	
	fHasProtocol		= other.fHasProtocol;
	fHasUserName		= other.fHasUserName;
	fHasPassword		= other.fHasPassword;
	fHasUserInfo		= other.fHasUserInfo;
	fHasHost			= other.fHasHost;
	fHasPort			= other.fHasPort;
	fHasAuthority		= other.fHasAuthority;
	fHasPath			= other.fHasPath;
	fHasRequest			= other.fHasRequest;
	fHasFragment		= other.fHasFragment;
	
	return *this;
}


const BUrl&
BUrl::operator=(const BString& string)
{
	SetUrlString(string);
	return *this;
}


const BUrl&
BUrl::operator=(const char* string)
{
	SetUrlString(string);
	return *this;
}


// #pragma mark URL to string conversion


BUrl::operator const char*() const
{
	return UrlString();
}


void
BUrl::_ResetFields()
{
	fHasProtocol = false;
	fHasUserName = false;
	fHasPassword = false;
	fHasUserInfo = false;
	fHasHost = false;
	fHasPort = false;
	fHasAuthority = false;
	fHasPath = false;
	fHasRequest = false;
	fHasFragment = false;

	fProtocol.Truncate(0);
	fUser.Truncate(0);
	fPassword.Truncate(0);
	fHost.Truncate(0);
	fPort = 0;
	fPath.Truncate(0);
	fRequest.Truncate(0);
	fFragment.Truncate(0);

	// Force re-generation of these fields
	fUrlStringValid = false;
	fUserInfoValid = false;
	fAuthorityValid = false;
}


void
BUrl::_ExplodeUrlString(const BString& url)
{
	// The regexp is provided in RFC3986 (URI generic syntax), Appendix B
	static RegExp urlMatcher(
		"^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?");

	_ResetFields();

	RegExp::MatchResult match = urlMatcher.Match(url.String());

	if(!match.HasMatched())
		return; // TODO error reporting

	// Scheme/Protocol
	url.CopyInto(fProtocol, match.GroupStartOffsetAt(1),
		match.GroupEndOffsetAt(1) - match.GroupStartOffsetAt(1));
	if (!_IsProtocolValid()) {
		fHasProtocol = false;
		fProtocol.Truncate(0);
	} else
		fHasProtocol = true;

	// Authority (including user credentials, host, and port
	url.CopyInto(fAuthority, match.GroupStartOffsetAt(3),
		match.GroupEndOffsetAt(3) - match.GroupStartOffsetAt(3));
	SetAuthority(fAuthority);

	// Path
	url.CopyInto(fPath, match.GroupStartOffsetAt(4),
		match.GroupEndOffsetAt(4) - match.GroupStartOffsetAt(4));
	if(!fPath.IsEmpty())
		fHasPath = true;

	// Query
	url.CopyInto(fRequest, match.GroupStartOffsetAt(6),
		match.GroupEndOffsetAt(6) - match.GroupStartOffsetAt(6));
	if(!fRequest.IsEmpty())
		fHasRequest = true;

	// Fragment
	url.CopyInto(fFragment, match.GroupStartOffsetAt(8),
		match.GroupEndOffsetAt(8) - match.GroupStartOffsetAt(8));
	if(!fFragment.IsEmpty())
		fHasFragment = true;
}


void
BUrl::SetAuthority(const BString& authority)
{
	fAuthority = authority;

	fHasPort = false;
	fHasUserInfo = false;
	fHasHost = false;

	if(fAuthority.IsEmpty())
		return;

	fHasAuthority = true;
	int32 userInfoEnd = fAuthority.FindFirst('@');

	// URL contains userinfo field
	if (userInfoEnd != -1) {
		BString userInfo;
		fAuthority.CopyInto(userInfo, 0, userInfoEnd);

		int16 colonDelimiter = userInfo.FindFirst(':', 0);

		if (colonDelimiter == 0) {
			SetPassword(userInfo);
		} else if (colonDelimiter != -1) {
			userInfo.CopyInto(fUser, 0, colonDelimiter);
			userInfo.CopyInto(fPassword, colonDelimiter + 1,
				userInfo.Length() - colonDelimiter);
			SetUserName(fUser);
			SetPassword(fPassword);
		} else {
			SetUserName(fUser);
		}

		fHasUserInfo = true;
	}


	// Extract the host part
	int16 hostEnd = fAuthority.FindFirst(':', userInfoEnd);
	userInfoEnd++;

	if(hostEnd < 0)
	{
		// no ':' found, the host extends to the end of the URL
		hostEnd = fAuthority.Length() + 1;
	}

	// The host is likely to be present if an authority is
	// defined, but in some weird cases, it's not.
	if (hostEnd != userInfoEnd) {
		fAuthority.CopyInto(fHost, userInfoEnd, hostEnd - userInfoEnd);
		SetHost(fHost);
	}

	// Extract the port part
	fPort = 0;
	if (fAuthority.ByteAt(hostEnd) == ':') {
		hostEnd++;
		int16 portEnd = fAuthority.Length();

		BString portString;
		fAuthority.CopyInto(portString, hostEnd, portEnd - hostEnd);
		fPort = atoi(portString.String());

		//  Even if the port is invalid, the URL is considered to
		// have a port.
		fHasPort = portString.Length() > 0;
	}
}


/*static*/ BString
BUrl::_DoUrlEncodeChunk(const BString& chunk, bool strict, bool directory)
{
	BString result;
	
	for (int32 i = 0; i < chunk.Length(); i++) {
		if (_IsUnreserved(chunk[i])
			|| (directory && (chunk[i] == '/' || chunk[i] == '\\')))
			result << chunk[i];
		else {
			if (chunk[i] == ' ' && !strict) {
				result << '+';
					// In non-strict mode, spaces are encoded by a plus sign
			} else {
				char hexString[5];
				snprintf(hexString, 5, "%X", chunk[i]);
				
				result << '%' << hexString;
			}
		}
	}
	
	return result;
}


/*static*/ BString
BUrl::_DoUrlDecodeChunk(const BString& chunk, bool strict)
{
	BString result;
	
	for (int32 i = 0; i < chunk.Length(); i++) {
		if (chunk[i] == '+' && !strict)
			result << ' ';
		else if (chunk[i] != '%')
			result << chunk[i];
		else {
			char hexString[] = { chunk[i+1], chunk[i+2], 0 };
			result << (char)strtol(hexString, NULL, 16);
			
			i += 2;
		}
	}
	return result;
}


bool
BUrl::_IsProtocolValid()
{
	for (int8 index = 0; index < fProtocol.Length(); index++) {
		char c = fProtocol[index];

		if (index == 0 && !isalpha(c))
			return false;
		else if (!isalnum(c) && c != '+' && c != '-' && c != '.')
			return false;
	}

	return fProtocol.Length() > 0;
}


bool
BUrl::_IsUnreserved(char c)
{
	if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
		return true;
	else
		return false;
}


bool
BUrl::_IsGenDelim(char c)
{
	if (c == ':' || c == '/' || c == '?' || c == '#' || c == '[' 
		|| c == ']' || c == '@')
		return true;
	else
		return false;
}


bool
BUrl::_IsSubDelim(char c)
{
	if (c == '!' || c == '$' || c == '&' || c == '\'' || c == '(' 
		|| c == ')' || c == '*' || c == '+' || c == ',' || c == ';'
		|| c == '=')
		return true;
	else
		return false;
}
