#ifndef __GIMP_OBJECT_H__
#define __GIMP_OBJECT_H__
/* This will be the base class of all Gimp classes */

/* There are three different access levels to a class:

   Just pointers:
     In this case (probably a header file for another type), you only
     need to declare pointers to a type. Just do #include
     "type_decl.h" before declaring those.

   Methods:
     In most cases, you also want to invoke a classes methods, that
     is, functions. To do this, simply #include "type.h" to get their
     declarations. This also brings the type in scope.

   Member access:
     You should only need access to a classes members if you are
     implementing that class, or creating subclasses. In this case,
     use #include "type_pvt.h". This brings the struct definition in
     scope.

   So, to sum it up, in typical cases of using a class, eg. GimpImage:

   A header for an unrelated class:
     #include "gimpimage_decl.h"
   
   The implementation of an unrelated class:
     #include "gimpimage.h"

     
   A private header for a derived class:
     #include "gimpimage_pvt.h"

   The implementation of the derived class:
     #include "gimpimage.h"
   
   The public header of the class itself:
     #include "gimpimage_decl.h"

   In the private header of the class itself, if it has pointers to
   the same class:
     #include "gimpimage_decl.h"
     
   The implementation of the class itself:
     #include "gimpimage.h"
     #include "gimpimage_pvt.h"

   The file "type_decl.h" should only have include guards and a
   typedef and possible forward declaration of the actual type. In
   most cases it will be a struct, but for non-gtkobject types it
   could be an enum or even an just an integer or anything. Just so it 
   brings the type name properly in the scope so pointers to it can be 
   used.

   The file "type.h" (should this one have a suffix too?) should have
   the external (public) method declarations for this class.

   The file "type_pvt.h" should have the actual definition of the
   object's internal structure. It should also include a typedef for
   the class in the form "GimpImagePvt". This is basically the same
   type as GimpImage, but it should be used where appropriate to
   remind us that we have private access to a type. The gtk class
   structure should also be defined and typedef'd properly here.

*/
     
#define DEF_GET_TYPE(class, prefix, parent_prefix) \
guint prefix##_get_type (void)\
{\
  static guint type = 0;\
  if (!type)\
    {\
      GtkTypeInfo info =\
      {\
	#class,\
	sizeof (class),\
	sizeof (class##Class),\
	(GtkClassInitFunc) prefix##_class_init,\
	(GtkObjectInitFunc) prefix##_init,\
	(GtkArgSetFunc) NULL,\
	(GtkArgGetFunc) NULL,\
      };\
      type = gtk_type_unique (parent_prefix##_get_type (), &info);\
    }\
  return type;\
}

#include "gimpobject_decl.h"
#define GIMP_OBJECT(obj) GTK_CHECK_CAST (obj, gimp_object_get_type (), GimpObject)


#define GIMP_OBJECT_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gimp_object_get_type(), GimpObjectClass)
     
     
#define GIMP_IS_OBJECT(obj) GTK_CHECK_TYPE (obj, gimp_object_get_type())


guint gimp_object_get_type(void);


/* Method declarations here */



#endif




	
