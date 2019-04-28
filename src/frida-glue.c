#include "frida-core.h"

#include <gum/gum.h>

static GThread * main_thread;
static GMainLoop * main_loop;
static GMainContext * main_context;

static gpointer run_main_loop (gpointer data);
static gboolean dummy_callback (gpointer data);
static gboolean stop_main_loop (gpointer data);

void
frida_init (void)
{
  static gsize frida_initialized = FALSE;

  g_thread_set_garbage_handler (frida_on_pending_garbage, NULL);
  glib_init ();
  gio_init ();
  gum_init ();
  frida_error_quark (); /* Initialize early so GDBus will pick it up */

  if (g_once_init_enter (&frida_initialized))
  {
    g_set_prgname ("frida");

    main_context = g_main_context_ref (g_main_context_default ());
    main_loop = g_main_loop_new (main_context, FALSE);
    main_thread = g_thread_new ("frida-main-loop", run_main_loop, NULL);

    g_once_init_leave (&frida_initialized, TRUE);
  }
}

void
frida_unref (gpointer obj)
{
  GSource * source;

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_HIGH);
  g_source_set_callback (source, dummy_callback, obj, g_object_unref);
  g_source_attach (source, main_context);
  g_source_unref (source);
}

void
frida_shutdown (void)
{
  GSource * source;

  g_assert (main_loop != NULL);

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_LOW);
  g_source_set_callback (source, stop_main_loop, NULL, NULL);
  g_source_attach (source, main_context);
  g_source_unref (source);

  g_thread_join (main_thread);
  main_thread = NULL;
}

void
frida_deinit (void)
{
  g_assert (main_loop != NULL);

  if (main_thread != NULL)
    frida_shutdown ();

  g_main_loop_unref (main_loop);
  main_loop = NULL;
  g_main_context_unref (main_context);
  main_context = NULL;

  gio_shutdown ();
  glib_shutdown ();
  gio_deinit ();
  glib_deinit ();
}

GMainContext *
frida_get_main_context (void)
{
  return main_context;
}

void
frida_version (guint * major, guint * minor, guint * micro, guint * nano)
{
  if (major != NULL)
    *major = FRIDA_MAJOR_VERSION;

  if (minor != NULL)
    *minor = FRIDA_MINOR_VERSION;

  if (micro != NULL)
    *micro = FRIDA_MICRO_VERSION;

  if (nano != NULL)
    *nano = FRIDA_NANO_VERSION;
}

const gchar *
frida_version_string (void)
{
  return FRIDA_VERSION;
}

static gpointer
run_main_loop (gpointer data)
{
  (void) data;

  g_main_context_push_thread_default (main_context);
  g_main_loop_run (main_loop);
  g_main_context_pop_thread_default (main_context);

  return NULL;
}

static gboolean
dummy_callback (gpointer data)
{
  (void) data;

  return FALSE;
}

static gboolean
stop_main_loop (gpointer data)
{
  (void) data;

  g_main_loop_quit (main_loop);

  return FALSE;
}
