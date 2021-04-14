
use std::mem::size_of;
use std::thread::sleep;
use libc::c_uchar;
use libc::{c_int, c_void};

#[repr(C)]
#[warn(improper_ctypes)]
struct VideoParam {
    width: u32,
    height: u32,
    fps: u32,
    file_or_stream_flag: u32,
    file_or_stream_url: String,
}
#[repr(C)]
#[warn(improper_ctypes)]
struct Video;


extern "C" {
    fn video_malloc(x:  &mut *mut Video) -> c_int;
    fn video_init(x: *mut Video, video_param: *mut VideoParam) -> c_int;
    fn rgb_to_yuv(x: *mut Video, rgb_buf: *mut c_uchar) -> c_int;
    fn generate_rgb(x: *mut Video, rgb_buf: *mut c_uchar) -> c_void;
    fn write_trail(x: *mut Video) -> c_int;
    fn video_free(x: &mut *mut Video) -> c_int;
}

fn main() {

    //let mut video_param_ptr: *mut VideoParam = std::ptr::null_mut();
    let mut video_ptr: *mut Video = std::ptr::null_mut();
    let mut video_param = VideoParam {
        width: 1820,
        height: 720,
        fps: 25,
        file_or_stream_flag: 1,
        file_or_stream_url: String::from("rtmp://192.168.31.66:1935/live/demo.flv"),
    };
    unsafe {
        let _c = video_malloc(&mut video_ptr);
        video_init(video_ptr, &mut video_param);
    }

    let mut rgb: Vec<u8> = Vec::with_capacity(3 * size_of::<u8>() * 1820 * 720);
    let d = std::time::Duration::from_micros(50);
    unsafe {
        for _x in 0..50000 {
            sleep(d);
            generate_rgb(video_ptr, rgb.as_mut_ptr());
            rgb_to_yuv(video_ptr, rgb.as_mut_ptr());
        }
        write_trail(video_ptr);
    }

    unsafe {
        let _c = video_free(&mut video_ptr);
    }
}
