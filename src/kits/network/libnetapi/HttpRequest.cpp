/*
 * Copyright 2010-2013 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christophe Huriaux, c.huriaux@gmail.com
 *		Niels Sascha Reedijk, niels.reedijk@gmail.com
 */


#include <HttpRequest.h>

#include <arpa/inet.h>
#include <stdio.h>

#include <cstdlib>
#include <deque>
#include <new>

#include <Debug.h>
#include <File.h>
#include <Socket.h>
#include <SecureSocket.h>


static const int32 kHttpBufferSize = 4096;
static const char* kHttpProtocolThreadStrStatus[
		B_PROT_HTTP_THREAD_STATUS__END - B_PROT_THREAD_STATUS__END]
	=  {
		"The remote server did not found the requested resource"
	};


BHttpRequest::BHttpRequest(const BUrl& url, bool ssl, const char* protocolName,
	BUrlProtocolListener* listener, BUrlContext* context)
	:
	BUrlRequest(url, listener, context, "BUrlProtocol.HTTP", protocolName),
	fSocket(NULL),
	fSSL(ssl),
	fRequestMethod(B_HTTP_GET),
	fHttpVersion(B_HTTP_11),
	fResult(url),
	fRequestStatus(kRequestInitialState),
	fOptHeaders(NULL),
	fOptPostFields(NULL),
	fOptInputData(NULL),
	fOptInputDataSize(-1),
	fOptFollowLocation(true)
{
	_ResetOptions();
	if (ssl)
		fSocket = new BSecureSocket();
	else
		fSocket = new BSocket();
}


BHttpRequest::~BHttpRequest()
{
	delete fSocket;

	delete fOptInputData;
	delete fOptHeaders;
	delete fOptPostFields;
}


void
BHttpRequest::SetMethod(const char* const method)
{
	fRequestMethod = method;
}


void
BHttpRequest::SetFollowLocation(bool follow)
{
	fOptFollowLocation = follow;
}


void
BHttpRequest::SetMaxRedirections(int8 redirections)
{
	fOptMaxRedirs = redirections;
}


void
BHttpRequest::SetReferrer(const BString& referrer)
{
	fOptReferer = referrer;
}


void
BHttpRequest::SetUserAgent(const BString& agent)
{
	fOptUserAgent = agent;
}


void
BHttpRequest::SetDiscardData(bool discard)
{
	fOptDiscardData = discard;
}


void
BHttpRequest::SetDisableListener(bool disable)
{
	fOptDisableListener = disable;
}


void
BHttpRequest::SetAutoReferrer(bool enable)
{
    fOptAutoReferer = enable;
}


void
BHttpRequest::SetHeaders(const BHttpHeaders& headers)
{
	AdoptHeaders(new BHttpHeaders(headers));
}


void
BHttpRequest::AdoptHeaders(BHttpHeaders* const headers)
{
	delete fOptHeaders;
	fOptHeaders = headers;
}


void
BHttpRequest::SetPostFields(const BHttpForm& fields)
{
	AdoptPostFields(new BHttpForm(fields));
}


void
BHttpRequest::AdoptPostFields(BHttpForm* const fields)
{
	delete fOptPostFields;
	fOptPostFields = fields;

	if (fOptPostFields != NULL)
		fRequestMethod = B_HTTP_POST;
}


void
BHttpRequest::AdoptInputData(BDataIO* const data, const ssize_t size)
{
	delete fOptInputData;
	fOptInputData = data;
	fOptInputDataSize = size;
}


void
BHttpRequest::SetUserName(const BString& name)
{
	fOptUsername = name;
}


void
BHttpRequest::SetPassword(const BString& password)
{
	fOptPassword = password;
}


/*static*/ bool
BHttpRequest::IsInformationalStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__INFORMATIONAL_BASE)
		&& (code <  B_HTTP_STATUS__INFORMATIONAL_END);
}


/*static*/ bool
BHttpRequest::IsSuccessStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__SUCCESS_BASE)
		&& (code <  B_HTTP_STATUS__SUCCESS_END);
}


/*static*/ bool
BHttpRequest::IsRedirectionStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__REDIRECTION_BASE)
		&& (code <  B_HTTP_STATUS__REDIRECTION_END);
}


/*static*/ bool
BHttpRequest::IsClientErrorStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__CLIENT_ERROR_BASE)
		&& (code <  B_HTTP_STATUS__CLIENT_ERROR_END);
}


/*static*/ bool
BHttpRequest::IsServerErrorStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__SERVER_ERROR_BASE)
		&& (code <  B_HTTP_STATUS__SERVER_ERROR_END);
}


/*static*/ int16
BHttpRequest::StatusCodeClass(int16 code)
{
	if (BHttpRequest::IsInformationalStatusCode(code))
		return B_HTTP_STATUS_CLASS_INFORMATIONAL;
	else if (BHttpRequest::IsSuccessStatusCode(code))
		return B_HTTP_STATUS_CLASS_SUCCESS;
	else if (BHttpRequest::IsRedirectionStatusCode(code))
		return B_HTTP_STATUS_CLASS_REDIRECTION;
	else if (BHttpRequest::IsClientErrorStatusCode(code))
		return B_HTTP_STATUS_CLASS_CLIENT_ERROR;
	else if (BHttpRequest::IsServerErrorStatusCode(code))
		return B_HTTP_STATUS_CLASS_SERVER_ERROR;

	return B_HTTP_STATUS_CLASS_INVALID;
}


const char*
BHttpRequest::StatusString(status_t threadStatus) const
{
	if (threadStatus < B_PROT_THREAD_STATUS__END)
		return BUrlRequest::StatusString(threadStatus);
	else if (threadStatus >= B_PROT_HTTP_THREAD_STATUS__END)
		return BUrlRequest::StatusString(-1);
	else
		return kHttpProtocolThreadStrStatus[threadStatus
			- B_PROT_THREAD_STATUS__END];
}


const BHttpResult&
BHttpRequest::Result() const
{
	return fResult;
}


void
BHttpRequest::_ResetOptions()
{
	delete fOptPostFields;
	delete fOptHeaders;

	fOptFollowLocation = true;
	fOptMaxRedirs = 8;
	fOptReferer = "";
	fOptUserAgent = "Services Kit (Haiku)";
	fOptUsername = "";
	fOptPassword = "";
	fOptAuthMethods = B_HTTP_AUTHENTICATION_BASIC | B_HTTP_AUTHENTICATION_DIGEST
		| B_HTTP_AUTHENTICATION_IE_DIGEST;
	fOptHeaders = NULL;
	fOptPostFields = NULL;
	fOptSetCookies = true;
	fOptDiscardData = false;
	fOptDisableListener = false;
	fOptAutoReferer = true;
}


status_t
BHttpRequest::_ProtocolLoop()
{
	printf("UHP[%p]::{Loop} %s\n", this, fUrl.UrlString().String());

	// Initialize the request redirection loop
	int8 maxRedirs = fOptMaxRedirs;
	bool newRequest;

	do {
		newRequest = false;

		// Result reset
		fOutputBuffer.Truncate(0, true);
		fOutputHeaders.Clear();
		fHeaders.Clear();
		_ResultHeaders().Clear();

		if (!_ResolveHostName()) {
			_EmitDebug(B_URL_PROTOCOL_DEBUG_ERROR,
				"Unable to resolve hostname (%s), aborting.",
					fUrl.Host().String());
			return B_PROT_CANT_RESOLVE_HOSTNAME;
		}

		_CreateRequest();
		_AddHeaders();
		_AddOutputBufferLine("");

		status_t requestStatus = _MakeRequest();
		if (requestStatus != B_PROT_SUCCESS)
			return requestStatus;

		// Prepare the referer for the next request if needed
		if (fOptAutoReferer)
			fOptReferer = fUrl.UrlString();

		switch (StatusCodeClass(fResult.StatusCode())) {
			case B_HTTP_STATUS_CLASS_INFORMATIONAL:
				// Header 100:continue should have been
				// handled in the _MakeRequest read loop
				break;

			case B_HTTP_STATUS_CLASS_SUCCESS:
				break;

			case B_HTTP_STATUS_CLASS_REDIRECTION:
				// Redirection has been explicitly disabled
				if (!fOptFollowLocation)
					break;

				//  TODO: Some browsers seems to translate POST requests to
				// GET when following a 302 redirection
				if (fResult.StatusCode() == B_HTTP_STATUS_MOVED_PERMANENTLY) {
					BString locationUrl = fHeaders["Location"];

					fUrl = BUrl(fUrl, locationUrl);

					if (--maxRedirs > 0) {
						newRequest = true;

						_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT,
							"Following: %s\n",
							fUrl.UrlString().String());
					}
				}
				break;

			case B_HTTP_STATUS_CLASS_CLIENT_ERROR:
				if (fResult.StatusCode() == B_HTTP_STATUS_UNAUTHORIZED) {
					BHttpAuthentication* authentication
						= &fContext->GetAuthentication(fUrl);
					status_t status = B_OK;

					if (authentication->Method() == B_HTTP_AUTHENTICATION_NONE) {
						// There is no authentication context for this
						// url yet, so let's create one.
						authentication = new BHttpAuthentication();
						status = authentication->Initialize(
							fHeaders["WWW-Authenticate"]);
						fContext->AddAuthentication(fUrl, authentication);
					}

					newRequest = false;
					if (fOptUsername.Length() > 0 && status == B_OK) {
						// If we received an username and password, add them
						// to the request. This will either change the
						// credentials for an existing request, or set them
						// for a new one we created just above.
						//
						// If this request handles HTTP redirections, it will
						// also automatically retry connecting and send the
						// login information.
						authentication->SetUserName(fOptUsername);
						authentication->SetPassword(fOptPassword);
						newRequest = true;
					}
				}
				break;

			case B_HTTP_STATUS_CLASS_SERVER_ERROR:
				break;

			default:
			case B_HTTP_STATUS_CLASS_INVALID:
				break;
		}
	} while (newRequest);

	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT,
		"%ld headers and %ld bytes of data remaining",
		fHeaders.CountHeaders(), fInputBuffer.Size());

	if (fResult.StatusCode() == 404)
		return B_PROT_HTTP_NOT_FOUND;

	return B_PROT_SUCCESS;
}


bool
BHttpRequest::_ResolveHostName()
{
	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Resolving %s",
		fUrl.UrlString().String());

	uint16_t port;
	if (fUrl.HasPort())
		port = fUrl.Port();
	else
		port = fSSL ? 443 : 80;

	fRemoteAddr = BNetworkAddress(AF_INET, fUrl.Host(), port);

	if (fRemoteAddr.InitCheck() != B_OK)
		return false;

	//! ProtocolHook:HostnameResolved
	if (fListener != NULL)
		fListener->HostnameResolved(this, fRemoteAddr.ToString().String());

	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Hostname resolved to: %s",
		fRemoteAddr.ToString().String());

	return true;
}


status_t
BHttpRequest::_MakeRequest()
{
	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Connection to %s on port %d.",
		fUrl.Authority().String(), fRemoteAddr.Port());
	status_t connectError = fSocket->Connect(fRemoteAddr);

	if (connectError != B_OK) {
		_EmitDebug(B_URL_PROTOCOL_DEBUG_ERROR, "Socket connection error %s",
			strerror(connectError));
		return B_PROT_CONNECTION_FAILED;
	}

	//! ProtocolHook:ConnectionOpened
	if (fListener != NULL)
		fListener->ConnectionOpened(this);

	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Connection opened.");

	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Sending request (size=%d)",
		fOutputBuffer.Length());
	fSocket->Write(fOutputBuffer.String(), fOutputBuffer.Length());
	fOutputBuffer.Truncate(0);
	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Request sent.");

	if (fRequestMethod == B_HTTP_POST && fOptPostFields != NULL) {
		if (fOptPostFields->GetFormType() != B_HTTP_FORM_MULTIPART) {
			fOutputBuffer = fOptPostFields->RawData();
			_EmitDebug(B_URL_PROTOCOL_DEBUG_TRANSFER_OUT,
				"%s", fOutputBuffer.String());
			fSocket->Write(fOutputBuffer.String(), fOutputBuffer.Length());
		} else {
			for (BHttpForm::Iterator it = fOptPostFields->GetIterator();
				const BHttpFormData* currentField = it.Next();
				) {
				_EmitDebug(B_URL_PROTOCOL_DEBUG_TRANSFER_OUT,
					it.MultipartHeader().String());
				fSocket->Write(it.MultipartHeader().String(),
					it.MultipartHeader().Length());

				switch (currentField->Type()) {
					case B_HTTPFORM_UNKNOWN:
						ASSERT(0);
						break;

					case B_HTTPFORM_STRING:
						fSocket->Write(currentField->String().String(),
							currentField->String().Length());
						break;

					case B_HTTPFORM_FILE:
						{
							BFile upFile(currentField->File().Path(),
								B_READ_ONLY);
							char readBuffer[kHttpBufferSize];
							ssize_t readSize;

							readSize = upFile.Read(readBuffer,
								sizeof(readBuffer));
							while (readSize > 0) {
								fSocket->Write(readBuffer, readSize);
								readSize = upFile.Read(readBuffer,
									sizeof(readBuffer));
							}
						}
						break;

					case B_HTTPFORM_BUFFER:
						fSocket->Write(currentField->Buffer(),
							currentField->BufferSize());
						break;
				}

				fSocket->Write("\r\n", 2);
			}

			fSocket->Write(fOptPostFields->GetMultipartFooter().String(),
				fOptPostFields->GetMultipartFooter().Length());
		}
	} else if ((fRequestMethod == B_HTTP_POST || fRequestMethod == B_HTTP_PUT)
		&& fOptInputData != NULL) {

		for (;;) {
			char outputTempBuffer[kHttpBufferSize];
			ssize_t read = fOptInputData->Read(outputTempBuffer,
				sizeof(outputTempBuffer));

			if (read <= 0)
				break;

			if (fOptInputDataSize < 0) {
				// Chunked transfer
				char hexSize[16];
				size_t hexLength = sprintf(hexSize, "%ld", read);

				fSocket->Write(hexSize, hexLength);
				fSocket->Write("\r\n", 2);
				fSocket->Write(outputTempBuffer, read);
				fSocket->Write("\r\n", 2);
			} else {
				fSocket->Write(outputTempBuffer, read);
			}
		}

		if (fOptInputDataSize < 0) {
			// Chunked transfer terminating sequence
			fSocket->Write("0\r\n\r\n", 5);
		}
	}
	fOutputBuffer.Truncate(0, true);

	fRequestStatus = kRequestInitialState;

	// Receive loop
	bool receiveEnd = false;
	bool parseEnd = false;
	bool readByChunks = false;
	bool readError = false;
	int32 receiveBufferSize = 32;
	ssize_t bytesRead = 0;
	ssize_t bytesReceived = 0;
	ssize_t bytesTotal = 0;
	char* inputTempBuffer = NULL;
	ssize_t chunkSize = -1;
	fQuit = false;

	while (!fQuit && !(receiveEnd && parseEnd)) {
		if (!receiveEnd) {
			fSocket->WaitForReadable();
			BNetBuffer chunk(receiveBufferSize);
			bytesRead = fSocket->Read(chunk.Data(), receiveBufferSize);

			if (bytesRead < 0) {
				readError = true;
				fQuit = true;
				continue;
			} else if (bytesRead == 0)
				receiveEnd = true;

			fInputBuffer.AppendData(chunk.Data(), bytesRead);
		} else
			bytesRead = 0;

		if (fRequestStatus < kRequestStatusReceived) {
			_ParseStatus();

			//! ProtocolHook:ResponseStarted
			if (fRequestStatus >= kRequestStatusReceived && fListener != NULL)
				fListener->ResponseStarted(this);
		} else if (fRequestStatus < kRequestHeadersReceived) {
			_ParseHeaders();

			if (fRequestStatus >= kRequestHeadersReceived) {
				receiveBufferSize = kHttpBufferSize;
				_ResultHeaders() = fHeaders;

				//! ProtocolHook:HeadersReceived
				if (fListener != NULL)
					fListener->HeadersReceived(this);

				// Parse received cookies
				if (fContext != NULL) {
					for (int32 i = 0;  i < fHeaders.CountHeaders(); i++) {
						if (fHeaders.HeaderAt(i).NameIs("Set-Cookie")) {
							fContext->GetCookieJar().AddCookie(
								fHeaders.HeaderAt(i).Value(), fUrl);
						}
					}
				}

				if (BString(fHeaders["Transfer-Encoding"]) == "chunked")
					readByChunks = true;

				int32 index = fHeaders.HasHeader("Content-Length");
				if (index != B_ERROR)
					bytesTotal = atoi(fHeaders.HeaderAt(index).Value());
				else
					bytesTotal = 0;
			}
		} else {
			// If Transfer-Encoding is chunked, we should read a complete
			// chunk in buffer before handling it
			if (readByChunks) {
				if (chunkSize >= 0) {
					if ((ssize_t)fInputBuffer.Size() >= chunkSize + 2)  {
							// 2 more bytes to handle the closing CR+LF
						bytesRead = chunkSize;
						inputTempBuffer = new char[chunkSize+2];
						fInputBuffer.RemoveData(inputTempBuffer, chunkSize+2);
						chunkSize = -1;
					} else {
						// Not enough data, try again later
						bytesRead = -1;
						inputTempBuffer = NULL;
					}
				} else {
					BString chunkHeader;
					if (_GetLine(chunkHeader) == B_ERROR) {
						chunkSize = -1;
						inputTempBuffer = NULL;
						bytesRead = -1;
					} else {
						// Format of a chunk header:
						// <chunk size in hex>[; optional data]
						int32 semiColonIndex = chunkHeader.FindFirst(";", 0);

						// Cut-off optional data if present
						if (semiColonIndex != -1) {
							chunkHeader.Remove(semiColonIndex,
								chunkHeader.Length() - semiColonIndex);
						}

						chunkSize = strtol(chunkHeader.String(), NULL, 16);
						PRINT(("BHP[%p] Chunk %s=%ld\n", this,
							chunkHeader.String(), chunkSize));
						if (chunkSize == 0) {
							fRequestStatus = kRequestContentReceived;
						}

						bytesRead = -1;
						inputTempBuffer = NULL;
					}
				}

				// A chunk of 0 bytes indicates the end of the chunked
				// transfer
				if (bytesRead == 0)
					receiveEnd = true;
			} else {
				bytesRead = fInputBuffer.Size();

				if (bytesRead > 0) {
					inputTempBuffer = new char[bytesRead];
					fInputBuffer.RemoveData(inputTempBuffer, bytesRead);
				}
			}

			if (bytesRead > 0) {
				bytesReceived += bytesRead;

				if (fListener != NULL) {
					fListener->DataReceived(this, inputTempBuffer, bytesRead);
					fListener->DownloadProgress(this, bytesReceived,
						bytesTotal);
				}

				if (bytesTotal > 0 && bytesReceived >= bytesTotal)
					receiveEnd = true;

				delete[] inputTempBuffer;
			}
		}

		parseEnd = (fInputBuffer.Size() == 0);
	}

	fSocket->Disconnect();

	if (readError)
		return B_PROT_READ_FAILED;

	return fQuit ? B_PROT_ABORTED : B_PROT_SUCCESS;
}


status_t
BHttpRequest::_GetLine(BString& destString)
{
	// Find a complete line in inputBuffer
	uint32 characterIndex = 0;

	while ((characterIndex < fInputBuffer.Size())
		&& ((fInputBuffer.Data())[characterIndex] != '\n'))
		characterIndex++;

	if (characterIndex == fInputBuffer.Size())
		return B_ERROR;

	char* temporaryBuffer = new(std::nothrow) char[characterIndex + 1];
	fInputBuffer.RemoveData(temporaryBuffer, characterIndex + 1);

	// Strip end-of-line character(s)
	if (temporaryBuffer[characterIndex-1] == '\r')
		destString.SetTo(temporaryBuffer, characterIndex - 1);
	else
		destString.SetTo(temporaryBuffer, characterIndex);

	delete[] temporaryBuffer;
	return B_OK;
}


void
BHttpRequest::_ParseStatus()
{
	// Status line should be formatted like: HTTP/M.m SSS ...
	// With:   M = Major version of the protocol
	//         m = Minor version of the protocol
	//       SSS = three-digit status code of the response
	//       ... = additional text info
	BString statusLine;
	if (_GetLine(statusLine) == B_ERROR)
		return;

	if (statusLine.CountChars() < 12)
		return;

	fRequestStatus = kRequestStatusReceived;

	BString statusCodeStr;
	BString statusText;
	statusLine.CopyInto(statusCodeStr, 9, 3);
	_SetResultStatusCode(atoi(statusCodeStr.String()));

	statusLine.CopyInto(_ResultStatusText(), 13, statusLine.Length() - 13);

	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Status line received: Code %d (%s)",
		atoi(statusCodeStr.String()), _ResultStatusText().String());
}


void
BHttpRequest::_ParseHeaders()
{
	BString currentHeader;
	if (_GetLine(currentHeader) == B_ERROR)
		return;

	// An empty line means the end of the header section
	if (currentHeader.Length() == 0) {
		fRequestStatus = kRequestHeadersReceived;
		return;
	}

	_EmitDebug(B_URL_PROTOCOL_DEBUG_HEADER_IN, "%s", currentHeader.String());
	fHeaders.AddHeader(currentHeader.String());
}


void
BHttpRequest::_CreateRequest()
{
	BString request(fRequestMethod);

	if (Url().HasPath())
		request << ' ' << Url().Path();
	else
		request << " /";

	if (Url().HasRequest())
		request << '?' << Url().Request();

	if (Url().HasFragment())
		request << '#' << Url().Fragment();

	request << ' ';

	switch (fHttpVersion) {
		case B_HTTP_11:
			request << "HTTP/1.1";
			break;

		default:
		case B_HTTP_10:
			request << "HTTP/1.0";
			break;
	}

	_AddOutputBufferLine(request.String());
}


void
BHttpRequest::_AddHeaders()
{
	// HTTP 1.1 additional headers
	if (fHttpVersion == B_HTTP_11) {
		fOutputHeaders.AddHeader("Host", Url().Host());

		fOutputHeaders.AddHeader("Accept", "*/*");
		fOutputHeaders.AddHeader("Accept-Encoding", "chunked");
			// Allow the remote server to send dynamic content by chunks
			// rather than waiting for the full content to be generated and
			// sending us data.

		fOutputHeaders.AddHeader("Connection", "close");
			// Let the remote server close the connection after response since
			// we don't handle multiple request on a single connection
	}

	// Classic HTTP headers
	if (fOptUserAgent.CountChars() > 0)
		fOutputHeaders.AddHeader("User-Agent", fOptUserAgent.String());

	if (fOptReferer.CountChars() > 0)
		fOutputHeaders.AddHeader("Referer", fOptReferer.String());

	// Authentication
	BHttpAuthentication& authentication = fContext->GetAuthentication(fUrl);
	if (authentication.Method() != B_HTTP_AUTHENTICATION_NONE) {
		if (fOptUsername.Length() > 0) {
			authentication.SetUserName(fOptUsername);
			authentication.SetPassword(fOptPassword);
		}

		BString request(fRequestMethod);
		fOutputHeaders.AddHeader("Authorization",
			authentication.Authorization(fUrl, request));
	}

	// Required headers for POST data
	if (fOptPostFields != NULL && fRequestMethod == B_HTTP_POST) {
		BString contentType;

		switch (fOptPostFields->GetFormType()) {
			case B_HTTP_FORM_MULTIPART:
				contentType << "multipart/form-data; boundary="
					<< fOptPostFields->GetMultipartBoundary() << "";
				break;

			case B_HTTP_FORM_URL_ENCODED:
				contentType << "application/x-www-form-urlencoded";
				break;
		}

		fOutputHeaders.AddHeader("Content-Type", contentType);
		fOutputHeaders.AddHeader("Content-Length",
			fOptPostFields->ContentLength());
	} else if (fOptInputData != NULL
			&& (fRequestMethod == B_HTTP_POST || fRequestMethod == B_HTTP_PUT)) {
		if (fOptInputDataSize >= 0)
			fOutputHeaders.AddHeader("Content-Length", fOptInputDataSize);
		else
			fOutputHeaders.AddHeader("Transfer-Encoding", "chunked");
	}

	// Optional headers specified by the user
	if (fOptHeaders != NULL) {
		for (int32 headerIndex = 0; headerIndex < fOptHeaders->CountHeaders();
				headerIndex++) {
			BHttpHeader& optHeader = (*fOptHeaders)[headerIndex];
			int32 replaceIndex = fOutputHeaders.HasHeader(optHeader.Name());

			// Add or replace the current option header to the
			// output header list
			if (replaceIndex == -1)
				fOutputHeaders.AddHeader(optHeader.Name(), optHeader.Value());
			else
				fOutputHeaders[replaceIndex].SetValue(optHeader.Value());
		}
	}

	// Context cookies
	if (fOptSetCookies && (fContext != NULL)) {
		BNetworkCookie* cookie;
		BString cookieString;

		for (BNetworkCookieJar::UrlIterator it
				= fContext->GetCookieJar().GetUrlIterator(fUrl);
				(cookie = it.Next()) != NULL;) {
			cookieString << "; ";
			cookieString << cookie->RawCookie(false);
		}

		// Remove the extra "; " we added before the first item
		cookieString.Remove(0, 2);

		if (cookieString.Length() > 0)
			fOutputHeaders.AddHeader("Cookie", cookieString);
	}

	// Write output headers to output stream
	for (int32 headerIndex = 0; headerIndex < fOutputHeaders.CountHeaders();
			headerIndex++) {
		_AddOutputBufferLine(fOutputHeaders.HeaderAt(headerIndex).Header());
	}
}


void
BHttpRequest::_AddOutputBufferLine(const char* line)
{
	_EmitDebug(B_URL_PROTOCOL_DEBUG_HEADER_OUT, "%s", line);
	fOutputBuffer << line << "\r\n";
}


BHttpHeaders&
BHttpRequest::_ResultHeaders()
{
	return fResult.fHeaders;
}


void
BHttpRequest::_SetResultStatusCode(int32 statusCode)
{
	fResult.fStatusCode = statusCode;
}


BString&
BHttpRequest::_ResultStatusText()
{
	return fResult.fStatusString;
}
