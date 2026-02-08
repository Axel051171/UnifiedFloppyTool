/**
 * @file uft_operations.c
 * @brief High-Level Operations API Implementation
 * 
 * Tool-unabhängige Schnittstelle für alle Disk-Operationen.
 */

#include "uft/uft_operations.h"
#include "uft/uft_tool_adapter.h"
#include "uft/uft_decoder_registry.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// API Version
// ============================================================================

bool uft_ops_api_compatible(uint32_t client_version) {
    uint32_t major = (client_version >> 16) & 0xFFFF;
    return (major == UFT_OPS_API_VERSION_MAJOR);
}

// ============================================================================
// Device Management
// ============================================================================

static struct {
    uft_device_info_t devices[16];
    size_t device_count;
    int selected_device;
} g_device_state = {
    .selected_device = -1
};

uft_error_t uft_scan_devices(uft_device_info_t* devices, 
                              size_t max_devices,
                              size_t* actual_count) {
    if (!devices || !actual_count) return UFT_ERROR_NULL_POINTER;
    
    g_device_state.device_count = 0;
    
    // Query each tool adapter for hardware
    const uft_tool_adapter_t* tools[16];
    size_t tool_count = uft_tool_list(tools, 16);
    
    for (size_t i = 0; i < tool_count && g_device_state.device_count < 16; i++) {
        const uft_tool_adapter_t* tool = tools[i];
        
        if (tool->capabilities & UFT_TOOL_CAP_HARDWARE) {
            if (tool->is_available && tool->is_available()) {
                char info[256] = {0};
                
                if (tool->detect_hardware && tool->detect_hardware(info, sizeof(info))) {
                    uft_device_info_t* dev = &g_device_state.devices[g_device_state.device_count];
                    
                    dev->index = (int)g_device_state.device_count;
                    snprintf(dev->name, sizeof(dev->name), "%s", tool->name);
                    snprintf(dev->firmware, sizeof(dev->firmware), "%s", tool->version);
                    dev->capabilities = tool->capabilities;
                    dev->connected = true;
                    
                    // Parse port from info if available
                    if (strstr(info, "/dev/")) {
                        char* p = strstr(info, "/dev/");
                        snprintf(dev->port, sizeof(dev->port), "%.31s", p);
                    } else if (strstr(info, "COM")) {
                        char* p = strstr(info, "COM");
                        snprintf(dev->port, sizeof(dev->port), "%.31s", p);
                    }
                    
                    g_device_state.device_count++;
                }
            }
        }
    }
    
    // Copy to output
    size_t copy_count = (g_device_state.device_count < max_devices) ?
                         g_device_state.device_count : max_devices;
    
    memcpy(devices, g_device_state.devices, copy_count * sizeof(uft_device_info_t));
    *actual_count = copy_count;
    
    return UFT_OK;
}

uft_error_t uft_select_device(int device_index) {
    if (device_index < 0 || (size_t)device_index >= g_device_state.device_count) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    g_device_state.selected_device = device_index;
    return UFT_OK;
}

// ============================================================================
// Disk Operations
// ============================================================================

uft_error_t uft_read_disk(int device_id,
                           const uft_tool_read_params_t* params,
                           uft_unified_image_t* output) {
    if (!output) return UFT_ERROR_NULL_POINTER;
    
    // Find appropriate tool
    const uft_tool_adapter_t* tool = uft_tool_find_for_operation(
        UFT_TOOL_CAP_READ | UFT_TOOL_CAP_HARDWARE);
    
    if (!tool) {
        return UFT_ERROR_NO_DEVICE;
    }
    
    if (!tool->read_disk) {
        return UFT_ERROR_NOT_IMPLEMENTED;
    }
    
    // Initialize tool if needed
    void* context = NULL;
    if (tool->init) {
        uft_error_t err = tool->init(&context);
        if (err != UFT_OK) return err;
    }
    
    // Perform read
    uft_error_t result = tool->read_disk(context, params, output);
    
    // Cleanup
    if (tool->cleanup) {
        tool->cleanup(context);
    }
    
    return result;
}

uft_error_t uft_write_disk(int device_id,
                            const uft_tool_write_params_t* params,
                            const uft_unified_image_t* input) {
    if (!input) return UFT_ERROR_NULL_POINTER;
    
    const uft_tool_adapter_t* tool = uft_tool_find_for_operation(
        UFT_TOOL_CAP_WRITE | UFT_TOOL_CAP_HARDWARE);
    
    if (!tool || !tool->write_disk) {
        return UFT_ERROR_NOT_IMPLEMENTED;
    }
    
    void* context = NULL;
    if (tool->init) {
        uft_error_t err = tool->init(&context);
        if (err != UFT_OK) return err;
    }
    
    uft_error_t result = tool->write_disk(context, params, input);
    
    if (tool->cleanup) {
        tool->cleanup(context);
    }
    
    return result;
}

// ============================================================================
// Image Operations
// ============================================================================

uft_error_t uft_open_image(const char* path, uft_unified_image_t* output) {
    if (!path || !output) return UFT_ERROR_NULL_POINTER;
    
    return uft_image_open(output, path);
}

uft_error_t uft_save_image(const uft_unified_image_t* image,
                            const char* path,
                            uft_format_t format) {
    if (!image || !path) return UFT_ERROR_NULL_POINTER;
    
    // Cast away const - save modifies internal state
    uft_unified_image_t* img = (uft_unified_image_t*)image;
    return uft_image_save(img, path, format);
}

uft_error_t uft_convert_image(const uft_unified_image_t* input,
                               uft_format_t target_format,
                               uft_unified_image_t* output) {
    if (!input || !output) return UFT_ERROR_NULL_POINTER;
    
    // First try using libflux_ctx or similar converter tool
    const uft_tool_adapter_t* tool = uft_tool_find_for_operation(UFT_TOOL_CAP_CONVERT);
    
    if (tool && tool->convert && input->path) {
        // Create temp output path
        char temp_path[512];
        snprintf(temp_path, sizeof(temp_path), "/tmp/uft_convert_%d.tmp", (int)getpid());
        
        uft_error_t err = tool->convert(NULL, input->path, temp_path, target_format);
        if (err == UFT_OK) {
            err = uft_image_open(output, temp_path);
            remove(temp_path);
            return err;
        }
    }
    
    // Fallback to internal conversion
    return uft_image_convert(input, target_format, output);
}

// ============================================================================
// Format Detection
// ============================================================================

uft_error_t uft_detect_format(const char* path, 
                               uft_format_t* format,
                               int* confidence) {
    if (!path || !format) return UFT_ERROR_NULL_POINTER;
    
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[4096];
    size_t header_size = fread(header, 1, sizeof(header), f);
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t file_size = (size_t)ftell(f);
    fclose(f);
    
    return uft_detect_format_from_data(header, header_size, format, confidence);
}

uft_error_t uft_detect_format_from_data(const uint8_t* data, 
                                         size_t size,
                                         uft_format_t* format,
                                         int* confidence) {
    if (!data || !format) return UFT_ERROR_NULL_POINTER;
    
    *format = UFT_FORMAT_UNKNOWN;
    int best_confidence = 0;
    
    // Try all registered format plugins
    extern const uft_format_plugin_t* uft_format_plugins[];
    extern size_t uft_format_plugin_count;
    
    for (size_t i = 0; i < uft_format_plugin_count; i++) {
        const uft_format_plugin_t* plugin = uft_format_plugins[i];
        if (plugin && plugin->probe) {
            int plugin_confidence = 0;
            if (plugin->probe(data, size, size, &plugin_confidence)) {
                if (plugin_confidence > best_confidence) {
                    best_confidence = plugin_confidence;
                    *format = plugin->format;
                }
            }
        }
    }
    
    if (confidence) *confidence = best_confidence;
    
    return (*format != UFT_FORMAT_UNKNOWN) ? UFT_OK : UFT_ERROR_FORMAT_UNKNOWN;
}

// ============================================================================
// Initialization
// ============================================================================

static bool g_ops_initialized = false;

void uft_ops_init(void) {
    if (g_ops_initialized) return;
    
    // Initialize subsystems
    extern void uft_register_builtin_decoders(void);
    extern void uft_register_builtin_tools(void);
    
    uft_register_builtin_decoders();
    uft_register_builtin_tools();
    
    g_ops_initialized = true;
}

void uft_ops_cleanup(void) {
    g_ops_initialized = false;
}
