#include "Dock.hpp"
#include <cassert>
#include <iostream>
int main(int argc, char** argv) {
 gtk_init(&argc, &argv);
 GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
 auto* plugin = XFCE_PANEL_PLUGIN(g_object_new(XFCE_TYPE_PANEL_PLUGIN, "name", "docklike-test", "unique-id", 99, nullptr));
 gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(plugin));
 construct(plugin);
 Dock::onPanelResize(48);
 for (int i=0;i<20;++i) {
  auto info=std::make_shared<AppInfo>(std::to_string(i), "", "application-x-executable", "Test");
  auto g=std::make_shared<Group>(info,true);
  Dock::mGroups.push(info,g);
  gtk_container_add(GTK_CONTAINER(Dock::mBox),g->mButton);
 }
 gtk_widget_show_all(window);
 for (int orientation=0;orientation<2;++orientation) {
  Dock::onPanelOrientationChange(static_cast<GtkOrientation>(orientation));
  for (int length : {1000,400,200,80,20,1000}) {
   GtkAllocation a={0,0,orientation?48:length,orientation?length:48};
   gtk_widget_size_allocate(Dock::mBox,&a);
   GList* children=gtk_container_get_children(GTK_CONTAINER(Dock::mBox));
   int end=0;
   for(GList* c=children;c;c=c->next) {
    GtkAllocation b; gtk_widget_get_allocation(GTK_WIDGET(c->data),&b);
    const int pos=orientation?b.y:b.x;
    const int size=orientation?b.height:b.width;
    assert(pos>=end); assert(pos+size<=length);
    end=pos+size;
   }
   g_list_free(children);
   int minimum,natural;
   if(orientation) gtk_widget_get_preferred_height(Dock::mBox,&minimum,&natural);
   else gtk_widget_get_preferred_width(Dock::mBox,&minimum,&natural);
   assert(minimum==20); assert(natural==996);
   std::cout<<orientation<<" "<<length<<" icon="<<Dock::mIconSize<<" extent="<<end<<std::endl;
   if(length==1000) assert(Dock::mIconSize==38);
  }
 }
 GList* children=gtk_container_get_children(GTK_CONTAINER(Dock::mBox));
 int index=0;
 for(GList* c=children;c;c=c->next) if(index++>=10) gtk_widget_hide(GTK_WIDGET(c->data));
 int minimum,natural;
 gtk_widget_get_preferred_height(Dock::mBox,&minimum,&natural);
 assert(minimum==10 && natural==496);
 GtkAllocation reduced={0,0,48,500};
 gtk_widget_size_allocate(Dock::mBox,&reduced);
 assert(Dock::mIconSize==38);
 g_list_free(children);
 std::cout<<"GTK allocation checks passed"<<std::endl;
 // Avoid unrelated panel teardown, normally managed by the host.
 return 0;
}
