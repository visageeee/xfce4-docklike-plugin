// SPDX-License-Identifier: GPL-3.0-or-later
#include "JumpList.hpp"
#include "Plugin.hpp"
#include <algorithm>
#include <set>

namespace JumpList
{
namespace
{
std::string lower(const char* text)
{
 gchar* s=g_utf8_strdown(text ? text : "", -1); std::string result=s; g_free(s); return result;
}
std::string basename(const char* text)
{
 gchar* s=g_path_get_basename(text ? text : ""); auto result=lower(s); g_free(s); return result;
}
std::string commandName(const char* command)
{
 gchar** args=nullptr; gint count=0;
 if(!command || !g_shell_parse_argv(command,&count,&args,nullptr)) return "";
 std::string name=count ? basename(args[0]) : ""; g_strfreev(args);
 // Generic launchers cannot identify an app on their own.
 if(name=="env" || name=="flatpak" || name=="snap" || name=="sh" || name=="bash" || name=="python3") return "";
 return name;
}
void errorDialog(const char* message)
{
 auto* dialog=gtk_message_dialog_new(nullptr,GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",message);
 g_signal_connect(dialog,"response",G_CALLBACK(+[](GtkDialog* d,gint,gpointer){gtk_widget_destroy(GTK_WIDGET(d));}),nullptr);
 gtk_widget_show(dialog);
}
std::string storagePath()
{
 gchar* path=xfce_panel_plugin_save_location(Plugin::mXfPlugin,true);
 std::string result=path ? std::string(path)+".jump-lists" : ""; g_free(path); return result;
}
std::string appKey(const AppInfo& app)
{
 const auto& id=app.mId.empty() ? app.mPath : app.mId;
 gchar* sum=g_compute_checksum_for_string(G_CHECKSUM_SHA256,id.c_str(),-1);
 std::string key=sum; g_free(sum); return key;
}
std::vector<std::string> pins(const AppInfo& app)
{
 auto* file=g_key_file_new(); auto path=storagePath(); std::vector<std::string> result;
 if(g_key_file_load_from_file(file,path.c_str(),G_KEY_FILE_NONE,nullptr)) {
  gsize count=0; gchar** uris=g_key_file_get_string_list(file,appKey(app).c_str(),"files",&count,nullptr);
  for(gsize i=0;i<count;++i) result.emplace_back(uris[i]);
  g_strfreev(uris);
 }
 g_key_file_unref(file); return result;
}
void setPin(const AppInfo& app,const std::string& uri,bool pin)
{
 auto path=storagePath(); if(path.empty()){errorDialog(_("Could not locate the pinned-file settings."));return;}
 auto* file=g_key_file_new(); GError* error=nullptr;
 if(!g_key_file_load_from_file(file,path.c_str(),G_KEY_FILE_NONE,&error)) {
  if(!g_error_matches(error,G_FILE_ERROR,G_FILE_ERROR_NOENT)){errorDialog(error->message);g_error_free(error);g_key_file_unref(file);return;}
  g_clear_error(&error);
 }
 auto list=pins(app); list.erase(std::remove(list.begin(),list.end(),uri),list.end());
 if(pin) list.push_back(uri);
 std::vector<const gchar*> data; for(const auto& entry:list)data.push_back(entry.c_str());
 g_key_file_set_string_list(file,appKey(app).c_str(),"files",data.data(),data.size());
 if(!g_key_file_save_to_file(file,path.c_str(),&error)){errorDialog(error->message);g_error_free(error);}
 g_key_file_unref(file);
}
struct Action { std::shared_ptr<AppInfo> app; std::string uri; };
void openFile(const Action& action)
{
 auto* context=gdk_display_get_app_launch_context(gdk_display_get_default());
 gdk_app_launch_context_set_timestamp(context,gtk_get_current_event_time());
 GList* uris=g_list_append(nullptr,const_cast<char*>(action.uri.c_str())); GError* error=nullptr;
 if(!g_app_info_launch_uris(G_APP_INFO(action.app->mGAppInfo.get()),uris,G_APP_LAUNCH_CONTEXT(context),&error)) {
  errorDialog(error->message);g_error_free(error);
 }
 g_list_free(uris);g_object_unref(context);
}
std::string fileLabel(const std::string& uri)
{
 auto* file=g_file_new_for_uri(uri.c_str());gchar* name=g_file_get_basename(file);
 std::string label=name ? name : uri;g_free(name);g_object_unref(file);return label;
}
GtkWidget* item(GtkWidget* menu,const std::string& label,std::shared_ptr<AppInfo> app,const std::string& uri,bool unpin=false)
{
 auto* row=gtk_menu_item_new_with_label(label.c_str());
 auto* action=new Action{app,uri};g_object_set_data_full(G_OBJECT(row),"jump-action",action,+[](gpointer p){delete static_cast<Action*>(p);});
 g_signal_connect(row,"activate",G_CALLBACK(+[](GtkMenuItem* menuItem,gpointer removeFlag){
  auto* selected=static_cast<Action*>(g_object_get_data(G_OBJECT(menuItem),"jump-action"));
  if(removeFlag)setPin(*selected->app,selected->uri,false);else openFile(*selected);
 }),GINT_TO_POINTER(unpin));
 auto* file=g_file_new_for_uri(uri.c_str());gchar* location=g_file_get_parse_name(file);
 gtk_widget_set_tooltip_text(row,location);g_free(location);g_object_unref(file);
 auto* labelWidget=gtk_bin_get_child(GTK_BIN(row));
 gtk_label_set_max_width_chars(GTK_LABEL(labelWidget),42);gtk_label_set_ellipsize(GTK_LABEL(labelWidget),PANGO_ELLIPSIZE_MIDDLE);
 gtk_menu_shell_append(GTK_MENU_SHELL(menu),row);return row;
}
void heading(GtkWidget* menu,const char* label)
{
 auto* row=gtk_menu_item_new_with_label(label);gtk_widget_set_sensitive(row,false);gtk_menu_shell_append(GTK_MENU_SHELL(menu),row);
}
}
std::vector<File> recentFiles(const AppInfo& app,GtkRecentManager* manager)
{
 std::vector<File> result; gboolean enabled=true;
 g_object_get(gtk_settings_get_default(),"gtk-recent-files-enabled",&enabled,nullptr);
 if(!enabled || !app.mGAppInfo) return result;
 auto* desktop=G_APP_INFO(app.mGAppInfo.get());
 std::set<std::string> names={lower(app.mId.c_str()),lower(app.mName.c_str()),lower(g_app_info_get_name(desktop))};
 names.insert(lower(g_app_info_get_id(desktop)));
 const auto exec=commandName(g_app_info_get_commandline(desktop));if(!exec.empty()) names.insert(exec);
 names.erase("");
 GList* items=gtk_recent_manager_get_items(manager);
 for(GList* it=items;it;it=it->next) {
  auto* info=static_cast<GtkRecentInfo*>(it->data);
  if(gtk_recent_info_get_private_hint(info))continue;
  // Restrict automatic history to existing local files; pinned files are explicit.
  if(!gtk_recent_info_is_local(info) || !gtk_recent_info_exists(info))continue;
  gsize count=0;gchar** apps=gtk_recent_info_get_applications(info,&count);bool matches=false;time_t latest=0;
  for(gsize i=0;i<count;++i) {
   const gchar* command=nullptr;guint uses=0;time_t modified=0;
   gtk_recent_info_get_application_info(info,apps[i],&command,&uses,&modified);
   if(names.count(lower(apps[i])) || (!exec.empty() && commandName(command)==exec)) {matches=true;latest=std::max(latest,modified);}
  }
  g_strfreev(apps);
  if(matches)result.push_back({gtk_recent_info_get_uri(info),gtk_recent_info_get_display_name(info),latest});
 }
 g_list_free_full(items,reinterpret_cast<GDestroyNotify>(gtk_recent_info_unref));
 std::stable_sort(result.begin(),result.end(),[](const File& a,const File& b){return a.modified>b.modified;});
 return result;
}
void append(GtkWidget* menu,std::shared_ptr<AppInfo> app)
{
 if(!app->mGAppInfo)return;
 auto* desktop=G_APP_INFO(app->mGAppInfo.get());
 if(!g_app_info_supports_files(desktop) && !g_app_info_supports_uris(desktop))return;
 auto pinned=pins(*app);auto recent=recentFiles(*app,gtk_recent_manager_get_default());
 if(!pinned.empty()) {
  heading(menu,_("Pinned files"));for(const auto& uri:pinned)item(menu,fileLabel(uri),app,uri);
 }
 unsigned shown=0;
 for(const auto& file:recent) {
  if(std::find(pinned.begin(),pinned.end(),file.uri)!=pinned.end())continue;
  if(shown++==0)heading(menu,_("Recent files"));
  item(menu,file.label,app,file.uri);if(shown==10)break;
 }
 auto* add=gtk_menu_item_new_with_label(_("Pin a file…"));
 g_object_set_data_full(G_OBJECT(add),"jump-app",new std::shared_ptr<AppInfo>(app),+[](gpointer p){delete static_cast<std::shared_ptr<AppInfo>*>(p);});
 g_signal_connect(add,"activate",G_CALLBACK(+[](GtkMenuItem* row,gpointer){
  auto selectedApp=*static_cast<std::shared_ptr<AppInfo>*>(g_object_get_data(G_OBJECT(row),"jump-app"));
  auto* dialog=gtk_file_chooser_dialog_new(_("Pin a file"),nullptr,GTK_FILE_CHOOSER_ACTION_OPEN,_("Cancel"),GTK_RESPONSE_CANCEL,_("Pin"),GTK_RESPONSE_ACCEPT,nullptr);
  g_object_set_data_full(G_OBJECT(dialog),"jump-app",new std::shared_ptr<AppInfo>(selectedApp),+[](gpointer p){delete static_cast<std::shared_ptr<AppInfo>*>(p);});
  g_signal_connect(dialog,"response",G_CALLBACK(+[](GtkDialog* responseDialog,gint response,gpointer){
   if(response==GTK_RESPONSE_ACCEPT){gchar* uri=gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(responseDialog));auto chosenApp=*static_cast<std::shared_ptr<AppInfo>*>(g_object_get_data(G_OBJECT(responseDialog),"jump-app"));if(uri)setPin(*chosenApp,uri,true);g_free(uri);}
   gtk_widget_destroy(GTK_WIDGET(responseDialog));
  }),nullptr);gtk_widget_show(dialog);
 }),nullptr);gtk_menu_shell_append(GTK_MENU_SHELL(menu),add);
 if(!pinned.empty()) {
  auto* remove=gtk_menu_item_new_with_label(_("Unpin file"));auto* submenu=gtk_menu_new();
  for(const auto& uri:pinned)item(submenu,fileLabel(uri),app,uri,true);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(remove),submenu);gtk_menu_shell_append(GTK_MENU_SHELL(menu),remove);
 }
 gtk_menu_shell_append(GTK_MENU_SHELL(menu),gtk_separator_menu_item_new());
}
}
