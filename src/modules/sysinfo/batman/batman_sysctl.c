#include "batman.h"

#include <sys/types.h>
#include <sys/sysctl.h>

#if defined(__FreeBSD__) || defined(__DragonFly__)
# include <dev/acpica/acpiio.h>
# include <sys/ioctl.h>
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <sys/param.h>
# include <sys/sensors.h>
#endif

EINTERN extern Eina_List *batman_device_batteries;
EINTERN extern Eina_List *batman_device_ac_adapters;

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
static Eina_Bool _batman_sysctl_battery_update_poll(void *data);
static int       _batman_sysctl_battery_update(Instance *inst);
#endif

int
_batman_sysctl_start(Instance *inst)
{
#if defined(__OpenBSD__) || defined(__NetBSD__)
   int mib[] = {CTL_HW, HW_SENSORS, 0, 0, 0};
   int devn, i = 0;
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);
   char name[256];

   if (eina_list_count(batman_device_batteries) != 0)
     {
        _batman_sysctl_battery_update(inst);
        return 1;
     }

   for (devn = 0;; devn++)
     {
        mib[2] = devn;
        if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENXIO)
               continue;
             if (errno == ENOENT)
               break;
          }

        snprintf(name, sizeof(name), "acpibat%d", i);
        if (!strcmp(name, snsrdev.xname))
          {
             Battery *bat = E_NEW(Battery, 1);
             if (!bat)
               return 0;
             bat->inst = inst;
             bat->udi = eina_stringshare_add(name),
             bat->mib = malloc(sizeof(int) * 5);
             if (!bat->mib) return 0;
             bat->mib[0] = mib[0];
             bat->mib[1] = mib[1];
             bat->mib[2] = mib[2];
             bat->technology = eina_stringshare_add("Unknown");
             bat->model = eina_stringshare_add("Unknown");
             bat->last_update = ecore_time_get();
             bat->vendor = eina_stringshare_add("Unknown");
             bat->poll = ecore_poller_add(ECORE_POLLER_CORE,
                                          inst->cfg->batman.poll_interval,
                                          _batman_sysctl_battery_update_poll, inst);
             batman_device_batteries = eina_list_append(batman_device_batteries, bat);
             ++i;
          }

        if (!strcmp("acpiac0", snsrdev.xname))
          {
             Ac_Adapter *ac = E_NEW(Ac_Adapter, 1);
             if (!ac) return 0;
             ac->inst = inst;
             ac->udi = eina_stringshare_add("acpiac0");
             ac->mib = malloc(sizeof(int) * 5);
             if (!ac->mib) return 0;
             ac->mib[0] = mib[0];
             ac->mib[1] = mib[1];
             ac->mib[2] = mib[2];
             batman_device_ac_adapters = eina_list_append(batman_device_ac_adapters, ac);
          }
     }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   size_t len;
   int i, count, fd;
   union acpi_battery_ioctl_arg battio;

   if (eina_list_count(batman_device_batteries) != 0)
     {
        _batman_sysctl_battery_update(inst);
        return 1;
     }

   if ((fd = open("/dev/acpi", O_RDONLY)) == -1)
     return 0;

   if (ioctl(fd, ACPIIO_BATT_GET_UNITS, &count) == -1)
     {
        close(fd);
        return 0;
     }

   for (i = 0; i < count; i++)
     {
        battio.unit = i;
        if (ioctl(fd, ACPIIO_BATT_GET_BIF, &battio) != -1)
          {
             Battery *bat = E_NEW(Battery, 1);
             if (!bat) return 0;

             bat->inst = inst;
             bat->last_update = ecore_time_get();
             bat->last_full_charge = battio.bif.lfcap;
             bat->model = eina_stringshare_add(battio.bif.model);
             bat->vendor = eina_stringshare_add(battio.bif.oeminfo);
             bat->technology = eina_stringshare_add(battio.bif.type);
             bat->poll = ecore_poller_add(ECORE_POLLER_CORE,
                                          inst->cfg->batman.poll_interval,
                                          _batman_sysctl_battery_update_poll, inst);

             batman_device_batteries = eina_list_append(batman_device_batteries, bat);
          }
     }

   close(fd);

   if ((sysctlbyname("hw.acpi.acline", NULL, &len, NULL, 0)) != -1)
     {
        Ac_Adapter *ac = E_NEW(Ac_Adapter, 1);
        if (!ac)
          return 0;
        ac->inst = inst;
        ac->mib = malloc(sizeof(int) * 3);
        if (!ac->mib) return 0;
        len = sizeof(ac->mib);
        sysctlnametomib("hw.acpi.acline", ac->mib, &len);

        ac->udi = eina_stringshare_add("hw.acpi.acline");

        batman_device_ac_adapters = eina_list_append(batman_device_ac_adapters, ac);
     }
#endif
   _batman_sysctl_battery_update(inst);

   return 1;
}

void
_batman_sysctl_stop(Instance *inst)
{
   Instance *child;
   Eina_List *l;
   Battery *bat;
   Ac_Adapter *ac;
   int bat_num = 0;

   /* This is a dummy battery we return here. */
   if (inst->cfg->batman.have_battery != 1)
     {
        return;
     }

   /* If this is NOT the last batman gadget then we return before
    * freeing everything. This is NOT optimal but is allows us to have
    * many gadgets and share the same data between multiple batman
    * gadget instances. The instance will be deleted.
    */

   EINA_LIST_FOREACH(sysinfo_instances, l, child)
     {
        if (inst->cfg->esm == E_SYSINFO_MODULE_BATMAN ||
            inst->cfg->esm == E_SYSINFO_MODULE_SYSINFO)
          {
             if (child != inst)
               {
                  bat_num++;
               }
          }
     }

   /* This is not the last batman gadget. */
   if (bat_num > 0) return;

   /* We have no batman or sysinfo gadgets remaining. We can safely
      remove these batteries and adapters. */

   EINA_LIST_FREE(batman_device_ac_adapters, ac)
     {
        E_FREE_FUNC(ac->udi, eina_stringshare_del);
        E_FREE(ac->mib);
        E_FREE(ac);
     }

   EINA_LIST_FREE(batman_device_batteries, bat)
     {
        E_FREE_FUNC(bat->udi, eina_stringshare_del);
        E_FREE_FUNC(bat->technology, eina_stringshare_del);
        E_FREE_FUNC(bat->model, eina_stringshare_del);
        E_FREE_FUNC(bat->vendor, eina_stringshare_del);
        E_FREE_FUNC(bat->poll, ecore_poller_del);
        E_FREE(bat->mib);
        E_FREE(bat);
     }
}

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
static Eina_Bool
_batman_sysctl_battery_update_poll(void *data EINA_UNUSED)
{
   Eina_List *l;
   Instance *inst;

   /* We need to ensure we update both batman and sysinfo instances. */
   EINA_LIST_FOREACH(sysinfo_instances, l, inst)
     {
        if (inst->cfg->esm == E_SYSINFO_MODULE_BATMAN ||
            inst->cfg->esm == E_SYSINFO_MODULE_SYSINFO)
          _batman_sysctl_battery_update(inst);
     }

   return EINA_TRUE;
}

#endif

static int
_batman_sysctl_battery_update(Instance *inst)
{
   Battery *bat;
   Ac_Adapter *ac;
   Eina_List *l;
#if defined(__OpenBSD__) || defined(__NetBSD__)
   double _time, charge;
   struct sensor s;
   size_t slen = sizeof(struct sensor);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   union acpi_battery_ioctl_arg battio;
   size_t len;
   int value, fd, i = 0;
#endif

   EINA_LIST_FOREACH(batman_device_batteries, l, bat)
     {
        /* update the poller interval */
        ecore_poller_poller_interval_set(bat->poll,
                                         inst->cfg->batman.poll_interval);
#if defined(__OpenBSD__) || defined(__NetBSD__)
        /* last full capacity */
        bat->mib[3] = 7;
        bat->mib[4] = 0;
        if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) != -1)
          {
             bat->last_full_charge = (double)s.value;
          }

        /* remaining capacity */
        bat->mib[3] = 7;
        bat->mib[4] = 3;
        if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) != -1)
          {
             charge = (double)s.value;
          }

        /* This is a workaround because there's an ACPI bug */
        if ((EINA_FLT_EQ(charge, 0.0)) || (EINA_FLT_EQ(bat->last_full_charge, 0.0)))
          {
             /* last full capacity */
             bat->mib[3] = 8;
             bat->mib[4] = 0;
             if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) != -1)
               {
                  bat->last_full_charge = (double)s.value;
                  if (bat->last_full_charge < 0) return EINA_TRUE;
               }

             /* remaining capacity */
             bat->mib[3] = 8;
             bat->mib[4] = 3;
             if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) != -1)
               {
                  charge = (double)s.value;
               }
          }

        bat->got_prop = 1;

        _time = ecore_time_get();
        if ((bat->got_prop) && (!EINA_FLT_EQ(charge, bat->current_charge)))
          bat->charge_rate = ((charge - bat->current_charge) / (_time - bat->last_update));
        bat->last_update = _time;
        bat->current_charge = charge;
        bat->percent = 100 * (bat->current_charge / bat->last_full_charge);
        if (bat->current_charge >= bat->last_full_charge)
          bat->percent = 100;

        if (bat->got_prop)
          {
             if (bat->charge_rate > 0)
               {
                  if (inst->cfg->batman.fuzzy && (++inst->cfg->batman.fuzzcount <= 10) && (bat->time_full > 0))
                    bat->time_full = (((bat->last_full_charge - bat->current_charge) / bat->charge_rate) + bat->time_full) / 2;
                  else
                    bat->time_full = (bat->last_full_charge - bat->current_charge) / bat->charge_rate;
                  bat->time_left = -1;
               }
             else
               {
                  if (inst->cfg->batman.fuzzy && (inst->cfg->batman.fuzzcount <= 10) && (bat->time_left > 0))
                    bat->time_left = (((0 - bat->current_charge) / bat->charge_rate) + bat->time_left) / 2;
                  else
                    bat->time_left = (0 - bat->current_charge) / bat->charge_rate;
                  bat->time_full = -1;
               }
          }
        else
          {
             bat->time_full = -1;
             bat->time_left = -1;
          }

        /* battery state 1: discharge, 2: charge */
        bat->mib[3] = 10;
        bat->mib[4] = 0;
        if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) == -1)
          {
             if (s.value == 2)
               bat->charging = 1;
             else
               bat->charging = 0;
          }
        _batman_device_update(bat->inst);
#elif defined(__FreeBSD__) || defined(__DragonFly__)

        if ((fd = open("/dev/acpi", O_RDONLY)) != -1)
          {
             battio.unit = i;

             if (ioctl(fd, ACPIIO_BATT_GET_BATTINFO, &battio) != -1)
               {
                  bat->got_prop = 1;

                  bat->percent = battio.battinfo.cap;

                  if (battio.battinfo.state & ACPI_BATT_STAT_CHARGING)
                    bat->charging = EINA_TRUE;
                  else
                    bat->charging = EINA_FALSE;

                  bat->time_left = battio.battinfo.min * 60;
                  if (bat->charge_rate > 0)
                    {
                       bat->time_full = (bat->last_full_charge - bat->current_charge) / bat->charge_rate;
                    }
               }
             else
               {
                  bat->time_full = bat->time_left = -1;
               }

             close(fd);
          }

        _batman_device_update(inst);
        i++;
#endif
     }

   EINA_LIST_FOREACH(batman_device_ac_adapters, l, ac)
     {
#if defined(__OpenBSD__) || defined(__NetBSD__)
        /* AC State */
        ac->mib[3] = 9;
        ac->mib[4] = 0;
        if (sysctl(ac->mib, 5, &s, &slen, NULL, 0) != -1)
          {
             if (s.value)
               ac->present = 1;
             else
               ac->present = 0;
          }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
        len = sizeof(value);
        if ((sysctl(ac->mib, 3, &value, &len, NULL, 0)) != -1)
          {
             ac->present = value;
          }
#endif
     }

   return EINA_TRUE;
}

