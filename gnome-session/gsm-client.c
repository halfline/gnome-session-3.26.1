/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Novell, Inc.
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gsm-client.h"
#include "org.gnome.SessionManager.Client.h"

static guint32 client_serial = 1;

#define GSM_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSM_TYPE_CLIENT, GsmClientPrivate))

struct GsmClientPrivate
{
        char            *id;
        char            *startup_id;
        char            *app_id;
        guint            status;
        GsmExportedClient *skeleton;
        GDBusConnection *connection;
};

enum {
        PROP_0,
        PROP_ID,
        PROP_STARTUP_ID,
        PROP_APP_ID,
        PROP_STATUS
};

enum {
        DISCONNECTED,
        END_SESSION_RESPONSE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE (GsmClient, gsm_client, G_TYPE_OBJECT)

#define GSM_CLIENT_DBUS_IFACE "org.gnome.SessionManager.Client"

static const GDBusErrorEntry gsm_client_error_entries[] = {
        { GSM_CLIENT_ERROR_GENERAL, GSM_CLIENT_DBUS_IFACE ".GeneralError" },
        { GSM_CLIENT_ERROR_NOT_REGISTERED, GSM_CLIENT_DBUS_IFACE ".NotRegistered" }
};

GQuark
gsm_client_error_quark (void)
{
        static volatile gsize quark_volatile = 0;

        g_dbus_error_register_error_domain ("gsm_client_error",
                                            &quark_volatile,
                                            gsm_client_error_entries,
                                            G_N_ELEMENTS (gsm_client_error_entries));
        return quark_volatile;
}

static guint32
get_next_client_serial (void)
{
        guint32 serial;

        serial = client_serial++;

        if ((gint32)client_serial < 0) {
                client_serial = 1;
        }

        return serial;
}

static gboolean
gsm_client_get_startup_id (GsmExportedClient     *skeleton,
                           GDBusMethodInvocation *invocation,
                           GsmClient             *client)
{
        gsm_exported_client_complete_get_startup_id (skeleton, invocation, client->priv->startup_id);
        return TRUE;
}

static gboolean
gsm_client_get_app_id (GsmExportedClient     *skeleton,
                       GDBusMethodInvocation *invocation,
                       GsmClient             *client)
{
        gsm_exported_client_complete_get_app_id (skeleton, invocation, client->priv->app_id);
        return TRUE;
}

static gboolean
gsm_client_get_restart_style_hint (GsmExportedClient     *skeleton,
                                   GDBusMethodInvocation *invocation,
                                   GsmClient             *client)
{
        guint hint;

        hint = GSM_CLIENT_GET_CLASS (client)->impl_get_restart_style_hint (client);
        gsm_exported_client_complete_get_restart_style_hint (skeleton, invocation, hint);
        return TRUE;
}

static gboolean
gsm_client_get_status (GsmExportedClient     *skeleton,
                       GDBusMethodInvocation *invocation,
                       GsmClient             *client)
{
        gsm_exported_client_complete_get_status (skeleton, invocation, client->priv->status);
        return TRUE;
}

static gboolean
gsm_client_get_unix_process_id (GsmExportedClient     *skeleton,
                                GDBusMethodInvocation *invocation,
                                GsmClient             *client)
{
        guint pid;

        pid = GSM_CLIENT_GET_CLASS (client)->impl_get_unix_process_id (client);
        gsm_exported_client_complete_get_unix_process_id (skeleton, invocation, pid);
        return TRUE;
}

static gboolean
gsm_client_stop_dbus (GsmExportedClient     *skeleton,
                      GDBusMethodInvocation *invocation,
                      GsmClient             *client)
{
        GError *error = NULL;
        gsm_client_stop (client, &error);

        if (error != NULL) {
                g_dbus_method_invocation_take_error (invocation, error);
        } else {
                gsm_exported_client_complete_stop (skeleton, invocation);
        }

        return TRUE;
}

static gboolean
register_client (GsmClient *client)
{
        GError *error = NULL;
        GsmExportedClient *skeleton;

        client->priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
        if (client->priv->connection == NULL) {
                g_critical ("error getting session bus: %s", error->message);
                g_error_free (error);
                return FALSE;
        }

        skeleton = gsm_exported_client_skeleton_new ();
        client->priv->skeleton = skeleton;
        g_debug ("exporting client to object path: %s", client->priv->id);
        g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
                                          client->priv->connection,
                                          client->priv->id, &error);

        if (error != NULL) {
                g_critical ("error exporting client on session bus: %s", error->message);
                g_error_free (error);
                return FALSE;
        }

        g_signal_connect (skeleton, "handle-get-app-id",
                          G_CALLBACK (gsm_client_get_app_id), client);
        g_signal_connect (skeleton, "handle-get-restart-style-hint",
                          G_CALLBACK (gsm_client_get_restart_style_hint), client);
        g_signal_connect (skeleton, "handle-get-startup-id",
                          G_CALLBACK (gsm_client_get_startup_id), client);
        g_signal_connect (skeleton, "handle-get-status",
                          G_CALLBACK (gsm_client_get_status), client);
        g_signal_connect (skeleton, "handle-get-unix-process-id",
                          G_CALLBACK (gsm_client_get_unix_process_id), client);
        g_signal_connect (skeleton, "handle-stop",
                          G_CALLBACK (gsm_client_stop_dbus), client);

        return TRUE;
}

static GObject *
gsm_client_constructor (GType                  type,
                        guint                  n_construct_properties,
                        GObjectConstructParam *construct_properties)
{
        GsmClient *client;
        gboolean   res;

        client = GSM_CLIENT (G_OBJECT_CLASS (gsm_client_parent_class)->constructor (type,
                                                                                    n_construct_properties,
                                                                                    construct_properties));

        g_free (client->priv->id);
        client->priv->id = g_strdup_printf ("/org/gnome/SessionManager/Client%u", get_next_client_serial ());

        res = register_client (client);
        if (! res) {
                g_warning ("Unable to register client with session bus");
        }

        return G_OBJECT (client);
}

static void
gsm_client_init (GsmClient *client)
{
        client->priv = GSM_CLIENT_GET_PRIVATE (client);
}

static void
gsm_client_finalize (GObject *object)
{
        GsmClient *client;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSM_IS_CLIENT (object));

        client = GSM_CLIENT (object);

        g_return_if_fail (client->priv != NULL);

        g_free (client->priv->id);
        g_free (client->priv->startup_id);
        g_free (client->priv->app_id);

        if (client->priv->skeleton != NULL) {
                g_dbus_interface_skeleton_unexport_from_connection (G_DBUS_INTERFACE_SKELETON (client->priv->skeleton),
                                                                    client->priv->connection);
                g_clear_object (&client->priv->skeleton);
        }

        g_clear_object (&client->priv->connection);

        G_OBJECT_CLASS (gsm_client_parent_class)->finalize (object);
}

void
gsm_client_set_status (GsmClient *client,
                       guint      status)
{
        g_return_if_fail (GSM_IS_CLIENT (client));
        if (client->priv->status != status) {
                client->priv->status = status;
                g_object_notify (G_OBJECT (client), "status");
        }
}

static void
gsm_client_set_startup_id (GsmClient  *client,
                           const char *startup_id)
{
        g_return_if_fail (GSM_IS_CLIENT (client));

        g_free (client->priv->startup_id);

        if (startup_id != NULL) {
                client->priv->startup_id = g_strdup (startup_id);
        } else {
                client->priv->startup_id = g_strdup ("");
        }
        g_object_notify (G_OBJECT (client), "startup-id");
}

void
gsm_client_set_app_id (GsmClient  *client,
                       const char *app_id)
{
        g_return_if_fail (GSM_IS_CLIENT (client));

        g_free (client->priv->app_id);

        if (app_id != NULL) {
                client->priv->app_id = g_strdup (app_id);
        } else {
                client->priv->app_id = g_strdup ("");
        }
        g_object_notify (G_OBJECT (client), "app-id");
}

static void
gsm_client_set_property (GObject       *object,
                         guint          prop_id,
                         const GValue  *value,
                         GParamSpec    *pspec)
{
        GsmClient *self;

        self = GSM_CLIENT (object);

        switch (prop_id) {
        case PROP_STARTUP_ID:
                gsm_client_set_startup_id (self, g_value_get_string (value));
                break;
        case PROP_APP_ID:
                gsm_client_set_app_id (self, g_value_get_string (value));
                break;
        case PROP_STATUS:
                gsm_client_set_status (self, g_value_get_uint (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsm_client_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
        GsmClient *self;

        self = GSM_CLIENT (object);

        switch (prop_id) {
        case PROP_STARTUP_ID:
                g_value_set_string (value, self->priv->startup_id);
                break;
        case PROP_APP_ID:
                g_value_set_string (value, self->priv->app_id);
                break;
        case PROP_STATUS:
                g_value_set_uint (value, self->priv->status);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static gboolean
default_stop (GsmClient *client,
              GError   **error)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), FALSE);

        g_warning ("Stop not implemented");

        return TRUE;
}

static void
gsm_client_dispose (GObject *object)
{
        GsmClient *client;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSM_IS_CLIENT (object));

        client = GSM_CLIENT (object);

        g_debug ("GsmClient: disposing %s", client->priv->id);

        G_OBJECT_CLASS (gsm_client_parent_class)->dispose (object);
}

static void
gsm_client_class_init (GsmClientClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gsm_client_get_property;
        object_class->set_property = gsm_client_set_property;
        object_class->constructor = gsm_client_constructor;
        object_class->finalize = gsm_client_finalize;
        object_class->dispose = gsm_client_dispose;

        klass->impl_stop = default_stop;

        signals[DISCONNECTED] =
                g_signal_new ("disconnected",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GsmClientClass, disconnected),
                              NULL, NULL, NULL,
                              G_TYPE_NONE,
                              0);
        signals[END_SESSION_RESPONSE] =
                g_signal_new ("end-session-response",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GsmClientClass, end_session_response),
                              NULL, NULL, NULL,
                              G_TYPE_NONE,
                              4, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_STRING);

        g_object_class_install_property (object_class,
                                         PROP_STARTUP_ID,
                                         g_param_spec_string ("startup-id",
                                                              "startup-id",
                                                              "startup-id",
                                                              "",
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_APP_ID,
                                         g_param_spec_string ("app-id",
                                                              "app-id",
                                                              "app-id",
                                                              "",
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_STATUS,
                                         g_param_spec_uint ("status",
                                                            "status",
                                                            "status",
                                                            0,
                                                            G_MAXINT,
                                                            GSM_CLIENT_UNREGISTERED,
                                                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

        g_type_class_add_private (klass, sizeof (GsmClientPrivate));
}

const char *
gsm_client_peek_id (GsmClient *client)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), NULL);

        return client->priv->id;
}

/**
 * gsm_client_peek_app_id:
 * @client: a #GsmClient.
 *
 * Note that the application ID might not be known; this happens when for XSMP
 * clients that we did not start ourselves, for instance.
 *
 * Returns: the application ID of the client, or %NULL if no such ID is
 * known. The string is owned by @client.
 **/
const char *
gsm_client_peek_app_id (GsmClient *client)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), NULL);

        return client->priv->app_id;
}

const char *
gsm_client_peek_startup_id (GsmClient *client)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), NULL);

        return client->priv->startup_id;
}

guint
gsm_client_peek_status (GsmClient *client)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), GSM_CLIENT_UNREGISTERED);

        return client->priv->status;
}

guint
gsm_client_peek_restart_style_hint (GsmClient *client)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), GSM_CLIENT_RESTART_NEVER);

        return GSM_CLIENT_GET_CLASS (client)->impl_get_restart_style_hint (client);
}

/**
 * gsm_client_get_app_name:
 * @client: a #GsmClient.
 *
 * Returns: a copy of the application name of the client, or %NULL if no such
 * name is known.
 **/
char *
gsm_client_get_app_name (GsmClient *client)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), NULL);

        return GSM_CLIENT_GET_CLASS (client)->impl_get_app_name (client);
}

gboolean
gsm_client_cancel_end_session (GsmClient *client,
                               GError   **error)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), FALSE);

        return GSM_CLIENT_GET_CLASS (client)->impl_cancel_end_session (client, error);
}


gboolean
gsm_client_query_end_session (GsmClient                *client,
                              GsmClientEndSessionFlag   flags,
                              GError                  **error)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), FALSE);

        return GSM_CLIENT_GET_CLASS (client)->impl_query_end_session (client, flags, error);
}

gboolean
gsm_client_end_session (GsmClient                *client,
                        GsmClientEndSessionFlag   flags,
                        GError                  **error)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), FALSE);

        return GSM_CLIENT_GET_CLASS (client)->impl_end_session (client, flags, error);
}

gboolean
gsm_client_stop (GsmClient *client,
                 GError   **error)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), FALSE);

        return GSM_CLIENT_GET_CLASS (client)->impl_stop (client, error);
}

void
gsm_client_disconnected (GsmClient *client)
{
        g_signal_emit (client, signals[DISCONNECTED], 0);
}

gboolean
gsm_client_request_save (GsmClient *client,
                         guint      flags,
                         GError   **error)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), FALSE);

        return GSM_CLIENT_GET_CLASS (client)->impl_request_save (client, flags, error);
}

GKeyFile *
gsm_client_save (GsmClient *client,
                 GError   **error)
{
        g_return_val_if_fail (GSM_IS_CLIENT (client), FALSE);

        return GSM_CLIENT_GET_CLASS (client)->impl_save (client, error);
}

void
gsm_client_end_session_response (GsmClient  *client,
                                 gboolean    is_ok,
                                 gboolean    do_last,
                                 gboolean    cancel,
                                 const char *reason)
{
        g_signal_emit (client, signals[END_SESSION_RESPONSE], 0,
                       is_ok, do_last, cancel, reason);
}
