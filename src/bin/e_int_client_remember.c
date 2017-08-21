#include "e.h"

/* PROTOTYPES - same all the time */
static void        *_create_data(E_Config_Dialog *cfd);
static void         _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int          _basic_apply_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int          _advanced_apply_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static Evas_Object *_advanced_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);

/* Actual config data we will be playing with whil the dialog is active */
#define MODE_NOTHING        0
#define MODE_GEOMETRY       E_REMEMBER_APPLY_POS | E_REMEMBER_APPLY_SIZE
#define MODE_LOCKS          E_REMEMBER_APPLY_LOCKS
#define MODE_VOLUME         E_REMEMBER_APPLY_VOLUME
#define MODE_GEOMETRY_LOCKS E_REMEMBER_APPLY_POS | E_REMEMBER_APPLY_SIZE | E_REMEMBER_APPLY_LOCKS
#define MODE_ALL            E_REMEMBER_APPLY_POS | E_REMEMBER_APPLY_SIZE | E_REMEMBER_APPLY_LAYER | \
  E_REMEMBER_APPLY_LOCKS | E_REMEMBER_APPLY_BORDER | E_REMEMBER_APPLY_STICKY |                      \
  E_REMEMBER_APPLY_DESKTOP | E_REMEMBER_APPLY_SHADE | E_REMEMBER_APPLY_ZONE |                       \
  E_REMEMBER_APPLY_SKIP_WINLIST | E_REMEMBER_APPLY_SKIP_PAGER |                                     \
  E_REMEMBER_APPLY_SKIP_TASKBAR | E_REMEMBER_APPLY_FULLSCREEN | E_REMEMBER_APPLY_ICON_PREF |        \
  E_REMEMBER_APPLY_OFFER_RESISTANCE | E_REMEMBER_APPLY_OPACITY
struct _E_Config_Dialog_Data
{
   E_Client *client;
   E_Remember *rem;
   /*- BASIC -*/
   int       mode;
   int       warned;
   /*- ADVANCED -*/
   char     *name;
   char     *class;
   char     *title;
   char     *role;
   char     *command;
   char     *desktop;
   struct
   {
      int match_name;
      int match_class;
      int match_title;
      int match_role;
      int match_type;
      int match_transient;
      int apply_first_only;
      int apply_pos;
      int apply_size;
      int apply_layer;
      int apply_locks;
      int apply_border;
      int apply_sticky;
      int apply_desktop;
      int apply_shade;
      int apply_fullscreen;
      int apply_zone;
      int apply_skip_winlist;
      int apply_skip_pager;
      int apply_skip_taskbar;
      int apply_run;
      int apply_icon_pref;
      int apply_desktop_file;
      int set_focus_on_start;
      int keep_settings;
      int offer_resistance;
      int apply_opacity;
      int apply_volume;
   } remember;

   int applied;
};

/* a nice easy setup function that does the dirty work */
E_API E_Config_Dialog *
e_int_client_remember_edit(E_Remember *rem)
{
   E_Config_Dialog_View *v;

   v = E_NEW(E_Config_Dialog_View, 1);
   /* methods */
   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _advanced_apply_data;
   v->basic.create_widgets = _advanced_create_widgets;
   v->override_auto_apply = 1;

   /* create config dialog for ec object/data */
   return e_config_dialog_new(NULL, _("Window Remember"),
                             "E", "_border_remember_edit_dialog",
                             NULL, 0, v, rem);
}

E_API void
e_int_client_remember(E_Client *ec)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   v = E_NEW(E_Config_Dialog_View, 1);
   if (v)
     {
        /* methods */
        v->create_cfdata = _create_data;
        v->free_cfdata = _free_data;
        v->basic.apply_cfdata = _basic_apply_data;
        v->basic.create_widgets = _basic_create_widgets;
        v->advanced.apply_cfdata = _advanced_apply_data;
        v->advanced.create_widgets = _advanced_create_widgets;
        v->override_auto_apply = 1;

        /* create config dialog for ec object/data */
        cfd = e_config_dialog_new(NULL,
                                  _("Window Remember"),
                                  "E", "_border_remember_dialog",
                                  NULL, 0, v, ec);
        ec->border_remember_dialog = cfd;
        evas_object_layer_set(e_win_client_get(cfd->dia->win)->frame, ec->layer);
     }
}

/**--CREATE--**/
static void
_clear_data(E_Config_Dialog_Data *cfdata)
{
   E_FREE(cfdata->name);
   E_FREE(cfdata->class);
   E_FREE(cfdata->title);
   E_FREE(cfdata->role);
   E_FREE(cfdata->command);
   E_FREE(cfdata->desktop);
   cfdata->mode = 0;
   memset(&cfdata->remember, 0, sizeof(cfdata->remember));
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
   E_Client *ec;
   E_Remember *rem;

   ec = cfdata->client;
   if (ec)
     rem = ec->remember;
   else
     rem = cfdata->rem;

   if (rem)
     {
        if (rem->name && rem->name[0])
          cfdata->name = strdup(rem->name);
        if (rem->class && rem->class[0])
          cfdata->class = strdup(rem->class);
        if (rem->title && rem->title[0])
          cfdata->title = strdup(rem->title);
        if (rem->role && rem->role[0])
          cfdata->role = strdup(rem->role);
        if (rem->prop.command && rem->prop.command[0])
          cfdata->command = strdup(rem->prop.command);
        if (rem->prop.desktop_file && rem->prop.desktop_file[0])
          cfdata->desktop = strdup(rem->prop.desktop_file);
        if (cfdata->desktop)
          cfdata->remember.apply_desktop_file = 1;
     }

   if (ec)
     {
        if (!cfdata->name &&
            ec->icccm.name &&
            ec->icccm.name[0])
          cfdata->name = strdup(ec->icccm.name);
        if (!cfdata->class &&
            ec->icccm.class &&
            ec->icccm.class[0])
          cfdata->class = strdup(ec->icccm.class);
        if (!cfdata->role &&
            ec->icccm.window_role &&
            ec->icccm.window_role[0])
          cfdata->role = strdup(ec->icccm.window_role);
        if (!cfdata->title)
          {
             const char *title = e_client_util_name_get(ec);
             if (title && title[0])
               cfdata->title = strdup(title);
          }
        if (!cfdata->desktop && ec->desktop)
          cfdata->desktop = strdup(ec->desktop->name);

        if (!cfdata->command &&
            (ec->icccm.command.argc > 0) &&
            (ec->icccm.command.argv))
          {
             char buf[4096];
             int i, j, k;

             buf[0] = 0;
             k = 0;
             for (i = 0; i < ec->icccm.command.argc; i++)
               {
                  if (i > 0)
                    {
                       buf[k] = ' ';
                       k++;
                    }
                  for (j = 0; ec->icccm.command.argv[i][j]; j++)
                    {
                       if (k >= (int)(sizeof(buf) - 10))
                         {
                            buf[k] = 0;
                            goto done;
                         }
                       if ((ec->icccm.command.argv[i][j] == ' ') ||
                           (ec->icccm.command.argv[i][j] == '\t') ||
                           (ec->icccm.command.argv[i][j] == '\\') ||
                           (ec->icccm.command.argv[i][j] == '\"') ||
                           (ec->icccm.command.argv[i][j] == '\'') ||
                           (ec->icccm.command.argv[i][j] == '$') ||
                           (ec->icccm.command.argv[i][j] == '%'))
                         {
                            buf[k] = '\\';
                            k++;
                         }
                       buf[k] = ec->icccm.command.argv[i][j];
                       k++;
                    }
               }
             buf[k] = 0;
     done:
             cfdata->command = strdup(buf);
          }
     }

   if (rem)
     {
        if (rem->apply_first_only) cfdata->remember.apply_first_only = 1;
        if (rem->keep_settings) cfdata->remember.keep_settings = 1;

        if (rem->match & E_REMEMBER_MATCH_NAME)
          cfdata->remember.match_name = 1;
        if (rem->match & E_REMEMBER_MATCH_CLASS)
          cfdata->remember.match_class = 1;
        if (rem->match & E_REMEMBER_MATCH_TITLE)
          cfdata->remember.match_title = 1;
        if (rem->match & E_REMEMBER_MATCH_ROLE)
          cfdata->remember.match_role = 1;
        if (rem->match & E_REMEMBER_MATCH_TYPE)
          cfdata->remember.match_type = 1;
        if (rem->match & E_REMEMBER_MATCH_TRANSIENT)
          cfdata->remember.match_transient = 1;
        if (rem->apply & E_REMEMBER_APPLY_POS)
          cfdata->remember.apply_pos = 1;
        if (rem->apply & E_REMEMBER_APPLY_SIZE)
          cfdata->remember.apply_size = 1;
        if (rem->apply & E_REMEMBER_APPLY_LAYER)
          cfdata->remember.apply_layer = 1;
        if (rem->apply & E_REMEMBER_APPLY_LOCKS)
          cfdata->remember.apply_locks = 1;
        if (rem->apply & E_REMEMBER_APPLY_BORDER)
          cfdata->remember.apply_border = 1;
        if (rem->apply & E_REMEMBER_APPLY_STICKY)
          cfdata->remember.apply_sticky = 1;
        if (rem->apply & E_REMEMBER_APPLY_DESKTOP)
          cfdata->remember.apply_desktop = 1;
        if (rem->apply & E_REMEMBER_APPLY_SHADE)
          cfdata->remember.apply_shade = 1;
        if (rem->apply & E_REMEMBER_APPLY_FULLSCREEN)
          cfdata->remember.apply_fullscreen = 1;
        if (rem->apply & E_REMEMBER_APPLY_ZONE)
          cfdata->remember.apply_zone = 1;
        if (rem->apply & E_REMEMBER_APPLY_SKIP_WINLIST)
          cfdata->remember.apply_skip_winlist = 1;
        if (rem->apply & E_REMEMBER_APPLY_SKIP_PAGER)
          cfdata->remember.apply_skip_pager = 1;
        if (rem->apply & E_REMEMBER_APPLY_SKIP_TASKBAR)
          cfdata->remember.apply_skip_taskbar = 1;
        if (rem->apply & E_REMEMBER_APPLY_RUN)
          cfdata->remember.apply_run = 1;
        if (rem->apply & E_REMEMBER_APPLY_ICON_PREF)
          cfdata->remember.apply_icon_pref = 1;
        if (rem->apply & E_REMEMBER_SET_FOCUS_ON_START)
          cfdata->remember.set_focus_on_start = 1;
        if (rem->apply & E_REMEMBER_APPLY_OFFER_RESISTANCE)
          cfdata->remember.offer_resistance = 1;
        if (rem->apply & E_REMEMBER_APPLY_OPACITY)
          cfdata->remember.apply_opacity = 1;
        if (rem->apply & E_REMEMBER_APPLY_VOLUME)
          cfdata->remember.apply_volume = 1;
     }

   if (!rem) cfdata->mode = MODE_NOTHING;
   else cfdata->mode = rem->apply;
}

static void *
_create_data(E_Config_Dialog *cfd)
{
   /* Create cfdata - cfdata is a temporary block of config data that this
    * dialog will be dealing with while configuring. it will be applied to
    * the running systems/config in the apply methods
    */
   E_Config_Dialog_Data *cfdata;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   if (!strcmp(cfd->class, "_border_remember_dialog"))
     cfdata->client = cfd->data;
   else
     cfdata->rem = cfd->data;
   cfdata->applied = 1;
   return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   /* Free the cfdata */
   free(cfdata->name);
   free(cfdata->class);
   free(cfdata->title);
   free(cfdata->role);
   free(cfdata->command);
   free(cfdata->desktop);

   if (cfdata->client)
     {
        if (!cfdata->applied && cfdata->client->remember)
          {
             e_remember_unuse(cfdata->client->remember);
             e_remember_del(cfdata->client->remember);
             e_config_save_queue();
          }

        cfdata->client->border_remember_dialog = NULL;
     }
   free(cfdata);
}

static void
_warning_dialog_show(void)
{
   E_Dialog *dia;

   dia = e_dialog_new(NULL, "E", "_border_remember_error_multi_dialog");
   e_dialog_title_set(dia, _("Window properties are not a unique match"));
   e_dialog_text_set
     (dia,
     _("You are trying to ask Enlightenment to remember to apply<ps/>"
       "properties (such as size, location, border style etc.) to<ps/>"
       "a window that <hilight>does not have unique properties</hilight>.<ps/>"
       "<ps/>"
       "This means it shares Name/Class, Transience, Role etc. properties<ps/>"
       "with more than 1 other window on the screen and remembering<ps/>"
       "properties for this window will apply to all other windows<ps/>"
       "that match these properties.<ps/>"
       "<ps/>"
       "This is just a warning in case you did not intend this to happen.<ps/>"
       "If you did, simply press <hilight>Apply</hilight> or <hilight>OK</hilight> buttons<ps/>"
       "and your settings will be accepted. Press <hilight>Cancel</hilight> if you<ps/>"
       "are not sure and nothing will be affected.")
     );
   e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
   elm_win_center(dia->win, 1, 1);
   e_dialog_show(dia);
}

static int
_check_matches(E_Remember *rem, int update)
{
   const Eina_List *l;
   E_Client *ec;
   const char *title;
   int n = 0;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        int match = rem->match;
        title = e_client_util_name_get(ec);

        if ((match & E_REMEMBER_MATCH_NAME) &&
            (e_util_glob_match(ec->icccm.name, rem->name)))
          match &= ~E_REMEMBER_MATCH_NAME;

        if ((match & E_REMEMBER_MATCH_CLASS) &&
            (e_util_glob_match(ec->icccm.class, rem->class)))
          match &= ~E_REMEMBER_MATCH_CLASS;

        if ((match & E_REMEMBER_MATCH_TITLE) &&
            (e_util_glob_match(title, rem->title)))
          match &= ~E_REMEMBER_MATCH_TITLE;

        if ((match & E_REMEMBER_MATCH_ROLE) &&
            ((!e_util_strcmp(rem->role, ec->icccm.window_role)) ||
             (e_util_both_str_empty(rem->role, ec->icccm.window_role))))
          match &= ~E_REMEMBER_MATCH_ROLE;

        if ((match & E_REMEMBER_MATCH_TYPE) &&
            (rem->type == (int)ec->netwm.type))
          match &= ~E_REMEMBER_MATCH_TYPE;

        if ((match & E_REMEMBER_MATCH_TRANSIENT) &&
            ((rem->transient && ec->icccm.transient_for != 0) ||
             (!rem->transient && (ec->icccm.transient_for == 0))))
          match &= ~E_REMEMBER_MATCH_TRANSIENT;

        if (match == 0) n++;

        if (update)
          {
             ec->changed = 1;
             ec->changes.icon = 1;
          }
        else if (n > 1)
          break;
     }
   return n;
}

static int
_basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   /* Actually take our cfdata settings and apply them in real life */
   E_Client *ec = cfdata->client;
   E_Remember *rem = ec->remember;

   if (cfdata->mode == MODE_NOTHING)
     {
        if (rem)
          {
             e_remember_unuse(rem);
             e_remember_del(rem);
          }
        e_config_save_queue();
        return 1;
     }

   if (!rem)
     {
        rem = e_remember_new();
        if (rem)
          {
             ec->remember = rem;
             cfdata->applied = 0;
          }
        else
          return 0;
     }

   e_remember_default_match_set(rem, cfdata->client);

   if ((!cfdata->warned) && (_check_matches(rem, 0) > 1))
     {
        _warning_dialog_show();
        cfdata->warned = 1;
        return 0;
     }

   rem->apply = cfdata->mode;
   rem->apply_first_only = 0;

   e_remember_use(rem);
   e_remember_update(ec);
   cfdata->applied = 1;
   e_config_save_queue();
   return 1; /* Apply was OK */
}

static int
_advanced_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   E_Client *ec = cfdata->client;
   E_Remember *rem;

   if (ec) rem = ec->remember;
   else rem = cfdata->rem;

   if (!rem)
     {
        cfdata->applied = 0;
        rem = e_remember_new();
        if (rem)
          e_remember_use(rem);
        else
          return 0;
     }
   else
     {
        if (rem->name) eina_stringshare_del(rem->name);
        if (rem->class) eina_stringshare_del(rem->class);
        if (rem->title) eina_stringshare_del(rem->title);
        if (rem->role) eina_stringshare_del(rem->role);
        if (rem->prop.command) eina_stringshare_del(rem->prop.command);
        if (rem->prop.desktop_file) eina_stringshare_del(rem->prop.desktop_file);
        rem->name = NULL;
        rem->class = NULL;
        rem->title = NULL;
        rem->role = NULL;
        rem->prop.command = NULL;
        rem->prop.desktop_file = NULL;
     }

   rem->match = 0;
   rem->apply_first_only = cfdata->remember.apply_first_only;

   if (cfdata->remember.match_name && cfdata->name && cfdata->name[0])
     {
        rem->match |= E_REMEMBER_MATCH_NAME;
        rem->name = eina_stringshare_add(cfdata->name);
     }
   if (cfdata->remember.match_class && cfdata->class && cfdata->class[0])
     {
        rem->match |= E_REMEMBER_MATCH_CLASS;
        rem->class = eina_stringshare_add(cfdata->class);
     }
   if (cfdata->remember.match_title && cfdata->title && cfdata->title[0])
     {
        rem->match |= E_REMEMBER_MATCH_TITLE;
        rem->title = eina_stringshare_add(cfdata->title);
     }
   if (cfdata->remember.match_role && cfdata->role && cfdata->role[0])
     {
        rem->match |= E_REMEMBER_MATCH_ROLE;
        rem->role = eina_stringshare_add(cfdata->role);
     }
   if (ec)
     {
        if (cfdata->remember.match_type)
          {
             rem->match |= E_REMEMBER_MATCH_TYPE;
             rem->type = ec->netwm.type;
          }

        if (cfdata->remember.match_transient)
          {
             rem->match |= E_REMEMBER_MATCH_TRANSIENT;
             if (ec->icccm.transient_for != 0)
               rem->transient = 1;
             else
               rem->transient = 0;
          }
     }

   if (!rem->match)
     {
        E_Dialog *dia;

        dia = e_dialog_new(NULL, "E", "_border_remember_error_noprop_dialog");
        e_dialog_title_set(dia, _("No match properties set"));
        e_dialog_text_set
          (dia,
          _("You are trying to ask Enlightenment to remember to apply<ps/>"
            "properties (such as size, location, border style etc.) to<ps/>"
            "a window <hilight>without specifying how to remember it</hilight>.<ps/>"
            "<ps/>"
            "You must specify at least 1 way of remembering this window.")
          );
        e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
        elm_win_center(dia->win, 1, 1);
        e_dialog_show(dia);
        if (ec)
          cfdata->client->remember = rem;
        return 0;
     }

   /* TODO warn when match doesn't match the current window
    * (when using globs) */

   if (!cfdata->warned)
     {
        if ((!cfdata->remember.apply_first_only) &&
            (_check_matches(rem, 0) > 1))
          {
             _warning_dialog_show();
             cfdata->warned = 1;
             return 0;
          }
     }

   if (cfdata->command && cfdata->command[0])
     rem->prop.command = eina_stringshare_add(cfdata->command);
   if (cfdata->remember.apply_desktop_file && cfdata->desktop)
     rem->prop.desktop_file = eina_stringshare_add(cfdata->desktop);

   rem->apply = 0;
   if (cfdata->remember.apply_pos)
     rem->apply |= E_REMEMBER_APPLY_POS;
   if (cfdata->remember.apply_size)
     rem->apply |= E_REMEMBER_APPLY_SIZE;
   if (cfdata->remember.apply_layer)
     rem->apply |= E_REMEMBER_APPLY_LAYER;
   if (cfdata->remember.apply_locks)
     rem->apply |= E_REMEMBER_APPLY_LOCKS;
   if (cfdata->remember.apply_border)
     rem->apply |= E_REMEMBER_APPLY_BORDER;
   if (cfdata->remember.apply_sticky)
     rem->apply |= E_REMEMBER_APPLY_STICKY;
   if (cfdata->remember.apply_desktop)
     rem->apply |= E_REMEMBER_APPLY_DESKTOP;
   if (cfdata->remember.apply_shade)
     rem->apply |= E_REMEMBER_APPLY_SHADE;
   if (cfdata->remember.apply_fullscreen)
     rem->apply |= E_REMEMBER_APPLY_FULLSCREEN;
   if (cfdata->remember.apply_zone)
     rem->apply |= E_REMEMBER_APPLY_ZONE;
   if (cfdata->remember.apply_skip_winlist)
     rem->apply |= E_REMEMBER_APPLY_SKIP_WINLIST;
   if (cfdata->remember.apply_skip_pager)
     rem->apply |= E_REMEMBER_APPLY_SKIP_PAGER;
   if (cfdata->remember.apply_skip_taskbar)
     rem->apply |= E_REMEMBER_APPLY_SKIP_TASKBAR;
   if (cfdata->remember.apply_run)
     rem->apply |= E_REMEMBER_APPLY_RUN;
   if (cfdata->remember.apply_icon_pref)
     rem->apply |= E_REMEMBER_APPLY_ICON_PREF;
   if (cfdata->remember.set_focus_on_start)
     rem->apply |= E_REMEMBER_SET_FOCUS_ON_START;
   if (cfdata->remember.offer_resistance)
     rem->apply |= E_REMEMBER_APPLY_OFFER_RESISTANCE;
   if (cfdata->remember.apply_opacity)
     rem->apply |= E_REMEMBER_APPLY_OPACITY;
   if (cfdata->remember.apply_volume)
     rem->apply |= E_REMEMBER_APPLY_VOLUME;

   if (ec && (!rem->apply && !rem->prop.desktop_file))
     {
        e_remember_unuse(rem);
        e_remember_del(rem);
        if (cfdata->client->remember)
          e_config_save_queue();
        return 1;
     }

   if (ec)
     {
        _check_matches(rem, 1);
        rem->keep_settings = 0;
        cfdata->client->remember = rem;
        e_remember_update(cfdata->client);
        cfdata->applied = 1;
     }
   rem->keep_settings = cfdata->remember.keep_settings;

   e_config_save_queue();
   return 1; /* Apply was OK */
}

/**--GUI--**/
static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd EINA_UNUSED, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   /* generate the core widget layout for a basic dialog */
   Evas_Object *o, *ob;
   E_Radio_Group *rg;

   _clear_data(cfdata);
   _fill_data(cfdata);
   o = e_widget_list_add(evas, 0, 0);
   rg = e_widget_radio_group_new(&(cfdata->mode));
   ob = e_widget_radio_add(evas, _("Nothing"), MODE_NOTHING, rg);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ob = e_widget_radio_add(evas, _("Size and Position"), MODE_GEOMETRY, rg);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ob = e_widget_radio_add(evas, _("Locks"), MODE_LOCKS, rg);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ob = e_widget_radio_add(evas, _("Size, Position and Locks"), MODE_GEOMETRY_LOCKS, rg);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ob = e_widget_radio_add(evas, _("Volume"), MODE_VOLUME, rg);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   ob = e_widget_radio_add(evas, _("All"), MODE_ALL, rg);
   e_widget_list_object_append(o, ob, 1, 1, 0.5);
   return o;
}

static Evas_Object *
_advanced_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   /* generate the core widget layout for an advanced dialog */
   Evas_Object *o, *ob, *of, *oc;

   _clear_data(cfdata);
   _fill_data(cfdata);
   o = e_widget_toolbook_add(evas, (48 * e_scale), (48 * e_scale));

   of = e_widget_list_add(evas, 0, 0);
   if (cfdata->name)
     {
        ob = e_widget_check_add(evas, _("Window name"),
                                &(cfdata->remember.match_name));
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
        ob = e_widget_entry_add(cfd->dia->win, &cfdata->name, NULL, NULL, NULL);
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
     }
   else
     {
        cfdata->remember.match_name = 0;
     }
   if (cfdata->class)
     {
        ob = e_widget_check_add(evas, _("Window class"),
                                &(cfdata->remember.match_class));
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
        ob = e_widget_entry_add(cfd->dia->win, &cfdata->class, NULL, NULL, NULL);
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
     }
   else
     {
        cfdata->remember.match_class = 0;
     }
   if (cfdata->title)
     {
        ob = e_widget_check_add(evas, _("Title"),
                                &(cfdata->remember.match_title));
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
        ob = e_widget_entry_add(cfd->dia->win, &cfdata->title, NULL, NULL, NULL);
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
     }
   else
     {
        cfdata->remember.match_title = 0;
     }
   if (cfdata->role)
     {
        ob = e_widget_check_add(evas, _("Window Role"),
                                &(cfdata->remember.match_role));
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
        ob = e_widget_entry_add(cfd->dia->win, &cfdata->role, NULL, NULL, NULL);
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
     }
   else
     {
        cfdata->remember.match_role = 0;
     }
   if (cfdata->client)
     {
        if (cfdata->client->netwm.type != E_WINDOW_TYPE_UNKNOWN)
          {
             ob = e_widget_check_add(evas, _("Window type"),
                                     &(cfdata->remember.match_type));
             e_widget_list_object_append(of, ob, 1, 0, 0.5);
          }
        else
          {
             cfdata->remember.match_type = 0;
          }
        ob = e_widget_label_add(evas, _("wildcard matches are allowed"));
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
        ob = e_widget_check_add(evas, _("Transience"),
                                &(cfdata->remember.match_transient));
        e_widget_list_object_append(of, ob, 1, 0, 0.5);
     }
   e_widget_toolbook_page_append(o, NULL, _("Identifiers"), of, 1, 1, 1, 1, 0.5, 0.0);

   of = e_widget_table_add(e_win_evas_win_get(evas), 0);
   ob = e_widget_check_add(evas, _("Position"),
                           &(cfdata->remember.apply_pos));
   e_widget_table_object_append(of, ob, 0, 0, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Size"),
                           &(cfdata->remember.apply_size));
   e_widget_table_object_append(of, ob, 0, 1, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Stacking"),
                           &(cfdata->remember.apply_layer));
   e_widget_table_object_append(of, ob, 0, 2, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Locks"),
                           &(cfdata->remember.apply_locks));
   e_widget_table_object_append(of, ob, 0, 3, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Border style"),
                           &(cfdata->remember.apply_border));
   e_widget_table_object_append(of, ob, 0, 4, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Icon Preference"),
                           &(cfdata->remember.apply_icon_pref));
   e_widget_table_object_append(of, ob, 0, 5, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Stickiness"),
                           &(cfdata->remember.apply_sticky));
   e_widget_table_object_append(of, ob, 0, 6, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Virtual Desktop"),
                           &(cfdata->remember.apply_desktop));
   e_widget_table_object_append(of, ob, 1, 0, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Shaded state"),
                           &(cfdata->remember.apply_shade));
   e_widget_table_object_append(of, ob, 1, 1, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Fullscreen state"),
                           &(cfdata->remember.apply_fullscreen));
   e_widget_table_object_append(of, ob, 1, 2, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Current Screen"),
                           &(cfdata->remember.apply_zone));
   e_widget_table_object_append(of, ob, 1, 3, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Skip Window List"),
                           &(cfdata->remember.apply_skip_winlist));
   e_widget_table_object_append(of, ob, 1, 4, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Skip Pager"),
                           &(cfdata->remember.apply_skip_pager));
   e_widget_table_object_append(of, ob, 1, 5, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Skip Taskbar"),
                           &(cfdata->remember.apply_skip_taskbar));
   e_widget_table_object_append(of, ob, 1, 6, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Offer Resistance"),
                           &(cfdata->remember.offer_resistance));
   e_widget_table_object_append(of, ob, 1, 7, 1, 1, 1, 0, 1, 0);
   ob = e_widget_check_add(evas, _("Opacity"),
                           &(cfdata->remember.apply_opacity));
   e_widget_table_object_append(of, ob, 0, 7, 1, 1, 1, 0, 1, 0);
   oc = e_widget_check_add(evas, _("Application file or name (.desktop)"),
                           &(cfdata->remember.apply_desktop_file));
   e_widget_table_object_append(of, ob, 0, 8, 1, 1, 1, 0, 1, 0);
   oc = e_widget_check_add(evas, _("Volume"),
                           &(cfdata->remember.apply_volume));
   e_widget_table_object_append(of, oc, 0, 9, 1, 1, 1, 0, 1, 0);
   ob = e_widget_entry_add(cfd->dia->win, &cfdata->desktop, NULL, NULL, NULL);
   e_widget_check_widget_disable_on_unchecked_add(oc, ob);
   e_widget_table_object_append(of, ob, 0, 10, 2, 1, 1, 0, 1, 0);
   e_widget_toolbook_page_append(o, NULL, _("Properties"), of, 1, 1, 1, 1, 0.5, 0.0);

   of = e_widget_table_add(e_win_evas_win_get(evas), 0);
   ob = e_widget_check_add(evas, _("Match only one window"),
                           &(cfdata->remember.apply_first_only));
   e_widget_table_object_append(of, ob, 0, 0, 1, 1, 1, 0, 1, 0);

   ob = e_widget_check_add(evas, _("Always focus on start"),
                           &(cfdata->remember.set_focus_on_start));
   e_widget_table_object_append(of, ob, 0, 1, 1, 1, 1, 0, 1, 0);

   ob = e_widget_check_add(evas, _("Keep current properties"),
                           &(cfdata->remember.keep_settings));
   e_widget_table_object_append(of, ob, 0, 2, 1, 1, 1, 0, 1, 0);

   if (cfdata->command)
     {
        ob = e_widget_check_add(evas, _("Start this program on login"),
                                &(cfdata->remember.apply_run));
        e_widget_table_object_append(of, ob, 0, 3, 1, 1, 1, 0, 1, 0);
     }
   e_widget_toolbook_page_append(o, NULL, _("Options"), of, 1, 1, 1, 1, 0.5, 0.0);
   e_widget_toolbook_page_show(o, 0);

   return o;
}

