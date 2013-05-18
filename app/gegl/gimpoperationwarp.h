/* GIMP - The GNU Image Manipulation Program
 *
 * gimpoperationwarp.h
 * Copyright (C) 2011 Michael Muré <batolettre@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GIMP_OPERATION_WARP_H__
#define __GIMP_OPERATION_WARP_H__


#include <gegl-plugin.h>
#include <operation/gegl-operation-filter.h>

#define GIMP_TYPE_OPERATION_WARP            (gimp_operation_warp_get_type ())
#define GIMP_OPERATION_WARP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_OPERATION_WARP, GimpOperationWarp))
#define GIMP_OPERATION_WARP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GIMP_TYPE_OPERATION_WARP, GimpOperationWarpClass))
#define GIMP_IS_OPERATION_WARP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_OPERATION_WARP))
#define GIMP_IS_OPERATION_WARP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIMP_TYPE_OPERATION_WARP))
#define GIMP_OPERATION_WARP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIMP_TYPE_OPERATION_WARP, GimpOperationWarpClass))


typedef struct _GimpOperationWarpClass GimpOperationWarpClass;

struct _GimpOperationWarp
{
  GeglOperationFilter  parent_instance;

  /* Properties of the operation */
  gdouble              strength;
  gdouble              size;
  gdouble              hardness;
  GeglPath            *stroke;
  GimpWarpBehavior     behavior;

  /* last stamp location for movement dependant behavior like move */
  gdouble              last_x;
  gdouble              last_y;
  gboolean             last_point_set;

  /* used to pass the temporary buffer to the function called by gegl_path_foreach */
  GeglBuffer          *buffer;

  gfloat              *lookup;
};

struct _GimpOperationWarpClass
{
  GeglOperationFilterClass  parent_class;
};


GType   gimp_operation_warp_get_type (void) G_GNUC_CONST;


#endif /* __GIMP_OPERATION_WARP_H__ */
