#include "Dock.hpp"
#include "LauncherEntry.hpp"
#include <cassert>
#include <iostream>
#include <unistd.h>
static void pump() { for(int n=0;n<30;++n){while(g_main_context_iteration(nullptr,false)){} usleep(10000);} }
int main(int argc,char**argv) {
 gtk_init(&argc,&argv);
 auto* plugin=XFCE_PANEL_PLUGIN(g_object_new(XFCE_TYPE_PANEL_PLUGIN,"name","docklike-test","unique-id",104,nullptr));
 construct(plugin);
 Settings::disableLauncherCounts.set(false);
 GError* error=nullptr;
 auto* publisher=g_dbus_connection_new_for_address_sync(g_getenv("DBUS_SESSION_BUS_ADDRESS"),GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT|G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),nullptr,nullptr,&error);
 assert(publisher && !error);pump();
 auto emit=[&](gint64 count,bool visible) {
  GVariantBuilder properties;g_variant_builder_init(&properties,G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&properties,"{sv}","count",g_variant_new_int64(count));
  g_variant_builder_add(&properties,"{sv}","count-visible",g_variant_new_boolean(visible));
  assert(g_dbus_connection_emit_signal(publisher,nullptr,"/badge/test","com.canonical.Unity.LauncherEntry","Update",g_variant_new("(sa{sv})","application://badge-test.desktop",&properties),&error));
  g_dbus_connection_flush_sync(publisher,nullptr,&error);assert(!error);pump();
 };
 emit(7,true);
 auto info=std::make_shared<AppInfo>("badge-test","","application-x-executable","Badge Test");
 Group* group=Dock::prepareGroup(info);
 assert(group->mLauncherCountVisible && group->mLauncherCount==7);
 assert(std::string(gtk_label_get_text(GTK_LABEL(group->mLauncherLabel)))=="7");
 emit(120,true);
 assert(std::string(gtk_label_get_text(GTK_LABEL(group->mLauncherLabel)))=="99+");
 emit(0,true);assert(!gtk_widget_get_visible(group->mLauncherLabel));
 emit(4,true);Settings::disableLauncherCounts.set(true);assert(!group->mLauncherCountVisible);
 Settings::disableLauncherCounts.set(false);assert(group->mLauncherCountVisible && group->mLauncherCount==4);
 emit(4,false);assert(!group->mLauncherCountVisible);
 emit(9,true);g_dbus_connection_close_sync(publisher,nullptr,&error);g_object_unref(publisher);pump();
 assert(!group->mLauncherCountVisible);
 std::cout<<"Cached badges, updates, zero/hidden counts, 99+, preference and sender exit passed\n";
 return 0;
}
