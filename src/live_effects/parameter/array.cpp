/*
 * Copyright (C) Johan Engelen 2008 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/parameter/array.h"

#include "helper-fns.h"

#include <2geom/coord.h>
#include <2geom/point.h>

namespace Inkscape {

namespace LivePathEffect {

template <>
double
ArrayParam<double>::readsvg(const gchar * str)
{
    double newx = Geom::infinity();
    sp_svg_number_read_d(str, &newx);
    return newx;
}

template <>
float
ArrayParam<float>::readsvg(const gchar * str)
{
    float newx = Geom::infinity();
    sp_svg_number_read_f(str, &newx);
    return newx;
}

template <>
Geom::Point
ArrayParam<Geom::Point>::readsvg(const gchar * str)
{
    gchar ** strarray = g_strsplit(str, ",", 2);
    double newx, newy;
    unsigned int success = sp_svg_number_read_d(strarray[0], &newx);
    success += sp_svg_number_read_d(strarray[1], &newy);
    g_strfreev (strarray);
    if (success == 2) {
        return Geom::Point(newx, newy);
    }
    return Geom::Point(Geom::infinity(),Geom::infinity());
}
//TODO: move maybe to svg-lenght.cpp
unsigned int
sp_svg_satellite_read_d(gchar const *str, Geom::Satellite *sat){
    if (!str) {
        return 0;
    }
    gchar ** strarray = g_strsplit(str, "*", 0);
    if(strarray[6] && !strarray[7]){
        sat->setSatelliteType(strarray[0]);
        sat->setIsTime(strncmp(strarray[1],"1",1) == 0);
        sat->setActive(strncmp(strarray[2],"1",1) == 0);
        sat->setHasMirror(strncmp(strarray[3],"1",1) == 0);
        sat->setHidden(strncmp(strarray[4],"1",1) == 0);
        double time,size;
        sp_svg_number_read_d(strarray[5], &size);
        sp_svg_number_read_d(strarray[6], &time);
        sat->setSize(size);
        sat->setTime(time);
        g_strfreev (strarray);
        return 1;
    }
    g_strfreev (strarray);
    return 0;
}

template <>
std::pair<int, Geom::Satellite>
ArrayParam<std::pair<int, Geom::Satellite> >::readsvg(const gchar * str)
{
    gchar ** strarray = g_strsplit(str, ",", 2);
    double index;
    std::pair<int, Geom::Satellite> result;
    unsigned int success = (int)sp_svg_number_read_d(strarray[0], &index);
    Geom::Satellite sat;
    success += sp_svg_satellite_read_d(strarray[1], &sat);
    g_strfreev (strarray);
    if (success == 2) {
        return std::make_pair(index, sat);
    }
    return std::make_pair((int)Geom::infinity(),sat);
}

} /* namespace LivePathEffect */

} /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
