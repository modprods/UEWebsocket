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

#pragma once

#include "Components/ActorComponent.h"
#include "UObject/NoExportTypes.h"
#include "Delegates/DelegateCombinations.h"
#include "WebSocketBase.generated.h"


DEFINE_LOG_CATEGORY_STATIC(WebSocket, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWebSocketConnectError, const FString&, error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWebSocketClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWebSocketConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWebSocketRecieve, const FString&, data);


#if PLATFORM_UWP
#include <collection.h>
#include <ppltasks.h>

class UWebSocketBase;

ref class FUWPSocketHelper sealed
{
public:
	FUWPSocketHelper();
	virtual ~FUWPSocketHelper();

	void MessageReceived(Windows::Networking::Sockets::MessageWebSocket^ sender, Windows::Networking::Sockets::MessageWebSocketMessageReceivedEventArgs^ args);
	void OnUWPClosed(Windows::Networking::Sockets::IWebSocket^ sender, Windows::Networking::Sockets::WebSocketClosedEventArgs^ args);
	void SetParent(int64 p);

private:
	int64 Parent;
};



#else
struct lws_context;
struct lws;
#endif

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class WEBSOCKET_API UWebSocketBase:public UObject
{
	GENERATED_BODY()
public:

	UWebSocketBase();

	virtual void BeginDestroy() override;
	
	UFUNCTION(BlueprintCallable, Category = WebSocket)
	void SendText(const FString& data);

	UFUNCTION(BlueprintCallable, Category = WebSocket)
	void Close();

	bool Connect(const FString& uri, const TMap<FString, FString>& header);

#if PLATFORM_UWP
	Concurrency::task<void> ConnectAsync(Platform::String^ uriString);
	void MessageReceived(Windows::Networking::Sockets::MessageWebSocket^ sender, Windows::Networking::Sockets::MessageWebSocketMessageReceivedEventArgs^ args);
	void OnUWPClosed(Windows::Networking::Sockets::IWebSocket^ sender, Windows::Networking::Sockets::WebSocketClosedEventArgs^ args);
	Concurrency::task<void> SendAsync(Platform::String^ message);
#endif

	UPROPERTY(BlueprintAssignable, Category = WebSocket)
	FWebSocketConnectError OnConnectError;

	UPROPERTY(BlueprintAssignable, Category = WebSocket)
	FWebSocketClosed OnClosed;

	UPROPERTY(BlueprintAssignable, Category = WebSocket)
	FWebSocketConnected OnConnectComplete;

	UPROPERTY(BlueprintAssignable, Category = WebSocket)
	FWebSocketRecieve OnReceiveData;

	void Cleanlws();
	void ProcessWriteable();
	void ProcessRead(const char* in, int len);
	bool ProcessHeader(unsigned char** p, unsigned char* end);

#if PLATFORM_UWP
	Windows::Networking::Sockets::MessageWebSocket^ messageWebSocket;
	Windows::Storage::Streams::DataWriter^ messageWriter;
	FUWPSocketHelper^ uwpSocketHelper;
#else
	struct lws_context* mlwsContext;
	struct lws* mlws;
#endif
	
	TArray<FString> mSendQueue;
	TMap<FString, FString> mHeaderMap;
};
