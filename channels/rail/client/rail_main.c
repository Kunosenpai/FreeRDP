/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL Virtual Channel Plugin
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
 * Copyright 2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/rail.h>
#include <freerdp/rail.h>

#include "rail_orders.h"
#include "rail_main.h"

void rail_send_channel_data(void* railObject, void* data, size_t length)
{
	wStream* s = NULL;
	railPlugin* plugin = (railPlugin*) railObject;

	s = Stream_New(NULL, length);
	Stream_Write(s, data, length);

	svc_plugin_send((rdpSvcPlugin*) plugin, s);
}

static void on_free_rail_channel_event(wMessage* event)
{
	rail_free_cloned_order(GetMessageType(event->id), event->wParam);
}

void rail_send_channel_event(void* rail_object, UINT16 event_type, void* param)
{
	void* payload = NULL;
	wMessage* out_event = NULL;
	railPlugin* plugin = (railPlugin*) rail_object;

	payload = rail_clone_order(event_type, param);

	if (payload)
	{
		out_event = freerdp_event_new(RailChannel_Class, event_type,
			on_free_rail_channel_event, payload);

		svc_plugin_send_event((rdpSvcPlugin*) plugin, out_event);
	}
}

static void rail_process_connect(rdpSvcPlugin* plugin)
{
	railPlugin* rail = (railPlugin*) plugin;

	rail->rail_order = rail_order_new();
	rail->rail_order->settings = (rdpSettings*) plugin->channel_entry_points.pExtendedData;
	rail->rail_order->plugin = rail;
}

static void rail_process_terminate(rdpSvcPlugin* plugin)
{

}

static void rail_process_receive(rdpSvcPlugin* plugin, wStream* s)
{
	railPlugin* rail = (railPlugin*) plugin;
	rail_order_recv(rail->rail_order, s);
	Stream_Free(s, TRUE);
}

static void rail_process_addin_args(rdpRailOrder* rail_order, rdpSettings* settings)
{
	char* exeOrFile;

	exeOrFile = settings->RemoteApplicationProgram;

	if (strlen(exeOrFile) >= 2)
	{
		if (strncmp(exeOrFile, "||", 2) != 0)
			rail_order->exec.flags |= RAIL_EXEC_FLAG_FILE;
	}

	rail_string_to_unicode_string(settings->RemoteApplicationProgram, &rail_order->exec.exeOrFile);
	rail_string_to_unicode_string(settings->ShellWorkingDirectory, &rail_order->exec.workingDir);
	rail_string_to_unicode_string(settings->RemoteApplicationCmdLine, &rail_order->exec.arguments);

	rail_send_client_exec_order(rail_order);
}

static void rail_recv_set_sysparams_event(rdpRailOrder* rail_order, wMessage* event)
{
	RAIL_SYSPARAM_ORDER* sysparam;

	/* Send System Parameters */

	sysparam = (RAIL_SYSPARAM_ORDER*) event->wParam;
	memmove(&rail_order->sysparam, sysparam, sizeof(RAIL_SYSPARAM_ORDER));

	rail_send_client_sysparams_order(rail_order);

	/* execute */

	rail_order->exec.flags = RAIL_EXEC_FLAG_EXPAND_ARGUMENTS;

	rail_process_addin_args(rail_order, rail_order->settings);
}

static void rail_recv_exec_remote_app_event(rdpRailOrder* rail_order, wMessage* event)
{
	/**
	 * TODO: replace event system by an API to allow the execution
	 * of multiple remote apps over the same connection. RAIL is
	 * always built-in, so clients can safely link to it.
	 */

	//rail_process_addin_args(rail_order, data);
}

static void rail_recv_activate_event(rdpRailOrder* rail_order, wMessage* event)
{
	RAIL_ACTIVATE_ORDER* activate = (RAIL_ACTIVATE_ORDER*) event->wParam;

	CopyMemory(&rail_order->activate, activate, sizeof(RAIL_ACTIVATE_ORDER));
	rail_send_client_activate_order(rail_order);
}

static void rail_recv_sysmenu_event(rdpRailOrder* rail_order, wMessage* event)
{
	RAIL_SYSMENU_ORDER* sysmenu = (RAIL_SYSMENU_ORDER*) event->wParam;

	CopyMemory(&rail_order->sysmenu, sysmenu, sizeof(RAIL_SYSMENU_ORDER));
	rail_send_client_sysmenu_order(rail_order);
}

static void rail_recv_syscommand_event(rdpRailOrder* rail_order, wMessage* event)
{
	RAIL_SYSCOMMAND_ORDER* syscommand = (RAIL_SYSCOMMAND_ORDER*) event->wParam;

	CopyMemory(&rail_order->syscommand, syscommand, sizeof(RAIL_SYSCOMMAND_ORDER));
	rail_send_client_syscommand_order(rail_order);
}

static void rail_recv_notify_event(rdpRailOrder* rail_order, wMessage* event)
{
	RAIL_NOTIFY_EVENT_ORDER* notify = (RAIL_NOTIFY_EVENT_ORDER*) event->wParam;

	CopyMemory(&rail_order->notify_event, notify, sizeof(RAIL_NOTIFY_EVENT_ORDER));
	rail_send_client_notify_event_order(rail_order);
}

static void rail_recv_window_move_event(rdpRailOrder* rail_order, wMessage* event)
{
	RAIL_WINDOW_MOVE_ORDER* window_move = (RAIL_WINDOW_MOVE_ORDER*) event->wParam;

	CopyMemory(&rail_order->window_move, window_move, sizeof(RAIL_WINDOW_MOVE_ORDER));
	rail_send_client_window_move_order(rail_order);
}

static void rail_recv_app_req_event(rdpRailOrder* rail_order, wMessage* event)
{
	RAIL_GET_APPID_REQ_ORDER* get_appid_req = (RAIL_GET_APPID_REQ_ORDER*) event->wParam;

	CopyMemory(&rail_order->get_appid_req, get_appid_req, sizeof(RAIL_GET_APPID_REQ_ORDER));
	rail_send_client_get_appid_req_order(rail_order);
}

static void rail_recv_langbarinfo_event(rdpRailOrder* rail_order, wMessage* event)
{
	RAIL_LANGBAR_INFO_ORDER* langbar_info = (RAIL_LANGBAR_INFO_ORDER*) event->wParam;

	CopyMemory(&rail_order->langbar_info, langbar_info, sizeof(RAIL_LANGBAR_INFO_ORDER));
	rail_send_client_langbar_info_order(rail_order);
}

static void rail_process_event(rdpSvcPlugin* plugin, wMessage* event)
{
	railPlugin* rail = NULL;
	rail = (railPlugin*) plugin;

	switch (GetMessageType(event->id))
	{
		case RailChannel_ClientSystemParam:
			rail_recv_set_sysparams_event(rail->rail_order, event);
			break;

		case RailChannel_ClientExecute:
			rail_recv_exec_remote_app_event(rail->rail_order, event);
			break;

		case RailChannel_ClientActivate:
			rail_recv_activate_event(rail->rail_order, event);
			break;

		case RailChannel_ClientSystemMenu:
			rail_recv_sysmenu_event(rail->rail_order, event);
			break;

		case RailChannel_ClientSystemCommand:
			rail_recv_syscommand_event(rail->rail_order, event);
			break;

		case RailChannel_ClientNotifyEvent:
			rail_recv_notify_event(rail->rail_order, event);
			break;

		case RailChannel_ClientWindowMove:
			rail_recv_window_move_event(rail->rail_order, event);
			break;

		case RailChannel_ClientGetAppIdRequest:
			rail_recv_app_req_event(rail->rail_order, event);
			break;

		case RailChannel_ClientLanguageBarInfo:
			rail_recv_langbarinfo_event(rail->rail_order, event);
			break;

		default:
			break;
	}

	freerdp_event_free(event);
}

/**
 * Callback Interface
 */

int rail_client_execute(RailClientContext* context, RAIL_EXEC_ORDER* exec)
{
	return 0;
}

int rail_client_activate(RailClientContext* context, RAIL_ACTIVATE_ORDER* activate)
{
	return 0;
}

int rail_get_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return 0;
}

int rail_client_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return 0;
}

int rail_server_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return 0;
}

int rail_client_system_command(RailClientContext* context, RAIL_SYSCOMMAND_ORDER* syscommand)
{
	return 0;
}

int rail_client_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	return 0;
}

int rail_server_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	return 0;
}

int rail_client_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	return 0;
}

int rail_server_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	return 0;
}

int rail_client_notify_event(RailClientContext* context, RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	return 0;
}

int rail_client_window_move(RailClientContext* context, RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	return 0;
}

int rail_server_local_move_size(RailClientContext* context, RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	return 0;
}

int rail_server_min_max_info(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	return 0;
}

int rail_client_information(RailClientContext* context, RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	return 0;
}

int rail_client_system_menu(RailClientContext* context, RAIL_SYSMENU_ORDER* sysmenu)
{
	return 0;
}

int rail_client_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	return 0;
}

int rail_server_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	return 0;
}

int rail_server_execute_result(RailClientContext* context, RAIL_EXEC_RESULT_ORDER* execResult)
{
	return 0;
}

int rail_client_get_appid_request(RailClientContext* context, RAIL_GET_APPID_REQ_ORDER* getAppIdReq)
{
	return 0;
}

int rail_server_get_appid_response(RailClientContext* context, RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	return 0;
}

/* rail is always built-in */
#define VirtualChannelEntry	rail_VirtualChannelEntry

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	railPlugin* _p;
	RailClientContext* context;
	CHANNEL_ENTRY_POINTS_EX* pEntryPointsEx;

	_p = (railPlugin*) malloc(sizeof(railPlugin));
	ZeroMemory(_p, sizeof(railPlugin));

	_p->plugin.channel_def.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(_p->plugin.channel_def.name, "rail");

	_p->plugin.connect_callback = rail_process_connect;
	_p->plugin.receive_callback = rail_process_receive;
	_p->plugin.event_callback = rail_process_event;
	_p->plugin.terminate_callback = rail_process_terminate;

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_EX)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (RailClientContext*) malloc(sizeof(RailClientContext));

		context->ClientExecute = rail_client_execute;
		context->ClientActivate = rail_client_activate;
		context->GetSystemParam = rail_get_system_param;
		context->ServerSystemParam = rail_server_system_param;
		context->ClientSystemCommand = rail_client_system_command;
		context->ClientHandshake = rail_client_handshake;
		context->ServerHandshake = rail_server_handshake;
		context->ClientNotifyEvent = rail_client_notify_event;
		context->ClientWindowMove = rail_client_window_move;
		context->ServerLocalMoveSize = rail_server_local_move_size;
		context->ServerMinMaxInfo = rail_server_min_max_info;
		context->ClientInformation = rail_client_information;
		context->ClientSystemMenu = rail_client_system_menu;
		context->ClientLanguageBarInfo = rail_client_language_bar_info;
		context->ServerLanguageBarInfo = rail_server_language_bar_info;
		context->ServerExecuteResult = rail_server_execute_result;
		context->ClientGetAppIdRequest = rail_client_get_appid_request;
		context->ServerGetAppIdResponse = rail_server_get_appid_response;

		*(pEntryPointsEx->ppInterface) = (void*) context;
	}

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}
