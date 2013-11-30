/*
 * WPA Supplicant / dbus-based control interface
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include <dbus/dbus.h>

#include "common.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "ctrl_iface_dbus.h"
#include "ctrl_iface_dbus_handlers.h"
#include "l2_packet.h"
#include "eap_methods.h"
#include "dbus_dict_helpers.h"
#include "wpa.h"


/**
 * wpas_dbus_new_invalid_opts_error - Return a new invalid options error message
 * @message: Pointer to incoming dbus message this error refers to
 * Returns: a dbus error message
 *
 * Convenience function to create and return an invalid options error
 */
static DBusMessage * wpas_dbus_new_invalid_opts_error(DBusMessage *message,
						      const char *arg)
{
	DBusMessage *reply;

	reply = dbus_message_new_error(message, WPAS_ERROR_INVALID_OPTS,
				      "Did not receive correct message "
				      "arguments.");
	if (arg != NULL)
		dbus_message_append_args(reply, DBUS_TYPE_STRING, &arg,
					 DBUS_TYPE_INVALID);

	return reply;
}


/**
 * wpas_dbus_new_success_reply - Return a new success reply message
 * @message: Pointer to incoming dbus message this reply refers to
 * Returns: a dbus message containing a single UINT32 that indicates
 *          success (ie, a value of 1)
 *
 * Convenience function to create and return a success reply message
 */
static DBusMessage * wpas_dbus_new_success_reply(DBusMessage *message)
{
	DBusMessage *reply;
	unsigned int success = 1;

	reply = dbus_message_new_method_return(message);
	dbus_message_append_args(reply, DBUS_TYPE_UINT32, &success,
				 DBUS_TYPE_INVALID);
	return reply;
}


static void wpas_dbus_free_wpa_interface(struct wpa_interface *iface)
{
	free((char *) iface->driver);
	free((char *) iface->driver_param);
	free((char *) iface->confname);
	free((char *) iface->bridge_ifname);
}


/**
 * wpas_dbus_global_add_interface - Request registration of a network interface
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: The object path of the new interface object,
 *          or a dbus error message with more information
 *
 * Handler function for "addInterface" method call. Handles requests
 * by dbus clients to register a network interface that wpa_supplicant
 * will manage.
 */
DBusMessage * wpas_dbus_global_add_interface(DBusMessage *message,
					     struct wpa_global *global)
{
	struct wpa_interface iface;
	char *ifname = NULL;
	DBusMessage *reply = NULL;
	DBusMessageIter iter;

	memset(&iface, 0, sizeof(iface));

	dbus_message_iter_init(message, &iter);

	/* First argument: interface name (DBUS_TYPE_STRING)
	 *    Required; must be non-zero length
	 */
	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		goto error;
	dbus_message_iter_get_basic(&iter, &ifname);
	if (!strlen(ifname))
		goto error;
	iface.ifname = ifname;

	/* Second argument: dict of options */
	if (dbus_message_iter_next(&iter)) {
		DBusMessageIter iter_dict;
		struct wpa_dbus_dict_entry entry;

		if (!wpa_dbus_dict_open_read(&iter, &iter_dict))
			goto error;
		while (wpa_dbus_dict_has_dict_entry(&iter_dict)) {
			if (!wpa_dbus_dict_get_entry(&iter_dict, &entry))
				goto error;
			if (!strcmp(entry.key, "driver") &&
			    (entry.type == DBUS_TYPE_STRING)) {
				iface.driver = strdup(entry.str_value);
				if (iface.driver == NULL)
					goto error;
			} else if (!strcmp(entry.key, "driver-params") &&
				   (entry.type == DBUS_TYPE_STRING)) {
				iface.driver_param = strdup(entry.str_value);
				if (iface.driver_param == NULL)
					goto error;
			} else if (!strcmp(entry.key, "config-file") &&
				   (entry.type == DBUS_TYPE_STRING)) {
				iface.confname = strdup(entry.str_value);
				if (iface.confname == NULL)
					goto error;
			} else if (!strcmp(entry.key, "bridge-ifname") &&
				   (entry.type == DBUS_TYPE_STRING)) {
				iface.bridge_ifname = strdup(entry.str_value);
				if (iface.bridge_ifname == NULL)
					goto error;
			} else {
				wpa_dbus_dict_entry_clear(&entry);
				goto error;
			}
			wpa_dbus_dict_entry_clear(&entry);
		}
	}

	/*
	 * Try to get the wpa_supplicant record for this iface, return
	 * an error if we already control it.
	 */
	if (wpa_supplicant_get_iface(global, iface.ifname) != 0) {
		reply = dbus_message_new_error(message,
					       WPAS_ERROR_EXISTS_ERROR,
					       "wpa_supplicant already "
					       "controls this interface.");
	} else {
		struct wpa_supplicant *wpa_s;
		/* Otherwise, have wpa_supplicant attach to it. */
		if ((wpa_s = wpa_supplicant_add_iface(global, &iface))) {
			const char *path = wpa_supplicant_get_dbus_path(wpa_s);
			reply = dbus_message_new_method_return(message);
			dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH,
			                         &path, DBUS_TYPE_INVALID);
		} else {
			reply = dbus_message_new_error(message,
						       WPAS_ERROR_ADD_ERROR,
						       "wpa_supplicant "
						       "couldn't grab this "
						       "interface.");
		}
	}
	wpas_dbus_free_wpa_interface(&iface);
	return reply;

error:
	wpas_dbus_free_wpa_interface(&iface);
	return wpas_dbus_new_invalid_opts_error(message, NULL);
}


/**
 * wpas_dbus_global_remove_interface - Request deregistration of an interface
 * @message: Pointer to incoming dbus message
 * @global: wpa_supplicant global data structure
 * Returns: a dbus message containing a UINT32 indicating success (1) or
 *          failure (0), or returns a dbus error message with more information
 *
 * Handler function for "removeInterface" method call.  Handles requests
 * by dbus clients to deregister a network interface that wpa_supplicant
 * currently manages.
 */
DBusMessage * wpas_dbus_global_remove_interface(DBusMessage *message,
						struct wpa_global *global)
{
	struct wpa_supplicant *wpa_s;
	char *path;
	DBusMessage *reply = NULL;

	if (!dbus_message_get_args(message, NULL,
				   DBUS_TYPE_OBJECT_PATH, &path,
				   DBUS_TYPE_INVALID)) {
		reply = wpas_dbus_new_invalid_opts_error(message, NULL);
		goto out;
	}

	wpa_s = wpa_supplicant_get_iface_by_dbus_path(global, path);
	if (wpa_s == NULL) {
		reply = wpas_dbus_new_invalid_iface_error(message);
		goto out;
	}

	if (!wpa_supplicant_remove_iface(global, wpa_s)) {
		reply = wpas_dbus_new_success_reply(message);
	} else {
		reply = dbus_message_new_error(message,
					       WPAS_ERROR_REMOVE_ERROR,
					       "wpa_supplicant couldn't "
					       "remove this interface.");
	}

out:
	return reply;
}


/**
 * wpas_dbus_global_get_interface - Get the object path for an interface name
 * @message: Pointer to incoming dbus message
 * @global: %wpa_supplicant global data structure
 * Returns: The object path of the interface object,
 *          or a dbus error message with more information
 *
 * Handler function for "getInterface" method call. Handles requests
 * by dbus clients for the object path of an specific network interface.
 */
DBusMessage * wpas_dbus_global_get_interface(DBusMessage *message,
					     struct wpa_global *global)
{
	DBusMessage *reply = NULL;
	const char *ifname;
	const char *path;
	struct wpa_supplicant *wpa_s;

	if (!dbus_message_get_args(message, NULL,
	                           DBUS_TYPE_STRING, &ifname,
	                           DBUS_TYPE_INVALID)) {
		reply = wpas_dbus_new_invalid_opts_error(message, NULL);
		goto out;
	}

	wpa_s = wpa_supplicant_get_iface(global, ifname);
	if (wpa_s == NULL) {
		reply = wpas_dbus_new_invalid_iface_error(message);
		goto out;
	}

	path = wpa_supplicant_get_dbus_path(wpa_s);
	if (path == NULL) {
		reply = dbus_message_new_error(message,
		                               WPAS_ERROR_INTERNAL_ERROR,
		                               "an internal error occurred "
		                               "getting the interface.");
		goto out;
	}

	reply = dbus_message_new_method_return(message);
	dbus_message_append_args(reply,
	                         DBUS_TYPE_OBJECT_PATH, &path,
	                         DBUS_TYPE_INVALID);

out:
	return reply;
}


/**
 * wpas_dbus_iface_scan - Request a wireless scan on an interface
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: a dbus message containing a UINT32 indicating success (1) or
 *          failure (0)
 *
 * Handler function for "scan" method call of a network device. Requests
 * that wpa_supplicant perform a wireless scan as soon as possible
 * on a particular wireless interface.
 */
DBusMessage * wpas_dbus_iface_scan(DBusMessage *message,
				   struct wpa_supplicant *wpa_s)
{
	wpa_s->scan_req = 2;
	wpa_supplicant_req_scan(wpa_s, 0, 0);
	return wpas_dbus_new_success_reply(message);
}


/**
 * wpas_dbus_iface_scan_results - Get the results of a recent scan request
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: a dbus message containing a dbus array of objects paths, or returns
 *          a dbus error message if not scan results could be found
 *
 * Handler function for "scanResults" method call of a network device. Returns
 * a dbus message containing the object paths of wireless networks found.
 */
DBusMessage * wpas_dbus_iface_scan_results(DBusMessage *message,
					   struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	DBusMessageIter iter;
	DBusMessageIter sub_iter;
	int i;

	/* Ensure we've actually got scan results to return */
	if (wpa_s->scan_results == NULL &&
	    wpa_supplicant_get_scan_results(wpa_s) < 0) {
		reply = dbus_message_new_error(message, WPAS_ERROR_SCAN_ERROR,
					       "An error ocurred getting scan "
					       "results.");
		goto out;
	}

	/* Create and initialize the return message */
	reply = dbus_message_new_method_return(message);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					 DBUS_TYPE_OBJECT_PATH_AS_STRING,
					 &sub_iter);

	/* Loop through scan results and append each result's object path */
	for (i = 0; i < wpa_s->num_scan_results; i++) {
		struct wpa_scan_result *res = &wpa_s->scan_results[i];
		char *path;

		path = wpa_zalloc(WPAS_DBUS_OBJECT_PATH_MAX);
		if (path == NULL) {
			perror("wpas_dbus_iface_scan_results[dbus]: out of "
			       "memory.");
			wpa_printf(MSG_ERROR, "dbus control interface: not "
				   "enough memory to send scan results "
				   "signal.");
			break;
		}
		/* Construct the object path for this network.  Note that ':'
		 * is not a valid character in dbus object paths.
		 */
		snprintf(path, WPAS_DBUS_OBJECT_PATH_MAX,
			 "%s/" WPAS_DBUS_BSSIDS_PART "/"
			 WPAS_DBUS_BSSID_FORMAT,
			 wpa_supplicant_get_dbus_path(wpa_s),
			 MAC2STR(res->bssid));
		dbus_message_iter_append_basic(&sub_iter,
					       DBUS_TYPE_OBJECT_PATH, &path);
		free(path);
	}

	dbus_message_iter_close_container(&iter, &sub_iter);

out:
	return reply;
}


/**
 * wpas_dbus_bssid_properties - Return the properties of a scanned network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * @res: wpa_supplicant scan result for which to get properties
 * Returns: a dbus message containing the properties for the requested network
 *
 * Handler function for "properties" method call of a scanned network.
 * Returns a dbus message containing the the properties.
 */
DBusMessage * wpas_dbus_bssid_properties(DBusMessage *message,
					 struct wpa_supplicant *wpa_s,
					 struct wpa_scan_result *res)
{
	DBusMessage *reply = NULL;
	char *bssid_data, *ssid_data, *wpa_ie_data, *rsn_ie_data;
	DBusMessageIter iter, iter_dict;

	/* dbus needs the address of a pointer to the actual value
	 * for array types, not the address of the value itself.
	 */
	bssid_data = (char *) &res->bssid;
	ssid_data = (char *) &res->ssid;
	wpa_ie_data = (char *) &res->wpa_ie;
	rsn_ie_data = (char *) &res->rsn_ie;

	/* Dump the properties into a dbus message */
	reply = dbus_message_new_method_return(message);

	dbus_message_iter_init_append(reply, &iter);
	if (!wpa_dbus_dict_open_write(&iter, &iter_dict))
		goto error;

	if (!wpa_dbus_dict_append_byte_array(&iter_dict, "bssid",
					     bssid_data, ETH_ALEN))
		goto error;
	if (!wpa_dbus_dict_append_byte_array(&iter_dict, "ssid",
					     ssid_data, res->ssid_len))
		goto error;
	if (res->wpa_ie_len) {
		if (!wpa_dbus_dict_append_byte_array(&iter_dict, "wpaie",
						     wpa_ie_data,
						     res->wpa_ie_len)) {
			goto error;
		}
	}
	if (res->rsn_ie_len) {
		if (!wpa_dbus_dict_append_byte_array(&iter_dict, "rsnie",
						     rsn_ie_data,
						     res->rsn_ie_len)) {
			goto error;
		}
	}
	if (res->freq) {
		if (!wpa_dbus_dict_append_int32(&iter_dict, "frequency",
						res->freq))
			goto error;
	}
	if (!wpa_dbus_dict_append_uint16(&iter_dict, "capabilities",
					 res->caps))
		goto error;
	if (!wpa_dbus_dict_append_int32(&iter_dict, "quality", res->qual))
		goto error;
	if (!wpa_dbus_dict_append_int32(&iter_dict, "noise", res->noise))
		goto error;
	if (!wpa_dbus_dict_append_int32(&iter_dict, "level", res->level))
		goto error;
	if (!wpa_dbus_dict_append_int32(&iter_dict, "maxrate", res->maxrate))
		goto error;

	if (!wpa_dbus_dict_close_write(&iter, &iter_dict))
		goto error;

	return reply;

error:
	if (reply)
		dbus_message_unref(reply);
	return dbus_message_new_error(message, WPAS_ERROR_INTERNAL_ERROR,
				      "an internal error occurred returning "
				      "BSSID properties.");
}


/**
 * wpas_dbus_iface_capabilities - Return interface capabilities
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a dict of strings
 *
 * Handler function for "capabilities" method call of an interface.
 */
DBusMessage * wpas_dbus_iface_capabilities(DBusMessage *message,
					   struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	struct wpa_driver_capa capa;
	int res;
	DBusMessageIter iter, iter_dict;
	char **eap_methods;
	size_t num_items;
	dbus_bool_t strict = FALSE;
	DBusMessageIter iter_dict_entry, iter_dict_val, iter_array;

	if (!dbus_message_get_args(message, NULL,
				   DBUS_TYPE_BOOLEAN, &strict,
				   DBUS_TYPE_INVALID))
		strict = FALSE;

	reply = dbus_message_new_method_return(message);

	dbus_message_iter_init_append(reply, &iter);
	if (!wpa_dbus_dict_open_write(&iter, &iter_dict))
		goto error;

	/* EAP methods */
	eap_methods = eap_get_names_as_string_array(&num_items);
	if (eap_methods) {
		dbus_bool_t success = FALSE;
		size_t i = 0;

		success = wpa_dbus_dict_append_string_array(
			&iter_dict, "eap", (const char **) eap_methods,
			num_items);

		/* free returned method array */
		while (eap_methods[i])
			free(eap_methods[i++]);
		free(eap_methods);

		if (!success)
			goto error;
	}

	res = wpa_drv_get_capa(wpa_s, &capa);

	/***** pairwise cipher */
	if (res < 0) {
		if (!strict) {
			const char *args[] = {"CCMP", "TKIP", "NONE"};
			if (!wpa_dbus_dict_append_string_array(
				    &iter_dict, "pairwise", args,
				    sizeof(args) / sizeof(char*)))
				goto error;
		}
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "pairwise",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto error;

		if (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "CCMP"))
				goto error;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_TKIP) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "TKIP"))
				goto error;
		}

		if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "NONE"))
				goto error;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto error;
	}

	/***** group cipher */
	if (res < 0) {
		if (!strict) {
			const char *args[] = {
				"CCMP", "TKIP", "WEP104", "WEP40"
			};
			if (!wpa_dbus_dict_append_string_array(
				    &iter_dict, "group", args,
				    sizeof(args) / sizeof(char*)))
				goto error;
		}
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "group",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto error;

		if (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "CCMP"))
				goto error;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_TKIP) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "TKIP"))
				goto error;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_WEP104) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "WEP104"))
				goto error;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_WEP40) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "WEP40"))
				goto error;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto error;
	}

	/***** key management */
	if (res < 0) {
		if (!strict) {
			const char *args[] = {
				"WPA-PSK", "WPA-EAP", "IEEE8021X", "WPA-NONE",
				"NONE"
			};
			if (!wpa_dbus_dict_append_string_array(
				    &iter_dict, "key_mgmt", args,
				    sizeof(args) / sizeof(char*)))
				goto error;
		}
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "key_mgmt",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto error;

		if (!wpa_dbus_dict_string_array_add_element(&iter_array,
							    "NONE"))
			goto error;

		if (!wpa_dbus_dict_string_array_add_element(&iter_array,
							    "IEEE8021X"))
			goto error;

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "WPA-EAP"))
				goto error;
		}

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "WPA-PSK"))
				goto error;
		}

		if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "WPA-NONE"))
				goto error;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto error;
	}

	/***** WPA protocol */
	if (res < 0) {
		if (!strict) {
			const char *args[] = { "RSN", "WPA" };
			if (!wpa_dbus_dict_append_string_array(
				    &iter_dict, "proto", args,
				    sizeof(args) / sizeof(char*)))
				goto error;
		}
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "proto",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto error;

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "RSN"))
				goto error;
		}

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "WPA"))
				goto error;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto error;
	}

	/***** auth alg */
	if (res < 0) {
		if (!strict) {
			const char *args[] = { "OPEN", "SHARED", "LEAP" };
			if (!wpa_dbus_dict_append_string_array(
				    &iter_dict, "auth_alg", args,
				    sizeof(args) / sizeof(char*)))
				goto error;
		}
	} else {
		if (!wpa_dbus_dict_begin_string_array(&iter_dict, "auth_alg",
						      &iter_dict_entry,
						      &iter_dict_val,
						      &iter_array))
			goto error;

		if (capa.auth & (WPA_DRIVER_AUTH_OPEN)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "OPEN"))
				goto error;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_SHARED)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "SHARED"))
				goto error;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_LEAP)) {
			if (!wpa_dbus_dict_string_array_add_element(
				    &iter_array, "LEAP"))
				goto error;
		}

		if (!wpa_dbus_dict_end_string_array(&iter_dict,
						    &iter_dict_entry,
						    &iter_dict_val,
						    &iter_array))
			goto error;
	}

	if (!wpa_dbus_dict_close_write(&iter, &iter_dict))
		goto error;

	return reply;

error:
	if (reply)
		dbus_message_unref(reply);
	return dbus_message_new_error(message, WPAS_ERROR_INTERNAL_ERROR,
				      "an internal error occurred returning "
				      "interface capabilities.");
}


/**
 * wpas_dbus_iface_add_network - Add a new configured network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing the object path of the new network
 *
 * Handler function for "addNetwork" method call of a network interface.
 */
DBusMessage * wpas_dbus_iface_add_network(DBusMessage *message,
					  struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	struct wpa_ssid *ssid;
	char *path = NULL;

	path = wpa_zalloc(WPAS_DBUS_OBJECT_PATH_MAX);
	if (path == NULL) {
		perror("wpas_dbus_iface_scan_results[dbus]: out of "
		       "memory.");
		wpa_printf(MSG_ERROR, "dbus control interface: not "
			   "enough memory to send scan results "
			   "signal.");
		goto out;
	}

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL) {
		reply = dbus_message_new_error(message,
					       WPAS_ERROR_ADD_NETWORK_ERROR,
					       "wpa_supplicant could not add "
					       "a network on this interface.");
		goto out;
	}
	ssid->disabled = 1;
	wpa_config_set_network_defaults(ssid);

	/* Construct the object path for this network. */
	snprintf(path, WPAS_DBUS_OBJECT_PATH_MAX,
		 "%s/" WPAS_DBUS_NETWORKS_PART "/%d",
		 wpa_supplicant_get_dbus_path(wpa_s),
		 ssid->id);

	reply = dbus_message_new_method_return(message);
	dbus_message_append_args(reply, DBUS_TYPE_OBJECT_PATH,
				 &path, DBUS_TYPE_INVALID);

out:
	free(path);
	return reply;
}


/**
 * wpas_dbus_iface_remove_network - Remove a configured network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a UINT32 indicating success (1) or
 *          failure (0)
 *
 * Handler function for "removeNetwork" method call of a network interface.
 */
DBusMessage * wpas_dbus_iface_remove_network(DBusMessage *message,
					     struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	const char *op;
	char *iface = NULL, *net_id = NULL;
	int id;
	struct wpa_ssid *ssid;

	if (!dbus_message_get_args(message, NULL,
	                           DBUS_TYPE_OBJECT_PATH, &op,
	                           DBUS_TYPE_INVALID)) {
		reply = wpas_dbus_new_invalid_opts_error(message, NULL);
		goto out;
	}

	/* Extract the network ID */
	iface = wpas_dbus_decompose_object_path(op, &net_id, NULL);
	if (iface == NULL) {
		reply = wpas_dbus_new_invalid_network_error(message);
		goto out;
	}
	/* Ensure the network is actually a child of this interface */
	if (strcmp(iface, wpa_supplicant_get_dbus_path(wpa_s)) != 0) {
		reply = wpas_dbus_new_invalid_network_error(message);
		goto out;
	}

	id = strtoul(net_id, NULL, 10);
	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		reply = wpas_dbus_new_invalid_network_error(message);
		goto out;
	}

	if (wpa_config_remove_network(wpa_s->conf, id) < 0) {
		reply = dbus_message_new_error(message,
					       WPAS_ERROR_REMOVE_NETWORK_ERROR,
					       "error removing the specified "
					       "on this interface.");
		goto out;
	}

	if (ssid == wpa_s->current_ssid)
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
	reply = wpas_dbus_new_success_reply(message);

out:
	free(iface);
	free(net_id);
	return reply;
}


static const char *dont_quote[] = {
	"key_mgmt", "proto", "pairwise", "auth_alg", "group", "eap",
	"opensc_engine_path", "pkcs11_engine_path", "pkcs11_module_path",
	"bssid", NULL
};

static dbus_bool_t should_quote_opt(const char *key)
{
	int i = 0;
	while (dont_quote[i] != NULL) {
		if (strcmp(key, dont_quote[i]) == 0)
			return FALSE;
		i++;
	}
	return TRUE;
}

/**
 * wpas_dbus_iface_set_network - Set options for a configured network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * @ssid: wpa_ssid structure for a configured network
 * Returns: a dbus message containing a UINT32 indicating success (1) or
 *          failure (0)
 *
 * Handler function for "set" method call of a configured network.
 */
DBusMessage * wpas_dbus_iface_set_network(DBusMessage *message,
					  struct wpa_supplicant *wpa_s,
					  struct wpa_ssid *ssid)
{
	DBusMessage *reply = NULL;
	struct wpa_dbus_dict_entry entry = { .type = DBUS_TYPE_STRING };
	DBusMessageIter	iter, iter_dict;

	dbus_message_iter_init(message, &iter);

	if (!wpa_dbus_dict_open_read(&iter, &iter_dict)) {
		reply = wpas_dbus_new_invalid_opts_error(message, NULL);
		goto out;
	}

	while (wpa_dbus_dict_has_dict_entry(&iter_dict)) {
		char *value = NULL;
		size_t size = 50;
		int ret;

		if (!wpa_dbus_dict_get_entry(&iter_dict, &entry)) {
			reply = wpas_dbus_new_invalid_opts_error(message,
								 NULL);
			goto out;
		}

		/* Type conversions, since wpa_supplicant wants strings */
		if (entry.type == DBUS_TYPE_ARRAY &&
		    entry.array_type == DBUS_TYPE_BYTE) {
			if (entry.array_len <= 0)
				goto error;

			size = entry.array_len * 2 + 1;
			value = wpa_zalloc(size);
			if (value == NULL)
				goto error;
			ret = wpa_snprintf_hex(value, size,
					(u8 *) entry.bytearray_value,
					entry.array_len);
			if (ret <= 0)
				goto error;
		} else if (entry.type == DBUS_TYPE_STRING) {
			if (should_quote_opt(entry.key)) {
				size = strlen(entry.str_value);
				/* Zero-length option check */
				if (size <= 0)
					goto error;
				size += 3;  /* For quotes and terminator */
				value = wpa_zalloc(size);
				if (value == NULL)
					goto error;
				ret = snprintf(value, size, "\"%s\"",
						entry.str_value);
				if (ret < 0 || (size_t) ret != (size - 1))
					goto error;
			} else {
				value = strdup(entry.str_value);
				if (value == NULL)
					goto error;
			}
		} else if (entry.type == DBUS_TYPE_UINT32) {
			value = wpa_zalloc(size);
			if (value == NULL)
				goto error;
			ret = snprintf(value, size, "%u", entry.uint32_value);
			if (ret <= 0)
				goto error;
		} else if (entry.type == DBUS_TYPE_INT32) {
			value = wpa_zalloc(size);
			if (value == NULL)
				goto error;
			ret = snprintf(value, size, "%d", entry.int32_value);
			if (ret <= 0)
				goto error;
		} else
			goto error;

		if (wpa_config_set(ssid, entry.key, value, 0) < 0)
			goto error;

		if ((strcmp(entry.key, "psk") == 0 &&
		     value[0] == '"' && ssid->ssid_len) ||
		    (strcmp(entry.key, "ssid") == 0 && ssid->passphrase))
			wpa_config_update_psk(ssid);

		free(value);
		wpa_dbus_dict_entry_clear(&entry);
		continue;

	error:
		free(value);
		reply = wpas_dbus_new_invalid_opts_error(message, entry.key);
		wpa_dbus_dict_entry_clear(&entry);
		break;
	}

	if (!reply)
		reply = wpas_dbus_new_success_reply(message);

out:
	return reply;
}


/**
 * wpas_dbus_iface_enable_network - Mark a configured network as enabled
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * @ssid: wpa_ssid structure for a configured network
 * Returns: A dbus message containing a UINT32 indicating success (1) or
 *          failure (0)
 *
 * Handler function for "enable" method call of a configured network.
 */
DBusMessage * wpas_dbus_iface_enable_network(DBusMessage *message,
					     struct wpa_supplicant *wpa_s,
					     struct wpa_ssid *ssid)
{
	if (wpa_s->current_ssid == NULL && ssid->disabled) {
		/*
		 * Try to reassociate since there is no current configuration
		 * and a new network was made available.
		 */
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
	ssid->disabled = 0;

	return wpas_dbus_new_success_reply(message);
}


/**
 * wpas_dbus_iface_disable_network - Mark a configured network as disabled
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * @ssid: wpa_ssid structure for a configured network
 * Returns: A dbus message containing a UINT32 indicating success (1) or
 *          failure (0)
 *
 * Handler function for "disable" method call of a configured network.
 */
DBusMessage * wpas_dbus_iface_disable_network(DBusMessage *message,
					      struct wpa_supplicant *wpa_s,
					      struct wpa_ssid *ssid)
{
	if (ssid == wpa_s->current_ssid)
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
	ssid->disabled = 1;

	return wpas_dbus_new_success_reply(message);
}


/**
 * wpas_dbus_iface_select_network - Attempt association with a configured network
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a UINT32 indicating success (1) or
 *          failure (0)
 *
 * Handler function for "selectNetwork" method call of network interface.
 */
DBusMessage * wpas_dbus_iface_select_network(DBusMessage *message,
					     struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	const char *op;
	struct wpa_ssid *ssid;
	char *iface_obj_path = NULL;
	char *network = NULL;

	if (strlen(dbus_message_get_signature(message)) == 0) {
		/* Any network */
		ssid = wpa_s->conf->ssid;
		while (ssid) {
			ssid->disabled = 0;
			ssid = ssid->next;
		}
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	} else {
		const char *obj_path;
		int nid;

		if (!dbus_message_get_args(message, NULL,
					   DBUS_TYPE_OBJECT_PATH, &op,
					   DBUS_TYPE_INVALID)) {
			reply = wpas_dbus_new_invalid_opts_error(message,
								 NULL);
			goto out;
		}

		/* Extract the network number */
		iface_obj_path = wpas_dbus_decompose_object_path(op,
								 &network,
								 NULL);
		if (iface_obj_path == NULL) {
			reply = wpas_dbus_new_invalid_iface_error(message);
			goto out;
		}
		/* Ensure the object path really points to this interface */
		obj_path = wpa_supplicant_get_dbus_path(wpa_s);
		if (strcmp(iface_obj_path, obj_path) != 0) {
			reply = wpas_dbus_new_invalid_network_error(message);
			goto out;
		}

		nid = strtoul(network, NULL, 10);
		if (errno == EINVAL) {
			reply = wpas_dbus_new_invalid_network_error(message);
			goto out;
		}

		ssid = wpa_config_get_network(wpa_s->conf, nid);
		if (ssid == NULL) {
			reply = wpas_dbus_new_invalid_network_error(message);
			goto out;
		}

		/* Finally, associate with the network */
		if (ssid != wpa_s->current_ssid && wpa_s->current_ssid)
			wpa_supplicant_disassociate(wpa_s,
						    REASON_DEAUTH_LEAVING);

		/* Mark all other networks disabled and trigger reassociation
		 */
		ssid = wpa_s->conf->ssid;
		while (ssid) {
			ssid->disabled = (nid != ssid->id);
			ssid = ssid->next;
		}
		wpa_s->disconnected = 0;
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}

	reply = wpas_dbus_new_success_reply(message);

out:
	free(iface_obj_path);
	free(network);
	return reply;
}


/**
 * wpas_dbus_iface_disconnect - Terminate the current connection
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a UINT32 indicating success (1) or
 *          failure (0)
 *
 * Handler function for "disconnect" method call of network interface.
 */
DBusMessage * wpas_dbus_iface_disconnect(DBusMessage *message,
					 struct wpa_supplicant *wpa_s)
{
	wpa_s->disconnected = 1;
	wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);

	return wpas_dbus_new_success_reply(message);
}


/**
 * wpas_dbus_iface_set_ap_scan - Control roaming mode
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a UINT32 indicating success (1) or
 *          failure (0)
 *
 * Handler function for "setAPScan" method call.
 */
DBusMessage * wpas_dbus_iface_set_ap_scan(DBusMessage *message,
					  struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	dbus_uint32_t ap_scan = 1;

	if (!dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32, &ap_scan,
				   DBUS_TYPE_INVALID)) {
		reply = wpas_dbus_new_invalid_opts_error(message, NULL);
		goto out;
	}

	if (ap_scan > 2) {
		reply = wpas_dbus_new_invalid_opts_error(message, NULL);
		goto out;
	}
	wpa_s->conf->ap_scan = ap_scan;
	reply = wpas_dbus_new_success_reply(message);

out:
	return reply;
}


/**
 * wpas_dbus_iface_get_state - Get interface state
 * @message: Pointer to incoming dbus message
 * @wpa_s: wpa_supplicant structure for a network interface
 * Returns: A dbus message containing a STRING representing the current
 *          interface state
 *
 * Handler function for "state" method call.
 */
DBusMessage * wpas_dbus_iface_get_state(DBusMessage *message,
					struct wpa_supplicant *wpa_s)
{
	DBusMessage *reply = NULL;
	const char *str_state;

	reply = dbus_message_new_method_return(message);
	if (reply != NULL) {
		str_state = wpa_supplicant_state_txt(wpa_s->wpa_state);
		dbus_message_append_args(reply, DBUS_TYPE_STRING, &str_state,
					 DBUS_TYPE_INVALID);
	}

	return reply;
}
