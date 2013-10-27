/*
 * Copyright 2010 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christophe Huriaux, c.huriaux@gmail.com
 */


#include <UrlProtocolDispatchingListener.h>
#include <Debug.h>

#include <assert.h>


const char* kUrlProtocolMessageType = "be:urlProtocolMessageType";
const char* kUrlProtocolCaller = "be:urlProtocolCaller";


BUrlProtocolDispatchingListener::BUrlProtocolDispatchingListener
	(BHandler* handler)
		:
		fMessenger(handler)
{
}


BUrlProtocolDispatchingListener::BUrlProtocolDispatchingListener
	(const BMessenger& messenger)
		: 
		fMessenger(messenger)
{
}


BUrlProtocolDispatchingListener::~BUrlProtocolDispatchingListener()
{
}


void
BUrlProtocolDispatchingListener::ConnectionOpened(BUrlRequest* caller)
{
	BMessage message(B_URL_PROTOCOL_NOTIFICATION);
	_SendMessage(&message, B_URL_PROTOCOL_CONNECTION_OPENED, caller);
}


void
BUrlProtocolDispatchingListener::HostnameResolved(BUrlRequest* caller,
	const char* ip)
{
	BMessage message(B_URL_PROTOCOL_NOTIFICATION);
	message.AddString("url:hostIp", ip);
	
	_SendMessage(&message, B_URL_PROTOCOL_HOSTNAME_RESOLVED, caller);
}


void
BUrlProtocolDispatchingListener::ResponseStarted(BUrlRequest* caller)
{
	BMessage message(B_URL_PROTOCOL_NOTIFICATION);
	_SendMessage(&message, B_URL_PROTOCOL_RESPONSE_STARTED, caller);
}


void
BUrlProtocolDispatchingListener::HeadersReceived(BUrlRequest* caller)
{
	BMessage message(B_URL_PROTOCOL_NOTIFICATION);
	_SendMessage(&message, B_URL_PROTOCOL_HEADERS_RECEIVED, caller);
}


void
BUrlProtocolDispatchingListener::DataReceived(BUrlRequest* caller,
	const char* data, ssize_t size)
{
	BMessage message(B_URL_PROTOCOL_NOTIFICATION);
	status_t result = message.AddData("url:data", B_STRING_TYPE, data, size,
		true, 1);
	assert(result == B_OK);
	
	_SendMessage(&message, B_URL_PROTOCOL_DATA_RECEIVED, caller);
}


void
BUrlProtocolDispatchingListener::DownloadProgress(BUrlRequest* caller,
	ssize_t bytesReceived, ssize_t bytesTotal)
{
	BMessage message(B_URL_PROTOCOL_NOTIFICATION);
	message.AddInt32("url:bytesReceived", bytesReceived);
	message.AddInt32("url:bytesTotal", bytesTotal);
	
	_SendMessage(&message, B_URL_PROTOCOL_DOWNLOAD_PROGRESS, caller);
}


void
BUrlProtocolDispatchingListener::UploadProgress(BUrlRequest* caller,
	ssize_t bytesSent, ssize_t bytesTotal)
{
	BMessage message(B_URL_PROTOCOL_NOTIFICATION);
	message.AddInt32("url:bytesSent", bytesSent);
	message.AddInt32("url:bytesTotal", bytesTotal);
	
	_SendMessage(&message, B_URL_PROTOCOL_UPLOAD_PROGRESS, caller);
}



void
BUrlProtocolDispatchingListener::RequestCompleted(BUrlRequest* caller,
	bool success)
{
	BMessage message(B_URL_PROTOCOL_NOTIFICATION);
	message.AddBool("url:success", success);
	
	_SendMessage(&message, B_URL_PROTOCOL_REQUEST_COMPLETED, caller);
}


void
BUrlProtocolDispatchingListener::_SendMessage(BMessage* message, 
	int8 notification, BUrlRequest* caller)
{
	ASSERT(message != NULL);
		
	message->AddPointer(kUrlProtocolCaller, caller);
	message->AddInt8(kUrlProtocolMessageType, notification);

#ifdef DEBUG
	status_t result = fMessenger.SendMessage(message);
	ASSERT(result == B_OK);
#else
	fMessenger.SendMessage(message);
#endif
}
