use rtsyn_plugin::{PluginApi, PluginString};
use serde_json::Value;
use std::ffi::c_void;

#[repr(C)]
struct HrStateCpp {
    x: f64,
    y: f64,
    z: f64,
    input_syn: f64,
    e: f64,
    mu: f64,
    s: f64,
    vh: f64,
    dt: f64,
    burst_duration: f64,
    period_seconds: f64,
    s_points: usize,
    cfg_x: f64,
    cfg_y: f64,
    cfg_z: f64,
}

extern "C" {
    fn hr_init(state: *mut HrStateCpp);
    fn hr_set_config(state: *mut HrStateCpp, key: *const u8, len: usize, value: f64);
    fn hr_set_input(state: *mut HrStateCpp, name: *const u8, len: usize, value: f64);
    fn hr_process(state: *mut HrStateCpp);
}

const INPUTS: &[&str] = &["i_syn"];
const OUTPUTS: &[&str] = &["Membrane potential (V)", "Membrane potential (mV)"];

extern "C" fn create(_id: u64) -> *mut c_void {
    let mut state = Box::new(HrStateCpp {
        x: 0.0,
        y: 0.0,
        z: 0.0,
        input_syn: 0.0,
        e: 0.0,
        mu: 0.0,
        s: 0.0,
        vh: 0.0,
        dt: 0.0,
        burst_duration: 0.0,
        period_seconds: 0.0,
        s_points: 1,
        cfg_x: 0.0,
        cfg_y: 0.0,
        cfg_z: 0.0,
    });
    unsafe {
        hr_init(&mut *state);
    }
    Box::into_raw(state) as *mut c_void
}

extern "C" fn destroy(handle: *mut c_void) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle as *mut HrStateCpp)); }
    }
}

extern "C" fn meta_json(_handle: *mut c_void) -> PluginString {
    let value = serde_json::json!({
        "name": "Hindmarsh Rose v2 C++",
        "default_vars": [
            ["x", -0.9013],
            ["y", -3.1594],
            ["z", 3.24782],
            ["e", 3.0],
            ["mu", 0.006],
            ["s", 4.0],
            ["vh", 1.0],
            ["burst_duration", 1.0]
        ]
    });
    PluginString::from_string(value.to_string())
}

extern "C" fn inputs_json(_handle: *mut c_void) -> PluginString {
    PluginString::from_string(serde_json::to_string(INPUTS).unwrap_or_default())
}

extern "C" fn outputs_json(_handle: *mut c_void) -> PluginString {
    PluginString::from_string(serde_json::to_string(OUTPUTS).unwrap_or_default())
}

extern "C" fn behavior_json(_handle: *mut c_void) -> PluginString {
    let behavior = serde_json::json!({
        "supports_start_stop": true,
        "supports_restart": true,
        "extendable_inputs": {"type": "none"},
        "loads_started": true
    });
    PluginString::from_string(behavior.to_string())
}

extern "C" fn ui_schema_json(_handle: *mut c_void) -> PluginString {
    let schema = serde_json::json!({
        "outputs": ["Membrane potential (V)", "Membrane potential (mV)"],
        "inputs": ["i_syn"],
        "variables": ["x", "y", "z"]
    });
    PluginString::from_string(schema.to_string())
}

extern "C" fn set_config_json(handle: *mut c_void, data: *const u8, len: usize) {
    if handle.is_null() || data.is_null() {
        return;
    }
    let state = handle as *mut HrStateCpp;
    let json_slice = unsafe { std::slice::from_raw_parts(data, len) };
    if let Ok(json_str) = std::str::from_utf8(json_slice) {
        if let Ok(config) = serde_json::from_str::<Value>(json_str) {
            for (key, value) in config.as_object().unwrap_or(&serde_json::Map::new()) {
                if let Some(val) = value.as_f64() {
                    unsafe {
                        hr_set_config(state, key.as_bytes().as_ptr(), key.len(), val);
                    }
                }
            }
        }
    }
}

extern "C" fn set_input(handle: *mut c_void, name: *const u8, len: usize, value: f64) {
    if handle.is_null() || name.is_null() {
        return;
    }
    unsafe {
        hr_set_input(handle as *mut HrStateCpp, name, len, value);
    }
}

extern "C" fn process(handle: *mut c_void, _tick: u64, _period_seconds: f64) {
    if handle.is_null() {
        return;
    }
    unsafe {
        hr_process(handle as *mut HrStateCpp);
    }
}

extern "C" fn get_output(handle: *mut c_void, name: *const u8, len: usize) -> f64 {
    if handle.is_null() || name.is_null() {
        return 0.0;
    }
    let state = unsafe { &*(handle as *const HrStateCpp) };
    let name_slice = unsafe { std::slice::from_raw_parts(name, len) };
    if let Ok(name_str) = std::str::from_utf8(name_slice) {
        match name_str {
            "x" => state.x,
            "y" => state.y,
            "z" => state.z,
            "Membrane potential (V)" => state.x,
            "Membrane potential (mV)" => state.x * 1000.0,
            _ => 0.0,
        }
    } else {
        0.0
    }
}

#[no_mangle]
pub extern "C" fn rtsyn_plugin_api() -> *const PluginApi {
    static API: PluginApi = PluginApi {
        create,
        destroy,
        meta_json,
        inputs_json,
        outputs_json,
        behavior_json: Some(behavior_json),
        ui_schema_json: Some(ui_schema_json),
        set_config_json,
        set_input,
        process,
        get_output,
    };
    &API
}
