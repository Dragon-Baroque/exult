/**
 ** A GTK widget showing the chunks from 'u7chunks'.
 **
 ** Written: 7/8/01 - JSF
 **/

#ifndef INCL_CHUNKLST
#define INCL_CHUNKLST 1

/*
Copyright (C) 2001-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "objbrowse.h"
#include "rect.h"
#include "shapedraw.h"

#include <iosfwd>
#include <vector>

class Image_buffer8;
class Shape_group;

/*
 *  Store information about an individual chunk shown in the list.
 */
class Chunk_info {
	friend class Chunk_chooser;
	int      num;
	TileRect box;    // Box where drawn.

	void set(int n, int rx, int ry, int rw, int rh) {
		num = n;
		box = TileRect(rx, ry, rw, rh);
	}
};

/*
 *  This class manages the list of chunks.
 */
class Chunk_chooser : public Object_browser, public Shape_draw {
	std::istream& chunkfile;    // Where chunks are read from (each is
	//   256 shape ID's (2 or 3 bytes).
	GtkWidget* sbar;          // Status bar.
	guint      sbar_sel;      // Status bar context for selection.
	int        num_chunks;    // Total # of chunks.
	// List of chunks we've read in.
	std::vector<unsigned char*> chunklist;
	int                         chunksz;     // # bytes/chunk.
	int                         headersz;    // # bytes in header.
	Chunk_info*                 info;        // An entry for each chunk drawn.
	int                         info_cnt;    // # entries in info.
	int  locate_cx, locate_cy;               // Last chunk found by 'locate'.
	bool drop_enabled;                       // So we only do it once.
	int  to_del;                             // Terrain # to delete, or -1.
	void (*sel_changed)();                   // Called when selection changes.
	// Blit onto screen.
	int  voffset;
	int  per_row;
	void show(int x, int y, int w, int h) override;
	void tell_server();
	void select(int new_sel);    // Show new selection.
	void render() override;      // Draw list.
	void setup_info(bool savepos = true) override;

	void set_background_color(guint32 c) override {
		Shape_draw::set_background_color(c);
	}

	int get_selected_id() override {
		return selected < 0 ? -1 : info[selected].num;
	}

	unsigned char* get_chunk(int chunknum);
	void           update_num_chunks(int new_num_chunks);
	void           set_chunk(const unsigned char* data, int datalen);
	void render_chunk(int chunknum, Image_buffer8* rwin, int xoff, int yoff);
	void scroll(int newpixel);    // Scroll.
	void scroll(bool upwards);
	void enable_controls();    // Enable/disable controls after sel.
	//   has changed.
	GMenu* create_popup() override;    // Popup menu.
public:
	Chunk_chooser(
			Vga_file* i, std::istream& cfile, unsigned char* palbuf, int w,
			int h, Shape_group* g = nullptr);
	~Chunk_chooser() override;
	bool server_response(int id, unsigned char* data, int datalen) override;
	void end_terrain_editing() override;
	// Turn off selection.
	void unselect(bool need_render = true);

	bool is_selected() {    // Is a chunk selected?
		return selected >= 0;
	}

	void set_selected_callback(void (*fun)()) {
		sel_changed = fun;
	}

	int get_count();              // Get # chunks we can display.
#if GTK_CHECK_VERSION(4, 0, 0)    // GTK 4
	// Configure when created/resized.
	static gint configure(
			GtkWidget* widget, int width, int height, gpointer user_data);
	// Blit to screen.
	static void expose(
			GtkDrawingArea* widget, cairo_t* cairo, int x, int y,
			gpointer user_data);
	// Handle mouse press.
	static gint mouse_press(
			GtkGestureClick* click_ctlr, int n_press, double x, double y,
			gpointer user_data);
#else                             // GTK 4
	// Configure when created/resized.
	static gint configure(
			GtkWidget* widget, GdkEvent* event, gpointer user_data);
	// Blit to screen.
	static gint expose(GtkWidget* widget, cairo_t* cairo, gpointer user_data);
	// Handle mouse press.
	static gint mouse_press(
			GtkWidget* widget, GdkEvent* event, gpointer user_data);
#endif                            // GTK 4
#if GTK_CHECK_VERSION(4, 0, 0)    // GTK 4
	// Give dragged chunk.
	static GdkContentProvider* drag_prepare(
			GtkDragSource* source, double x, double y, gpointer user_data);
	static void drag_begin(
			GtkDragSource* source, GdkDrag* drag, gpointer user_data);
	// Handler for drop.
	static gboolean drag_data_received(
			GtkDropTarget* dest, GValue* value, gdouble x, gdouble y,
			gpointer user_data);
	void enable_drop();
#else     // GTK 4
	// Give dragged chunk.
	static void drag_data_get(
			GtkWidget* widget, GdkDragContext* context,
			GtkSelectionData* seldata, guint info, guint time,
			gpointer user_data);
	static gint drag_begin(
			GtkWidget* widget, GdkDragContext* context, gpointer user_data);
	// Handler for drop.
	static void drag_data_received(
			GtkWidget* widget, GdkDragContext* context, gint x, gint y,
			GtkSelectionData* seldata, guint info, guint time,
			gpointer user_data);
	void enable_drop();
#endif    // GTK 4
	// Handle scrollbar.
	static void scrolled(GtkAdjustment* adj, gpointer user_data);
	void        locate(int dir);    // Locate terrain on game map.
	void        locate(bool upwards) override;
	void        locate_response(const unsigned char* data, int datalen);
	void        insert(bool dup);    // Insert new chunk.
	void        del();               // Delete current chunk.
	void        insert_response(const unsigned char* data, int datalen);
	void        delete_response(const unsigned char* data, int datalen);
	void        move(bool upwards) override;    // Move current selected chunk.
	void        swap_response(const unsigned char* data, int datalen);
#if GTK_CHECK_VERSION(4, 0, 0)    // GTK 4
#else                             // GTK 4
	static gint drag_motion(
			GtkWidget* widget, GdkEvent* event, gpointer user_data);
#endif                            // GTK 4
};

#endif
