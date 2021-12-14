/*************************************************************************/
/*  openxr_device.h                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef OPENXR_DRIVER_H
#define OPENXR_DRIVER_H

#ifdef OPENXR_DUMMY
// if OpenXR is not supported on the platform, we still have other parts of Godot
// attempting to use the OpenXR driver, so we need a dummy

class OpenXRDevice {
public:
	static void setup_global_defs(){};
	static bool openxr_is_enabled() { return false; };
	static OpenXRDevice *get_singleton() { return nullptr; };

	bool is_initialized() { return false; };
	bool is_running() { return false; };
	bool initialise(const String &p_rendering_driver) { return false; };
	bool initialise_session() { return false; };
	void finish(){};
};

#else

#include "core/error/error_macros.h"
#include "core/math/camera_matrix.h"
#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/os/memory.h"
#include "core/string/ustring.h"
#include "core/templates/map.h"
#include "core/templates/vector.h"

#include "thirdparty/openxr/src/common/xr_linear.h"
#include <openxr/openxr.h>

#include "openxr_composition_layer_provider.h"
#include "openxr_extension_wrapper.h"

// Note, OpenXR code that we wrote for our plugin makes use of C++20 notation for initialising structs which ensures zeroing out unspecified members.
// Godot is currently restricted to C++17 which doesn't allow this notation. Make sure critical fields are set.

// forward declarations, we don't want to include these fully
class OpenXRVulkanExtension;

class OpenXRDevice {
public:
	enum TrackingConfidence {
		TRACKING_CONFIDENCE_NO,
		TRACKING_CONFIDENCE_LOW,
		TRACKING_CONFIDENCE_HIGH
	};

private:
	// our singleton
	static OpenXRDevice *singleton;

	// layers
	uint32_t num_layer_properties = 0;
	XrApiLayerProperties *layer_properties = nullptr;

	// extensions
	uint32_t num_supported_extensions = 0;
	XrExtensionProperties *supported_extensions = nullptr;
	Vector<OpenXRExtensionWrapper *> registered_extension_wrappers;
	Vector<const char *> enabled_extensions;

	// composition layer providers
	Vector<OpenXRCompositionLayerProvider *> composition_layer_providers;

	// view configuration
	uint32_t num_view_configuration_types = 0;
	XrViewConfigurationType *supported_view_configuration_types = nullptr;

	// reference spaces
	uint32_t num_reference_spaces = 0;
	XrReferenceSpaceType *supported_reference_spaces = nullptr;

	// swapchains (note these are platform dependent)
	uint32_t num_swapchain_formats = 0;
	int64_t *supported_swapchain_formats = nullptr;

	// configuration
	XrFormFactor form_factor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrViewConfigurationType view_configuration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	XrReferenceSpaceType reference_space = XR_REFERENCE_SPACE_TYPE_STAGE;
	XrEnvironmentBlendMode environment_blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

	// state
	XrInstance instance = XR_NULL_HANDLE;
	XrSystemId system_id;
	String system_name;
	uint32_t vendor_id;
	XrSystemTrackingProperties tracking_properties;
	XrSession session = XR_NULL_HANDLE;
	XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;
	bool running = false;
	XrFrameState frame_state = {};

	OpenXRGraphicsExtensionWrapper *graphics_extension = nullptr;
	XrSystemGraphicsProperties graphics_properties;
	void *swapchain_graphics_data = nullptr;
	uint32_t image_index = 0;
	bool image_acquired = false;

	uint32_t view_count = 0;
	XrViewConfigurationView *view_configuration_views = nullptr;
	XrView *views = nullptr;
	XrCompositionLayerProjectionView *projection_views = nullptr;
	XrSwapchain swapchain = XR_NULL_HANDLE;

	XrSpace play_space = XR_NULL_HANDLE;
	XrSpace view_space = XR_NULL_HANDLE;
	bool view_pose_valid = false;
	TrackingConfidence head_pose_confidence = TRACKING_CONFIDENCE_NO;

	bool load_layer_properties();
	bool load_supported_extensions();
	bool is_extension_supported(const char *p_extension) const;

	// instance
	bool create_instance();
	bool get_system_info();
	bool load_supported_view_configuration_types();
	bool is_view_configuration_supported(XrViewConfigurationType p_configuration_type) const;
	bool load_supported_view_configuration_views(XrViewConfigurationType p_configuration_type);
	void destroy_instance();

	// session
	bool create_session();
	bool load_supported_reference_spaces();
	bool is_reference_space_supported(XrReferenceSpaceType p_reference_space);
	bool setup_spaces();
	bool load_supported_swapchain_formats();
	bool is_swapchain_format_supported(int64_t p_swapchain_format);
	bool create_main_swapchain();
	void destroy_session();

	// swapchains
	bool create_swapchain(int64_t p_swapchain_format, uint32_t p_width, uint32_t p_height, uint32_t p_sample_count, uint32_t p_array_size, XrSwapchain &r_swapchain, void **r_swapchain_graphics_data);
	bool acquire_image(XrSwapchain p_swapchain, uint32_t &r_image_index);
	bool release_image(XrSwapchain p_swapchain);

	// state changes
	bool poll_events();
	bool on_state_idle();
	bool on_state_ready();
	bool on_state_synchronized();
	bool on_state_visible();
	bool on_state_focused();
	bool on_state_stopping();
	bool on_state_loss_pending();
	bool on_state_exiting();

protected:
	friend class OpenXRVulkanExtension;

	XrInstance get_instance() const { return instance; };
	XrSystemId get_system_id() const { return system_id; };
	XrSession get_session() const { return session; };

	// helper method to convert an XrPosef to a Transform3D
	Transform3D transform_from_pose(const XrPosef &p_pose);

	// helper method to get a valid Transform3D from an openxr space location
	TrackingConfidence transform_from_location(const XrSpaceLocation &p_location, Transform3D &r_transform);
	TrackingConfidence transform_from_location(const XrHandJointLocationEXT &p_location, Transform3D &r_transform);
	void parse_velocities(const XrSpaceVelocity &p_velocity, Vector3 &r_linear_velocity, Vector3 r_angular_velocity);

public:
	static void setup_global_defs();
	static bool openxr_is_enabled();
	static OpenXRDevice *get_singleton();

	String get_error_string(XrResult result);
	String get_view_configuration_name(XrViewConfigurationType p_view_configuration) const;
	String get_reference_space_name(XrReferenceSpaceType p_reference_space) const;
	String get_swapchain_format_name(int64_t p_swapchain_format) const;
	String get_structure_type_name(XrStructureType p_structure_type) const;
	String get_session_state_name(XrSessionState p_session_state) const;
	String make_xr_version_string(XrVersion p_version);

	void register_extension_wrapper(OpenXRExtensionWrapper *p_extension_wrapper);

	bool is_initialized();
	bool is_running();
	bool initialise(const String &p_rendering_driver);
	bool initialise_session();
	void finish();

	XrTime get_next_frame_time() { return frame_state.predictedDisplayTime + frame_state.predictedDisplayPeriod; };
	bool can_render() { return instance != XR_NULL_HANDLE && session != XR_NULL_HANDLE && running && view_pose_valid && frame_state.shouldRender; };

	Size2 get_recommended_target_size();
	TrackingConfidence get_head_center(Transform3D &r_transform, Vector3 &r_linear_velocity, Vector3 &r_angular_velocity);
	bool get_view_transform(uint32_t p_view, Transform3D &r_transform);
	bool get_view_projection(uint32_t p_view, double p_z_near, double p_z_far, CameraMatrix &p_camera_matrix);
	bool process();

	void pre_render();
	bool pre_draw_viewport(RID p_render_target);
	void post_draw_viewport(RID p_render_target);
	void end_frame();

	String get_default_action_map_resource_name();

	OpenXRDevice();
	~OpenXRDevice();
};

#endif

#endif // !OPENXR_DRIVER_H
