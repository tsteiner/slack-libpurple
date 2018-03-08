#ifndef G_DECLARE_FINAL_TYPE

#define G_DECLARE_FINAL_TYPE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, ParentName) \
  GType module_obj_name##_get_type (void);                                                               \
  typedef struct _##ModuleObjName ModuleObjName;                                                         \
  typedef struct { ParentName##Class parent_class; } ModuleObjName##Class;                               \
                                                                                                         \
  static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) {                                     \
    return G_TYPE_CHECK_INSTANCE_CAST (ptr, module_obj_name##_get_type (), ModuleObjName); }             \
  static inline gboolean MODULE##_IS_##OBJ_NAME (gpointer ptr) {                                         \
    return G_TYPE_CHECK_INSTANCE_TYPE (ptr, module_obj_name##_get_type ()); }                            

#endif
