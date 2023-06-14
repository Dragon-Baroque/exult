/*
 * Copyright (C) 2016-2018 - Marzo Sette Torres Junior
 * Copyright (C) 2021-2022 - The Exult Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// This reader removes a lot of warnings of C-style casts. It should be included
// after GTK+ and GDK headers.
// No, I am not proud of this, but it works.

#ifndef INCL_GTK_REDEFINES
#define INCL_GTK_REDEFINES

// IWYU pragma: always_keep

#include <cstring>
#include <type_traits>

// Do casts from pointers without too much undefined behavior.
template <typename To, typename From>
inline To safe_pointer_cast(From pointer) {
	// NOLINTNEXTLINE(bugprone-sizeof-expression)
	constexpr const size_t SizeFrom = sizeof(From);
	// NOLINTNEXTLINE(bugprone-sizeof-expression)
	constexpr const size_t SizeTo = sizeof(To);
	static_assert(
			std::is_pointer_v<From> && std::is_pointer_v<To>
					&& SizeFrom == SizeTo,
			"Pointer sizes do not match");
	To output;
	std::memcpy(
			static_cast<void*>(&output), static_cast<void*>(&pointer),
			SizeFrom);
	return output;
}

template <typename T1, typename T2>
inline T1* gtk_cast(T2* obj) {
	if constexpr (std::is_same_v<T1, T2>) {
		return obj;
	}
	if constexpr (!std::is_same_v<T1, T2>) {
		return safe_pointer_cast<T1*>(obj);
	}
}

#ifdef __clang__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-macros"
#	pragma GCC diagnostic ignored "-Wreserved-macro-identifier"
#endif

// NOLINTBEGIN(bugprone-reserved-identifier)
#if defined(_G_TYPE_CIC)
#	undef _G_TYPE_CIC
#	if defined(G_DISABLE_CAST_CHECKS) || defined(__OPTIMIZE__)
#		define _G_TYPE_CIC(ip, gt, ct) gtk_cast<ct>(ip)
#	else /* G_DISABLE_CAST_CHECKS */
#		define _G_TYPE_CIC(ip, gt, ct)              \
			gtk_cast<ct>(g_type_check_instance_cast( \
					gtk_cast<GTypeInstance>(ip), gt))
#	endif /* G_DISABLE_CAST_CHECKS */
#endif     /* defined(_G_TYPE_CIC) */

#if defined(_G_TYPE_CCC)
#	undef _G_TYPE_CCC
#	if defined(G_DISABLE_CAST_CHECKS) || defined(__OPTIMIZE__)
#		define _G_TYPE_CCC(cp, gt, ct) gtk_cast<ct>(cp)
#	else /* G_DISABLE_CAST_CHECKS */
#		define _G_TYPE_CCC(cp, gt, ct) \
			gtk_cast<ct>(g_type_check_class_cast(gtk_cast<GTypeClass>(cp), gt))
#	endif /* G_DISABLE_CAST_CHECKS */
#endif     /* defined(_G_TYPE_CCC) */

#ifdef _G_TYPE_CHI
#	undef _G_TYPE_CHI
#	define _G_TYPE_CHI(ip) g_type_check_instance(gtk_cast<GTypeInstance>(ip))
#endif /* _G_TYPE_CHI */

#ifdef _G_TYPE_CHV
#	undef _G_TYPE_CHV
#	define _G_TYPE_CHV(vl) g_type_check_value(gtk_cast<GValue>(vl))
#endif /* _G_TYPE_CHV */

#ifdef _G_TYPE_IGC
#	undef _G_TYPE_IGC
#	define _G_TYPE_IGC(ip, gt, ct) \
		gtk_cast<ct>(gtk_cast<GTypeInstance>(ip)->g_class)
#endif /* _G_TYPE_IGC */

#ifdef _G_TYPE_IGI
#	undef _G_TYPE_IGI
#	define _G_TYPE_IGI(ip, gt, ct)         \
		gtk_cast<ct>(g_type_interface_peek( \
				gtk_cast<GTypeInstance>(ip)->g_class, gt))
#endif /* _G_TYPE_IGI */

#ifdef _G_TYPE_CIFT
#	undef _G_TYPE_CIFT
#	define _G_TYPE_CIFT(ip, ft)                  \
		g_type_check_instance_is_fundamentally_a( \
				gtk_cast<GTypeInstance>(ip), ft)
#endif /* _G_TYPE_CIFT */

#ifdef _G_TYPE_CIT
#	undef _G_TYPE_CIT
#	define _G_TYPE_CIT(ip, gt)                              \
		[&](GTypeInstance* ins, GType t) -> gboolean {       \
			if (!ins) {                                      \
				return FALSE;                                \
			}                                                \
			if (ins->g_class && ins->g_class->g_type == t) { \
				return TRUE;                                 \
			}                                                \
			return g_type_check_instance_is_a(ins, t);       \
		}(gtk_cast<GTypeInstance>(ip), gt)
#endif /* _G_TYPE_CIT */

#ifdef _G_TYPE_CCT
#	undef _G_TYPE_CCT
#	define _G_TYPE_CCT(cp, gt)                     \
		[&](GTypeClass* cls, GType t) -> gboolean { \
			if (!cls) {                             \
				return FALSE;                       \
			}                                       \
			if (cls->g_type == t) {                 \
				return TRUE;                        \
			}                                       \
			return g_type_check_class_is_a(cls, t); \
		}(gtk_cast<GTypeClass>(cp), gt)
#endif /* _G_TYPE_CCT */

#ifdef _G_TYPE_CVH
#	undef _G_TYPE_CVH
#	define _G_TYPE_CVH(vl, gt)                       \
		[&](const GValue* val, GType t) -> gboolean { \
			if (!val) {                               \
				return FALSE;                         \
			}                                         \
			if (val->g_type == t) {                   \
				return TRUE;                          \
			}                                         \
			return g_type_check_value_holds(val, t);  \
		}(gtk_cast<const GValue>(vl), gt)
#endif /* _G_TYPE_CVH */

#ifdef G_CALLBACK
#	undef G_CALLBACK
#	define G_CALLBACK(f) safe_pointer_cast<GCallback>(f)
#endif /* G_CALLBACK */

#ifdef G_TYPE_MAKE_FUNDAMENTAL
#	undef G_TYPE_MAKE_FUNDAMENTAL
#	define G_TYPE_MAKE_FUNDAMENTAL(x) \
		static_cast<GType>((x) << G_TYPE_FUNDAMENTAL_SHIFT)
#endif /* G_TYPE_MAKE_FUNDAMENTAL */

#if defined(g_list_previous) || defined(g_list_next)
#	undef g_list_previous
#	undef g_list_next
#	define g_list_previous(list) \
		((list) ? gtk_cast<GList>(list)->prev : nullptr)
#	define g_list_next(list) ((list) ? gtk_cast<GList>(list)->next : nullptr)
#endif /* defined(g_list_previous) || defined(g_list_next) */

#ifdef g_signal_connect
#	undef g_signal_connect
#	define g_signal_connect(instance, detailed_signal, c_handler, data)     \
		g_signal_connect_data(                                               \
				(instance), (detailed_signal), (c_handler), (data), nullptr, \
				static_cast<GConnectFlags>(0))
#endif /* g_signal_connect */

#ifdef g_utf8_next_char
#	undef g_utf8_next_char
#	define g_utf8_next_char(p) \
		((p) + g_utf8_skip[*safe_pointer_cast<const guchar*>(p)])
#endif /* g_utf8_next_char */

#ifdef G_LOG_DOMAIN
#	undef G_LOG_DOMAIN
#	define G_LOG_DOMAIN nullptr
#endif

// NOLINTEND(bugprone-reserved-identifier)
#ifdef __clang__
#	pragma GCC diagnostic pop
#endif

#if GTK_CHECK_VERSION(4, 0, 0)    // GTK 4

#	include "ignore_unused_variable_warning.h"
#	define GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER \
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER

gint gtk_dialog_run(GtkDialog* d);

static inline void gtk_window_resize(GtkWindow* w, int a, int b) {
	gtk_widget_set_size_request(GTK_WIDGET(w), a, b);
}

#	define gtk_buildable_get_name(w) gtk_buildable_get_buildable_id((w))

static inline void gdk_cairo_get_clip_rectangle(
		cairo_t* cr, GdkRectangle* rect) {
	double x1, y1, x2, y2;
	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
	x1           = floor(x1);
	y1           = floor(y1);
	x2           = ceil(x2);
	y2           = ceil(y2);
	rect->x      = CLAMP(x1, G_MININT, G_MAXINT);
	rect->y      = CLAMP(y1, G_MININT, G_MAXINT);
	rect->width  = CLAMP(x2 - x1, G_MININT, G_MAXINT);
	rect->height = CLAMP(y2 - y1, G_MININT, G_MAXINT);
}

#	define gtk_menu_bar_new_from_model(m) \
		gtk_popover_menu_bar_new_from_model((m))

static inline GtkWidget* gtk_popover_new_from_model(
		GtkWidget* p, GMenuModel* m) {
	GtkWidget* r
			= gtk_popover_menu_new_from_model_full(m, GTK_POPOVER_MENU_NESTED);
	gtk_widget_set_parent(r, p);
	return r;
}

#	define GtkBin               GtkWidget
#	define GtkContainer         GtkWidget
#	define GTK_BIN(w)           GTK_WIDGET((w))
#	define GTK_CONTAINER(w)     GTK_WIDGET((w))
#	define gtk_bin_get_child(b) gtk_widget_get_first_child(b)

#	define gtk_box_reorder_child(b, c, n) \
		gtk_box_reorder_child_after((b), (c), nullptr)

#	define GTK_BUTTON_BOX(w)          GTK_BOX((w))
#	define GTK_BUTTONBOX_START        0
#	define gtk_button_box_new(o)      gtk_box_new((o), 0)
#	define gtk_button_set_image(w, i) gtk_button_set_child((w), (i))

static inline bool gtk_widget_destroy(GtkWidget* w) {
	// Do not gtk_window_destroy a GTK4 GtkPopover
	//   since its GtkWindow ancestor is Studio main window.
	// Instead only unparent it :
	//   undoes the set_parent in gtk_popover_new_from_model,
	//   causes the right sequence : object_unref then widget_dispose.
	if (GTK_IS_POPOVER(w)) {
		gtk_widget_unparent(w);
		return true;
	}
	GtkWidget* win = gtk_widget_get_ancestor(w, GTK_TYPE_WINDOW);
	if (GTK_IS_WINDOW(win)) {
		gtk_window_destroy(GTK_WINDOW(win));
	}
	return true;
}

#	define gtk_entry_get_text(w) gtk_editable_get_text(GTK_EDITABLE((w)))
#	define gtk_entry_set_text(w, t) \
		gtk_editable_set_text(GTK_EDITABLE((w)), (t))

static inline bool gtk_css_provider_load_from_path(
		GtkCssProvider* p, const char* s, GError** e) {
	ignore_unused_variable_warning(e);
	gtk_css_provider_load_from_path(p, s);
	return true;
}

static inline void gtk_box_pack_start(
		GtkBox* b, GtkWidget* w, gboolean e, gboolean f, guint p) {
	gtk_box_append(b, w);
	if (gtk_orientable_get_orientation(GTK_ORIENTABLE(b))
		== GTK_ORIENTATION_HORIZONTAL) {
		gtk_widget_set_hexpand(w, e);
		gtk_widget_set_halign(w, f ? GTK_ALIGN_FILL : GTK_ALIGN_START);
		gtk_widget_set_margin_start(w, p + gtk_widget_get_margin_start(w));
		gtk_widget_set_margin_end(w, p + gtk_widget_get_margin_end(w));
	} else {
		gtk_widget_set_vexpand(w, e);
		gtk_widget_set_valign(w, f ? GTK_ALIGN_FILL : GTK_ALIGN_START);
		gtk_widget_set_margin_top(w, p + gtk_widget_get_margin_top(w));
		gtk_widget_set_margin_bottom(w, p + gtk_widget_get_margin_bottom(w));
	}
}

static inline void gtk_container_add(GtkWidget* b, GtkWidget* w) {
	if (GTK_IS_FRAME(b)) {
		gtk_frame_set_child(GTK_FRAME(b), w);
	}
	if (GTK_IS_BOX(b)) {
		gtk_box_append(GTK_BOX(b), w);
	}
	if (GTK_IS_VIEWPORT(b)) {
		gtk_viewport_set_child(GTK_VIEWPORT(b), w);
	}
}

#else     // GTK 4
#endif    // GTK 4
#endif
