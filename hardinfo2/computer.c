/*
 *    HardInfo - Displays System Information
 *    Copyright (C) 2003-2008 Leandro A. F. Pereira <leandro@hardinfo.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, version 2.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <config.h>
#include <time.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#include <hardinfo.h>
#include <iconcache.h>
#include <shell.h>

#include <vendor.h>

/* Callbacks */
gchar *callback_summary();
gchar *callback_os();
gchar *callback_modules();
gchar *callback_boots();
gchar *callback_locales();
gchar *callback_fs();
gchar *callback_display();
gchar *callback_network();
gchar *callback_users();
gchar *callback_env_var();
gchar *callback_dev();

/* Scan callbacks */
void scan_summary(gboolean reload);
void scan_os(gboolean reload);
void scan_modules(gboolean reload);
void scan_boots(gboolean reload);
void scan_locales(gboolean reload);
void scan_fs(gboolean reload);
void scan_display(gboolean reload);
void scan_network(gboolean reload);
void scan_users(gboolean reload);
void scan_env_var(gboolean reload);
void scan_dev(gboolean reload);

static ModuleEntry entries[] = {
    {"Summary", "summary.png", callback_summary, scan_summary},
    {"Operating System", "os.png", callback_os, scan_os},
    {"Kernel Modules", "module.png", callback_modules, scan_modules},
    {"Boots", "boot.png", callback_boots, scan_boots},
    {"Languages", "language.png", callback_locales, scan_locales},
    {"Filesystems", "dev_removable.png", callback_fs, scan_fs},
    {"Display", "monitor.png", callback_display, scan_display},
    {"Environment Variables", "environment.png", callback_env_var, scan_env_var},
    {"Development", "devel.png", callback_dev, scan_dev},
    {"Users", "users.png", callback_users, scan_users},
    {NULL},
};

#include "computer.h"

static GHashTable *moreinfo = NULL;
static gchar *module_list = NULL;
static Computer *computer = NULL;

#include <arch/this/modules.h>
#include <arch/common/languages.h>
#include <arch/this/alsa.h>
#include <arch/common/display.h>
#include <arch/this/loadavg.h>
#include <arch/this/memory.h>
#include <arch/this/uptime.h>
#include <arch/this/os.h>
#include <arch/this/filesystem.h>
#include <arch/common/users.h>
#include <arch/this/boots.h>
#include <arch/common/environment.h>

gchar *hi_more_info(gchar * entry)
{
    gchar *info = (gchar *) g_hash_table_lookup(moreinfo, entry);

    if (info)
	return g_strdup(info);

    return g_strdup_printf("[%s]", entry);
}

gchar *hi_get_field(gchar * field)
{
    gchar *tmp;

    if (g_str_equal(field, "Memory")) {
	MemoryInfo *mi = computer_get_memory();
	tmp = g_strdup_printf("%dMB (%dMB used)", mi->total, mi->used);
	g_free(mi);
    } else if (g_str_equal(field, "Uptime")) {
	tmp = computer_get_formatted_uptime();
    } else if (g_str_equal(field, "Date/Time")) {
	time_t t = time(NULL);

	tmp = g_new0(gchar, 64);
	strftime(tmp, 64, "%c", localtime(&t));
    } else if (g_str_equal(field, "Load Average")) {
	tmp = computer_get_formatted_loadavg();
    } else {
	tmp = g_strdup("");
    }

    return tmp;
}

void scan_summary(gboolean reload)
{
    SCAN_START();
    module_entry_scan_all_except(entries, 0);
    computer->alsa = computer_get_alsainfo();
    SCAN_END();
}

void scan_os(gboolean reload)
{
    SCAN_START();
    computer->os = computer_get_os();
    SCAN_END();
}

void scan_modules(gboolean reload)
{
    SCAN_START();
    scan_modules_do();
    SCAN_END();
}

void scan_boots(gboolean reload)
{
    SCAN_START();
    scan_boots_real();
    SCAN_END();
}

void scan_locales(gboolean reload)
{
    SCAN_START();
    scan_os(FALSE);
    scan_languages(computer->os);
    SCAN_END();
}

void scan_fs(gboolean reload)
{
    SCAN_START();
    scan_filesystems();
    SCAN_END();
}

void scan_display(gboolean reload)
{
    SCAN_START();
    computer->display = computer_get_display();
    SCAN_END();
}

void scan_users(gboolean reload)
{
    SCAN_START();
    scan_users_do();
    SCAN_END();
}

static gchar *dev_list = NULL;
void scan_dev(gboolean reload)
{
    SCAN_START();
    
    g_free(dev_list);
    dev_list = g_strdup("[Programming Languages]\n"
                        "None=yet\n");
    
    SCAN_END();
}

gchar *callback_dev()
{
    return dev_list;
}

/* Table based off imvirt by Thomas Liske <liske@ibh.de>
   Copyright (c) 2008 IBH IT-Service GmbH under GPLv2. */
gchar *computer_get_virtualization()
{
    gboolean found = FALSE;
    gint i, j;
    gchar *files[] = {
        "/proc/scsi/scsi",
        "/proc/cpuinfo",
        "/var/log/dmesg",
        NULL
    };
    struct {
        gchar *str;
        gchar *vmtype;
    } vm_types[] = {
        /* VMWare */
        { "VMWare", "VMWare" },
        { ": VMWare Virtual IDE CDROM Drive", "VMWare" },
        /* QEMU */
        { "QEMU", "QEMU" },
        { "QEMU Virtual CPU", "QEMU" },
        { ": QEMU HARDDISK", "QEMU" },
        { ": QEMU CD-ROM", "QEMU" },
        /* Generic Virtual Machine */
        { ": Virtual HD,", "Virtual Machine (Unknown)" },
        { ": Virtual CD,", "Virtual Machine (Unknown)" },
        /* Virtual Box */
        { "VBOX", "VirtualBox" },
        { ": VBOX HARDDISK", "VirtualBox" },
        { ": VBOX CD-ROM", "VirtualBox" },
        /* Xen */
        { "Xen virtual console", "Xen" },
        { "Xen reported: ", "Xen" },
        { "xen-vbd: registered block device", "Xen" },
        { NULL }
    };
    
    DEBUG("Detecting virtual machine");

    if (g_file_test("/proc/xen", G_FILE_TEST_EXISTS)) {
         DEBUG("/proc/xen found; assuming Xen");
         return g_strdup("Xen");
    }
    
    for (i = 0; files[i+1]; i++) {
         gchar buffer[512];
         FILE *file;
         
         if ((file = fopen(files[i], "r"))) {
              while (fgets(buffer, 512, file)) {
                  for (j = 0; vm_types[j+1].str; j++) {
                      if (strstr(buffer, vm_types[j].str)) {
                         found = TRUE;
                         break;
                      }
                  }
              }
              
              fclose(file);
              
              if (found) {
                  DEBUG("%s found (by reading file %s)",
                        vm_types[j].vmtype, files[i]);
                  return g_strdup(vm_types[j].vmtype);
              }
         }
         
    }
    
    DEBUG("no virtual machine detected; assuming physical machine");
    
    return g_strdup("Physical machine");
}

gchar *callback_summary()
{
    gchar *processor_name, *alsa_cards;
    gchar *input_devices, *printers;
    gchar *storage_devices, *summary;
    gchar *virt;
    
    processor_name  = module_call_method("devices::getProcessorName");
    alsa_cards      = computer_get_alsacards(computer);
    input_devices   = module_call_method("devices::getInputDevices");
    printers        = module_call_method("devices::getPrinters");
    storage_devices = module_call_method("devices::getStorageDevices");
    virt            = computer_get_virtualization();

    DEBUG("%s", virt);
    summary = g_strdup_printf("[$ShellParam$]\n"
			      "UpdateInterval$Memory=1000\n"
			      "UpdateInterval$Date/Time=1000\n"
			      "#ReloadInterval=5000\n"
			      "[Computer]\n"
			      "Processor=%s\n"
			      "Memory=...\n"
			      "Virtual Machine=%s\n"
			      "Operating System=%s\n"
			      "User Name=%s\n"
			      "Date/Time=...\n"
			      "[Display]\n"
			      "Resolution=%dx%d pixels\n"
			      "OpenGL Renderer=%s\n"
			      "X11 Vendor=%s\n"
			      "[Multimedia]\n"
			      "\n%s\n"
			      "[Input Devices]\n%s\n"
			      "\n%s\n"
			      "\n%s\n",
			      processor_name,
			      virt,
			      computer->os->distro,
			      computer->os->username,
			      computer->display->width,
			      computer->display->height,
			      computer->display->ogl_renderer,
			      computer->display->vendor,
			      alsa_cards,
			      input_devices, printers, storage_devices);

    g_free(processor_name);
    g_free(alsa_cards);
    g_free(input_devices);
    g_free(printers);
    g_free(storage_devices);
    g_free(virt);

    return summary;
}

gchar *callback_os()
{
    return g_strdup_printf("[$ShellParam$]\n"
			   "UpdateInterval$Uptime=10000\n"
			   "UpdateInterval$Load Average=1000\n"
			   "[Version]\n"
			   "Kernel=%s\n"
			   "Compiled=%s\n"
			   "C Library=%s\n"
			   "Default C Compiler=%s\n"
			   "Distribution=%s\n"
			   "[Current Session]\n"
			   "Computer Name=%s\n"
			   "User Name=%s\n"
			   "#Language=%s\n"
			   "Home Directory=%s\n"
			   "Desktop Environment=%s\n"
			   "[Misc]\n"
			   "Uptime=...\n"
			   "Load Average=...",
			   computer->os->kernel,
			   computer->os->compiled_date,
			   computer->os->libc,
			   computer->os->gcc,
			   computer->os->distro,
			   computer->os->hostname,
			   computer->os->username,
			   computer->os->language,
			   computer->os->homedir, computer->os->desktop);
}

gchar *callback_modules()
{
    return g_strdup_printf("[Loaded Modules]\n"
			   "%s"
			   "[$ShellParam$]\n"
			   "ViewType=1\n"
			   "ColumnTitle$TextValue=Name\n"
			   "ColumnTitle$Value=Description\n"
			   "ShowColumnHeaders=true\n", module_list);
}

gchar *callback_boots()
{
    return g_strdup_printf("[$ShellParam$]\n"
			   "ColumnTitle$TextValue=Date & Time\n"
			   "ColumnTitle$Value=Kernel Version\n"
			   "ShowColumnHeaders=true\n"
			   "\n"
			   "%s", computer->os->boots);
}

gchar *callback_locales()
{
    return g_strdup_printf("[$ShellParam$]\n"
			   "ViewType=1\n"
			   "ColumnTitle$TextValue=Language Code\n"
			   "ColumnTitle$Value=Name\n"
			   "ShowColumnHeaders=true\n"
			   "[Available Languages]\n"
			   "%s", computer->os->languages);
}

gchar *callback_fs()
{
    return g_strdup_printf("[$ShellParam$]\n"
			   "ViewType=4\n"
			   "ReloadInterval=5000\n"
			   "Zebra=1\n"
			   "NormalizePercentage=false\n"
			   "ColumnTitle$Extra1=Mount Point\n"
			   "ColumnTitle$Progress=Usage\n"
			   "ColumnTitle$TextValue=Device\n"
			   "ShowColumnHeaders=true\n"
			   "[Mounted File Systems]\n%s\n", fs_list);
}

gchar *callback_display()
{
    return g_strdup_printf("[Display]\n"
			   "Resolution=%dx%d pixels\n"
			   "Vendor=%s\n"
			   "Version=%s\n"
			   "[Monitors]\n"
			   "%s"
			   "[Extensions]\n"
			   "%s"
			   "[OpenGL]\n"
			   "Vendor=%s\n"
			   "Renderer=%s\n"
			   "Version=%s\n"
			   "Direct Rendering=%s\n",
			   computer->display->width,
			   computer->display->height,
			   computer->display->vendor,
			   computer->display->version,
			   computer->display->monitors,
			   computer->display->extensions,
			   computer->display->ogl_vendor,
			   computer->display->ogl_renderer,
			   computer->display->ogl_version,
			   computer->display->dri ? "Yes" : "No");
}

gchar *callback_users()
{
    return g_strdup_printf("[$ShellParam$]\n"
			   "ReloadInterval=10000\n"
			   "ViewType=1\n"
			   "[Users]\n"
			   "%s\n", users);
}

gchar *get_os_kernel(void)
{
    scan_os(FALSE);
    return g_strdup(computer->os->kernel);
}

gchar *get_kernel_module_description(gchar *module)
{
    gchar *description;
    
    if (!_module_hash_table) {
        scan_modules(FALSE);
    }
    
    description = g_hash_table_lookup(_module_hash_table, module);
    if (!description) {
        return NULL;
    }
    
    return g_strdup(description);
}

ShellModuleMethod *hi_exported_methods(void)
{
    static ShellModuleMethod m[] = {
	{"getOSKernel", get_os_kernel},
	{"getKernelModuleDescription", get_kernel_module_description},
	{NULL}
    };

    return m;
}

ModuleEntry *hi_module_get_entries(void)
{
    return entries;
}

gchar *hi_module_get_name(void)
{
    return g_strdup("Computer");
}

guchar hi_module_get_weight(void)
{
    return 80;
}

gchar **hi_module_get_dependencies(void)
{
    static gchar *deps[] = { "devices.so", NULL };

    return deps;
}

void hi_module_init(void)
{
    computer = g_new0(Computer, 1);
    moreinfo =
	g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

ModuleAbout *hi_module_get_about(void)
{
    static ModuleAbout ma[] = {
	{
	 .author = "Leandro A. F. Pereira",
	 .description = "Gathers high-level computer information",
	 .version = VERSION,
	 .license = "GNU GPL version 2"}
    };

    return ma;
}
