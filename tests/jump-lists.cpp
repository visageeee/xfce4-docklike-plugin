#include "Dock.hpp"
#include "JumpList.hpp"
#include <glib/gstdio.h>
#include <cassert>
#include <iostream>
#include <unistd.h>
static void pump(){for(int i=0;i<30;++i){while(g_main_context_iteration(nullptr,false)){}usleep(10000);}}
static GtkWidget* find(GtkWidget* menu,const char* text){GList* rows=gtk_container_get_children(GTK_CONTAINER(menu));GtkWidget* result=nullptr;for(auto* l=rows;l;l=l->next)if(g_strcmp0(gtk_menu_item_get_label(GTK_MENU_ITEM(l->data)),text)==0)result=GTK_WIDGET(l->data);g_list_free(rows);return result;}
int main(int argc,char** argv){
 gchar* dir=g_dir_make_tmp("dock-jump-test-XXXXXX",nullptr);assert(dir);std::string base=dir;g_free(dir);
 g_setenv("XDG_DATA_HOME",(base+"/data").c_str(),true);
 g_setenv("XDG_CONFIG_HOME",(base+"/config").c_str(),true);
 g_mkdir_with_parents((base+"/data").c_str(),0700);g_mkdir_with_parents((base+"/config").c_str(),0700);
 gtk_init(&argc,&argv);g_object_set(gtk_settings_get_default(),"gtk-recent-files-enabled",true,nullptr);
 auto* plugin=XFCE_PANEL_PLUGIN(g_object_new(XFCE_TYPE_PANEL_PLUGIN,"name","docklike-test","unique-id",108,nullptr));construct(plugin);
 std::string document=base+"/A document's name.txt",other=base+"/other.txt",secret=base+"/private.txt",missing=base+"/missing.txt";
 for(const auto& f:{document,other,secret})assert(g_file_set_contents(f.c_str(),"data",-1,nullptr));
 std::string executable=base+"/jump-capture",received=base+"/received";
 std::string script="#!/bin/sh\nprintf '%s' \"$1\" > '"+received+"'\n";
 assert(g_file_set_contents(executable.c_str(),script.c_str(),-1,nullptr));g_chmod(executable.c_str(),0700);
 auto* key=g_key_file_new();g_key_file_set_string(key,"Desktop Entry","Type","Application");g_key_file_set_string(key,"Desktop Entry","Name","JumpTest");g_key_file_set_string(key,"Desktop Entry","Exec",(executable+" %f").c_str());
 auto* desktop=g_desktop_app_info_new_from_keyfile(key);g_key_file_unref(key);assert(desktop);
 auto app=std::make_shared<AppInfo>("jump-test","","text-x-generic","JumpTest",desktop);
 auto* manager=gtk_recent_manager_get_default();

 auto add=[&](const std::string& path,const char* appName,bool priv){gchar* uri=g_filename_to_uri(path.c_str(),nullptr,nullptr);std::string command=std::string(appName==std::string("JumpTest")?executable:"/bin/false")+" %u";GtkRecentData data={};data.mime_type=const_cast<char*>("text/plain");data.app_name=const_cast<char*>(appName);data.app_exec=const_cast<char*>(command.c_str());data.is_private=priv;assert(gtk_recent_manager_add_full(manager,uri,&data));g_free(uri);};
 add(document,"JumpTest",false);add(other,"OtherApp",false);add(secret,"JumpTest",true);add(missing,"JumpTest",false);
 auto recent=JumpList::recentFiles(*app,manager);assert(recent.size()==1);assert(recent[0].label=="A document's name.txt");
 g_object_set(gtk_settings_get_default(),"gtk-recent-files-enabled",false,nullptr);assert(JumpList::recentFiles(*app,manager).empty());g_object_set(gtk_settings_get_default(),"gtk-recent-files-enabled",true,nullptr);
 auto* group=Dock::prepareGroup(app);auto keep=Dock::mGroups.get(app);group->mPinned=true;group->updateStyle();assert(group->mWindowsCount==0);
 std::cerr<<"Building menu\n";auto* menu=group->buildContextMenu();auto* row=find(menu,"A document's name.txt");assert(row);std::cerr<<"Launching file\n";gtk_menu_item_activate(GTK_MENU_ITEM(row));pump();gchar* contents=nullptr;assert(g_file_get_contents(received.c_str(),&contents,nullptr,nullptr));assert(contents==document);g_free(contents);
 std::cerr<<"Pin dialog\n";gtk_menu_item_activate(GTK_MENU_ITEM(find(menu,"Pin a file…")));pump();
 GList* tops=gtk_window_list_toplevels();GtkWidget* chooser=nullptr;for(auto* l=tops;l;l=l->next)if(GTK_IS_FILE_CHOOSER_DIALOG(l->data))chooser=GTK_WIDGET(l->data);g_list_free(tops);assert(chooser);
 assert(gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser),document.c_str()));pump();std::cerr<<"Accept pin\n";gtk_dialog_response(GTK_DIALOG(chooser),GTK_RESPONSE_ACCEPT);pump();gtk_widget_destroy(menu);
 std::cerr<<"Rebuild menu\n";menu=group->buildContextMenu();assert(find(menu,"Pinned files"));assert(find(menu,"A document's name.txt"));assert(!find(menu,"Recent files"));
 auto* unpin=find(menu,"Unpin file");assert(unpin);auto* sub=gtk_menu_item_get_submenu(GTK_MENU_ITEM(unpin));gtk_menu_item_activate(GTK_MENU_ITEM(find(sub,"A document's name.txt")));gtk_widget_destroy(menu);
 menu=group->buildContextMenu();assert(!find(menu,"Pinned files"));assert(find(menu,"Recent files"));gtk_widget_destroy(menu);
 std::cout<<"App-specific history, privacy, missing files, closed-app launch, filename quoting, pin persistence and unpin passed\n";
 return 0;
}
