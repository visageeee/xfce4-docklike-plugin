// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <gtk/gtk.h>
#include <algorithm>
#include "Settings.hpp"

namespace IconHover
{
struct State
{
 double amount = 0, start = 0, target = 0;
 gint64 started = 0;
 guint tick = 0;
};
inline State* state(GtkWidget* image)
{
 return static_cast<State*>(g_object_get_data(G_OBJECT(image), "docklike-icon-hover"));
}
inline void target(GtkWidget* image, double value)
{
 auto* s = state(image);
 if (!Settings::iconHoverEnabled || Settings::iconHoverBrightness == 0) value = 0;
 if (s->target == value) return;
 s->start = s->amount; s->target = value;
 GdkFrameClock* clock = gtk_widget_get_frame_clock(image);
 gboolean animate = true;
 g_object_get(gtk_widget_get_settings(image), "gtk-enable-animations", &animate, nullptr);
 if (!animate || !clock) {
  if (s->tick) { gtk_widget_remove_tick_callback(image, s->tick); s->tick = 0; }
  s->amount = value; gtk_widget_queue_draw(image); return;
 }
 s->started = gdk_frame_clock_get_frame_time(clock);
 if (s->tick) return;
 s->tick = gtk_widget_add_tick_callback(image, +[](GtkWidget* widget, GdkFrameClock* frame, gpointer) -> gboolean {
  auto* animation = state(widget);
  const double duration = animation->target > animation->start ? 90000.0 : 140000.0;
  const double t = std::min(1.0, std::max(0.0, (gdk_frame_clock_get_frame_time(frame) - animation->started) / duration));
  const double eased = 1.0 - (1.0-t)*(1.0-t);
  animation->amount = animation->start + (animation->target-animation->start)*eased;
  gtk_widget_queue_draw(widget);
  if (t >= 1.0) { animation->tick = 0; return G_SOURCE_REMOVE; }
  return G_SOURCE_CONTINUE;
 }, nullptr, nullptr);
}
inline void bind(GtkWidget* image, GtkWidget* button)
{
 g_object_set_data_full(G_OBJECT(image), "docklike-icon-hover", new State(), +[](gpointer p) { delete static_cast<State*>(p); });
 g_signal_connect(image, "draw", G_CALLBACK(+[](GtkWidget* widget, cairo_t* cr, gpointer) -> gboolean {
  const double amount = state(widget)->amount;
  if (amount <= 0 || !Settings::iconHoverEnabled || Settings::iconHoverBrightness == 0) return false;
  cairo_save(cr);
  cairo_push_group(cr);
  // Invoke GtkImage's draw implementation directly, without re-emitting draw.
  GTK_WIDGET_CLASS(g_type_class_peek(GTK_TYPE_IMAGE))->draw(widget, cr);
  // Lift the icon's colours while retaining its original alpha silhouette.
  cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
  cairo_set_source_rgba(cr, 1, 1, 1, Settings::iconHoverBrightness.get() / 100.0 * amount);
  cairo_paint(cr);
  cairo_pop_group_to_source(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  cairo_paint(cr);
  cairo_restore(cr);
  return true;
 }), nullptr);
 gtk_widget_add_events(button, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
 g_signal_connect_object(button, "enter-notify-event", G_CALLBACK(+[](GtkWidget*, GdkEventCrossing* event, GtkWidget* icon) -> gboolean {
  if (event->detail != GDK_NOTIFY_INFERIOR) target(icon, 1);
  return false;
 }), G_OBJECT(image), GConnectFlags(0));
 g_signal_connect_object(button, "leave-notify-event", G_CALLBACK(+[](GtkWidget*, GdkEventCrossing* event, GtkWidget* icon) -> gboolean {
  if (event->detail != GDK_NOTIFY_INFERIOR) target(icon, 0);
  return false;
 }), G_OBJECT(image), GConnectFlags(0));
 g_signal_connect(image, "unmap", G_CALLBACK(+[](GtkWidget* widget, gpointer) {
  auto* s = state(widget);
  if (s->tick) gtk_widget_remove_tick_callback(widget, s->tick);
  *s = State();
 }), nullptr);
}
}
