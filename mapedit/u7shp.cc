/*
 * SHP loading file filter for The GIMP version 3.x.
 *
 * (C) 2000-2001 Tristan Tarrant
 * (C) 2001-2004 Willem Jan Palenstijn
 * (C) 2010-2025 The Exult Team
 *
 * You can find the most recent version of this file in the Exult sources,
 * available from https://exult.info/
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

/*
 * GIMP Side of the Shape load / export plugin
 */

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wvariadic-macros"
#	pragma GCC diagnostic ignored "-Wignored-qualifiers"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wpedantic"
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	else
#		pragma GCC diagnostic ignored "-Wc99-extensions"
#		pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#		if __clang_major__ >= 16
#			pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#		endif
#	endif
#endif    // __GNUC__
#ifdef USE_STRICT_GTK
#	define GTK_DISABLE_SINGLE_INCLUDES
#	define GSEAL_ENABLE
#	define GNOME_DISABLE_DEPRECATED
#	define GTK_DISABLE_DEPRECATED
#	define GDK_DISABLE_DEPRECATED
#else
#	define GDK_DISABLE_DEPRECATION_WARNINGS
#	define GLIB_DISABLE_DEPRECATION_WARNINGS
#endif    // USE_STRICT_GTK
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "U7obj.h"
#include "databuf.h"
#include "gtk_redefines.h"
#include "ibuf8.h"
#include "ignore_unused_variable_warning.h"
#include "vgafile.h"

#include <cerrno>
#include <iostream>
#include <string>
#include <vector>

/*
 * GIMP Side of the Shape load / export plugin
 *   Borrowed from plug-ins/common/file-cel.c
 *      -- KISS CEL file format plug-in
 *      -- (copyright) 1997,1998 Nick Lamb (njl195@zepler.org.uk)
 */

#define LOAD_PROC      "file-shp-load"
#define EXPORT_PROC    "file-shp-export"
#define PLUG_IN_BINARY "file-shp"
#define PLUG_IN_ROLE   "gimp-file-shp"

using Shp      = struct _Shp;
using ShpClass = struct _ShpClass;

struct _Shp {
	GimpPlugIn parent_instance;
};

struct _ShpClass {
	GimpPlugInClass parent_class;
};

#define SHP_TYPE (shp_get_type())
#define SHP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SHP_TYPE, Shp))

GType shp_get_type(void) G_GNUC_CONST;

static GList*         shp_query_procedures(GimpPlugIn* plug_in);
static GimpProcedure* shp_create_procedure(
		GimpPlugIn* plug_in, const gchar* name);

static GimpValueArray* shp_load(
		GimpProcedure* procedure, GimpRunMode run_mode, GFile* file,
		GimpMetadata* metadata, GimpMetadataLoadFlags* flags,
		GimpProcedureConfig* config, gpointer run_data);
static GimpValueArray* shp_export(
		GimpProcedure* procedure, GimpRunMode run_mode, GimpImage* image,
		GFile* file, GimpExportOptions* options, GimpMetadata* metadata,
		GimpProcedureConfig* config, gpointer run_data);
static gboolean shp_palette_dialog(
		const gchar* title, GimpProcedure* procedure,
		GimpProcedureConfig* config);

static gint       load_palette(GFile* file, guchar palette[], GError** error);
static GimpImage* load_image(
		GFile* file, GFile* palette_file, GimpRunMode run_mode, GError** error);
static gboolean export_image(
		GFile* file, GimpImage* image, GimpRunMode run_mode, GError** error);

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	if defined(__llvm__) || defined(__clang__)
#		if __clang_major__ >= 16
#			pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#		endif
#	endif
#endif    // __GNUC__
G_DEFINE_TYPE(Shp, shp, GIMP_TYPE_PLUG_IN)
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

GIMP_MAIN(SHP_TYPE)

static void shp_class_init(ShpClass* klass) {
	GimpPlugInClass* plug_in_class  = GIMP_PLUG_IN_CLASS(klass);
	plug_in_class->query_procedures = shp_query_procedures;
	plug_in_class->create_procedure = shp_create_procedure;
	plug_in_class->set_i18n         = nullptr;
}

static void shp_init(Shp* shp) {
	ignore_unused_variable_warning(shp);
}

static GList* shp_query_procedures(GimpPlugIn* plug_in) {
	ignore_unused_variable_warning(plug_in);
	GList* list = nullptr;
	list        = g_list_append(list, g_strdup(LOAD_PROC));
	list        = g_list_append(list, g_strdup(EXPORT_PROC));
	return list;
}

static GimpProcedure* shp_create_procedure(
		GimpPlugIn* plug_in, const gchar* name) {
	GimpProcedure* procedure = nullptr;
	if (!strcmp(name, LOAD_PROC)) {
		procedure = gimp_load_procedure_new(
				plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN, shp_load, nullptr,
				nullptr);
		gimp_procedure_set_menu_label(procedure, "Ultima VII Shape");
		gimp_procedure_set_documentation(
				procedure, "Loads files in Ultima VII Shape file format",
				"This plug-in loads individual Ultima VII "
				"Shape files.",
				name);
		gimp_procedure_set_attribution(
				procedure, "The Exult Team", "The Exult Team", "2010-2025");
		gimp_file_procedure_set_extensions(
				GIMP_FILE_PROCEDURE(procedure), "shp");
		gimp_procedure_add_file_argument(
				procedure, "palette-file", "_Palette file",
				"PAL file to load palette from", G_PARAM_READWRITE);
	} else if (!strcmp(name, EXPORT_PROC)) {
		procedure = gimp_export_procedure_new(
				plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN, FALSE, shp_export,
				nullptr, nullptr);
		gimp_procedure_set_image_types(procedure, "RGB*, INDEXED*");
		gimp_procedure_set_menu_label(procedure, "Ultima VII Shape");
		gimp_procedure_set_documentation(
				procedure, "Exports files in Ultima VII Shape file format",
				"This plug-in exports individual Ultima VII "
				"Shape files.",
				name);
		gimp_procedure_set_attribution(
				procedure, "The Exult Team", "The Exult Team", "2010-2025");
		gimp_file_procedure_set_handles_remote(
				GIMP_FILE_PROCEDURE(procedure), TRUE);
		gimp_file_procedure_set_extensions(
				GIMP_FILE_PROCEDURE(procedure), "shp");
		gimp_export_procedure_set_capabilities(
				GIMP_EXPORT_PROCEDURE(procedure),
				static_cast<GimpExportCapabilities>(
						GIMP_EXPORT_CAN_HANDLE_RGB
						| GIMP_EXPORT_CAN_HANDLE_ALPHA
						| GIMP_EXPORT_CAN_HANDLE_LAYERS
						| GIMP_EXPORT_CAN_HANDLE_INDEXED),
				nullptr, nullptr, nullptr);
		gimp_procedure_add_file_argument(
				procedure, "palette-file", "_Palette file",
				"PAL file to save palette to", G_PARAM_READWRITE);
	}
	return procedure;
}

static GimpValueArray* shp_load(
		GimpProcedure* procedure, GimpRunMode run_mode, GFile* file,
		GimpMetadata* metadata, GimpMetadataLoadFlags* flags,
		GimpProcedureConfig* config, gpointer run_data) {
	ignore_unused_variable_warning(
			procedure, run_mode, file, metadata, flags, config, run_data);
	GimpValueArray* return_vals;
	GimpImage*      image        = nullptr;
	GFile*          palette_file = nullptr;
	GError*         error        = nullptr;

	gegl_init(nullptr, nullptr);

	if (error != nullptr) {
		return gimp_procedure_new_return_values(
				procedure, GIMP_PDB_EXECUTION_ERROR, error);
	}

	g_object_get(config, "palette-file", &palette_file, nullptr);
	if (run_mode != GIMP_RUN_NONINTERACTIVE) {
		g_clear_object(&palette_file);
		if (shp_palette_dialog("Load PAL Palette", procedure, config)) {
			g_object_get(config, "palette-file", &palette_file, nullptr);
		}
	}

	image = load_image(file, palette_file, run_mode, &error);
	g_clear_object(&palette_file);
	if (!image) {
		return gimp_procedure_new_return_values(
				procedure, GIMP_PDB_EXECUTION_ERROR, error);
	}
	return_vals = gimp_procedure_new_return_values(
			procedure, GIMP_PDB_SUCCESS, nullptr);
	GIMP_VALUES_SET_IMAGE(return_vals, 1, image);
	return return_vals;
}

static GimpValueArray* shp_export(
		GimpProcedure* procedure, GimpRunMode run_mode, GimpImage* image,
		GFile* file, GimpExportOptions* options, GimpMetadata* metadata,
		GimpProcedureConfig* config, gpointer run_data) {
	ignore_unused_variable_warning(
			procedure, run_mode, image, file, options, metadata, config,
			run_data);
	GimpPDBStatusType status = GIMP_PDB_SUCCESS;
	GimpExportReturn  expret = GIMP_EXPORT_IGNORE;
	GError*           error  = nullptr;

	gegl_init(nullptr, nullptr);

	expret = gimp_export_options_get_image(options, &image);

	if (!export_image(file, image, run_mode, &error)) {
		status = GIMP_PDB_EXECUTION_ERROR;
	}

	if (expret == GIMP_EXPORT_EXPORT) {
		gimp_image_delete(image);
	}

	return gimp_procedure_new_return_values(procedure, status, error);
}

static gboolean shp_palette_dialog(
		const gchar* title, GimpProcedure* procedure,
		GimpProcedureConfig* config) {
	GtkWidget* dialog;
	gboolean   run;

	gimp_ui_init(PLUG_IN_BINARY);
	dialog = gimp_procedure_dialog_new(
			GIMP_PROCEDURE(procedure), GIMP_PROCEDURE_CONFIG(config), title);
	gimp_procedure_dialog_set_ok_label(GIMP_PROCEDURE_DIALOG(dialog), "_Open");
	gimp_procedure_dialog_fill(GIMP_PROCEDURE_DIALOG(dialog), nullptr);
	run = gimp_procedure_dialog_run(GIMP_PROCEDURE_DIALOG(dialog));

	gtk_widget_destroy(dialog);
	return run;
}

/*
 * Exult Side of the Shape load / export plugin
 */

static guchar gimp_cmap[768] = {
		0x00, 0x00, 0x00, 0xF8, 0xF0, 0xCC, 0xF4, 0xE4, 0xA4, 0xF0, 0xDC, 0x78,
		0xEC, 0xD0, 0x50, 0xEC, 0xC8, 0x28, 0xD8, 0xAC, 0x20, 0xC4, 0x94, 0x18,
		0xB0, 0x80, 0x10, 0x9C, 0x68, 0x0C, 0x88, 0x54, 0x08, 0x74, 0x44, 0x04,
		0x60, 0x30, 0x00, 0x4C, 0x24, 0x00, 0x38, 0x14, 0x00, 0xF8, 0xFC, 0xFC,
		0xFC, 0xD8, 0xD8, 0xFC, 0xB8, 0xB8, 0xFC, 0x98, 0x9C, 0xFC, 0x78, 0x80,
		0xFC, 0x58, 0x64, 0xFC, 0x38, 0x4C, 0xFC, 0x1C, 0x34, 0xDC, 0x14, 0x28,
		0xC0, 0x0C, 0x1C, 0xA4, 0x08, 0x14, 0x88, 0x04, 0x0C, 0x6C, 0x00, 0x04,
		0x50, 0x00, 0x00, 0x34, 0x00, 0x00, 0x18, 0x00, 0x00, 0xFC, 0xEC, 0xD8,
		0xFC, 0xDC, 0xB8, 0xFC, 0xCC, 0x98, 0xFC, 0xBC, 0x7C, 0xFC, 0xAC, 0x5C,
		0xFC, 0x9C, 0x3C, 0xFC, 0x8C, 0x1C, 0xFC, 0x7C, 0x00, 0xE0, 0x6C, 0x00,
		0xC0, 0x60, 0x00, 0xA4, 0x50, 0x00, 0x88, 0x44, 0x00, 0x6C, 0x34, 0x00,
		0x50, 0x24, 0x00, 0x34, 0x18, 0x00, 0x18, 0x08, 0x00, 0xFC, 0xFC, 0xD8,
		0xF4, 0xF4, 0x9C, 0xEC, 0xEC, 0x60, 0xE4, 0xE4, 0x2C, 0xDC, 0xDC, 0x00,
		0xC0, 0xC0, 0x00, 0xA4, 0xA4, 0x00, 0x88, 0x88, 0x00, 0x6C, 0x6C, 0x00,
		0x50, 0x50, 0x00, 0x34, 0x34, 0x00, 0x18, 0x18, 0x00, 0xD8, 0xFC, 0xD8,
		0xB0, 0xFC, 0xAC, 0x8C, 0xFC, 0x80, 0x6C, 0xFC, 0x54, 0x50, 0xFC, 0x28,
		0x38, 0xFC, 0x00, 0x28, 0xDC, 0x00, 0x1C, 0xC0, 0x00, 0x14, 0xA4, 0x00,
		0x0C, 0x88, 0x00, 0x04, 0x6C, 0x00, 0x00, 0x50, 0x00, 0x00, 0x34, 0x00,
		0x00, 0x18, 0x00, 0xD4, 0xD8, 0xFC, 0xB8, 0xB8, 0xFC, 0x98, 0x98, 0xFC,
		0x7C, 0x7C, 0xFC, 0x5C, 0x5C, 0xFC, 0x3C, 0x3C, 0xFC, 0x00, 0x00, 0xFC,
		0x00, 0x00, 0xE0, 0x00, 0x00, 0xC0, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x88,
		0x00, 0x00, 0x6C, 0x00, 0x00, 0x50, 0x00, 0x00, 0x34, 0x00, 0x00, 0x18,
		0xE8, 0xC8, 0xE8, 0xD4, 0x98, 0xD4, 0xC4, 0x6C, 0xC4, 0xB0, 0x48, 0xB0,
		0xA0, 0x28, 0xA0, 0x8C, 0x10, 0x8C, 0x7C, 0x00, 0x7C, 0x6C, 0x00, 0x6C,
		0x60, 0x00, 0x60, 0x50, 0x00, 0x50, 0x44, 0x00, 0x44, 0x34, 0x00, 0x34,
		0x24, 0x00, 0x24, 0x18, 0x00, 0x18, 0xF4, 0xE8, 0xE4, 0xEC, 0xDC, 0xD4,
		0xE4, 0xCC, 0xC0, 0xE0, 0xC0, 0xB0, 0xD8, 0xB0, 0xA0, 0xD0, 0xA4, 0x90,
		0xC8, 0x98, 0x80, 0xC4, 0x8C, 0x74, 0xAC, 0x7C, 0x64, 0x98, 0x6C, 0x58,
		0x80, 0x5C, 0x4C, 0x6C, 0x4C, 0x3C, 0x54, 0x3C, 0x30, 0x3C, 0x2C, 0x24,
		0x28, 0x1C, 0x14, 0x10, 0x0C, 0x08, 0xEC, 0xEC, 0xEC, 0xDC, 0xDC, 0xDC,
		0xCC, 0xCC, 0xCC, 0xBC, 0xBC, 0xBC, 0xAC, 0xAC, 0xAC, 0x9C, 0x9C, 0x9C,
		0x8C, 0x8C, 0x8C, 0x7C, 0x7C, 0x7C, 0x6C, 0x6C, 0x6C, 0x60, 0x60, 0x60,
		0x50, 0x50, 0x50, 0x44, 0x44, 0x44, 0x34, 0x34, 0x34, 0x24, 0x24, 0x24,
		0x18, 0x18, 0x18, 0x08, 0x08, 0x08, 0xE8, 0xE0, 0xD4, 0xD8, 0xC8, 0xB0,
		0xC8, 0xB0, 0x90, 0xB8, 0x98, 0x70, 0xA8, 0x84, 0x58, 0x98, 0x70, 0x40,
		0x88, 0x5C, 0x2C, 0x7C, 0x4C, 0x18, 0x6C, 0x3C, 0x0C, 0x5C, 0x34, 0x0C,
		0x4C, 0x2C, 0x0C, 0x3C, 0x24, 0x0C, 0x2C, 0x1C, 0x08, 0x20, 0x14, 0x08,
		0xEC, 0xE8, 0xE4, 0xDC, 0xD4, 0xD0, 0xCC, 0xC4, 0xBC, 0xBC, 0xB0, 0xAC,
		0xAC, 0xA0, 0x98, 0x9C, 0x90, 0x88, 0x8C, 0x80, 0x78, 0x7C, 0x70, 0x68,
		0x6C, 0x60, 0x5C, 0x60, 0x54, 0x50, 0x50, 0x48, 0x44, 0x44, 0x3C, 0x38,
		0x34, 0x30, 0x2C, 0x24, 0x20, 0x20, 0x18, 0x14, 0x14, 0xE0, 0xE8, 0xD4,
		0xC8, 0xD4, 0xB4, 0xB4, 0xC0, 0x98, 0x9C, 0xAC, 0x7C, 0x88, 0x98, 0x60,
		0x70, 0x84, 0x4C, 0x5C, 0x70, 0x38, 0x4C, 0x5C, 0x28, 0x40, 0x50, 0x20,
		0x38, 0x44, 0x1C, 0x30, 0x3C, 0x18, 0x28, 0x30, 0x14, 0x20, 0x24, 0x10,
		0x18, 0x1C, 0x08, 0x0C, 0x10, 0x04, 0xEC, 0xD8, 0xCC, 0xDC, 0xB8, 0xA0,
		0xCC, 0x98, 0x7C, 0xBC, 0x80, 0x5C, 0xAC, 0x64, 0x3C, 0x9C, 0x50, 0x24,
		0x8C, 0x3C, 0x0C, 0x7C, 0x2C, 0x00, 0x6C, 0x24, 0x00, 0x60, 0x20, 0x00,
		0x50, 0x1C, 0x00, 0x44, 0x14, 0x00, 0x34, 0x10, 0x00, 0x24, 0x0C, 0x00,
		0xF0, 0xF0, 0xFC, 0xE4, 0xE4, 0xFC, 0xD8, 0xD8, 0xFC, 0xCC, 0xCC, 0xFC,
		0xC0, 0xC0, 0xFC, 0xB4, 0xB4, 0xFC, 0xA8, 0xA8, 0xFC, 0x9C, 0x9C, 0xFC,
		0x84, 0xD0, 0x00, 0x84, 0xB0, 0x00, 0x7C, 0x94, 0x00, 0x68, 0x78, 0x00,
		0x50, 0x58, 0x00, 0x3C, 0x40, 0x00, 0x2C, 0x24, 0x00, 0x1C, 0x08, 0x00,
		0x20, 0x00, 0x00, 0xEC, 0xD8, 0xC4, 0xDC, 0xC0, 0xB4, 0xCC, 0xB4, 0xA0,
		0xBC, 0x9C, 0x94, 0xAC, 0x90, 0x80, 0x9C, 0x84, 0x74, 0x8C, 0x74, 0x64,
		0x7C, 0x64, 0x58, 0x6C, 0x54, 0x4C, 0x60, 0x48, 0x44, 0x50, 0x40, 0x38,
		0x44, 0x34, 0x2C, 0x34, 0x2C, 0x24, 0x24, 0x18, 0x18, 0x18, 0x10, 0x10,
		0xFC, 0xF8, 0xFC, 0xAC, 0xD4, 0xF0, 0x70, 0xAC, 0xE4, 0x34, 0x8C, 0xD8,
		0x00, 0x6C, 0xD0, 0x30, 0x8C, 0xD8, 0x6C, 0xB0, 0xE4, 0xB0, 0xD4, 0xF0,
		0xFC, 0xFC, 0xF8, 0xFC, 0xEC, 0x40, 0xFC, 0xC0, 0x28, 0xFC, 0x8C, 0x10,
		0xFC, 0x50, 0x00, 0xC8, 0x38, 0x00, 0x98, 0x28, 0x00, 0x68, 0x18, 0x00,
		0x7C, 0xDC, 0x7C, 0x44, 0xB4, 0x44, 0x18, 0x90, 0x18, 0x00, 0x6C, 0x00,
		0xF8, 0xB8, 0xFC, 0xFC, 0x64, 0xEC, 0xFC, 0x00, 0xB4, 0xCC, 0x00, 0x70,
		0xFC, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFC, 0x00, 0xFC, 0x00, 0x00,
		0xFC, 0xFC, 0xFC, 0x61, 0x61, 0x61, 0xC0, 0xC0, 0xC0, 0xFC, 0x00, 0xF1};

struct u7frame {
	guchar* pixels;
	size_t  datalen;
	gint16  leftX;
	gint16  leftY;
	gint16  rightX;
	gint16  rightY;
	gint16  width;
	gint16  height;
};

struct u7shape {
	u7frame* frames;
	size_t   num_frames;
};

struct Bounds {
	int xright = -1;
	int xleft  = -1;
	int yabove = -1;
	int ybelow = -1;
};

Bounds get_shape_bounds(Shape_file& shape) {
	if (shape.is_rle()) {
		Bounds bounds;
		for (const auto& frame : shape) {
			bounds.xright = std::max(bounds.xright, frame->get_xright());
			bounds.xleft  = std::max(bounds.xleft, frame->get_xleft());
			bounds.yabove = std::max(bounds.yabove, frame->get_yabove());
			bounds.ybelow = std::max(bounds.ybelow, frame->get_ybelow());
		}
		return bounds;
	}
	// Shape is composed of flats
	return Bounds{7, 0, 0, 7};
}

/* Load Shape image into GIMP */

static GimpImage* load_image(
		GFile* file, GFile* palette_file, GimpRunMode run_mode,
		GError** error) {
	ignore_unused_variable_warning(file, palette_file, run_mode, error);
#if 0
	FILE*  fp;          /* Read file pointer */
	guchar header[32],  /* File header */
			file_mark,  /* KiSS file type */
			bpp;        /* Bits per pixel */
	gint height, width, /* Dimensions of image */
			offx, offy, /* Layer offsets */
			colors;     /* Number of colors */

	GimpImage*  image;  /* Image */
	GimpLayer*  layer;  /* Layer */
	guchar*     buf;    /* Temporary buffer */
	guchar*     line;   /* Pixel data */
	GeglBuffer* buffer; /* Buffer for layer */

	gint   i, j, k; /* Counters */
	size_t n_read;  /* Number of items read from file */

	gimp_progress_init_printf(_("Opening '%s'"), gimp_file_get_utf8_name(file));

	/* Open the file for reading */
	fp = g_fopen(g_file_peek_path(file), "r");

	if (fp == nullptr) {
		g_set_error(
				error, G_FILE_ERROR, g_file_error_from_errno(errno),
				_("Could not open '%s' for reading: %s"),
				gimp_file_get_utf8_name(file), g_strerror(errno));
		return nullptr;
	}

	/* Get the image dimensions and create the image... */

	n_read = fread(header, 4, 1, fp);

	if (n_read < 1) {
		g_set_error(
				error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
				_("EOF or error while reading image header"));
		fclose(fp);
		return nullptr;
	}

	if (strncmp((const gchar*)header, "KiSS", 4)) {
		colors = 16;
		bpp    = 4;
		width  = header[0] + (256 * header[1]);
		height = header[2] + (256 * header[3]);
		offx   = 0;
		offy   = 0;
	} else { /* New-style image file, read full header */
		n_read = fread(header, 28, 1, fp);

		if (n_read < 1) {
			g_set_error(
					error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
					_("EOF or error while reading image header"));
			fclose(fp);
			return nullptr;
		}

		file_mark = header[0];
		if (file_mark != 0x20 && file_mark != 0x21) {
			g_set_error(
					error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
					_("is not a CEL image file"));
			fclose(fp);
			return nullptr;
		}

		bpp = header[1];
		switch (bpp) {
		case 4:
		case 8:
		case 32:
			colors = (1 << bpp);
			break;
		default:
			g_set_error(
					error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
					_("illegal bpp value in image: %hhu"), bpp);
			fclose(fp);
			return nullptr;
		}

		width  = header[4] + (256 * header[5]);
		height = header[6] + (256 * header[7]);
		offx   = header[8] + (256 * header[9]);
		offy   = header[10] + (256 * header[11]);
	}

	if ((width == 0) || (height == 0) || (width + offx > GIMP_MAX_IMAGE_SIZE)
		|| (height + offy > GIMP_MAX_IMAGE_SIZE)) {
		g_set_error(
				error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
				_("illegal image dimensions: width: %d, horizontal offset: "
				  "%d, height: %d, vertical offset: %d"),
				width, offx, height, offy);
		fclose(fp);
		return nullptr;
	}

	if (bpp == 32) {
		image = gimp_image_new(width + offx, height + offy, GIMP_RGB);
	} else {
		image = gimp_image_new(width + offx, height + offy, GIMP_INDEXED);
	}

	if (!image) {
		g_set_error(error, 0, 0, _("Can't create a new image"));
		fclose(fp);
		return nullptr;
	}

	/* Create an indexed-alpha layer to hold the image... */
	if (bpp == 32) {
		layer = gimp_layer_new(
				image, _("Background"), width, height, GIMP_RGBA_IMAGE, 100,
				gimp_image_get_default_new_layer_mode(image));
	} else {
		layer = gimp_layer_new(
				image, _("Background"), width, height, GIMP_INDEXEDA_IMAGE, 100,
				gimp_image_get_default_new_layer_mode(image));
	}
	gimp_image_insert_layer(image, layer, nullptr, 0);
	gimp_layer_set_offsets(layer, offx, offy);

	/* Get the drawable and set the pixel region for our load... */

	buffer = gimp_drawable_get_buffer(GIMP_DRAWABLE(layer));

	/* Read the image in and give it to GIMP a line at a time */
	buf  = g_new(guchar, width * 4);
	line = g_new(guchar, (width + 1) * 4);

	for (i = 0; i < height && !feof(fp); ++i) {
		switch (bpp) {
		case 4:
			n_read = fread(buf, (width + 1) / 2, 1, fp);

			if (n_read < 1) {
				g_set_error(
						error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
						_("EOF or error while reading image data"));
				fclose(fp);
				return nullptr;
			}

			for (j = 0, k = 0; j < width * 2; j += 4, ++k) {
				if (buf[k] / 16 == 0) {
					line[j]     = 16;
					line[j + 1] = 0;
				} else {
					line[j]     = (buf[k] / 16) - 1;
					line[j + 1] = 255;
				}

				if (buf[k] % 16 == 0) {
					line[j + 2] = 16;
					line[j + 3] = 0;
				} else {
					line[j + 2] = (buf[k] % 16) - 1;
					line[j + 3] = 255;
				}
			}
			break;

		case 8:
			n_read = fread(buf, width, 1, fp);

			if (n_read < 1) {
				g_set_error(
						error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
						_("EOF or error while reading image data"));
				fclose(fp);
				return nullptr;
			}

			for (j = 0, k = 0; j < width * 2; j += 2, ++k) {
				if (buf[k] == 0) {
					line[j]     = 255;
					line[j + 1] = 0;
				} else {
					line[j]     = buf[k] - 1;
					line[j + 1] = 255;
				}
			}
			break;

		case 32:
			n_read = fread(line, width * 4, 1, fp);

			if (n_read < 1) {
				g_set_error(
						error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
						_("EOF or error while reading image data"));
				fclose(fp);
				return nullptr;
			}

			/* The CEL file order is BGR so we need to swap B and R
			 * to get the Gimp RGB order.
			 */
			for (j = 0; j < width; j++) {
				guint8 tmp      = line[j * 4];
				line[j * 4]     = line[j * 4 + 2];
				line[j * 4 + 2] = tmp;
			}
			break;

		default:
			g_set_error(
					error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
					_("Unsupported bit depth (%d)!"), bpp);
			fclose(fp);
			return nullptr;
		}

		gegl_buffer_set(
				buffer, GEGL_RECTANGLE(0, i, width, 1), 0, nullptr, line,
				GEGL_AUTO_ROWSTRIDE);

		gimp_progress_update((float)i / (float)height);
	}

	/* Close image files, give back allocated memory */

	fclose(fp);
	g_free(buf);
	g_free(line);

	if (bpp != 32) {
		/* Use palette from file or otherwise default grey palette */
		guchar palette[256 * 3];

		if (palette_file != nullptr) {
			colors = load_palette(palette_file, palette, error);
			if (colors < 0 || *error) {
				return nullptr;
			}
		} else {
			for (i = 0; i < colors; ++i) {
				palette[i * 3] = palette[i * 3 + 1] = palette[i * 3 + 2]
						= i * 256 / colors;
			}
		}

		gimp_palette_set_colormap(
				gimp_image_get_palette(image), babl_format("R'G'B' u8"),
				palette + 3, (colors - 1) * 3);
	}

	/* Now get everything redrawn and hand back the finished image */

	g_object_unref(buffer);

	gimp_progress_update(1.0);

	return image;
#endif
	// static gint32 load_image(gchar* filename) {
	//	Shape_file shape(filename);
	if (!g_file_query_exists(file, nullptr)) {
		return nullptr;
	}
	Shape_file shape(gimp_file_get_utf8_name(file));
	if (shape.get_num_frames() == 0) {
		return nullptr;
	}
#ifdef DEBUG
	std::cout << "num_frames = " << shape.get_num_frames() << '\n';
#endif
	if (palette_file) {
		load_palette(palette_file, nullptr, error);
	}
	const Bounds  bounds = get_shape_bounds(shape);
	GimpImageType image_type;
	if (shape.is_rle()) {
		image_type = GIMP_INDEXEDA_IMAGE;
	} else {
		image_type = GIMP_INDEXED_IMAGE;
	}

	GimpImage* image = gimp_image_new(
			bounds.xleft + bounds.xright + 1, bounds.yabove + bounds.ybelow + 1,
			GIMP_INDEXED);
	//	gimp_image_set_filename(image, filename);
	//	gimp_image_set_filename(image, gimp_file_get_utf8_name(file));
	//	gimp_image_set_colormap(image, gimp_cmap, 256);
	gimp_palette_set_colormap(
			gimp_image_get_palette(image), babl_format("R'G'B' u8"), gimp_cmap,
			(256) * 3);
	int framenum = 0;
	for (auto& frame : shape) {
		const std::string framename = "Frame " + std::to_string(framenum);
		GimpLayer*        layer     = gimp_layer_new(
                image, framename.c_str(), frame->get_width(),
                frame->get_height(), image_type, 100,
                gimp_image_get_default_new_layer_mode(image));
		gimp_image_insert_layer(image, layer, nullptr, 0);
		gimp_item_transform_translate(
				GIMP_ITEM(layer), bounds.xleft - frame->get_xleft(),
				bounds.yabove - frame->get_yabove());

		GeglBuffer* drawable = gimp_drawable_get_buffer(GIMP_DRAWABLE(layer));
		const GeglRectangle rect{
				0, 0, gegl_buffer_get_width(drawable),
				gegl_buffer_get_height(drawable)};

		Image_buffer8 img(
				frame->get_width(),
				frame->get_height());    // Render into a buffer.
		const unsigned char transp = 255;
		img.fill8(transp);    // Fill with transparent pixel.
		if (!frame->is_empty()) {
			frame->paint(&img, frame->get_xleft(), frame->get_yabove());
		}
		std::vector<unsigned char> pixels;
		const size_t num_pixels = frame->get_width() * frame->get_height();
		const unsigned char* pixel_data = nullptr;
		if (shape.is_rle()) {
			// Need to expand from (pixel)* to (pixel, alpha)*.
			pixels.reserve(num_pixels * 2);
			const auto* data = img.get_bits();
			for (const auto* ptr = data; ptr != data + num_pixels; ptr++) {
				pixels.push_back(*ptr);
				pixels.push_back(*ptr == transp ? 0 : 255);    // Alpha
			}
			pixel_data = pixels.data();
		} else {
			pixel_data = img.get_bits();
		}

		gegl_buffer_set(
				drawable, &rect, 0, nullptr, pixel_data, GEGL_AUTO_ROWSTRIDE);
		g_object_unref(drawable);
		framenum++;
	}

	gimp_image_add_hguide(image, bounds.yabove);
	gimp_image_add_vguide(image, bounds.xleft);
#ifdef DEBUG
	std::cout << "Added hguide=" << bounds.yabove << '\n'
			  << "Added vguide=" << bounds.xleft << '\n';
#endif

	return image;
}

static gint load_palette(GFile* file, guchar palette[], GError** error) {
	ignore_unused_variable_warning(file, palette, error);
#if 0
	GFileInputStream* input;
	guchar            header[32]; /* File header */
	guchar            buffer[2];
	guchar            file_mark, bpp;
	gint              i, colors = 0;
	gssize            n_read;

	input = g_file_read(file, nullptr, error);
	if (input == nullptr) {
		return -1;
	}

	n_read = g_input_stream_read(
			G_INPUT_STREAM(input), header, 4, nullptr, error);

	if (n_read < 1) {
		g_object_unref(input);
		return -1;
	}

	if (!strncmp((const gchar*)header, "KiSS", 4)) {
		n_read = g_input_stream_read(
				G_INPUT_STREAM(input), header + 4, 28, nullptr, error);

		if (n_read < 1) {
			g_object_unref(input);
			return -1;
		}

		file_mark = header[4];
		if (file_mark != 0x10) {
			g_object_unref(input);
			g_set_error(
					error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
					_("'%s': is not a KCF palette file"),
					gimp_file_get_utf8_name(file));
			return -1;
		}

		bpp = header[5];
		if (bpp != 12 && bpp != 24) {
			g_object_unref(input);
			g_set_error(
					error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
					_("'%s': illegal bpp value in palette: %hhu"),
					gimp_file_get_utf8_name(file), bpp);
			return -1;
		}

		colors = header[8] + header[9] * 256;
		if (colors != 16 && colors != 256) {
			g_object_unref(input);
			g_set_error(
					error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
					_("'%s': illegal number of colors: %u"),
					gimp_file_get_utf8_name(file), colors);
			return -1;
		}

		switch (bpp) {
		case 12:
			for (i = 0; i < colors; ++i) {
				n_read = g_input_stream_read(
						G_INPUT_STREAM(input), buffer, 2, nullptr, error);

				if (n_read == 1) {
					g_object_unref(input);
					g_set_error(
							error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
							_("'%s': EOF or error while reading "
							  "palette data"),
							gimp_file_get_utf8_name(file));
					return -1;
				} else if (n_read < 1) {
					g_object_unref(input);
					return -1;
				}

				palette[i * 3]     = buffer[0] & 0xf0;
				palette[i * 3 + 1] = (buffer[1] & 0x0f) * 16;
				palette[i * 3 + 2] = (buffer[0] & 0x0f) * 16;
			}
			break;
		case 24:
			n_read = g_input_stream_read(
					G_INPUT_STREAM(input), palette, 3 * colors, nullptr, error);

			if (n_read < 1) {
				g_object_unref(input);
				return -1;
			} else if (n_read < 3 * colors) {
				g_object_unref(input);
				g_set_error(
						error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
						_("'%s': EOF or error while reading palette data"),
						gimp_file_get_utf8_name(file));
				return -1;
			}
			break;
		default:
			g_assert_not_reached();
		}
	} else {
		colors = 16;
		if (!g_seekable_seek(
					G_SEEKABLE(input), 0, G_SEEK_SET, nullptr, error)) {
			g_object_unref(input);
			return -1;
		}

		for (i = 0; i < colors; ++i) {
			n_read = g_input_stream_read(
					G_INPUT_STREAM(input), buffer, 2, nullptr, error);

			if (n_read < 1) {
				g_object_unref(input);
				return -1;
			} else if (n_read == 1) {
				g_object_unref(input);
				g_set_error(
						error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
						_("'%s': EOF or error while reading palette data"),
						gimp_file_get_utf8_name(file));
				return -1;
			}

			palette[i * 3]     = buffer[0] & 0xf0;
			palette[i * 3 + 1] = (buffer[1] & 0x0f) * 16;
			palette[i * 3 + 2] = (buffer[0] & 0x0f) * 16;
		}
	}

	return colors;
#endif
	// static void load_palette(const std::string& filename) {
	//	const U7object pal(filename, 0);
	const U7object pal(gimp_file_get_utf8_name(file), 0);
	size_t         len;
	auto           data = pal.retrieve(len);
	if (!data || len == 0) {
		return 0;
	}
	const auto* ptr = data.get();
	if (len == 768) {
		for (unsigned i = 0; i < 256; i++) {
			gimp_cmap[i * 3 + 0] = Read1(ptr) << 2;
			gimp_cmap[i * 3 + 1] = Read1(ptr) << 2;
			gimp_cmap[i * 3 + 2] = Read1(ptr) << 2;
		}
	} else if (len == 1536) {
		// Double palette
		for (unsigned i = 0; i < 256; i++) {
			gimp_cmap[i * 3 + 0] = Read1(ptr) << 2;
			Read1(ptr);    // Skip entry from second palette
			gimp_cmap[i * 3 + 1] = Read1(ptr) << 2;
			Read1(ptr);    // Skip entry from second palette
			gimp_cmap[i * 3 + 2] = Read1(ptr) << 2;
			Read1(ptr);    // Skip entry from second palette
		}
	}
	return 256;
}

static gboolean export_image(
		GFile* file, GimpImage* image, GimpRunMode run_mode, GError** error) {
	ignore_unused_variable_warning(file, image, run_mode, error);
#if 0
	GOutputStream* output;
	GeglBuffer*    buffer;
	const Babl*    format;
	GCancellable*  cancellable;
	gint           width;
	gint           height;
	guchar         header[32];     /* File header */
	gint           bpp;            /* Bit per pixel */
	gint           colors = 0;     /* Number of colors */
	gint           type;           /* type of layer */
	gint           offx, offy;     /* Layer offsets */
	guchar*        buf  = nullptr; /* Temporary buffer */
	guchar*        line = nullptr; /* Pixel data */
	gint           i, j, k;        /* Counters */

	/* Check that this is an indexed image, fail otherwise */
	type = gimp_drawable_type(drawable);

	if (type == GIMP_INDEXEDA_IMAGE) {
		bpp    = 4;
		format = nullptr;
	} else {
		bpp    = 32;
		format = babl_format("R'G'B'A u8");
	}

	/* Find out how offset this layer was */
	gimp_drawable_get_offsets(drawable, &offx, &offy);

	buffer = gimp_drawable_get_buffer(drawable);

	width  = gegl_buffer_get_width(buffer);
	height = gegl_buffer_get_height(buffer);

	gimp_progress_init_printf(
			_("Exporting '%s'"), gimp_file_get_utf8_name(file));

	output = G_OUTPUT_STREAM(g_file_replace(
			file, nullptr, FALSE, G_FILE_CREATE_NONE, nullptr, error));
	if (output) {
		GOutputStream* buffered;

		buffered = g_buffered_output_stream_new(output);
		g_object_unref(output);

		output = buffered;
	} else {
		return FALSE;
	}

	/* Headers */
	memset(header, 0, 32);
	strcpy((gchar*)header, "KiSS");
	header[4] = 0x20;

	/* Work out whether to save as 8bit or 4bit */
	if (bpp < 32) {
		colors = gimp_palette_get_color_count(gimp_image_get_palette(image));

		if (colors > 15) {
			header[5] = 8;
		} else {
			header[5] = 4;
		}
	} else {
		header[5] = 32;
	}

	/* Fill in the blanks ... */
	header[8]  = width % 256;
	header[9]  = width / 256;
	header[10] = height % 256;
	header[11] = height / 256;
	header[12] = offx % 256;
	header[13] = offx / 256;
	header[14] = offy % 256;
	header[15] = offy / 256;

	if (!g_output_stream_write_all(
				output, header, 32, nullptr, nullptr, error)) {
		goto fail;
	}

	/* Arrange for memory etc. */
	buf  = g_new(guchar, width * 4);
	line = g_new(guchar, (width + 1) * 4);

	/* Get the image from GIMP one line at a time and write it out */
	for (i = 0; i < height; ++i) {
		gegl_buffer_get(
				buffer, GEGL_RECTANGLE(0, i, width, 1), 1.0, format, line,
				GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

		memset(buf, 0, width);

		if (bpp == 32) {
			for (j = 0; j < width; j++) {
				buf[4 * j]     = line[4 * j + 2]; /* B */
				buf[4 * j + 1] = line[4 * j + 1]; /* G */
				buf[4 * j + 2] = line[4 * j + 0]; /* R */
				buf[4 * j + 3] = line[4 * j + 3]; /* Alpha */
			}

			if (!g_output_stream_write_all(
						output, buf, width * 4, nullptr, nullptr, error)) {
				goto fail;
			}
		} else if (colors > 16) {
			for (j = 0, k = 0; j < width * 2; j += 2, ++k) {
				if (line[j + 1] > 127) {
					buf[k] = line[j] + 1;
				}
			}

			if (!g_output_stream_write_all(
						output, buf, width, nullptr, nullptr, error)) {
				goto fail;
			}
		} else {
			for (j = 0, k = 0; j < width * 2; j += 4, ++k) {
				buf[k] = 0;

				if (line[j + 1] > 127) {
					buf[k] += (line[j] + 1) << 4;
				}

				if (line[j + 3] > 127) {
					buf[k] += (line[j + 2] + 1);
				}
			}

			if (!g_output_stream_write_all(
						output, buf, width + 1 / 2, nullptr, nullptr, error)) {
				goto fail;
			}
		}

		gimp_progress_update((float)i / (float)height);
	}

	if (!g_output_stream_close(output, nullptr, error)) {
		goto fail;
	}

	gimp_progress_update(1.0);

	g_free(buf);
	g_free(line);
	g_object_unref(buffer);
	g_object_unref(output);

	return TRUE;

fail:

	cancellable = g_cancellable_new();
	g_cancellable_cancel(cancellable);
	g_output_stream_close(output, cancellable, nullptr);
	g_object_unref(cancellable);

	g_free(buf);
	g_free(line);
	g_object_unref(buffer);
	g_object_unref(output);

	return FALSE;
#endif
	// static gint32 export_image(
	//		gchar* filename, gint32 image_ID, gint32 drawable_ID,
	//		gint32 orig_image_ID) {
	//	ignore_unused_variable_warning(drawable_ID, orig_image_ID);
	if (run_mode != GIMP_RUN_NONINTERACTIVE) {
		std::string name_buf("Saving ");
		//		name_buf += filename;
		name_buf += gimp_file_get_utf8_name(file);
		name_buf += ':';
		gimp_progress_init(name_buf.c_str());
	}

	// Find the guides...
	int hotx = -1;
	int hoty = -1;
	for (gint32 guide_ID = gimp_image_find_next_guide(image, 0); guide_ID > 0;
		 guide_ID        = gimp_image_find_next_guide(image, guide_ID)) {
#ifdef DEBUG
		std::cout << "Found guide " << guide_ID << ':';
#endif

		switch (gimp_image_get_guide_orientation(image, guide_ID)) {
		case GIMP_ORIENTATION_HORIZONTAL:
			if (hoty < 0) {
				hoty = gimp_image_get_guide_position(image, guide_ID);
#ifdef DEBUG
				std::cout << " horizontal=" << hoty << '\n';
#endif
			}
			break;
		case GIMP_ORIENTATION_VERTICAL:
			if (hotx < 0) {
				hotx = gimp_image_get_guide_position(image, guide_ID);
#ifdef DEBUG
				std::cout << " vertical=" << hotx << '\n';
#endif
			}
			break;
		case GIMP_ORIENTATION_UNKNOWN:
			break;
		}
	}

	GList* layers  = g_list_reverse(gimp_image_list_layers(image));
	gint32 nlayers = g_list_length(layers);
	std::cout << "SHP: Exporting " << g_list_length(layers) << " layers"
			  << std::endl;

	if (layers && !gimp_drawable_is_indexed(GIMP_DRAWABLE(layers->data))) {
		g_message("SHP: You can only save indexed images!");
		return FALSE;
	}

	Shape  shape(nlayers);
	GList* cur_layer = g_list_first(layers);
	for (auto& frame : shape) {
		GeglBuffer* drawable
				= gimp_drawable_get_buffer(GIMP_DRAWABLE(cur_layer->data));
		gint offsetX;
		gint offsetY;
		gimp_drawable_get_offsets(
				GIMP_DRAWABLE(cur_layer->data), &offsetX, &offsetY);
		const int    width           = gegl_buffer_get_width(drawable);
		const int    height          = gegl_buffer_get_height(drawable);
		const int    xleft           = hotx - offsetX;
		const int    yabove          = hoty - offsetY;
		const Babl*  format          = gegl_buffer_get_format(drawable);
		const size_t num_pixels      = width * height;
		const size_t bytes_per_pixel = babl_format_get_bytes_per_pixel(format);
		std::vector<guchar> pix(num_pixels * bytes_per_pixel);
		const GeglRectangle rect{
				0, 0, gegl_buffer_get_width(drawable),
				gegl_buffer_get_height(drawable)};
		gegl_buffer_get(
				drawable, &rect, 1.0, format, pix.data(), GEGL_AUTO_ROWSTRIDE,
				GEGL_ABYSS_NONE);
		g_object_unref(drawable);

		std::vector<unsigned char> out;
		unsigned char*             outptr;
		const bool has_alpha = babl_format_has_alpha(format) != 0;
		if (has_alpha) {
			out.reserve(pix.size() / bytes_per_pixel);
			for (size_t ii = 0; ii < pix.size(); ii += bytes_per_pixel) {
				out.push_back(pix[ii + 1] == 0 ? 255 : pix[ii]);
			}
			outptr = out.data();
		} else {
			outptr = pix.data();
		}
		frame = std::make_unique<Shape_frame>(
				outptr, width, height, xleft, yabove, has_alpha);
		cur_layer = g_list_next(cur_layer);
	}

	//	OFileDataSource ds(filename);
	OFileDataSource ds(gimp_file_get_utf8_name(file));
	if (!ds.good()) {
		//		g_message("SHP: can't create \"%s\"\n", filename);
		g_message("SHP: can't create \"%s\"\n", gimp_file_get_utf8_name(file));
		return -1;
	}
	shape.write(ds);
	g_list_free(layers);

	return TRUE;
}
