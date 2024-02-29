/* This file is generated by venus-protocol.  See vn_protocol_renderer.h. */

/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef VN_PROTOCOL_RENDERER_INFO_H
#define VN_PROTOCOL_RENDERER_INFO_H

#include "vn_protocol_renderer_defines.h"

struct vn_info_extension_table {
   union {
      bool enabled[115];
      struct {
         bool EXT_4444_formats;
         bool EXT_attachment_feedback_loop_layout;
         bool EXT_border_color_swizzle;
         bool EXT_calibrated_timestamps;
         bool EXT_color_write_enable;
         bool EXT_command_serialization;
         bool EXT_conditional_rendering;
         bool EXT_conservative_rasterization;
         bool EXT_custom_border_color;
         bool EXT_depth_clip_control;
         bool EXT_depth_clip_enable;
         bool EXT_descriptor_indexing;
         bool EXT_dynamic_rendering_unused_attachments;
         bool EXT_extended_dynamic_state;
         bool EXT_extended_dynamic_state2;
         bool EXT_extended_dynamic_state3;
         bool EXT_external_memory_dma_buf;
         bool EXT_fragment_shader_interlock;
         bool EXT_graphics_pipeline_library;
         bool EXT_host_query_reset;
         bool EXT_image_2d_view_of_3d;
         bool EXT_image_drm_format_modifier;
         bool EXT_image_robustness;
         bool EXT_image_view_min_lod;
         bool EXT_index_type_uint8;
         bool EXT_inline_uniform_block;
         bool EXT_line_rasterization;
         bool EXT_load_store_op_none;
         bool EXT_memory_budget;
         bool EXT_multi_draw;
         bool EXT_mutable_descriptor_type;
         bool EXT_non_seamless_cube_map;
         bool EXT_pci_bus_info;
         bool EXT_pipeline_creation_cache_control;
         bool EXT_pipeline_creation_feedback;
         bool EXT_primitive_topology_list_restart;
         bool EXT_primitives_generated_query;
         bool EXT_private_data;
         bool EXT_provoking_vertex;
         bool EXT_queue_family_foreign;
         bool EXT_rasterization_order_attachment_access;
         bool EXT_robustness2;
         bool EXT_sampler_filter_minmax;
         bool EXT_scalar_block_layout;
         bool EXT_separate_stencil_usage;
         bool EXT_shader_demote_to_helper_invocation;
         bool EXT_shader_stencil_export;
         bool EXT_shader_subgroup_ballot;
         bool EXT_shader_viewport_index_layer;
         bool EXT_subgroup_size_control;
         bool EXT_texel_buffer_alignment;
         bool EXT_texture_compression_astc_hdr;
         bool EXT_tooling_info;
         bool EXT_transform_feedback;
         bool EXT_vertex_attribute_divisor;
         bool EXT_vertex_input_dynamic_state;
         bool EXT_ycbcr_2plane_444_formats;
         bool KHR_16bit_storage;
         bool KHR_8bit_storage;
         bool KHR_bind_memory2;
         bool KHR_buffer_device_address;
         bool KHR_copy_commands2;
         bool KHR_create_renderpass2;
         bool KHR_dedicated_allocation;
         bool KHR_depth_stencil_resolve;
         bool KHR_descriptor_update_template;
         bool KHR_device_group;
         bool KHR_device_group_creation;
         bool KHR_draw_indirect_count;
         bool KHR_driver_properties;
         bool KHR_dynamic_rendering;
         bool KHR_external_fence;
         bool KHR_external_fence_capabilities;
         bool KHR_external_fence_fd;
         bool KHR_external_memory;
         bool KHR_external_memory_capabilities;
         bool KHR_external_memory_fd;
         bool KHR_external_semaphore;
         bool KHR_external_semaphore_capabilities;
         bool KHR_external_semaphore_fd;
         bool KHR_format_feature_flags2;
         bool KHR_get_memory_requirements2;
         bool KHR_get_physical_device_properties2;
         bool KHR_image_format_list;
         bool KHR_imageless_framebuffer;
         bool KHR_maintenance1;
         bool KHR_maintenance2;
         bool KHR_maintenance3;
         bool KHR_maintenance4;
         bool KHR_multiview;
         bool KHR_pipeline_library;
         bool KHR_push_descriptor;
         bool KHR_relaxed_block_layout;
         bool KHR_sampler_mirror_clamp_to_edge;
         bool KHR_sampler_ycbcr_conversion;
         bool KHR_separate_depth_stencil_layouts;
         bool KHR_shader_atomic_int64;
         bool KHR_shader_clock;
         bool KHR_shader_draw_parameters;
         bool KHR_shader_float16_int8;
         bool KHR_shader_float_controls;
         bool KHR_shader_integer_dot_product;
         bool KHR_shader_non_semantic_info;
         bool KHR_shader_subgroup_extended_types;
         bool KHR_shader_terminate_invocation;
         bool KHR_spirv_1_4;
         bool KHR_storage_buffer_storage_class;
         bool KHR_synchronization2;
         bool KHR_timeline_semaphore;
         bool KHR_uniform_buffer_standard_layout;
         bool KHR_variable_pointers;
         bool KHR_vulkan_memory_model;
         bool KHR_zero_initialize_workgroup_memory;
         bool MESA_venus_protocol;
         bool VALVE_mutable_descriptor_type;
      };
   };
};

#define VN_INFO_EXTENSION_MAX_NUMBER (500)

struct vn_info_extension {
   const char *name;
   uint32_t number;
   uint32_t spec_version;
};

/* sorted by extension names for bsearch */
static const uint32_t _vn_info_extension_count = 115;
static const struct vn_info_extension _vn_info_extensions[115] = {
   { "VK_EXT_4444_formats", 341, 1 },
   { "VK_EXT_attachment_feedback_loop_layout", 340, 2 },
   { "VK_EXT_border_color_swizzle", 412, 1 },
   { "VK_EXT_calibrated_timestamps", 185, 2 },
   { "VK_EXT_color_write_enable", 382, 1 },
   { "VK_EXT_command_serialization", 384, 1 },
   { "VK_EXT_conditional_rendering", 82, 2 },
   { "VK_EXT_conservative_rasterization", 102, 1 },
   { "VK_EXT_custom_border_color", 288, 12 },
   { "VK_EXT_depth_clip_control", 356, 1 },
   { "VK_EXT_depth_clip_enable", 103, 1 },
   { "VK_EXT_descriptor_indexing", 162, 2 },
   { "VK_EXT_dynamic_rendering_unused_attachments", 500, 1 },
   { "VK_EXT_extended_dynamic_state", 268, 1 },
   { "VK_EXT_extended_dynamic_state2", 378, 1 },
   { "VK_EXT_extended_dynamic_state3", 456, 2 },
   { "VK_EXT_external_memory_dma_buf", 126, 1 },
   { "VK_EXT_fragment_shader_interlock", 252, 1 },
   { "VK_EXT_graphics_pipeline_library", 321, 1 },
   { "VK_EXT_host_query_reset", 262, 1 },
   { "VK_EXT_image_2d_view_of_3d", 394, 1 },
   { "VK_EXT_image_drm_format_modifier", 159, 2 },
   { "VK_EXT_image_robustness", 336, 1 },
   { "VK_EXT_image_view_min_lod", 392, 1 },
   { "VK_EXT_index_type_uint8", 266, 1 },
   { "VK_EXT_inline_uniform_block", 139, 1 },
   { "VK_EXT_line_rasterization", 260, 1 },
   { "VK_EXT_load_store_op_none", 401, 1 },
   { "VK_EXT_memory_budget", 238, 1 },
   { "VK_EXT_multi_draw", 393, 1 },
   { "VK_EXT_mutable_descriptor_type", 495, 1 },
   { "VK_EXT_non_seamless_cube_map", 423, 1 },
   { "VK_EXT_pci_bus_info", 213, 2 },
   { "VK_EXT_pipeline_creation_cache_control", 298, 3 },
   { "VK_EXT_pipeline_creation_feedback", 193, 1 },
   { "VK_EXT_primitive_topology_list_restart", 357, 1 },
   { "VK_EXT_primitives_generated_query", 383, 1 },
   { "VK_EXT_private_data", 296, 1 },
   { "VK_EXT_provoking_vertex", 255, 1 },
   { "VK_EXT_queue_family_foreign", 127, 1 },
   { "VK_EXT_rasterization_order_attachment_access", 464, 1 },
   { "VK_EXT_robustness2", 287, 1 },
   { "VK_EXT_sampler_filter_minmax", 131, 2 },
   { "VK_EXT_scalar_block_layout", 222, 1 },
   { "VK_EXT_separate_stencil_usage", 247, 1 },
   { "VK_EXT_shader_demote_to_helper_invocation", 277, 1 },
   { "VK_EXT_shader_stencil_export", 141, 1 },
   { "VK_EXT_shader_subgroup_ballot", 65, 1 },
   { "VK_EXT_shader_viewport_index_layer", 163, 1 },
   { "VK_EXT_subgroup_size_control", 226, 2 },
   { "VK_EXT_texel_buffer_alignment", 282, 1 },
   { "VK_EXT_texture_compression_astc_hdr", 67, 1 },
   { "VK_EXT_tooling_info", 246, 1 },
   { "VK_EXT_transform_feedback", 29, 1 },
   { "VK_EXT_vertex_attribute_divisor", 191, 3 },
   { "VK_EXT_vertex_input_dynamic_state", 353, 2 },
   { "VK_EXT_ycbcr_2plane_444_formats", 331, 1 },
   { "VK_KHR_16bit_storage", 84, 1 },
   { "VK_KHR_8bit_storage", 178, 1 },
   { "VK_KHR_bind_memory2", 158, 1 },
   { "VK_KHR_buffer_device_address", 258, 1 },
   { "VK_KHR_copy_commands2", 338, 1 },
   { "VK_KHR_create_renderpass2", 110, 1 },
   { "VK_KHR_dedicated_allocation", 128, 3 },
   { "VK_KHR_depth_stencil_resolve", 200, 1 },
   { "VK_KHR_descriptor_update_template", 86, 1 },
   { "VK_KHR_device_group", 61, 4 },
   { "VK_KHR_device_group_creation", 71, 1 },
   { "VK_KHR_draw_indirect_count", 170, 1 },
   { "VK_KHR_driver_properties", 197, 1 },
   { "VK_KHR_dynamic_rendering", 45, 1 },
   { "VK_KHR_external_fence", 114, 1 },
   { "VK_KHR_external_fence_capabilities", 113, 1 },
   { "VK_KHR_external_fence_fd", 116, 1 },
   { "VK_KHR_external_memory", 73, 1 },
   { "VK_KHR_external_memory_capabilities", 72, 1 },
   { "VK_KHR_external_memory_fd", 75, 1 },
   { "VK_KHR_external_semaphore", 78, 1 },
   { "VK_KHR_external_semaphore_capabilities", 77, 1 },
   { "VK_KHR_external_semaphore_fd", 80, 1 },
   { "VK_KHR_format_feature_flags2", 361, 2 },
   { "VK_KHR_get_memory_requirements2", 147, 1 },
   { "VK_KHR_get_physical_device_properties2", 60, 2 },
   { "VK_KHR_image_format_list", 148, 1 },
   { "VK_KHR_imageless_framebuffer", 109, 1 },
   { "VK_KHR_maintenance1", 70, 2 },
   { "VK_KHR_maintenance2", 118, 1 },
   { "VK_KHR_maintenance3", 169, 1 },
   { "VK_KHR_maintenance4", 414, 2 },
   { "VK_KHR_multiview", 54, 1 },
   { "VK_KHR_pipeline_library", 291, 1 },
   { "VK_KHR_push_descriptor", 81, 2 },
   { "VK_KHR_relaxed_block_layout", 145, 1 },
   { "VK_KHR_sampler_mirror_clamp_to_edge", 15, 3 },
   { "VK_KHR_sampler_ycbcr_conversion", 157, 14 },
   { "VK_KHR_separate_depth_stencil_layouts", 242, 1 },
   { "VK_KHR_shader_atomic_int64", 181, 1 },
   { "VK_KHR_shader_clock", 182, 1 },
   { "VK_KHR_shader_draw_parameters", 64, 1 },
   { "VK_KHR_shader_float16_int8", 83, 1 },
   { "VK_KHR_shader_float_controls", 198, 4 },
   { "VK_KHR_shader_integer_dot_product", 281, 1 },
   { "VK_KHR_shader_non_semantic_info", 294, 1 },
   { "VK_KHR_shader_subgroup_extended_types", 176, 1 },
   { "VK_KHR_shader_terminate_invocation", 216, 1 },
   { "VK_KHR_spirv_1_4", 237, 1 },
   { "VK_KHR_storage_buffer_storage_class", 132, 1 },
   { "VK_KHR_synchronization2", 315, 1 },
   { "VK_KHR_timeline_semaphore", 208, 2 },
   { "VK_KHR_uniform_buffer_standard_layout", 254, 1 },
   { "VK_KHR_variable_pointers", 121, 1 },
   { "VK_KHR_vulkan_memory_model", 212, 3 },
   { "VK_KHR_zero_initialize_workgroup_memory", 326, 1 },
   { "VK_MESA_venus_protocol", 385, 1 },
   { "VK_VALVE_mutable_descriptor_type", 352, 1 },
};

static inline uint32_t
vn_info_wire_format_version(void)
{
    return 1;
}

static inline uint32_t
vn_info_vk_xml_version(void)
{
    return VK_MAKE_API_VERSION(0, 1, 3, 269);
}

static inline int
vn_info_extension_compare(const void *name, const void *ext)
{
   return strcmp(name, ((const struct vn_info_extension *)ext)->name);
}

static inline int32_t
vn_info_extension_index(const char *name)
{
   const struct vn_info_extension *ext = bsearch(name, _vn_info_extensions,
      _vn_info_extension_count, sizeof(*_vn_info_extensions),
      vn_info_extension_compare);
   return ext ? ext - _vn_info_extensions : -1;
}

static inline const struct vn_info_extension *
vn_info_extension_get(int32_t index)
{
   assert(index >= 0 && (uint32_t)index < _vn_info_extension_count);
   return &_vn_info_extensions[index];
}

static inline void
vn_info_extension_mask_init(uint32_t *out_mask)
{
   for (uint32_t i = 0; i < _vn_info_extension_count; i++) {
       out_mask[_vn_info_extensions[i].number / 32] |= 1 << (_vn_info_extensions[i].number % 32);
   }
}

#endif /* VN_PROTOCOL_RENDERER_INFO_H */
