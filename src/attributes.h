#ifndef __SP_ATTRIBUTES_H__
#define __SP_ATTRIBUTES_H__

/*
 * Lookup dictionary for attributes/properties
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2002 Lauris Kaplinski
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

unsigned int sp_attribute_lookup (const unsigned char *key);
const unsigned char *sp_attribute_name (unsigned char id);

#define SP_ATTRIBUTE_IS_CSS(k) (((k) >= SP_PROP_FONT) && ((k) <= SP_PROP_WRITING_MODE))

/* This is not nice, but we have to ensure name->id works */
#define SP_ATTR_FILL SP_PROP_FILL

enum {
	SP_ATTR_INVALID,
	/* SPObject */
	SP_ATTR_ID,
	/* SPItem */
	SP_ATTR_TRANSFORM,
	SP_ATTR_SODIPODI_INSENSITIVE,
	SP_ATTR_SODIPODI_NONPRINTABLE,
	SP_ATTR_STYLE,
	/* SPAnchor */
	SP_ATTR_XLINK_HREF,
	SP_ATTR_XLINK_TYPE,
	SP_ATTR_XLINK_ROLE,
	SP_ATTR_XLINK_ARCROLE,
	SP_ATTR_XLINK_TITLE,
	SP_ATTR_XLINK_SHOW,
	SP_ATTR_XLINK_ACTUATE,
	SP_ATTR_TARGET,
	/* SPRoot */
	SP_ATTR_VERSION,
	SP_ATTR_WIDTH,
	SP_ATTR_HEIGHT,
	SP_ATTR_VIEWBOX,
	SP_ATTR_PRESERVEASPECTRATIO,
	SP_ATTR_SODIPODI_VERSION,
	/* SPNamedView */
	SP_ATTR_VIEWONLY,
	SP_ATTR_SHOWGRID,
	SP_ATTR_SNAPTOGRID,
	SP_ATTR_SHOWGUIDES,
	SP_ATTR_SNAPTOGUIDES,
	SP_ATTR_GRIDTOLERANCE,
	SP_ATTR_GUIDETOLERANCE,
	SP_ATTR_GRIDORIGINX,
	SP_ATTR_GRIDORIGINY,
	SP_ATTR_GRIDSPACINGX,
	SP_ATTR_GRIDSPACINGY,
	SP_ATTR_GRIDCOLOR,
	SP_ATTR_GRIDOPACITY,
	SP_ATTR_GUIDECOLOR,
	SP_ATTR_GUIDEOPACITY,
	SP_ATTR_GUIDEHICOLOR,
	SP_ATTR_GUIDEHIOPACITY,
	SP_ATTR_SHOWBORDER,
	SP_ATTR_BORDERLAYER,
	/* SPGuide */
	SP_ATTR_ORIENTATION,
	SP_ATTR_POSITION,
	/* SPImage */
	SP_ATTR_X,
	SP_ATTR_Y,
	/* SPPath */
	SP_ATTR_D,
	/* SPRect */
	SP_ATTR_RX,
	SP_ATTR_RY,
	/* SPEllipse */
	SP_ATTR_R,
	SP_ATTR_CX,
	SP_ATTR_CY,
	SP_ATTR_SODIPODI_CX,
	SP_ATTR_SODIPODI_CY,
	SP_ATTR_SODIPODI_RX,
	SP_ATTR_SODIPODI_RY,
	SP_ATTR_SODIPODI_START,
	SP_ATTR_SODIPODI_END,
	SP_ATTR_SODIPODI_OPEN,
	/* SPStar */
	SP_ATTR_SODIPODI_SIDES,
	SP_ATTR_SODIPODI_R1,
	SP_ATTR_SODIPODI_R2,
	SP_ATTR_SODIPODI_ARG1,
	SP_ATTR_SODIPODI_ARG2,
	/* SPSpiral */
	SP_ATTR_SODIPODI_EXPANSION,
	SP_ATTR_SODIPODI_REVOLUTION,
	SP_ATTR_SODIPODI_RADIUS,
	SP_ATTR_SODIPODI_ARGUMENT,
	SP_ATTR_SODIPODI_T0,
	/* SPLine */
	SP_ATTR_X1,
	SP_ATTR_Y1,
	SP_ATTR_X2,
	SP_ATTR_Y2,
	/* SPPolyline */
	SP_ATTR_POINTS,
	/* SPTSpan */
	SP_ATTR_DX,
	SP_ATTR_DY,
	SP_ATTR_ROTATE,
	SP_ATTR_SODIPODI_ROLE,
	/* SPText */
	SP_ATTR_SODIPODI_LINESPACING,
	/* SPStop */
	SP_ATTR_OFFSET,
	/* SPGradient */
	SP_ATTR_GRADIENTUNITS,
	SP_ATTR_GRADIENTTRANSFORM,
	SP_ATTR_SPREADMETHOD,
	/* SPRadialGradient */
	SP_ATTR_FX,
	SP_ATTR_FY,
	/* SPPattern */
	SP_ATTR_PATTERNUNITS,
	SP_ATTR_PATTERNCONTENTUNITS,
	SP_ATTR_PATTERNTRANSFORM,
	/* SPClipPath */
	SP_ATTR_CLIPPATHUNITS,
	/* SPMask */
	SP_ATTR_MASKUNITS,
	SP_ATTR_MASKCONTENTUNITS,
	/* SPMarker */
	SP_ATTR_MARKERUNITS,
	SP_ATTR_REFX,
	SP_ATTR_REFY,
	SP_ATTR_MARKERWIDTH,
	SP_ATTR_MARKERHEIGHT,
	SP_ATTR_ORIENT,
	/* Animations */
	SP_ATTR_ATTRIBUTENAME,
	SP_ATTR_ATTRIBUTETYPE,
	SP_ATTR_BEGIN,
	SP_ATTR_DUR,
	SP_ATTR_END,
	SP_ATTR_MIN,
	SP_ATTR_MAX,
	SP_ATTR_RESTART,
	SP_ATTR_REPEATCOUNT,
	SP_ATTR_REPEATDUR,
	/* Interpolating animations */
	SP_ATTR_CALCMODE,
	SP_ATTR_VALUES,
	SP_ATTR_KEYTIMES,
	SP_ATTR_KEYSPLINES,
	SP_ATTR_FROM,
	SP_ATTR_TO,
	SP_ATTR_BY,
	SP_ATTR_ADDITIVE,
	SP_ATTR_ACCUMULATE,

	/* CSS2 */
	/* Font */
	SP_PROP_FONT,
	SP_PROP_FONT_FAMILY,
	SP_PROP_FONT_SIZE,
	SP_PROP_FONT_SIZE_ADJUST,
	SP_PROP_FONT_STRETCH,
	SP_PROP_FONT_STYLE,
	SP_PROP_FONT_VARIANT,
	SP_PROP_FONT_WEIGHT,
	/* Text */
	SP_PROP_DIRECTION,
	SP_PROP_LETTER_SPACING,
	SP_PROP_TEXT_DECORATION,
	SP_PROP_UNICODE_BIDI,
	SP_PROP_WORD_SPACING,
	/* Misc */
	SP_PROP_CLIP,
	SP_PROP_COLOR,
	SP_PROP_CURSOR,
	SP_PROP_DISPLAY,
	SP_PROP_OVERFLOW,
	SP_PROP_VISIBILITY,
	/* SVG */
	/* Clip/Mask */
	SP_PROP_CLIP_PATH,
	SP_PROP_CLIP_RULE,
	SP_PROP_MASK,
	SP_PROP_OPACITY,
	/* Filter */
	SP_PROP_ENABLE_BACKGROUND,
	SP_PROP_FILTER,
	SP_PROP_FLOOD_COLOR,
	SP_PROP_FLOOD_OPACITY,
	SP_PROP_LIGHTING_COLOR,
	/* Gradient */
	SP_PROP_STOP_COLOR,
	SP_PROP_STOP_OPACITY,
	/* Interactivity */
	SP_PROP_POINTER_EVENTS,
	/* Paint */
	SP_PROP_COLOR_INTERPOLATION,
	SP_PROP_COLOR_INTERPOLATION_FILTERS,
	SP_PROP_COLOR_PROFILE,
	SP_PROP_COLOR_RENDERING,
	SP_PROP_FILL,
	SP_PROP_FILL_OPACITY,
	SP_PROP_FILL_RULE,
	SP_PROP_IMAGE_RENDERING,
	SP_PROP_MARKER,
	SP_PROP_MARKER_END,
	SP_PROP_MARKER_MID,
	SP_PROP_MARKER_START,
	SP_PROP_SHAPE_RENDERING,
	SP_PROP_STROKE,
	SP_PROP_STROKE_DASHARRAY,
	SP_PROP_STROKE_DASHOFFSET,
	SP_PROP_STROKE_LINECAP,
	SP_PROP_STROKE_LINEJOIN,
	SP_PROP_STROKE_MITERLIMIT,
	SP_PROP_STROKE_OPACITY,
	SP_PROP_STROKE_WIDTH,
	SP_PROP_TEXT_RENDERING,
	/* Text */
	SP_PROP_ALIGNMENT_BASELINE,
	SP_PROP_BASELINE_SHIFT,
	SP_PROP_DOMINANT_BASELINE,
	SP_PROP_GLYPH_ORIENTATION_HORIZONTAL,
	SP_PROP_GLYPH_ORIENTATION_VERTICAL,
	SP_PROP_KERNING,
	SP_PROP_TEXT_ANCHOR,
	SP_PROP_WRITING_MODE
};

#endif
