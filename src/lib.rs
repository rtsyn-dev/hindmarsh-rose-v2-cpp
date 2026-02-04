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
const OUTPUTS: &[&str] = &["x", "y", "z"];

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
    if handle.is_null() {
        return;
    }
    unsafe {
        drop(Box::from_raw(handle as *mut HrStateCpp));
    }
}

extern "C" fn meta_json(_handle: *mut c_void) -> PluginString {
    let value = serde_json::json!({
        "name": "Hindmarsh Rose Dyn C++",
        "kind": "hindmarsh_rose_dyn_cpp"
    });
    PluginString::from_string(value.to_string())
}

extern "C" fn inputs_json(_handle: *mut c_void) -> PluginString {
    PluginString::from_string(serde_json::to_string(INPUTS).unwrap_or_default())
}

extern "C" fn outputs_json(_handle: *mut c_void) -> PluginString {
    PluginString::from_string(serde_json::to_string(OUTPUTS).unwrap_or_default())
}

extern "C" fn set_config_json(handle: *mut c_void, data: *const u8, len: usize) {
    if handle.is_null() || data.is_null() || len == 0 {
        return;
    }
    let slice = unsafe { std::slice::from_raw_parts(data, len) };
    if let Ok(json) = serde_json::from_slice::<Value>(slice) {
        let state = handle as *mut HrStateCpp;
        if let Some(map) = json.as_object() {
            for (key, value) in map {
                if let Some(num) = value.as_f64() {
                    unsafe {
                        hr_set_config(state, key.as_bytes().as_ptr(), key.len(), num);
                    }
                }
            }
        }
    }
}

extern "C" fn set_input(handle: *mut c_void, name: *const u8, len: usize, value: f64) {
    if handle.is_null() || name.is_null() || len == 0 {
        return;
    }
    let slice = unsafe { std::slice::from_raw_parts(name, len) };
    if let Ok(name) = std::str::from_utf8(slice) {
        unsafe {
            hr_set_input(
                handle as *mut HrStateCpp,
                name.as_bytes().as_ptr(),
                name.len(),
                value,
            );
        }
    }
}

extern "C" fn process(handle: *mut c_void, _tick: u64, period_seconds: f64) {
    if handle.is_null() {
        return;
    }
    let state = handle as *mut HrStateCpp;
    
    // Update period_seconds from runtime to respect workspace settings
    unsafe {
        if ((*state).period_seconds - period_seconds).abs() > f64::EPSILON {
            hr_set_config(state, b"period_seconds".as_ptr(), 14, period_seconds);
        }
        hr_process(state);
    }
}

extern "C" fn get_output(handle: *mut c_void, name: *const u8, len: usize) -> f64 {
    if handle.is_null() || name.is_null() || len == 0 {
        return 0.0;
    }
    let slice = unsafe { std::slice::from_raw_parts(name, len) };
    if let Ok(name) = std::str::from_utf8(slice) {
        let state = unsafe { &*(handle as *mut HrStateCpp) };
        return match name {
            "x" => state.x,
            "y" => state.y,
            "z" => state.z,
            _ => 0.0,
        };
    }
    0.0
}

#[no_mangle]
pub extern "C" fn rtsyn_plugin_api() -> *const PluginApi {
    static API: PluginApi = PluginApi {
        create,
        destroy,
        meta_json,
        inputs_json,
        outputs_json,
        set_config_json,
        set_input,
        process,
        get_output,
    };
    &API as *const PluginApi
}
