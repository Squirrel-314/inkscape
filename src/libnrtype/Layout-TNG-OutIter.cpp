/*
 * Inkscape::Text::Layout - text layout engine output functions using iterators
 *
 * Authors:
 *   Richard Hughes <cyreve@users.sf.net>
 *
 * Copyright (C) 2005 Richard Hughes
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include "Layout-TNG.h"
#include "livarot/Path.h"
#include "font-instance.h"
#include "livarot/Shape.h"
#include "Layout-TNG-Scanline-Maker.h"
#include "svg/svg-types.h"
#include "libnr/nr-matrix-rotate-ops.h"
#include "libnr/nr-matrix-translate-ops.h"
#include "libnr/nr-point-matrix-ops.h"
#include "libnr/nr-rotate-matrix-ops.h"
#include "libnr/nr-translate-matrix-ops.h"
#include "libnr/nr-translate-rotate-ops.h"
#include "display/curve.h"

namespace Inkscape {
namespace Text {

Layout::iterator Layout::_cursorXOnLineToIterator(unsigned line_index, double local_x) const
{
    unsigned char_index = _lineToCharacter(line_index);
    int best_char_index = -1;
    double best_x_difference = DBL_MAX;

    if (char_index == _characters.size()) return end();
    for ( ; char_index < _characters.size() ; char_index++) {
        if (_characters[char_index].chunk(this).in_line != line_index) break;
        if (_characters[char_index].char_attributes.is_mandatory_break) break;
        if (!_characters[char_index].char_attributes.is_cursor_position) continue;
        double this_x_difference = fabs(_characters[char_index].x + _characters[char_index].span(this).x_start + _characters[char_index].chunk(this).left_x - local_x);
        if (this_x_difference < best_x_difference) {
            best_char_index = char_index;
            best_x_difference = this_x_difference;
        }
    }
    // also try the very end of a para (not lines though because the space wraps)
    if (char_index == _characters.size() || _characters[char_index].char_attributes.is_mandatory_break) {
        double this_x_difference;
        if (char_index == 0) this_x_difference = fabs(_spans.front().x_end + _chunks.front().left_x - local_x);
        else this_x_difference = fabs(_characters[char_index - 1].span(this).x_end + _characters[char_index - 1].chunk(this).left_x - local_x);
        if (this_x_difference < best_x_difference) {
            best_char_index = char_index;
            best_x_difference = this_x_difference;
        }
    }
    if (best_char_index == -1) return iterator(this, char_index);
    return iterator(this, best_char_index);
}

double Layout::_getChunkWidth(unsigned chunk_index) const
{
    double chunk_width = 0.0;
    unsigned span_index;
    if (chunk_index) {
        span_index = _lineToSpan(_chunks[chunk_index].in_line);
        for ( ; span_index < _spans.size() && _spans[span_index].in_chunk < chunk_index ; span_index++);
    } else
        span_index = 0;
    for ( ; span_index < _spans.size() && _spans[span_index].in_chunk == chunk_index ; span_index++)
        chunk_width = std::max(chunk_width, (double)std::max(_spans[span_index].x_start, _spans[span_index].x_end));
    return chunk_width;
}

/* this function really belongs to Path. I'll probably move it there eventually,
hence the Path-esque coding style */
template<typename T> inline static T square(T x) {return x*x;}
static Path::cut_position PointToCurvilignPosition(Path const &path, NR::Point const &pos)
{
    unsigned bestSeg = 0;
    double bestRangeSquared = DBL_MAX;
    double bestT = 0.0; // you need a sentinel, or make sure that you prime with correct values.

    for (unsigned i = 1 ; i < path.pts.size() ; i++) {
        if (path.pts[i].isMoveTo == polyline_moveto) continue;
        NR::Point p1, p2, localPos;
        double thisRangeSquared;
        double t;

        if (path.pts[i - 1].p == path.pts[i].p) {
            thisRangeSquared = square(path.pts[i].p[NR::X] - pos[NR::X]) + square(path.pts[i].p[NR::Y] - pos[NR::Y]);
            t = 0.0;
        } else {
            // we rotate all our coordinates so we're always looking at a mostly vertical line.
            if (fabs(path.pts[i - 1].p[NR::X] - path.pts[i].p[NR::X]) < fabs(path.pts[i - 1].p[NR::Y] - path.pts[i].p[NR::Y])) {
                p1 = path.pts[i - 1].p;
                p2 = path.pts[i].p;
                localPos = pos;
            } else {
                p1 = path.pts[i - 1].p.cw();
                p2 = path.pts[i].p.cw();
                localPos = pos.cw();
            }
            double gradient = (p2[NR::X] - p1[NR::X]) / (p2[NR::Y] - p1[NR::Y]);
            double intersection = p1[NR::X] - gradient * p1[NR::Y];
            /*
              orthogonalGradient = -1.0 / gradient; // you are going to have numerical problems here.
              orthogonalIntersection = localPos[NR::X] - orthogonalGradient * localPos[NR::Y];
              nearestY = (orthogonalIntersection - intersection) / (gradient - orthogonalGradient);

              expand out nearestY fully :
              nearestY = (localPos[NR::X] - (-1.0 / gradient) * localPos[NR::Y] - intersection) / (gradient - (-1.0 / gradient));

              multiply top and bottom by gradient:
              nearestY = (localPos[NR::X] * gradient - (-1.0) * localPos[NR::Y] - intersection * gradient) / (gradient * gradient - (-1.0));

              and simplify to get:
            */
            double nearestY =  (localPos[NR::X] * gradient + localPos[NR::Y] - intersection * gradient)
                             / (gradient * gradient + 1.0);
            t = (nearestY - p1[NR::Y]) / (p2[NR::Y] - p1[NR::Y]);
            if (t <= 0.0) thisRangeSquared = square(p1[NR::X] - localPos[NR::X]) + square(p1[NR::Y] - localPos[NR::Y]);
            else if (t >= 1.0) thisRangeSquared = square(p2[NR::X] - localPos[NR::X]) + square(p2[NR::Y] - localPos[NR::Y]);
            else thisRangeSquared = square(nearestY * gradient + intersection - localPos[NR::X]) + square(nearestY - localPos[NR::Y]);
        }

        if (thisRangeSquared < bestRangeSquared) {
            bestSeg = i;
            bestRangeSquared = thisRangeSquared;
            bestT = t;
        }
    }
    Path::cut_position result;
    if (bestSeg == 0) {
        result.piece = 0;
        result.t = 0.0;
    } else {
        result.piece = path.pts[bestSeg].piece;
        if (result.piece == path.pts[bestSeg - 1].piece)
            result.t = path.pts[bestSeg - 1].t * (1.0 - bestT) + path.pts[bestSeg].t * bestT;
        else
            result.t = path.pts[bestSeg].t * bestT;
    }
    return result;
}

// this one also belongs to Path
// returns the length of the path up to the position indicated by t (0..1)
static double PositionToLength(Path const &path, int piece, double t)
{
    double length = 0.0;
    for (unsigned i = 1 ; i < path.pts.size() ; i++) {
        if (path.pts[i].isMoveTo == polyline_moveto) continue;
        if (path.pts[i].piece == piece && t < path.pts[i].t) {
            length += NR::L2((t - path.pts[i - 1].t) / (path.pts[i].t - path.pts[i - 1].t) * (path.pts[i].p - path.pts[i - 1].p));
            break;
        }
        length += NR::L2(path.pts[i].p - path.pts[i - 1].p);
    }
    return length;
}

/* getting the cursor position for a mouse click is not as simple as it might
seem. The two major problems are flows set up in multiple columns and large
dy adjustments such that text does not belong to the line it appears to. In
the worst case it's possible to have two characters on top of each other, in
which case the one we pick is arbitrary.

This is a 3-stage (2 pass) algorithm:
1) search all the spans to see if the point is contained in one, if so take
   that. Note that this will collect all clicks from the current UI because
   of how the hit detection of nrarena objects works.
2) if that fails, run through all the chunks finding a best guess of the one
   the user wanted. This is the one whose y coordinate is nearest, or if
   there's a tie, the x.
3) search in that chunk using x-coordinate only to find the position.
*/
Layout::iterator Layout::getNearestCursorPositionTo(double x, double y) const
{
    if (_lines.empty()) return begin();
    double local_x = x;
    double local_y = y;

    if (_path_fitted) {
        Path::cut_position position = PointToCurvilignPosition(*_path_fitted, NR::Point(x, y));
        local_x = PositionToLength(*_path_fitted, position.piece, position.t);
        return _cursorXOnLineToIterator(0, local_x + _chunks.front().left_x);
    }

    if (_directions_are_orthogonal(_blockProgression(), TOP_TO_BOTTOM)) {
        local_x = y;
        local_y = x;
    }
    // stage 1:
    for (unsigned span_index = 0 ; span_index < _spans.size() ; span_index++) {
        double span_left, span_right;
        if (_spans[span_index].x_start < _spans[span_index].x_end) {
            span_left = _spans[span_index].x_start;
            span_right = _spans[span_index].x_end;
        } else {
            span_left = _spans[span_index].x_end;
            span_right = _spans[span_index].x_start;
        }
        if (   local_x >= _chunks[_spans[span_index].in_chunk].left_x + span_left
            && local_x <= _chunks[_spans[span_index].in_chunk].left_x + span_right
            && local_y >= _spans[span_index].line(this).baseline_y + _spans[span_index].baseline_shift - _spans[span_index].line_height.ascent
            && local_y <= _spans[span_index].line(this).baseline_y + _spans[span_index].baseline_shift + _spans[span_index].line_height.descent) {
            return _cursorXOnLineToIterator(_chunks[_spans[span_index].in_chunk].in_line, local_x);
        }
    }
    
    // stage 2:
    unsigned span_index = 0;
    unsigned chunk_index;
    int best_chunk_index = -1;
    double best_y_range = DBL_MAX;
    double best_x_range = DBL_MAX;
    for (chunk_index = 0 ; chunk_index < _chunks.size() ; chunk_index++) {
        LineHeight line_height = {0.0, 0.0, 0.0};
        double chunk_width = 0.0;
        for ( ; span_index < _spans.size() && _spans[span_index].in_chunk == chunk_index ; span_index++) {
            line_height.max(_spans[span_index].line_height);
            chunk_width = std::max(chunk_width, (double)std::max(_spans[span_index].x_start, _spans[span_index].x_end));
        }
        double this_y_range;
        if (local_y < _lines[_chunks[chunk_index].in_line].baseline_y - line_height.ascent)
            this_y_range = _lines[_chunks[chunk_index].in_line].baseline_y - line_height.ascent - local_y;
        else if (local_y > _lines[_chunks[chunk_index].in_line].baseline_y + line_height.descent)
            this_y_range = local_y - (_lines[_chunks[chunk_index].in_line].baseline_y + line_height.descent);
        else
            this_y_range = 0.0;
        if (this_y_range <= best_y_range) {
            if (this_y_range < best_y_range) best_x_range = DBL_MAX;
            double this_x_range;
            if (local_x < _chunks[chunk_index].left_x)
                this_x_range = _chunks[chunk_index].left_x - local_y;
            else if (local_x > _chunks[chunk_index].left_x + chunk_width)
                this_x_range = local_x - (_chunks[chunk_index].left_x + chunk_width);
            else
                this_x_range = 0.0;
            if (this_x_range < best_x_range) {
                best_y_range = this_y_range;
                best_x_range = this_x_range;
                best_chunk_index = chunk_index;
            }
        }
    }

    // stage 3:
    if (best_chunk_index == -1) return begin();    // never happens
    return _cursorXOnLineToIterator(_chunks[best_chunk_index].in_line, local_x);
}

Layout::iterator Layout::getLetterAt(double x, double y) const
{
    NR::Point point(x, y);

    double rotation;
    for (iterator it = begin() ; it != end() ; it.nextCharacter()) {
        NR::Rect box = characterBoundingBox(it, &rotation);
        // todo: rotation
        if (box.contains(point)) return it;
    }
    return end();
}

Layout::iterator Layout::sourceToIterator(void *source_cookie, Glib::ustring::const_iterator text_iterator) const
{
    unsigned source_index;
    if (_characters.empty()) return end();
    for (source_index = 0 ; source_index < _input_stream.size() ; source_index++)
        if (_input_stream[source_index]->source_cookie == source_cookie) break;
    if (source_index == _input_stream.size()) return end();

    unsigned char_index = _sourceToCharacter(source_index);
    
    if (_input_stream[source_index]->Type() != TEXT_SOURCE)
        return iterator(this, char_index);

    InputStreamTextSource const *text_source = static_cast<InputStreamTextSource const *>(_input_stream[source_index]);
    if (text_iterator <= text_source->text_begin) return iterator(this, char_index);
    if (text_iterator >= text_source->text_end) {
        if (source_index == _input_stream.size() - 1) return end();
        return iterator(this, _sourceToCharacter(source_index + 1));
    }
    Glib::ustring::const_iterator iter_text = text_source->text_begin;
    for ( ; char_index < _characters.size() ; char_index++) {
        if (iter_text == text_iterator)
            return iterator(this, char_index);
        iter_text++;
    }
    return end(); // never happens
}

Layout::iterator Layout::sourceToIterator(void *source_cookie) const
{
    return sourceToIterator(source_cookie, Glib::ustring::const_iterator(std::string::const_iterator(NULL)));
}

NR::Rect Layout::glyphBoundingBox(iterator const &it, double *rotation) const
{
   if (rotation) *rotation = _glyphs[it._glyph_index].rotation;
   return _glyphs[it._glyph_index].span(this).font->BBox(_glyphs[it._glyph_index].glyph);
}

NR::Point Layout::characterAnchorPoint(iterator const &it) const
{
    if (_characters.empty())
        return _empty_cursor_shape.position;
    if (it._char_index == _characters.size()) {
        return NR::Point(_chunks.back().left_x + _spans.back().x_end, _lines.back().baseline_y + _spans.back().baseline_shift);
    } else {
        return NR::Point(_characters[it._char_index].chunk(this).left_x
                             + _spans[_characters[it._char_index].in_span].x_start
                             + _characters[it._char_index].x,
                         _characters[it._char_index].line(this).baseline_y
                             + _characters[it._char_index].span(this).baseline_shift);
    }
}

NR::Point Layout::chunkAnchorPoint(iterator const &it) const
{
    unsigned chunk_index;

    if (_chunks.empty())
        return NR::Point(0.0, 0.0);

    if (_characters.empty())
        chunk_index = 0;
    else if (it._char_index == _characters.size())
        chunk_index = _chunks.size() - 1;
    else chunk_index = _characters[it._char_index].span(this).in_chunk;

    Alignment alignment = _paragraphs[_lines[_chunks[chunk_index].in_line].in_paragraph].alignment;
    if (alignment == LEFT || alignment == FULL)
        return NR::Point(_chunks[chunk_index].left_x, _lines[chunk_index].baseline_y);

    double chunk_width = _getChunkWidth(chunk_index);
    if (alignment == RIGHT)
        return NR::Point(_chunks[chunk_index].left_x + chunk_width, _lines[chunk_index].baseline_y);
    //centre
    return NR::Point(_chunks[chunk_index].left_x + chunk_width * 0.5, _lines[chunk_index].baseline_y);
}

NR::Rect Layout::characterBoundingBox(iterator const &it, double *rotation) const
{
    NR::Point top_left, bottom_right;
    unsigned char_index = it._char_index;

    if (_path_fitted) {
        double cluster_half_width = 0.0;
        for (int glyph_index = _characters[char_index].in_glyph ; _glyphs[glyph_index].in_character == char_index ; glyph_index++)
            cluster_half_width += _glyphs[glyph_index].width;
        cluster_half_width *= 0.5;

        double midpoint_offset = _characters[char_index].span(this).x_start + _characters[char_index].x + cluster_half_width;
        int unused = 0;
        Path::cut_position *midpoint_otp = const_cast<Path*>(_path_fitted)->CurvilignToPosition(1, &midpoint_offset, unused);
        if (midpoint_offset >= 0.0 && midpoint_otp != NULL && midpoint_otp[0].piece >= 0) {
            NR::Point midpoint;
            NR::Point tangent;
            Span const &span = _characters[char_index].span(this);
            double top = span.baseline_shift - span.line_height.ascent;
            double bottom = span.baseline_shift + span.line_height.descent;

            const_cast<Path*>(_path_fitted)->PointAndTangentAt(midpoint_otp[0].piece, midpoint_otp[0].t, midpoint, tangent);
            top_left[NR::X] = midpoint[NR::X] - cluster_half_width;
            top_left[NR::Y] = midpoint[NR::Y] + top;
            bottom_right[NR::X] = midpoint[NR::X] + cluster_half_width;
            bottom_right[NR::Y] = midpoint[NR::Y] + bottom;
            if (rotation)
                *rotation = atan2(tangent[1], tangent[0]);
        }
        g_free(midpoint_otp);
    } else {
        if (it._char_index == _characters.size()) {
            top_left[NR::X] = bottom_right[NR::X] = _chunks.back().left_x + _spans.back().x_end;
            char_index--;
        } else {
            double span_x = _spans[_characters[it._char_index].in_span].x_start + _characters[it._char_index].chunk(this).left_x;
            top_left[NR::X] = span_x + _characters[it._char_index].x;
            if (it._char_index + 1 == _characters.size() || _characters[it._char_index + 1].in_span != _characters[it._char_index].in_span)
                bottom_right[NR::X] = _spans[_characters[it._char_index].in_span].x_end + _characters[it._char_index].chunk(this).left_x;
            else
                bottom_right[NR::X] = span_x + _characters[it._char_index + 1].x;
        }

        double baseline_y = _characters[char_index].line(this).baseline_y + _characters[char_index].span(this).baseline_shift;
        if (_directions_are_orthogonal(_blockProgression(), TOP_TO_BOTTOM)) {
            double span_height = _spans[_characters[char_index].in_span].line_height.ascent + _spans[_characters[char_index].in_span].line_height.descent;
            top_left[NR::Y] = top_left[NR::X];
            top_left[NR::X] = baseline_y - span_height * 0.5;
            bottom_right[NR::Y] = bottom_right[NR::X];
            bottom_right[NR::X] = baseline_y + span_height * 0.5;
        } else {
            top_left[NR::Y] = baseline_y - _spans[_characters[char_index].in_span].line_height.ascent;
            bottom_right[NR::Y] = baseline_y + _spans[_characters[char_index].in_span].line_height.descent;
        }

        if (rotation) {
            if (it._glyph_index == -1)
                *rotation = 0.0;
            else if (it._glyph_index == (int)_glyphs.size())
                *rotation = _glyphs.back().rotation;
            else
                *rotation = _glyphs[it._glyph_index].rotation;
        }
    }

    return NR::Rect(top_left, bottom_right);
}

std::vector<NR::Point> Layout::createSelectionShape(iterator const &it_start, iterator const &it_end, NR::Matrix const &transform) const
{
    std::vector<NR::Point> quads;
    unsigned char_index;
    unsigned end_char_index;
    
    if (it_start._char_index < it_end._char_index) {
        char_index = it_start._char_index;
        end_char_index = it_end._char_index;
    } else {
        char_index = it_end._char_index;
        end_char_index = it_start._char_index;
    }
    for ( ; char_index < end_char_index ; ) {
        if (_characters[char_index].in_glyph == -1) {
            char_index++;
            continue;
        }
        double char_rotation = _glyphs[_characters[char_index].in_glyph].rotation;
        unsigned span_index = _characters[char_index].in_span;

        NR::Point top_left, bottom_right;
        if (_path_fitted || char_rotation != 0.0) {
            NR::Rect box = characterBoundingBox(iterator(this, char_index), &char_rotation);
            top_left = box.min();
            bottom_right = box.max();
            char_index++;
        } else {   // for straight text we can be faster by combining all the character boxes in a span into one box
            double span_x = _spans[span_index].x_start + _spans[span_index].chunk(this).left_x;
            top_left[NR::X] = span_x + _characters[char_index].x;
            while (char_index < end_char_index && _characters[char_index].in_span == span_index)
                char_index++;
            if (char_index == _characters.size() || _characters[char_index].in_span != span_index)
                bottom_right[NR::X] = _spans[span_index].x_end + _spans[span_index].chunk(this).left_x;
            else
                bottom_right[NR::X] = span_x + _characters[char_index].x;

            double baseline_y = _spans[span_index].line(this).baseline_y + _spans[span_index].baseline_shift;
            if (_directions_are_orthogonal(_blockProgression(), TOP_TO_BOTTOM)) {
                double span_height = _spans[span_index].line_height.ascent + _spans[span_index].line_height.descent;
                top_left[NR::Y] = top_left[NR::X];
                top_left[NR::X] = baseline_y - span_height * 0.5;
                bottom_right[NR::Y] = bottom_right[NR::X];
                bottom_right[NR::X] = baseline_y + span_height * 0.5;
            } else {
                top_left[NR::Y] = baseline_y - _spans[span_index].line_height.ascent;
                bottom_right[NR::Y] = baseline_y + _spans[span_index].line_height.descent;
            }
        }

        NR::Rect char_box(top_left, bottom_right);
        if (char_box.extent(NR::X) == 0.0 || char_box.extent(NR::Y) == 0.0)
            continue;
        NR::Point center_of_rotation((top_left[NR::X] + bottom_right[NR::X]) * 0.5,
                                     top_left[NR::Y] + _spans[span_index].line_height.ascent);
        NR::Matrix total_transform = NR::translate(-center_of_rotation) * NR::rotate(char_rotation) * NR::translate(center_of_rotation) * transform;
        for(int i = 0; i < 4; i ++)
            quads.push_back(char_box.corner(i) * total_transform);
    }
    return quads;
}

void Layout::queryCursorShape(iterator const &it, NR::Point *position, double *height, double *rotation) const
{
    if (_characters.empty()) {
        *position = _empty_cursor_shape.position;
        *height = _empty_cursor_shape.height;
        *rotation = _empty_cursor_shape.rotation;
    } else {
        // we want to cursor to be positioned where the left edge of a character that is about to be typed will be.
        // this means x & rotation are the current values but y & height belong to the previous character.
        // this isn't quite right because dx attributes will be moved along, but it's good enough
        Span const *span;
        if (_path_fitted) {
            // text on a path
            double x;
            if (it._char_index >= _characters.size()) {
                span = &_spans.back();
                x = span->x_end + _chunks.back().left_x - _chunks[0].left_x;
            } else {
                span = &_spans[_characters[it._char_index].in_span];
                x = _chunks[span->in_chunk].left_x + span->x_start + _characters[it._char_index].x - _chunks[0].left_x;
                if (it._char_index != 0)
                    span = &_spans[_characters[it._char_index - 1].in_span];
            }
            double path_length = const_cast<Path*>(_path_fitted)->Length();
            double x_on_path = x;
            if (x_on_path < 0.0) x_on_path = 0.0;

            int unused = 0;
                // as far as I know these functions are const, they're just not marked as such
            Path::cut_position *path_parameter_list = const_cast<Path*>(_path_fitted)->CurvilignToPosition(1, &x_on_path, unused);
            Path::cut_position path_parameter;
            if (path_parameter_list != NULL && path_parameter_list[0].piece >= 0)
                path_parameter = path_parameter_list[0];
            else {
                path_parameter.piece = _path_fitted->descr_cmd.size() - 1;
                path_parameter.t = 0.9999;   // 1.0 will get the wrong tangent
            }
            g_free(path_parameter_list);

            NR::Point point;
            NR::Point tangent;
            const_cast<Path*>(_path_fitted)->PointAndTangentAt(path_parameter.piece, path_parameter.t, point, tangent);
            if (x < 0.0)
                point += x * tangent;
            if (x > path_length )
                point += (x - path_length) * tangent;
            *rotation = atan2(tangent);
            (*position)[NR::X] = point[NR::X] - tangent[NR::Y] * span->baseline_shift;
            (*position)[NR::Y] = point[NR::Y] + tangent[NR::X] * span->baseline_shift;
        } else {
            // text is not on a path
            if (it._char_index >= _characters.size()) {
                span = &_spans.back();
                (*position)[NR::X] = _chunks[span->in_chunk].left_x + span->x_end;
                *rotation = _glyphs.empty() ? 0.0 : _glyphs.back().rotation;
            } else {
                span = &_spans[_characters[it._char_index].in_span];
                (*position)[NR::X] = _chunks[span->in_chunk].left_x + span->x_start + _characters[it._char_index].x;
                if (it._glyph_index == -1) *rotation = 0.0;
                else if(it._glyph_index == 0) *rotation = _glyphs[0].rotation;
                else *rotation = _glyphs[it._glyph_index - 1].rotation;
                // the first char in a line wants to have the y of the new line, so in that case we don't switch to the previous span
                if (it._char_index != 0 && _characters[it._char_index - 1].chunk(this).in_line == _chunks[span->in_chunk].in_line)
                    span = &_spans[_characters[it._char_index - 1].in_span];
            }
            (*position)[NR::Y] = span->line(this).baseline_y + span->baseline_shift;
        }
        // up to now *position is the baseline point, not the final point which will be the bottom of the descent
        if (_directions_are_orthogonal(_blockProgression(), TOP_TO_BOTTOM)) {
            *height = span->line_height.ascent + span->line_height.descent;
            *rotation += M_PI / 2;
            std::swap((*position)[NR::X], (*position)[NR::Y]);
            (*position)[NR::X] -= sin(*rotation) * *height * 0.5;
            (*position)[NR::Y] += cos(*rotation) * *height * 0.5;
        } else {
            double caret_slope_run = 0.0, caret_slope_rise = 1.0;
            if (span->font)
                const_cast<font_instance*>(span->font)->FontSlope(caret_slope_run, caret_slope_rise);
            double caret_slope = atan2(caret_slope_run, caret_slope_rise);
            *height = (span->line_height.ascent + span->line_height.descent) / cos(caret_slope);
            *rotation += caret_slope;
            (*position)[NR::X] -= sin(*rotation) * span->line_height.descent;
            (*position)[NR::Y] += cos(*rotation) * span->line_height.descent;
        }
    }
}

void Layout::getSourceOfCharacter(iterator const &it, void **source_cookie, Glib::ustring::iterator *text_iterator) const
{
    if (it._char_index == _characters.size()) {
        *source_cookie = NULL;
        return;
    }
    InputStreamItem *stream_item = _input_stream[_spans[_characters[it._char_index].in_span].in_input_stream_item];
    *source_cookie = stream_item->source_cookie;
    if (text_iterator && stream_item->Type() == TEXT_SOURCE) {
        InputStreamTextSource const *text_source = static_cast<InputStreamTextSource const *>(stream_item);
        Glib::ustring::const_iterator text_iter_const = text_source->text_begin;
        unsigned char_index = it._char_index;
        unsigned original_input_source_index = _spans[_characters[char_index].in_span].in_input_stream_item;
        // confusing algorithm because the iterator goes forwards while the index goes backwards.
        // It's just that it's faster doing it that way
        while (char_index && _spans[_characters[char_index - 1].in_span].in_input_stream_item == original_input_source_index) {
            ++text_iter_const;
            char_index--;
        }
        text_source->text->begin().base() + (text_iter_const.base() - text_source->text->begin().base());
        *text_iterator = Glib::ustring::iterator(std::string::iterator(const_cast<char*>(&*text_source->text->begin().base() + (text_iter_const.base() - text_source->text->begin().base()))));
             // the caller owns the string, so they're going to want a non-const iterator
    }
}

void Layout::simulateLayoutUsingKerning(iterator const &from, iterator const &to, OptionalTextTagAttrs *result) const
{
    SPSVGLength zero_length;
    zero_length = 0.0;

    result->x.clear();
    result->y.clear();
    result->dx.clear();
    result->dy.clear();
    result->rotate.clear();
    if (to._char_index <= from._char_index)
        return;
    result->dx.reserve(to._char_index - from._char_index);
    result->dy.reserve(to._char_index - from._char_index);
    result->rotate.reserve(to._char_index - from._char_index);
    for (unsigned char_index = from._char_index ; char_index < to._char_index ; char_index++) {
        if (!_characters[char_index].char_attributes.is_char_break)
            continue;
        if (char_index == 0)
            continue;
        if (_characters[char_index].chunk(this).in_line != _characters[char_index - 1].chunk(this).in_line)
            continue;

        unsigned prev_cluster_char_index;
        for (prev_cluster_char_index = char_index - 1 ;
             prev_cluster_char_index != 0 && !_characters[prev_cluster_char_index].char_attributes.is_cursor_position ;
             prev_cluster_char_index--);
        if (_characters[char_index].span(this).in_chunk == _characters[char_index - 1].span(this).in_chunk) {
            // dx is zero for the first char in a chunk
            // this algorithm works by comparing the summed widths of the glyphs with the observed
            // difference in x coordinates of characters, and subtracting the two to produce the x kerning.
            double glyphs_width = 0.0;
            if (_characters[prev_cluster_char_index].in_glyph != -1)
                for (int glyph_index = _characters[prev_cluster_char_index].in_glyph ; glyph_index < _characters[char_index].in_glyph ; glyph_index++)
                    glyphs_width += _glyphs[glyph_index].width;
            if (_characters[char_index].span(this).direction == RIGHT_TO_LEFT)
                glyphs_width = -glyphs_width;
            double dx = (_characters[char_index].x + _characters[char_index].span(this).x_start - _characters[prev_cluster_char_index].x - _characters[prev_cluster_char_index].span(this).x_start) - glyphs_width;
            if (fabs(dx) > 0.0001) {
                result->dx.resize(char_index - from._char_index + 1, zero_length);
                result->dx.back() = dx;
            }
        }
        double dy = _characters[char_index].span(this).baseline_shift - _characters[prev_cluster_char_index].span(this).baseline_shift;
        if (fabs(dy) > 0.0001) {
            result->dy.resize(char_index - from._char_index + 1, zero_length);
            result->dy.back() = dy;
        }
        if (_characters[char_index].in_glyph != -1 && _glyphs[_characters[char_index].in_glyph].rotation != 0.0) {
            result->rotate.resize(char_index - from._char_index + 1, zero_length);
            result->rotate.back() = _glyphs[_characters[char_index].in_glyph].rotation;
        }
    }
}

#define PREV_START_OF_ITEM(this_func)                                                    \
    {                                                                                    \
        _cursor_moving_vertically = false;                                               \
        if (_char_index == 0) return false;                                              \
        _char_index--;                                                                   \
        return this_func();                                                              \
    }
// end of macro

#define THIS_START_OF_ITEM(item_getter)                                                  \
    {                                                                                    \
        _cursor_moving_vertically = false;                                               \
        if (_char_index == 0) return false;                                              \
        unsigned original_item;                                                          \
        if (_char_index == _parent_layout->_characters.size()) {                         \
            _char_index--;                                                               \
            original_item = item_getter;                                                 \
        } else {                                                                         \
            original_item = item_getter;                                                 \
            _char_index--;                                                               \
        }                                                                                \
        while (item_getter == original_item) {                                           \
            if (_char_index == 0) {                                                      \
                _glyph_index = _parent_layout->_characters[_char_index].in_glyph;        \
                return true;                                                             \
            }                                                                            \
            _char_index--;                                                               \
        }                                                                                \
        _char_index++;                                                                   \
        _glyph_index = _parent_layout->_characters[_char_index].in_glyph;                \
        return true;                                                                     \
    }
// end of macro

#define NEXT_START_OF_ITEM(item_getter)                                                  \
    {                                                                                    \
        _cursor_moving_vertically = false;                                               \
        if (_char_index == _parent_layout->_characters.size()) return false;             \
        unsigned original_item = item_getter;                                            \
        for( ; ; ) {                                                                     \
            _char_index++;                                                               \
            if (_char_index == _parent_layout->_characters.size()) {                     \
                _glyph_index = _parent_layout->_glyphs.size();                           \
                return false;                                                            \
            }                                                                            \
            if (item_getter != original_item) break;                                     \
        }                                                                                \
        _glyph_index = _parent_layout->_characters[_char_index].in_glyph;                \
        return true;                                                                     \
    }
// end of macro

bool Layout::iterator::prevStartOfSpan()
    PREV_START_OF_ITEM(thisStartOfSpan);

bool Layout::iterator::thisStartOfSpan()
    THIS_START_OF_ITEM(_parent_layout->_characters[_char_index].in_span);

bool Layout::iterator::nextStartOfSpan()
    NEXT_START_OF_ITEM(_parent_layout->_characters[_char_index].in_span);


bool Layout::iterator::prevStartOfChunk()
    PREV_START_OF_ITEM(thisStartOfChunk);

bool Layout::iterator::thisStartOfChunk()
    THIS_START_OF_ITEM(_parent_layout->_characters[_char_index].span(_parent_layout).in_chunk);

bool Layout::iterator::nextStartOfChunk()
    NEXT_START_OF_ITEM(_parent_layout->_characters[_char_index].span(_parent_layout).in_chunk);


bool Layout::iterator::prevStartOfLine()
    PREV_START_OF_ITEM(thisStartOfLine);

bool Layout::iterator::thisStartOfLine()
    THIS_START_OF_ITEM(_parent_layout->_characters[_char_index].chunk(_parent_layout).in_line);

bool Layout::iterator::nextStartOfLine()
    NEXT_START_OF_ITEM(_parent_layout->_characters[_char_index].chunk(_parent_layout).in_line);


bool Layout::iterator::prevStartOfShape()
    PREV_START_OF_ITEM(thisStartOfShape);

bool Layout::iterator::thisStartOfShape()
    THIS_START_OF_ITEM(_parent_layout->_characters[_char_index].line(_parent_layout).in_shape);

bool Layout::iterator::nextStartOfShape()
    NEXT_START_OF_ITEM(_parent_layout->_characters[_char_index].line(_parent_layout).in_shape);


bool Layout::iterator::prevStartOfParagraph()
    PREV_START_OF_ITEM(thisStartOfParagraph);

bool Layout::iterator::thisStartOfParagraph()
    THIS_START_OF_ITEM(_parent_layout->_characters[_char_index].line(_parent_layout).in_paragraph);

bool Layout::iterator::nextStartOfParagraph()
    NEXT_START_OF_ITEM(_parent_layout->_characters[_char_index].line(_parent_layout).in_paragraph);


bool Layout::iterator::prevStartOfSource()
    PREV_START_OF_ITEM(thisStartOfSource);

bool Layout::iterator::thisStartOfSource()
    THIS_START_OF_ITEM(_parent_layout->_characters[_char_index].span(_parent_layout).in_input_stream_item);

bool Layout::iterator::nextStartOfSource()
    NEXT_START_OF_ITEM(_parent_layout->_characters[_char_index].span(_parent_layout).in_input_stream_item);


bool Layout::iterator::thisEndOfLine()
{
    if (_char_index == _parent_layout->_characters.size()) return false;
    if (nextStartOfLine())
    {
        if (_char_index && _parent_layout->_characters[_char_index - 1].char_attributes.is_white)
            return prevCursorPosition();
        return true;
    }
    if (_char_index && _parent_layout->_characters[_char_index - 1].chunk(_parent_layout).in_line != _parent_layout->_lines.size() - 1)
        return prevCursorPosition();   // for when the last paragraph is empty
    return false;
}

void Layout::iterator::beginCursorUpDown()
{
    if (_char_index == _parent_layout->_characters.size())
        _x_coordinate = _parent_layout->_chunks.back().left_x + _parent_layout->_spans.back().x_end;
    else
        _x_coordinate = _parent_layout->_characters[_char_index].x + _parent_layout->_characters[_char_index].span(_parent_layout).x_start + _parent_layout->_characters[_char_index].chunk(_parent_layout).left_x;
    _cursor_moving_vertically = true;
}

bool Layout::iterator::nextLineCursor()
{
    if (!_cursor_moving_vertically)
        beginCursorUpDown();
    if (_char_index == _parent_layout->_characters.size())
        return false;
    unsigned line_index = _parent_layout->_characters[_char_index].chunk(_parent_layout).in_line;
    if (line_index == _parent_layout->_lines.size() - 1) return false;
    if (_parent_layout->_lines[line_index + 1].in_shape != _parent_layout->_lines[line_index].in_shape) {
        // switching between shapes: adjust the stored x to compensate
        _x_coordinate +=   _parent_layout->_chunks[_parent_layout->_spans[_parent_layout->_lineToSpan(line_index + 1)].in_chunk].left_x
                         - _parent_layout->_chunks[_parent_layout->_spans[_parent_layout->_lineToSpan(line_index)].in_chunk].left_x;
    }
    _char_index = _parent_layout->_cursorXOnLineToIterator(line_index + 1, _x_coordinate)._char_index;
    _glyph_index = _parent_layout->_characters[_char_index].in_glyph;
    return true;
}

bool Layout::iterator::prevLineCursor()
{
    if (!_cursor_moving_vertically)
        beginCursorUpDown();
    unsigned line_index;
    if (_char_index == _parent_layout->_characters.size())
        line_index = _parent_layout->_lines.size() - 1;
    else
        line_index = _parent_layout->_characters[_char_index].chunk(_parent_layout).in_line;
    if (line_index == 0) return false;
    if (_parent_layout->_lines[line_index - 1].in_shape != _parent_layout->_lines[line_index].in_shape) {
        // switching between shapes: adjust the stored x to compensate
        _x_coordinate +=   _parent_layout->_chunks[_parent_layout->_spans[_parent_layout->_lineToSpan(line_index - 1)].in_chunk].left_x
                         - _parent_layout->_chunks[_parent_layout->_spans[_parent_layout->_lineToSpan(line_index)].in_chunk].left_x;
    }
    _char_index = _parent_layout->_cursorXOnLineToIterator(line_index - 1, _x_coordinate)._char_index;
    _glyph_index = _parent_layout->_characters[_char_index].in_glyph;
    return true;
}

#define NEXT_WITH_ATTRIBUTE_SET(attr)                                                            \
    {                                                                                            \
        _cursor_moving_vertically = false;                                                       \
        for ( ; ; ) {                                                                            \
            if (_char_index >= _parent_layout->_characters.size() - 1) {                         \
                _char_index = _parent_layout->_characters.size();                                \
                _glyph_index = _parent_layout->_glyphs.size();                                   \
                return false;                                                                    \
            }                                                                                    \
            _char_index++;                                                                       \
            if (_parent_layout->_characters[_char_index].char_attributes.attr) break;            \
        }                                                                                        \
        _glyph_index = _parent_layout->_characters[_char_index].in_glyph;                        \
        return true;                                                                             \
    }
// end of macro

#define PREV_WITH_ATTRIBUTE_SET(attr)                                                            \
    {                                                                                            \
        _cursor_moving_vertically = false;                                                       \
        for ( ; ; ) {                                                                            \
            if (_char_index == 0) {                                                              \
                _glyph_index = 0;                                                                \
                return true;                                                                     \
            }                                                                                    \
            _char_index--;                                                                       \
            if (_parent_layout->_characters[_char_index].char_attributes.attr) break;            \
        }                                                                                        \
        _glyph_index = _parent_layout->_characters[_char_index].in_glyph;                        \
        return true;                                                                             \
    }
// end of macro

bool Layout::iterator::nextCursorPosition()
    NEXT_WITH_ATTRIBUTE_SET(is_cursor_position);

bool Layout::iterator::prevCursorPosition()
    PREV_WITH_ATTRIBUTE_SET(is_cursor_position);

bool Layout::iterator::nextStartOfWord()
    NEXT_WITH_ATTRIBUTE_SET(is_word_start);

bool Layout::iterator::prevStartOfWord()
    PREV_WITH_ATTRIBUTE_SET(is_word_start);

bool Layout::iterator::nextEndOfWord()
    NEXT_WITH_ATTRIBUTE_SET(is_word_end);

bool Layout::iterator::prevEndOfWord()
    PREV_WITH_ATTRIBUTE_SET(is_word_end);

bool Layout::iterator::nextStartOfSentence()
    NEXT_WITH_ATTRIBUTE_SET(is_sentence_start);

bool Layout::iterator::prevStartOfSentence()
    PREV_WITH_ATTRIBUTE_SET(is_sentence_start);

bool Layout::iterator::nextEndOfSentence()
    NEXT_WITH_ATTRIBUTE_SET(is_sentence_end);

bool Layout::iterator::prevEndOfSentence()
    PREV_WITH_ATTRIBUTE_SET(is_sentence_end);

bool Layout::iterator::_cursorLeftOrRightLocalX(Direction direction)
{
    // the only reason this function is so complicated is to enable visual cursor
    // movement moving in to or out of counterdirectional runs
    if (_parent_layout->_characters.empty()) return false;
    unsigned old_span_index;
    Direction old_span_direction;
    if (_char_index == _parent_layout->_characters.size())
        old_span_index = _parent_layout->_spans.size() - 1;
    else
        old_span_index = _parent_layout->_characters[_char_index].in_span;
    old_span_direction = _parent_layout->_spans[old_span_index].direction;
    Direction para_direction = _parent_layout->_spans[old_span_index].paragraph(_parent_layout).base_direction;

    int scan_direction;
    unsigned old_char_index = _char_index;
    if (old_span_direction != para_direction
        && ((_char_index == 0 && direction == para_direction)
            || (_char_index == _parent_layout->_characters.size() && direction != para_direction))) {
        // the end of the text is actually in the middle because of reordering. Do cleverness
        scan_direction = direction == para_direction ? +1 : -1;
    } else {
        if (direction == old_span_direction) {
            if (!nextCursorPosition()) return false;
        } else {
            if (!prevCursorPosition()) return false;
        }

        unsigned new_span_index = _parent_layout->_characters[_char_index].in_span;
        if (new_span_index == old_span_index) return true;
        if (old_span_direction != _parent_layout->_spans[new_span_index].direction) {
            // we must jump to the other end of a counterdirectional run
            scan_direction = direction == para_direction ? +1 : -1;
        } else if (_parent_layout->_spans[old_span_index].in_chunk != _parent_layout->_spans[new_span_index].in_chunk) {
            // we might have to do a weird jump when we would have crossed a chunk/line break
            if (_parent_layout->_spans[old_span_index].line(_parent_layout).in_paragraph != _parent_layout->_spans[new_span_index].line(_parent_layout).in_paragraph)
                return true;
            if (old_span_direction == para_direction)
                return true;
            scan_direction = direction == para_direction ? +1 : -1;
        } else
            return true;    // same direction, same chunk: no cleverness required
    }

    unsigned new_span_index = old_span_index;
    for ( ; ; ) {
        if (scan_direction > 0) {
            if (new_span_index == _parent_layout->_spans.size() - 1) {
                if (_parent_layout->_spans[new_span_index].direction == old_span_direction) {
                    _char_index = old_char_index;
                    return false;    // the visual end is in the logical middle
                }
                break;
            }
            new_span_index++;
        } else {
            if (new_span_index == 0) {
                if (_parent_layout->_spans[new_span_index].direction == old_span_direction) {
                    _char_index = old_char_index;
                    return false;    // the visual end is in the logical middle
                }
                break;
            }
            new_span_index--;
        }
        if (_parent_layout->_spans[new_span_index].direction == para_direction) {
            if (para_direction == old_span_direction)
                new_span_index -= scan_direction;
            break;
        }
        if (_parent_layout->_spans[new_span_index].in_chunk != _parent_layout->_spans[old_span_index].in_chunk)
            break;
    }

    // found the correct span, now find the correct character
    if (_parent_layout->_spans[new_span_index].direction != direction) {
        if (new_span_index >= _parent_layout->_spans.size() - 1)
            _char_index = _parent_layout->_characters.size();
        else
            _char_index = _parent_layout->_spanToCharacter(new_span_index + 1) - 1;
    } else
        _char_index = _parent_layout->_spanToCharacter(new_span_index);
    if (_char_index == _parent_layout->_characters.size()) {
        _glyph_index = _parent_layout->_glyphs.size();
        return false;
    }
    _glyph_index = _parent_layout->_characters[_char_index].in_glyph;
    return _char_index != 0;
}

bool Layout::iterator::cursorUp()
{
    Direction block_progression = _parent_layout->_blockProgression();
    if(block_progression == TOP_TO_BOTTOM)
        return prevLineCursor();
    else if(block_progression == BOTTOM_TO_TOP)
        return nextLineCursor();
    else
        return _cursorLeftOrRightLocalX(RIGHT_TO_LEFT);
}

bool Layout::iterator::cursorDown()
{
    Direction block_progression = _parent_layout->_blockProgression();
    if(block_progression == TOP_TO_BOTTOM)
        return nextLineCursor();
    else if(block_progression == BOTTOM_TO_TOP)
        return prevLineCursor();
    else
        return _cursorLeftOrRightLocalX(LEFT_TO_RIGHT);
}

bool Layout::iterator::cursorLeft()
{
    Direction block_progression = _parent_layout->_blockProgression();
    if(block_progression == LEFT_TO_RIGHT)
        return prevLineCursor();
    else if(block_progression == RIGHT_TO_LEFT)
        return nextLineCursor();
    else
        return _cursorLeftOrRightLocalX(RIGHT_TO_LEFT);
}

bool Layout::iterator::cursorRight()
{
    Direction block_progression = _parent_layout->_blockProgression();
    if(block_progression == LEFT_TO_RIGHT)
        return nextLineCursor();
    else if(block_progression == RIGHT_TO_LEFT)
        return prevLineCursor();
    else
        return _cursorLeftOrRightLocalX(LEFT_TO_RIGHT);
}

}//namespace Text
}//namespace Inkscape
