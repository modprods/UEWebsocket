/*
* uewebsocket - unreal engine 4 websocket plugin
*
* Copyright (C) 2017 feiwu <feixuwu@outlook.com>
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Lesser General Public
*  License as published by the Free Software Foundation:
*  version 2.1 of the License.
*
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Lesser General Public License for more details.
*
*  You should have received a copy of the GNU Lesser General Public
*  License along with this library; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*  MA  02110-1301  USA
*/


#include "WebSocket.h"
#include "WebSocketContext.h"
#include "UObjectGlobals.h"
#include "WebSocketBase.h"
#include "Paths.h"
#include "FileManager.h"
#include "FileHelper.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Base64.h"
#include <fstream>

#define MAX_PAYLOAD	64*1024

extern TSharedPtr<UWebSocketContext> s_websocketCtx;

#if PLATFORM_UWP

#else
static struct lws_protocols protocols[] = {
	/* first protocol must always be HTTP handler */

	{
		"",		/* name - can be overridden with -e */
		UWebSocketContext::callback_echo,
		0,
		MAX_PAYLOAD,
	},
	{
		NULL, NULL, 0		/* End of list */
	}
};

static const struct lws_extension exts[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate; client_no_context_takeover"
	},
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{ NULL, NULL, NULL /* terminator */ }
};
#endif

void UWebSocketContext::BeginDestroy()
{
	Super::BeginDestroy();
}

UWebSocketContext::~UWebSocketContext()
{
	UE_LOG(LogInit, Log, TEXT("release UWebSocketContext"));
}

#if PLATFORM_UWP

#else
int UWebSocketContext::callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	void* pUser = lws_wsi_user(wsi);
	UWebSocketBase* pWebSocketBase = (UWebSocketBase*)pUser;

	switch (reason)
	{
	case LWS_CALLBACK_CLOSED:
		if (!pWebSocketBase) return -1;
		pWebSocketBase->Cleanlws();
		pWebSocketBase->OnClosed.Broadcast();
		break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
	{
		if (!pWebSocketBase) return -1;
		FString strError = UTF8_TO_TCHAR(in);
		UE_LOG(WebSocket, Error, TEXT("libwebsocket connect error:%s"), *strError);
		pWebSocketBase->Cleanlws();
		pWebSocketBase->OnConnectError.Broadcast(strError);
	}
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		if (!pWebSocketBase) return -1;
		pWebSocketBase->OnConnectComplete.Broadcast();
		break;

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
	{
		if (!pWebSocketBase) return -1;

		unsigned char **p = (unsigned char **)in, *end = (*p) + len;
		if (!pWebSocketBase->ProcessHeader(p, end))
		{
			return -1;
		}
	}
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		if (!pWebSocketBase) return -1;
		pWebSocketBase->ProcessRead((const char*)in, (int)len);
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE:
		if (!pWebSocketBase) return -1;
		pWebSocketBase->ProcessWriteable();
		break;

	default:
		break;
	}

	return 0;
}
#endif

UWebSocketContext::UWebSocketContext()
{
#if PLATFORM_UWP

#else
	mlwsContext = nullptr;
#endif
}

extern char g_caArray[];

void UWebSocketContext::CreateCtx()
{
#if PLATFORM_UWP

#else
	struct lws_context_creation_info info;
	memset(&info, 0, sizeof info);

	info.protocols = protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;

	info.port = -1;
	info.gid = -1;
	info.uid = -1;
	info.extensions = exts;
	info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;
	info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	FString PEMFilename = FPaths::ProjectSavedDir() / TEXT("ca-bundle.pem");
	PEMFilename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*PEMFilename);
#if PLATFORM_ANDROID
	extern FString GExternalFilePath;
	PEMFilename = GExternalFilePath / TEXT("ca-bundle.pem");
#endif
	mstrCAPath = TCHAR_TO_UTF8(*PEMFilename);
	std::ofstream f(mstrCAPath.c_str() );
	if (f.is_open())
	{
		FString strBase64CA(ANSI_TO_TCHAR(g_caArray) );
		FString strCAContent;
		FBase64::Decode(strBase64CA, strCAContent);
		std::string strOriginCA = TCHAR_TO_UTF8(*strCAContent);
		f.write(strOriginCA.c_str(), strOriginCA.size());
		f.close();
	}
	else
	{
		UE_LOG(LogInit, Log, TEXT(" websocket: fail open file: '%s'"), *PEMFilename);
		UKismetSystemLibrary::PrintString(this, TEXT(" websocket: fail open file:") + PEMFilename, true, true, FLinearColor(0.0, 0.66, 1.0), 1000);
	}
	
	UE_LOG(LogInit, Log, TEXT(" websocket: using generated PEM file: '%s'"), *PEMFilename);
	//UKismetSystemLibrary::PrintString(this, TEXT("full dir:") + PEMFilename, true, true, FLinearColor(0.0, 0.66, 1.0), 1000);

	info.ssl_ca_filepath = mstrCAPath.c_str();
	mlwsContext = lws_create_context(&info);
	if (mlwsContext == nullptr)
	{
		//UE_LOG(WebSocket, Error, TEXT("libwebsocket Init fail"));
	}
#endif
}

void UWebSocketContext::Tick(float DeltaTime)
{
#if PLATFORM_UWP

#else
	if (mlwsContext != nullptr)
	{
		lws_callback_on_writable_all_protocol(mlwsContext, &protocols[0]);
		lws_service(mlwsContext, 0);
	}
#endif
}

bool UWebSocketContext::IsTickable() const
{
	return true;
}

TStatId UWebSocketContext::GetStatId() const
{
	return TStatId();
}

UWebSocketBase* UWebSocketContext::Connect(const FString& uri, const TMap<FString, FString>& header, bool& connectFail)
{
#if PLATFORM_UWP

#else
	if (mlwsContext == nullptr)
	{
		return nullptr;
	}
#endif

	UWebSocketBase* pNewSocketBase = NewObject<UWebSocketBase>();

#if PLATFORM_UWP

#else
	pNewSocketBase->mlwsContext = mlwsContext;
#endif

	connectFail = !(pNewSocketBase->Connect(uri, header) );

	return pNewSocketBase;
}

UWebSocketBase* UWebSocketContext::Connect(const FString& uri, bool& connectFail)
{
	return Connect(uri, TMap<FString, FString>(), connectFail);
}
